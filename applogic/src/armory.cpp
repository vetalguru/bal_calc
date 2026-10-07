#include <ballistics/applogic/armory.h>
#include <ballistics/applogic/library.h>
#include <ballistics/storage/repository.h>
#include <ballistics/units.h>
#include <sqlite_manager/transaction.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

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

Error Invalid(const std::string& problem) { return Error(ErrorCode::kConstraint, 0, problem); }

// Cartridges brought in from files belong to the library, not the user.
bool IsLibraryCartridge(const CartridgeRecord& c) { return c.source.rfind("import:", 0) == 0; }

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return s;
}

// The leading number of a calibre: ".308 Win" -> "308", "6.5x47" -> "6.5".
std::string CaliberNumber(const std::string& caliber) {
    std::string out;
    for (char ch : caliber) {
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            out += ch;
        } else if (ch == '.' || ch == ',') {
            if (!out.empty()) {
                out += '.';
            }
        } else if (!out.empty()) {
            break;
        }
    }
    while (!out.empty() && out.back() == '.') {
        out.pop_back();
    }
    return out;
}

// Pairs that use a rifle or a cartridge.
Result<std::vector<ProfileRecord>> PairsWhere(Database& db, Id rifle_id, Id cartridge_id) {
    auto all = Repository<ProfileRecord>(db).List();
    if (!all) {
        return all.error();
    }
    std::vector<ProfileRecord> out;
    for (ProfileRecord& p : all.value()) {
        if ((rifle_id != 0 && p.rifle_id == rifle_id) ||
            (cartridge_id != 0 && p.cartridge_id == cartridge_id)) {
            out.push_back(std::move(p));
        }
    }
    return out;
}

Status RemovePairs(Database& db, Id rifle_id, Id cartridge_id) {
    auto pairs = PairsWhere(db, rifle_id, cartridge_id);
    if (!pairs) {
        return pairs.error();
    }
    for (const ProfileRecord& p : pairs.value()) {
        if (Status s = Repository<ProfileRecord>(db).Remove(p.id); !s) {
            return s;
        }
    }
    return sqlite_manager::Ok();
}

// A private bullet nothing references any more goes away.
void RemoveOwnBullet(Database& db, Id bullet_id) {
    auto b = Repository<BulletRecord>(db).Get(bullet_id);
    if (b && b.value() && b.value()->source == kSourceUser) {
        Repository<BulletRecord>(db).Remove(bullet_id).ok();  // refused while still used
    }
}

CartridgeSummary Summary(Database& db, const CartridgeRecord& c) {
    CartridgeSummary s;
    s.id = c.id;
    s.name = c.name;
    s.caliber = c.caliber;
    s.muzzle_velocity_mps = c.muzzle_velocity_mps;
    if (auto b = Repository<BulletRecord>(db).Get(c.bullet_id); b && b.value()) {
        s.bullet_name = b.value()->name;
    }
    return s;
}

void FillBullet(CartridgeForm& f, const BulletRecord& b) {
    f.bullet_name = b.name;
    f.drag_table = b.drag_table;
    f.bc = b.bc ? *b.bc : (b.bc_bands.empty() ? 0.0 : b.bc_bands.front().bc_lb_in2);
    f.mass_gr = units::KgToGrain(b.mass_kg);
    f.diameter_in = units::MToInch(b.diameter_m);
    f.length_in = units::MToInch(b.length_m);
}

}  // namespace

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

bool SameCaliber(const std::string& a, const std::string& b) {
    const std::string na = CaliberNumber(a);
    return !na.empty() && na == CaliberNumber(b);
}

// ---- Rifles ---------------------------------------------------------------

std::string Validate(const RifleForm& f) {
    if (f.name.empty()) {
        return "Enter a rifle name.";
    }
    if (f.twist_in < 0.0) {
        return "Bullet length and twist cannot be negative.";
    }
    if (!(f.sight_height_cm >= 0.0 && f.sight_height_cm < 30.0)) {
        return "Sight height must be between 0 and 30 cm.";
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

Result<std::vector<RifleSummary>> ListRifles(Database& db) {
    auto all = Repository<RifleRecord>(db).List();
    if (!all) {
        return all.error();
    }
    std::vector<RifleSummary> out;
    for (const RifleRecord& r : all.value()) {
        out.push_back({r.id, r.name, r.caliber});
    }
    return out;
}

Result<RifleForm> LoadRifleForm(Database& db, Id rifle_id) {
    auto rifle = Require<RifleRecord>(db, rifle_id, "rifle");
    if (!rifle) {
        return rifle.error();
    }
    const RifleRecord& r = rifle.value();
    RifleForm f;
    f.rifle_id = r.id;
    f.name = r.name;
    f.caliber = r.caliber;
    f.sight_height_cm = r.sight_height_m * 100.0;
    f.twist_in = units::MToInch(std::fabs(r.twist_m));
    f.twist_left = r.twist_m < 0.0;
    if (r.scope_id) {
        auto scope = Require<ScopeRecord>(db, *r.scope_id, "scope");
        if (!scope) {
            return scope.error();
        }
        const ScopeRecord& s = scope.value();
        f.click_units = s.click_units;
        f.click_value = RadToClick(f.click_units, s.click_vertical_rad);
        f.reticle_id = s.reticle_id.value_or(0);
        f.focal_plane = s.focal_plane;
        f.sfp_reference_magnification = s.sfp_reference_magnification;
        f.min_magnification = s.min_magnification;
        f.max_magnification = s.max_magnification;
    }
    f.zero_range_m = r.zero_range_m;
    f.zero_temperature_c = units::KToC(r.zero_atmosphere.temperature_k);
    f.zero_pressure_hpa = r.zero_atmosphere.pressure_pa / 100.0;
    f.zero_altitude_m = r.zero_atmosphere.altitude_m;
    f.zero_humidity_pct = r.zero_atmosphere.humidity * 100.0;
    f.zero_powder_c = units::KToC(r.zero_powder_temp_k);
    return f;
}

Result<Id> SaveRifleForm(Database& db, const RifleForm& f) {
    if (const std::string problem = Validate(f); !problem.empty()) {
        return Invalid(problem);
    }
    auto txn = sqlite_manager::Transaction::Begin(db.connection());
    if (!txn) {
        return txn.error();
    }
    RifleRecord r;
    ScopeRecord s;
    if (f.rifle_id != 0) {
        auto rr = Require<RifleRecord>(db, f.rifle_id, "rifle");
        if (!rr) {
            return rr.error();
        }
        r = std::move(rr).value();
        if (r.scope_id) {
            auto sr = Require<ScopeRecord>(db, *r.scope_id, "scope");
            if (!sr) {
                return sr.error();
            }
            s = std::move(sr).value();
        }
    }

    s.name = f.name;
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

    r.name = f.name;
    r.caliber = f.caliber;
    r.sight_height_m = f.sight_height_cm / 100.0;
    r.twist_m = units::InchToM(f.twist_in) * (f.twist_left ? -1.0 : 1.0);
    r.scope_id = s.id;
    r.zero_range_m = f.zero_range_m;
    r.zero_atmosphere = {f.zero_altitude_m, f.zero_pressure_hpa * 100.0,
                         units::CToK(f.zero_temperature_c), f.zero_humidity_pct / 100.0};
    r.zero_powder_temp_k = units::CToK(f.zero_powder_c);
    if (auto id = Repository<RifleRecord>(db).Save(r); !id) {
        return id.error();
    }
    if (Status st = txn.value().Commit(); !st) {
        return st.error();
    }
    return r.id;
}

Status DeleteRifle(Database& db, Id rifle_id) {
    auto r = Require<RifleRecord>(db, rifle_id, "rifle");
    if (!r) {
        return r.error();
    }
    auto txn = sqlite_manager::Transaction::Begin(db.connection());
    if (!txn) {
        return txn.error();
    }
    if (Status s = RemovePairs(db, rifle_id, 0); !s) {
        return s;
    }
    if (Status s = Repository<RifleRecord>(db).Remove(rifle_id); !s) {
        return s;
    }
    if (r.value().scope_id) {
        Repository<ScopeRecord>(db).Remove(*r.value().scope_id).ok();  // kept if shared
    }
    return txn.value().Commit();
}

// ---- Cartridges -----------------------------------------------------------

std::string Validate(const CartridgeForm& f) {
    if (f.name.empty()) {
        return "Enter a cartridge name.";
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
    if (f.length_in < 0.0) {
        return "Bullet length and twist cannot be negative.";
    }
    return {};
}

Result<std::vector<CartridgeSummary>> ListCartridges(Database& db, const std::string& caliber) {
    auto all = Repository<CartridgeRecord>(db).List();
    if (!all) {
        return all.error();
    }
    std::vector<CartridgeSummary> out;
    for (const CartridgeRecord& c : all.value()) {
        if (!IsLibraryCartridge(c)) {
            out.push_back(Summary(db, c));
        }
    }
    if (!caliber.empty()) {
        std::stable_partition(out.begin(), out.end(), [&](const CartridgeSummary& c) {
            return SameCaliber(c.caliber, caliber);
        });
    }
    return out;
}

Result<std::vector<CartridgeSummary>> ListLibraryCartridges(Database& db,
                                                            const std::string& filter) {
    auto all = Repository<CartridgeRecord>(db).List();
    if (!all) {
        return all.error();
    }
    const std::string needle = Lower(filter);
    std::vector<CartridgeSummary> out;
    for (const CartridgeRecord& c : all.value()) {
        if (IsLibraryCartridge(c) &&
            (needle.empty() || Lower(c.name).find(needle) != std::string::npos ||
             Lower(c.caliber).find(needle) != std::string::npos)) {
            out.push_back(Summary(db, c));
        }
    }
    return out;
}

Result<CartridgeForm> CartridgeFromLibrary(Database& db, Id library_cartridge_id) {
    auto f = LoadCartridgeForm(db, library_cartridge_id);
    if (!f) {
        return f.error();
    }
    CartridgeForm out = std::move(f).value();
    out.cartridge_id = 0;
    out.copy_of = library_cartridge_id;
    return out;
}

Result<CartridgeForm> WithLibraryBullet(Database& db, CartridgeForm f, Id bullet_id) {
    auto b = Require<BulletRecord>(db, bullet_id, "bullet");
    if (!b) {
        return b.error();
    }
    f.library_bullet_id = b.value().id;
    FillBullet(f, b.value());
    if (f.caliber.empty()) {
        f.caliber = b.value().caliber;
    }
    return f;
}

Result<CartridgeForm> LoadCartridgeForm(Database& db, Id cartridge_id) {
    auto cart = Require<CartridgeRecord>(db, cartridge_id, "cartridge");
    if (!cart) {
        return cart.error();
    }
    const CartridgeRecord& c = cart.value();
    auto bullet = Require<BulletRecord>(db, c.bullet_id, "bullet");
    if (!bullet) {
        return bullet.error();
    }
    const BulletRecord& b = bullet.value();
    CartridgeForm f;
    f.cartridge_id = c.id;
    f.name = c.name;
    f.caliber = c.caliber.empty() ? b.caliber : c.caliber;
    f.library_bullet_id = b.source == kSourceUser ? 0 : b.id;
    FillBullet(f, b);
    f.muzzle_velocity_mps = c.muzzle_velocity_mps;
    f.powder_reference_c = units::KToC(c.reference_powder_temp_k);
    f.powder_sensitivity_pct_per_c = c.powder_sensitivity_per_k * 100.0;
    return f;
}

Result<Id> SaveCartridgeForm(Database& db, const CartridgeForm& f) {
    if (const std::string problem = Validate(f); !problem.empty()) {
        return Invalid(problem);
    }
    auto txn = sqlite_manager::Transaction::Begin(db.connection());
    if (!txn) {
        return txn.error();
    }
    CartridgeRecord c;
    BulletRecord b;
    const Id existing = f.cartridge_id != 0 ? f.cartridge_id : f.copy_of;
    if (existing != 0) {
        auto cr = Require<CartridgeRecord>(db, existing, "cartridge");
        if (!cr) {
            return cr.error();
        }
        c = std::move(cr).value();
        auto br = Require<BulletRecord>(db, c.bullet_id, "bullet");
        if (!br) {
            return br.error();
        }
        b = std::move(br).value();
    }
    if (f.cartridge_id == 0) {
        c.id = 0;  // a copy: new row
        c.source = kSourceUser;
        if (b.source == kSourceUser) {
            b.id = 0;  // and its own bullet copied too
        }
    }

    const Id previous_bullet = f.cartridge_id != 0 ? b.id : 0;
    if (f.library_bullet_id != 0) {
        // Use the library bullet untouched.
        auto lib = Require<BulletRecord>(db, f.library_bullet_id, "bullet");
        if (!lib) {
            return lib.error();
        }
        b = std::move(lib).value();
    } else {
        if (b.source != kSourceUser) {
            // Detached from a library bullet: start the cartridge's own copy.
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
        // The form edits a single BC; bands and curves of the cartridge's
        // own bullet are kept as they are.
        if (b.drag_kind == storage::kDragKindBc) {
            b.drag_table = f.drag_table;
            b.bc = f.bc;
        }
        if (auto id = Repository<BulletRecord>(db).Save(b); !id) {
            return id.error();
        }
    }

    c.name = f.name;
    c.caliber = f.caliber;
    c.bullet_id = b.id;
    c.muzzle_velocity_mps = f.muzzle_velocity_mps;
    c.reference_powder_temp_k = units::CToK(f.powder_reference_c);
    c.powder_sensitivity_per_k = f.powder_sensitivity_pct_per_c / 100.0;
    if (auto id = Repository<CartridgeRecord>(db).Save(c); !id) {
        return id.error();
    }
    if (previous_bullet != 0 && previous_bullet != b.id) {
        RemoveOwnBullet(db, previous_bullet);
    }
    if (Status st = txn.value().Commit(); !st) {
        return st.error();
    }
    return c.id;
}

Status DeleteCartridge(Database& db, Id cartridge_id) {
    auto c = Require<CartridgeRecord>(db, cartridge_id, "cartridge");
    if (!c) {
        return c.error();
    }
    auto txn = sqlite_manager::Transaction::Begin(db.connection());
    if (!txn) {
        return txn.error();
    }
    if (Status s = RemovePairs(db, 0, cartridge_id); !s) {
        return s;
    }
    if (Status s = Repository<CartridgeRecord>(db).Remove(cartridge_id); !s) {
        return s;
    }
    RemoveOwnBullet(db, c.value().bullet_id);
    return txn.value().Commit();
}

// ---- Pairs ----------------------------------------------------------------

Result<Id> EnsureProfile(Database& db, Id rifle_id, Id cartridge_id) {
    auto rifle = Require<RifleRecord>(db, rifle_id, "rifle");
    if (!rifle) {
        return rifle.error();
    }
    auto cart = Require<CartridgeRecord>(db, cartridge_id, "cartridge");
    if (!cart) {
        return cart.error();
    }
    auto pairs = PairsWhere(db, rifle_id, 0);
    if (!pairs) {
        return pairs.error();
    }
    for (const ProfileRecord& p : pairs.value()) {
        if (p.cartridge_id == cartridge_id) {
            return p.id;
        }
    }
    ProfileRecord p;
    p.name = rifle.value().name + " / " + cart.value().name;
    p.rifle_id = rifle_id;
    p.cartridge_id = cartridge_id;
    if (auto id = Repository<ProfileRecord>(db).Save(p); !id) {
        return id.error();
    }
    return p.id;
}

Status SetZeroOffset(Database& db, Id profile_id, double up_cm, double right_cm) {
    auto p = Require<ProfileRecord>(db, profile_id, "profile");
    if (!p) {
        return p.error();
    }
    if (!(std::fabs(up_cm) <= 100.0 && std::fabs(right_cm) <= 100.0)) {
        return Invalid("Point-of-impact shift must be within 100 cm.");
    }
    ProfileRecord r = std::move(p).value();
    r.zero_offset_up_m = up_cm / 100.0;
    r.zero_offset_right_m = right_cm / 100.0;
    if (auto id = Repository<ProfileRecord>(db).Save(r); !id) {
        return id.error();
    }
    return sqlite_manager::Ok();
}

Result<Id> CreateSampleProfile(Database& db, const std::string& rifle_name,
                               const std::string& cartridge_name) {
    RifleForm r;
    r.name = rifle_name;
    r.caliber = ".308 Win";
    r.sight_height_cm = 5.0;
    r.twist_in = 10.0;
    r.click_units = kClickMrad;
    r.click_value = 0.1;
    r.zero_range_m = 100.0;
    auto rifle_id = SaveRifleForm(db, r);
    if (!rifle_id) {
        return rifle_id.error();
    }

    CartridgeForm c;
    c.name = cartridge_name;
    c.caliber = ".308 Win";
    c.bullet_name = "Sierra MatchKing 175 gr HPBT";
    c.drag_table = "G7";
    c.bc = 0.243;
    c.mass_gr = 175.0;
    c.diameter_in = 0.308;
    c.length_in = 1.24;
    c.muzzle_velocity_mps = 790.0;
    c.powder_sensitivity_pct_per_c = 0.08;
    if (auto lib = ListLibraryBullets(db, "MatchKing 175 gr HPBT"); lib && !lib.value().empty()) {
        if (auto with = WithLibraryBullet(db, c, lib.value().front().id)) {
            c = std::move(with).value();
        }
    }
    auto cartridge_id = SaveCartridgeForm(db, c);
    if (!cartridge_id) {
        return cartridge_id.error();
    }
    return EnsureProfile(db, rifle_id.value(), cartridge_id.value());
}

}  // namespace ballistics::applogic
