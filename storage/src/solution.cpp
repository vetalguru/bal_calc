#include <ballistics/storage/solution.h>

#include <cmath>
#include <exception>
#include <string>
#include <utility>

#include <ballistics/effects.h>
#include <ballistics/storage/repository.h>

namespace ballistics::storage {

namespace {

using sqlite_manager::Error;
using sqlite_manager::ErrorCode;

Error DataError(std::string message) { return Error(ErrorCode::kError, 0, std::move(message)); }

template <typename T>
Result<T> Require(Database& db, Id id, const char* what) {
    auto r = Repository<T>(db).Get(id);
    if (!r) {
        return r.error();
    }
    if (!r.value()) {
        return DataError(std::string(what) + " " + std::to_string(id) + " not found");
    }
    return std::move(*r.value());
}

std::optional<DragTableId> TableByName(const std::string& name) {
    for (auto id : {DragTableId::kG1, DragTableId::kG2, DragTableId::kG5, DragTableId::kG6,
                    DragTableId::kG7, DragTableId::kG8, DragTableId::kGI, DragTableId::kGS,
                    DragTableId::kRA4}) {
        if (name == DragTableName(id)) {
            return id;
        }
    }
    return std::nullopt;
}

} // namespace

Result<LoadedProfile> LoadProfile(Database& db, Id profile_id) {
    LoadedProfile p;
    {
        auto r = Require<ProfileRecord>(db, profile_id, "profile");
        if (!r) {
            return r.error();
        }
        p.profile = std::move(r).value();
    }
    {
        auto r = Require<RifleRecord>(db, p.profile.rifle_id, "rifle");
        if (!r) {
            return r.error();
        }
        p.rifle = std::move(r).value();
    }
    if (p.profile.scope_id) {
        auto r = Require<ScopeRecord>(db, *p.profile.scope_id, "scope");
        if (!r) {
            return r.error();
        }
        p.scope = std::move(r).value();
    }
    {
        auto r = Require<CartridgeRecord>(db, p.profile.cartridge_id, "cartridge");
        if (!r) {
            return r.error();
        }
        p.cartridge = std::move(r).value();
    }
    {
        auto r = Require<BulletRecord>(db, p.cartridge.bullet_id, "bullet");
        if (!r) {
            return r.error();
        }
        p.bullet = std::move(r).value();
    }
    if (p.bullet.curve_id) {
        auto r = Require<DragCurveRecord>(db, *p.bullet.curve_id, "drag curve");
        if (!r) {
            return r.error();
        }
        p.curve = std::move(r).value();
    }
    return p;
}

Result<DragModel> MakeDragModel(const BulletRecord& b, const DragCurveRecord* curve) {
    try {
        if (b.drag_kind == kDragKindCurve) {
            if (curve == nullptr) {
                return DataError("bullet '" + b.name + "' has no drag curve");
            }
            return DragModel::FromCurve(curve->points, b.mass_kg, b.diameter_m, b.form_factor);
        }
        const auto table = TableByName(b.drag_table);
        if (!table) {
            return DataError("bullet '" + b.name + "': unknown drag table " + b.drag_table);
        }
        if (b.drag_kind == kDragKindMultiBc) {
            return DragModel::FromMultiBc(*table, b.bc_bands);
        }
        if (!b.bc) {
            return DataError("bullet '" + b.name + "' has no BC");
        }
        return DragModel::FromBc(*table, *b.bc);
    } catch (const std::exception& e) {
        return DataError("bullet '" + b.name + "': " + e.what());
    }
}

double MuzzleVelocity(const CartridgeRecord& c, double powder_temp_k, double velocity_scale) {
    PowderSensitivity powder;
    powder.reference_velocity_mps = c.muzzle_velocity_mps;
    powder.reference_temperature_k = c.reference_powder_temp_k;
    powder.fraction_per_kelvin = c.powder_sensitivity_per_k;
    powder.table = c.velocity_points;
    return MuzzleVelocityAt(powder, powder_temp_k) * velocity_scale;
}

Result<Solution> Solve(const LoadedProfile& p, const ConditionsRecord& conditions,
                       double max_range_m, const SolverOptions& options) {
    auto drag = MakeDragModel(p.bullet, p.curve ? &*p.curve : nullptr);
    if (!drag) {
        return drag.error();
    }

    Shot base;
    base.drag = drag.value().Scaled(p.profile.drag_scale);
    base.mass_kg = p.bullet.mass_kg;
    base.bullet_diameter_m = p.bullet.diameter_m;
    base.bullet_length_m = p.bullet.length_m;
    base.twist_m = p.rifle.twist_m;
    base.sight_height_m = p.rifle.sight_height_m;

    // Zero: the profile's zero conditions, level, still air.
    Shot zero_shot = base;
    zero_shot.atmosphere = p.profile.zero_atmosphere;
    zero_shot.muzzle_velocity_mps =
        MuzzleVelocity(p.cartridge, p.profile.zero_powder_temp_k, p.profile.velocity_scale);
    if (conditions.latitude_rad) {
        zero_shot.gravity_mps2 =
            LocalGravity(*conditions.latitude_rad, zero_shot.atmosphere.altitude_m);
    }
    const ZeroResult zero = FindZero(zero_shot, p.profile.zero_range_m, p.profile.zero_offset_up_m,
                                     options, p.profile.zero_offset_right_m);
    if (!zero.converged) {
        return DataError("could not zero at " + std::to_string(p.profile.zero_range_m) + " m");
    }

    Shot shot = base;
    shot.atmosphere = conditions.atmosphere;
    shot.muzzle_velocity_mps =
        MuzzleVelocity(p.cartridge,
                       conditions.powder_temp_k.value_or(conditions.atmosphere.temperature_k),
                       p.profile.velocity_scale);
    shot.look_angle_rad = conditions.look_angle_rad;
    shot.cant_rad = conditions.cant_rad;
    shot.winds = conditions.winds;
    shot.latitude_rad = conditions.latitude_rad;
    shot.azimuth_rad = conditions.azimuth_rad;
    if (conditions.latitude_rad) {
        shot.gravity_mps2 = LocalGravity(*conditions.latitude_rad, shot.atmosphere.altitude_m);
    }
    shot.elevation_rad = zero.elevation_rad;
    shot.windage_rad = zero.windage_rad;

    Trajectory traj = Fly(shot, max_range_m, options);
    return Solution{std::move(shot), zero, std::move(traj)};
}

double ToClicks(double angle_rad, double click_rad, bool round) {
    if (!(click_rad > 0.0)) {
        return 0.0;
    }
    const double clicks = angle_rad / click_rad;
    return round ? std::round(clicks) : clicks;
}

} // namespace ballistics::storage
