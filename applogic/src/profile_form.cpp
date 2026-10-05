#include <ballistics/applogic/profile_form.h>

#include <cmath>
#include <utility>

#include <sqlite_manager/transaction.h>

#include <ballistics/applogic/library.h>
#include <ballistics/storage/repository.h>
#include <ballistics/units.h>

namespace ballistics::applogic {

namespace {

using sqlite_manager::Error;
using sqlite_manager::ErrorCode;
using storage::BulletRecord;
using storage::CartridgeRecord;
using storage::Database;
using storage::ProfileRecord;
using storage::Repository;
using storage::RifleRecord;
using storage::ScopeRecord;

// Shooter's MOA: one inch at 100 yards.
constexpr double kSmoaRad = units::kMetersPerInch / (100.0 * units::kMetersPerYard);

template <typename T>
Result<T> Require(Database& db, Id id, const char* what) {
    auto r = Repository<T>(db).Get(id);
    if (!r) {
        return r.error();
    }
    if (!r.value()) {
        return Error(ErrorCode::kNotFound, 0, std::string(what) + " not found");
    }
    return std::move(*r.value());
}

} // namespace

double ClickToRad(const std::string& u, double v) {
    if (!(v > 0.0)) {
        return 0.0;
    }
    if (u == kClickMrad) {
        return units::MradToRad(v);
    }
    if (u == kClickMoa) {
        return units::MoaToRad(v);
    }
    if (u == kClickSmoa) {
        return v * kSmoaRad;
    }
    if (u == kClickCm100m) {
        return v * 0.01 / 100.0;
    }
    return 0.0;
}

double RadToClick(const std::string& u, double rad) {
    const double one = ClickToRad(u, 1.0);
    return one > 0.0 ? rad / one : 0.0;
}

std::string Validate(const ProfileForm& f) {
    if (f.name.empty()) {
        return "Enter a profile name.";
    }
    if (!(f.muzzle_velocity_mps > 50.0 && f.muzzle_velocity_mps < 2000.0)) {
        return "Muzzle velocity must be between 50 and 2000 m/s.";
    }
    if (f.library_bullet_id == 0) {
        if (!(f.bc > 0.0 && f.bc < 2.0)) {
            return "Ballistic coefficient must be between 0 and 2.";
        }
        if (!(f.mass_gr > 0.0)) {
            return "Enter the bullet weight.";
        }
        if (!(f.diameter_in > 0.0 && f.diameter_in < 1.0)) {
            return "Bullet diameter must be between 0 and 1 inch.";
        }
    }
    if (f.length_in < 0.0 || f.twist_in < 0.0) {
        return "Bullet length and twist cannot be negative.";
    }
    if (!(f.zero_range_m >= 10.0 && f.zero_range_m <= 1000.0)) {
        return "Zero range must be between 10 and 1000 m.";
    }
    if (!(ClickToRad(f.click_units, f.click_value) > 0.0)) {
        return "Enter the scope click value.";
    }
    if (f.min_magnification < 0.0 || f.max_magnification < 0.0 ||
        (f.max_magnification > 0.0 && f.min_magnification > f.max_magnification)) {
        return "Check the scope magnification range.";
    }
    if (!(f.zero_pressure_hpa > 300.0 && f.zero_pressure_hpa < 1200.0)) {
        return "Zero pressure must be between 300 and 1200 hPa.";
    }
    if (f.zero_humidity_pct < 0.0 || f.zero_humidity_pct > 100.0) {
        return "Humidity must be between 0 and 100 %.";
    }
    return {};
}

Result<ProfileForm> WithLibraryBullet(Database& db, ProfileForm f, Id bullet_id) {
    auto b = Require<BulletRecord>(db, bullet_id, "bullet");
    if (!b) {
        return b.error();
    }
    const BulletRecord& r = b.value();
    f.library_bullet_id = r.id;
    f.bullet_name = r.name;
    f.drag_table = r.drag_table;
    f.bc = r.bc ? *r.bc : (r.bc_bands.empty() ? 0.0 : r.bc_bands.front().bc_lb_in2);
    f.mass_gr = units::KgToGrain(r.mass_kg);
    f.diameter_in = units::MToInch(r.diameter_m);
    f.length_in = units::MToInch(r.length_m);
    if (f.caliber.empty()) {
        f.caliber = r.caliber;
    }
    return f;
}

Result<ProfileForm> LoadProfileForm(Database& db, Id profile_id) {
    auto p = Require<ProfileRecord>(db, profile_id, "profile");
    if (!p) {
        return p.error();
    }
    auto rifle = Require<RifleRecord>(db, p.value().rifle_id, "rifle");
    auto cart = Require<CartridgeRecord>(db, p.value().cartridge_id, "cartridge");
    if (!rifle) {
        return rifle.error();
    }
    if (!cart) {
        return cart.error();
    }
    auto bullet = Require<BulletRecord>(db, cart.value().bullet_id, "bullet");
    if (!bullet) {
        return bullet.error();
    }

    const ProfileRecord& pr = p.value();
    const RifleRecord& r = rifle.value();
    const CartridgeRecord& c = cart.value();
    const BulletRecord& b = bullet.value();

    ProfileForm f;
    f.profile_id = pr.id;
    f.name = pr.name;
    f.caliber = r.caliber.empty() ? b.caliber : r.caliber;
    f.sight_height_cm = r.sight_height_m * 100.0;
    f.twist_in = units::MToInch(std::fabs(r.twist_m));
    f.twist_left = r.twist_m < 0.0;
    if (pr.scope_id) {
        auto scope = Require<ScopeRecord>(db, *pr.scope_id, "scope");
        if (!scope) {
            return scope.error();
        }
        f.click_units = scope.value().click_units;
        f.click_value = RadToClick(f.click_units, scope.value().click_vertical_rad);
        f.reticle_id = scope.value().reticle_id.value_or(0);
        f.focal_plane = scope.value().focal_plane;
        f.sfp_reference_magnification = scope.value().sfp_reference_magnification;
        f.min_magnification = scope.value().min_magnification;
        f.max_magnification = scope.value().max_magnification;
    }
    f.library_bullet_id = b.source == kSourceUser ? 0 : b.id;
    f.bullet_name = b.name;
    f.drag_table = b.drag_table;
    if (b.bc) {
        f.bc = *b.bc;
    } else if (!b.bc_bands.empty()) {
        f.bc = b.bc_bands.front().bc_lb_in2;
    }
    f.mass_gr = units::KgToGrain(b.mass_kg);
    f.diameter_in = units::MToInch(b.diameter_m);
    f.length_in = units::MToInch(b.length_m);
    f.muzzle_velocity_mps = c.muzzle_velocity_mps;
    f.powder_reference_c = units::KToC(c.reference_powder_temp_k);
    f.powder_sensitivity_pct_per_c = c.powder_sensitivity_per_k * 100.0;
    f.zero_range_m = pr.zero_range_m;
    f.zero_offset_up_cm = pr.zero_offset_up_m * 100.0;
    f.zero_offset_right_cm = pr.zero_offset_right_m * 100.0;
    f.zero_temperature_c = units::KToC(pr.zero_atmosphere.temperature_k);
    f.zero_pressure_hpa = pr.zero_atmosphere.pressure_pa / 100.0;
    f.zero_altitude_m = pr.zero_atmosphere.altitude_m;
    f.zero_humidity_pct = pr.zero_atmosphere.humidity * 100.0;
    f.zero_powder_c = units::KToC(pr.zero_powder_temp_k);
    return f;
}

Result<Id> SaveProfileForm(Database& db, const ProfileForm& f) {
    if (const std::string problem = Validate(f); !problem.empty()) {
        return Error(ErrorCode::kConstraint, 0, problem);
    }
    auto txn = sqlite_manager::Transaction::Begin(db.connection());
    if (!txn) {
        return txn.error();
    }

    ProfileRecord p;
    RifleRecord r;
    ScopeRecord s;
    CartridgeRecord c;
    BulletRecord b;
    if (f.profile_id != 0) {
        auto pr = Require<ProfileRecord>(db, f.profile_id, "profile");
        if (!pr) {
            return pr.error();
        }
        p = std::move(pr).value();
        auto rr = Require<RifleRecord>(db, p.rifle_id, "rifle");
        auto cr = Require<CartridgeRecord>(db, p.cartridge_id, "cartridge");
        if (!rr) {
            return rr.error();
        }
        if (!cr) {
            return cr.error();
        }
        r = std::move(rr).value();
        c = std::move(cr).value();
        auto br = Require<BulletRecord>(db, c.bullet_id, "bullet");
        if (!br) {
            return br.error();
        }
        b = std::move(br).value();
        if (p.scope_id) {
            auto sr = Require<ScopeRecord>(db, *p.scope_id, "scope");
            if (!sr) {
                return sr.error();
            }
            s = std::move(sr).value();
        }
    }

    const Id previous_bullet = b.id;
    if (f.library_bullet_id != 0) {
        // Use the library bullet untouched.
        auto lib = Require<BulletRecord>(db, f.library_bullet_id, "bullet");
        if (!lib) {
            return lib.error();
        }
        b = std::move(lib).value();
    } else {
        if (b.source != kSourceUser) {
            // Detached from a library bullet: start the profile's own copy.
            b.id = 0;
            b.source = kSourceUser;
            b.drag_kind = storage::kDragKindBc;
            b.bc_bands.clear();
            b.curve_id.reset();
        }
        b.name = f.bullet_name.empty() ? f.name : f.bullet_name;
        b.caliber = f.caliber;
        b.diameter_m = units::InchToM(f.diameter_in);
        b.mass_kg = units::GrainToKg(f.mass_gr);
        b.length_m = units::InchToM(f.length_in);
        // The form edits a single BC; bands and curves of the profile's own
        // bullet are kept as they are.
        if (b.drag_kind == storage::kDragKindBc) {
            b.drag_table = f.drag_table;
            b.bc = f.bc;
        }
        if (auto id = Repository<BulletRecord>(db).Save(b); !id) {
            return id.error();
        }
    }

    c.name = f.name;
    c.bullet_id = b.id;
    c.muzzle_velocity_mps = f.muzzle_velocity_mps;
    c.reference_powder_temp_k = units::CToK(f.powder_reference_c);
    c.powder_sensitivity_per_k = f.powder_sensitivity_pct_per_c / 100.0;
    if (auto id = Repository<CartridgeRecord>(db).Save(c); !id) {
        return id.error();
    }

    r.name = f.name;
    r.caliber = f.caliber;
    r.sight_height_m = f.sight_height_cm / 100.0;
    r.twist_m = units::InchToM(f.twist_in) * (f.twist_left ? -1.0 : 1.0);
    if (auto id = Repository<RifleRecord>(db).Save(r); !id) {
        return id.error();
    }

    s.name = s.name.empty() ? f.name : s.name;
    s.click_units = f.click_units;
    s.click_vertical_rad = s.click_horizontal_rad = ClickToRad(f.click_units, f.click_value);
    s.reticle_id = f.reticle_id != 0 ? std::optional<Id>(f.reticle_id) : std::nullopt;
    s.focal_plane = f.focal_plane == "sfp" ? "sfp" : "ffp";
    s.sfp_reference_magnification = f.sfp_reference_magnification;
    s.min_magnification = f.min_magnification;
    s.max_magnification = f.max_magnification;
    if (auto id = Repository<ScopeRecord>(db).Save(s); !id) {
        return id.error();
    }

    p.name = f.name;
    p.rifle_id = r.id;
    p.cartridge_id = c.id;
    p.scope_id = s.id;
    p.zero_range_m = f.zero_range_m;
    p.zero_offset_up_m = f.zero_offset_up_cm / 100.0;
    p.zero_offset_right_m = f.zero_offset_right_cm / 100.0;
    p.zero_atmosphere = {f.zero_altitude_m, f.zero_pressure_hpa * 100.0,
                         units::CToK(f.zero_temperature_c), f.zero_humidity_pct / 100.0};
    p.zero_powder_temp_k = units::CToK(f.zero_powder_c);
    if (auto id = Repository<ProfileRecord>(db).Save(p); !id) {
        return id.error();
    }

    // A private bullet the profile no longer uses goes away (kept if
    // anything else still references it).
    if (previous_bullet != 0 && previous_bullet != b.id) {
        auto old_bullet = Repository<BulletRecord>(db).Get(previous_bullet);
        if (old_bullet && old_bullet.value() && old_bullet.value()->source == kSourceUser) {
            Repository<BulletRecord>(db).Remove(previous_bullet).ok();
        }
    }

    if (Status st = txn.value().Commit(); !st) {
        return st.error();
    }
    return p.id;
}

Status DeleteProfile(Database& db, Id profile_id) {
    auto p = Require<ProfileRecord>(db, profile_id, "profile");
    if (!p) {
        return p.error();
    }
    auto cart = Repository<CartridgeRecord>(db).Get(p.value().cartridge_id);
    if (Status s = Repository<ProfileRecord>(db).Remove(profile_id); !s) {
        return s;
    }
    // Owned records go too, unless something else still references them
    // (the foreign keys refuse; that is fine).
    Repository<RifleRecord>(db).Remove(p.value().rifle_id).ok();
    if (p.value().scope_id) {
        Repository<ScopeRecord>(db).Remove(*p.value().scope_id).ok();
    }
    if (Repository<CartridgeRecord>(db).Remove(p.value().cartridge_id).ok() && cart &&
        cart.value()) {
        // Only the profile's private bullet; library bullets stay.
        auto bullet = Repository<BulletRecord>(db).Get(cart.value()->bullet_id);
        if (bullet && bullet.value() && bullet.value()->source == kSourceUser) {
            Repository<BulletRecord>(db).Remove(cart.value()->bullet_id).ok();
        }
    }
    return sqlite_manager::Ok();
}

} // namespace ballistics::applogic
