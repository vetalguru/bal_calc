#include <ballistics/applogic/library.h>

#include <algorithm>
#include <cctype>
#include <utility>

#include <ballistics/storage/repository.h>
#include <ballistics/units.h>

namespace ballistics::applogic {

namespace {

using sqlite_manager::Error;
using sqlite_manager::ErrorCode;
using storage::BulletRecord;
using storage::Repository;

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool Contains(const std::string& haystack, const std::string& needle) {
    return Lower(haystack).find(needle) != std::string::npos;
}

} // namespace

Result<std::vector<BulletSummary>> ListLibraryBullets(storage::Database& db,
                                                      const std::string& filter) {
    auto all = Repository<BulletRecord>(db).List();
    if (!all) {
        return all.error();
    }
    const std::string needle = Lower(filter);
    std::vector<BulletSummary> out;
    for (const BulletRecord& b : all.value()) {
        if (b.source == kSourceUser) {
            continue; // private to a profile
        }
        if (!needle.empty() && !Contains(b.name, needle) && !Contains(b.manufacturer, needle) &&
            !Contains(b.caliber, needle)) {
            continue;
        }
        BulletSummary s;
        s.id = b.id;
        s.name = b.name;
        s.manufacturer = b.manufacturer;
        s.caliber = b.caliber;
        s.mass_gr = units::KgToGrain(b.mass_kg);
        s.diameter_in = units::MToInch(b.diameter_m);
        s.drag_kind = b.drag_kind;
        s.drag_table = b.drag_table;
        s.bc = b.bc ? *b.bc : (b.bc_bands.empty() ? 0.0 : b.bc_bands.front().bc_lb_in2);
        s.bc_bands = static_cast<int>(b.bc_bands.size());
        s.source = b.source;
        out.push_back(std::move(s));
    }
    return out;
}

std::string Validate(const BulletForm& f) {
    if (f.name.empty()) {
        return "Enter the bullet name.";
    }
    if (!(f.mass_gr > 0.0)) {
        return "Enter the bullet weight.";
    }
    if (!(f.diameter_in > 0.0 && f.diameter_in < 1.0)) {
        return "Bullet diameter must be between 0 and 1 inch.";
    }
    if (f.length_in < 0.0) {
        return "Bullet length and twist cannot be negative.";
    }
    if (f.has_custom_curve) {
        return {};
    }
    if (f.bands.empty()) {
        if (!(f.bc > 0.0 && f.bc < 2.0)) {
            return "Ballistic coefficient must be between 0 and 2.";
        }
    } else {
        for (const BcBand& b : f.bands) {
            if (!(b.bc > 0.0 && b.bc < 2.0) || !(b.velocity_mps > 0.0)) {
                return "Each BC band needs a velocity and a BC between 0 and 2.";
            }
        }
    }
    return {};
}

Result<BulletForm> LoadBulletForm(storage::Database& db, Id bullet_id) {
    auto r = Repository<BulletRecord>(db).Get(bullet_id);
    if (!r) {
        return r.error();
    }
    if (!r.value()) {
        return Error(ErrorCode::kNotFound, 0, "bullet not found");
    }
    const BulletRecord& b = *r.value();
    BulletForm f;
    f.id = b.id;
    f.name = b.name;
    f.manufacturer = b.manufacturer;
    f.caliber = b.caliber;
    f.mass_gr = units::KgToGrain(b.mass_kg);
    f.diameter_in = units::MToInch(b.diameter_m);
    f.length_in = units::MToInch(b.length_m);
    f.drag_table = b.drag_table;
    f.bc = b.bc.value_or(0.0);
    for (const BcPoint& p : b.bc_bands) {
        f.bands.push_back({p.velocity_mps, p.bc_lb_in2});
    }
    f.notes = b.notes;
    f.source = b.source;
    f.has_custom_curve = b.drag_kind == storage::kDragKindCurve;
    return f;
}

Result<Id> SaveBulletForm(storage::Database& db, const BulletForm& f) {
    if (const std::string problem = Validate(f); !problem.empty()) {
        return Error(ErrorCode::kConstraint, 0, problem);
    }
    BulletRecord b;
    if (f.id != 0) {
        auto r = Repository<BulletRecord>(db).Get(f.id);
        if (!r) {
            return r.error();
        }
        if (!r.value()) {
            return Error(ErrorCode::kNotFound, 0, "bullet not found");
        }
        b = std::move(*r.value());
    }
    b.name = f.name;
    b.manufacturer = f.manufacturer;
    b.caliber = f.caliber;
    b.mass_kg = units::GrainToKg(f.mass_gr);
    b.diameter_m = units::InchToM(f.diameter_in);
    b.length_m = units::InchToM(f.length_in);
    b.notes = f.notes;
    b.source = f.source.empty() ? kSourceLibrary : f.source;
    if (b.drag_kind != storage::kDragKindCurve) {
        b.drag_table = f.drag_table;
        b.bc_bands.clear();
        if (f.bands.empty()) {
            b.drag_kind = storage::kDragKindBc;
            b.bc = f.bc;
        } else {
            b.drag_kind = storage::kDragKindMultiBc;
            b.bc.reset();
            for (const BcBand& band : f.bands) {
                b.bc_bands.push_back({band.velocity_mps, band.bc});
            }
            std::sort(b.bc_bands.begin(), b.bc_bands.end(),
                      [](const BcPoint& x, const BcPoint& y) { return x.velocity_mps > y.velocity_mps; });
        }
    }
    return Repository<BulletRecord>(db).Save(b);
}

Status DeleteBullet(storage::Database& db, Id bullet_id) {
    if (Status s = Repository<BulletRecord>(db).Remove(bullet_id); !s) {
        if (s.error().code == ErrorCode::kConstraint) {
            return Error(ErrorCode::kConstraint, s.error().sqlite_code,
                         "This bullet is used by a cartridge and cannot be deleted.");
        }
        return s;
    }
    return sqlite_manager::Ok();
}

} // namespace ballistics::applogic
