#include <ballistics/applogic/profile_io.h>

#include <cmath>
#include <optional>
#include <set>
#include <utility>

#include <nlohmann/json.hpp>
#include <sqlite_manager/transaction.h>

#include <ballistics/applogic/library.h>
#include <ballistics/storage/repository.h>

namespace ballistics::applogic {

namespace {

using nlohmann::json;
using sqlite_manager::Error;
using sqlite_manager::ErrorCode;
using namespace storage;

constexpr const char* kRifleFormat = "balcalc-rifle";
constexpr const char* kCartridgeFormat = "balcalc-cartridge";
constexpr const char* kLegacyProfileFormat = "balcalc-profile";
constexpr int kVersion = 1;

Error BadFile(const std::string& what) {
    return Error(ErrorCode::kFormat, 0, "Not a valid Holdmark file: " + what);
}

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

json Opt(const std::optional<double>& v) { return v ? json(*v) : json(nullptr); }

json ToJson(const Atmosphere& a) {
    return {{"altitude_m", a.altitude_m},
            {"pressure_pa", a.pressure_pa},
            {"temperature_k", a.temperature_k},
            {"humidity", a.humidity}};
}

Atmosphere AtmosphereFrom(const json& j) {
    Atmosphere a;
    a.altitude_m = j.at("altitude_m").get<double>();
    a.pressure_pa = j.at("pressure_pa").get<double>();
    a.temperature_k = j.at("temperature_k").get<double>();
    a.humidity = j.at("humidity").get<double>();
    return a;
}

std::optional<double> OptFrom(const json& j, const char* key) {
    if (!j.contains(key) || j.at(key).is_null()) {
        return std::nullopt;
    }
    return j.at(key).get<double>();
}

bool Same(double a, double b) { return std::fabs(a - b) <= 1e-9 * std::max(1.0, std::fabs(a)); }

// An existing library bullet with the same identity and drag, if any.
std::optional<Id> FindSameBullet(Database& db, const BulletRecord& b) {
    if (b.drag_kind == kDragKindCurve) {
        return std::nullopt; // curves are compared by id only; import a copy
    }
    auto all = Repository<BulletRecord>(db).List(b.name);
    if (!all) {
        return std::nullopt;
    }
    for (const BulletRecord& x : all.value()) {
        if (x.source == kSourceUser || x.name != b.name || x.drag_kind != b.drag_kind ||
            x.drag_table != b.drag_table || !Same(x.mass_kg, b.mass_kg) ||
            !Same(x.diameter_m, b.diameter_m) || x.bc.has_value() != b.bc.has_value() ||
            (x.bc && !Same(*x.bc, *b.bc)) || x.bc_bands.size() != b.bc_bands.size()) {
            continue;
        }
        bool bands_equal = true;
        for (std::size_t i = 0; i < x.bc_bands.size(); ++i) {
            bands_equal = bands_equal && Same(x.bc_bands[i].velocity_mps, b.bc_bands[i].velocity_mps) &&
                          Same(x.bc_bands[i].bc_lb_in2, b.bc_bands[i].bc_lb_in2);
        }
        if (bands_equal) {
            return x.id;
        }
    }
    return std::nullopt;
}

template <typename T>
std::string UniqueName(Database& db, const std::string& name) {
    auto all = Repository<T>(db).List();
    std::set<std::string> taken;
    if (all) {
        for (const auto& r : all.value()) {
            taken.insert(r.name);
        }
    }
    if (taken.count(name) == 0) {
        return name;
    }
    for (int n = 2;; ++n) {
        const std::string candidate = name + " (" + std::to_string(n) + ")";
        if (taken.count(candidate) == 0) {
            return candidate;
        }
    }
}

// ---- Export ---------------------------------------------------------------

Result<json> ScopeJson(Database& db, Id scope_id) {
    auto s = Require<ScopeRecord>(db, scope_id, "scope");
    if (!s) {
        return s.error();
    }
    const ScopeRecord& sr = s.value();
    json out = {{"name", sr.name},
                {"click_units", sr.click_units},
                {"click_vertical_rad", sr.click_vertical_rad},
                {"click_horizontal_rad", sr.click_horizontal_rad},
                {"min_magnification", sr.min_magnification},
                {"max_magnification", sr.max_magnification},
                {"notes", sr.notes},
                {"focal_plane", sr.focal_plane},
                {"sfp_reference_magnification", sr.sfp_reference_magnification},
                {"reticle", nullptr}};
    if (sr.reticle_id) {
        auto ret = Require<ReticleRecord>(db, *sr.reticle_id, "reticle");
        if (!ret) {
            return ret.error();
        }
        const ReticleRecord& t = ret.value();
        out["reticle"] = {{"name", t.name},
                          {"units", t.units},
                          {"focal_plane", t.focal_plane},
                          {"reference_magnification", t.reference_magnification},
                          {"definition", t.definition},
                          {"source", t.source}};
    }
    return out;
}

Result<json> BulletJson(Database& db, Id bullet_id) {
    auto b = Require<BulletRecord>(db, bullet_id, "bullet");
    if (!b) {
        return b.error();
    }
    const BulletRecord& br = b.value();
    json bands = json::array();
    for (const BcPoint& band : br.bc_bands) {
        bands.push_back({band.velocity_mps, band.bc_lb_in2});
    }
    json out = {{"name", br.name},
                {"manufacturer", br.manufacturer},
                {"caliber", br.caliber},
                {"diameter_m", br.diameter_m},
                {"mass_kg", br.mass_kg},
                {"length_m", br.length_m},
                {"drag_kind", br.drag_kind},
                {"drag_table", br.drag_table},
                {"bc", Opt(br.bc)},
                {"form_factor", br.form_factor},
                {"source", br.source},
                {"notes", br.notes},
                {"bc_bands", bands},
                {"curve", nullptr}};
    if (br.curve_id) {
        auto curve = Require<DragCurveRecord>(db, *br.curve_id, "drag curve");
        if (!curve) {
            return curve.error();
        }
        json points = json::array();
        for (const DragPoint& pt : curve.value().points) {
            points.push_back({pt.mach, pt.cd});
        }
        out["curve"] = {{"name", curve.value().name},
                        {"source", curve.value().source},
                        {"points", points}};
    }
    return out;
}

// ---- Import ---------------------------------------------------------------

Result<Id> ReadBullet(Database& db, const json& jb) {
    BulletRecord b;
    b.name = jb.at("name").get<std::string>();
    b.manufacturer = jb.value("manufacturer", "");
    b.caliber = jb.value("caliber", "");
    b.diameter_m = jb.at("diameter_m").get<double>();
    b.mass_kg = jb.at("mass_kg").get<double>();
    b.length_m = jb.value("length_m", 0.0);
    b.drag_kind = jb.at("drag_kind").get<std::string>();
    b.drag_table = jb.value("drag_table", "G7");
    b.bc = OptFrom(jb, "bc");
    b.form_factor = jb.value("form_factor", 1.0);
    b.source = jb.value("source", "");
    b.notes = jb.value("notes", "");
    for (const json& band : jb.value("bc_bands", json::array())) {
        b.bc_bands.push_back({band.at(0).get<double>(), band.at(1).get<double>()});
    }
    if (jb.contains("curve") && !jb.at("curve").is_null()) {
        DragCurveRecord curve;
        curve.name = jb.at("curve").value("name", b.name);
        curve.source = jb.at("curve").value("source", "");
        for (const json& pt : jb.at("curve").at("points")) {
            curve.points.push_back({pt.at(0).get<double>(), pt.at(1).get<double>()});
        }
        if (auto id = Repository<DragCurveRecord>(db).Save(curve); !id) {
            return id.error();
        }
        b.curve_id = curve.id;
    }
    if (auto same = FindSameBullet(db, b)) {
        return *same;
    }
    if (auto id = Repository<BulletRecord>(db).Save(b); !id) {
        return id.error();
    }
    return b.id;
}

Result<Id> ReadCartridge(Database& db, const json& jc, Id bullet_id,
                         const std::string& fallback_caliber) {
    CartridgeRecord c;
    c.name = UniqueName<CartridgeRecord>(db, jc.at("name").get<std::string>());
    c.caliber = jc.value("caliber", fallback_caliber);
    c.bullet_id = bullet_id;
    c.muzzle_velocity_mps = jc.at("muzzle_velocity_mps").get<double>();
    c.reference_powder_temp_k = jc.value("reference_powder_temp_k", 288.15);
    c.powder_sensitivity_per_k = jc.value("powder_sensitivity_per_k", 0.0);
    c.barrel_length_m = jc.value("barrel_length_m", 0.0);
    c.source = jc.value("source", "");
    if (c.source.rfind("import:", 0) == 0) {
        c.source = kSourceUser; // shared with the user: theirs, not the library's
    }
    c.notes = jc.value("notes", "");
    for (const json& vp : jc.value("velocity_points", json::array())) {
        c.velocity_points.emplace_back(vp.at(0).get<double>(), vp.at(1).get<double>());
    }
    if (auto id = Repository<CartridgeRecord>(db).Save(c); !id) {
        return id.error();
    }
    return c.id;
}

Result<std::optional<Id>> ReadScope(Database& db, const json& doc) {
    if (!doc.contains("scope") || doc.at("scope").is_null()) {
        return std::optional<Id>{};
    }
    const json& js = doc.at("scope");
    ScopeRecord s;
    s.name = js.at("name").get<std::string>();
    s.click_units = js.value("click_units", "mrad");
    s.click_vertical_rad = js.at("click_vertical_rad").get<double>();
    s.click_horizontal_rad = js.at("click_horizontal_rad").get<double>();
    s.min_magnification = js.value("min_magnification", 0.0);
    s.max_magnification = js.value("max_magnification", 0.0);
    s.notes = js.value("notes", "");
    s.focal_plane = js.value("focal_plane", "ffp");
    s.sfp_reference_magnification = js.value("sfp_reference_magnification", 0.0);
    if (js.contains("reticle") && !js.at("reticle").is_null()) {
        const json& jt = js.at("reticle");
        ReticleRecord t;
        t.name = jt.at("name").get<std::string>();
        t.units = jt.value("units", "mrad");
        t.focal_plane = jt.value("focal_plane", "ffp");
        t.reference_magnification = jt.value("reference_magnification", 0.0);
        t.definition = jt.value("definition", "");
        t.source = jt.value("source", "");
        if (auto id = Repository<ReticleRecord>(db).Save(t); !id) {
            return id.error();
        }
        s.reticle_id = t.id;
    }
    if (auto id = Repository<ScopeRecord>(db).Save(s); !id) {
        return id.error();
    }
    return std::optional<Id>(s.id);
}

// The rifle; its zero comes from `zero` (the rifle object itself, or the
// "profile" object of a legacy file).
Result<Id> ReadRifle(Database& db, const json& jr, const json& zero, std::optional<Id> scope_id) {
    RifleRecord r;
    r.name = UniqueName<RifleRecord>(db, jr.at("name").get<std::string>());
    r.caliber = jr.value("caliber", "");
    r.barrel_length_m = jr.value("barrel_length_m", 0.0);
    r.twist_m = jr.value("twist_m", 0.0);
    r.sight_height_m = jr.at("sight_height_m").get<double>();
    r.notes = jr.value("notes", "");
    r.scope_id = scope_id;
    r.zero_range_m = zero.at("zero_range_m").get<double>();
    r.zero_atmosphere = AtmosphereFrom(zero.at("zero_atmosphere"));
    r.zero_powder_temp_k = zero.value("zero_powder_temp_k", r.zero_atmosphere.temperature_k);
    if (auto id = Repository<RifleRecord>(db).Save(r); !id) {
        return id.error();
    }
    return r.id;
}

Status ImportInto(Database& db, const json& doc, const std::string& format, Imported& out) {
    if (format == kRifleFormat) {
        auto scope = ReadScope(db, doc);
        if (!scope) {
            return scope.error();
        }
        const json& jr = doc.at("rifle");
        auto rifle = ReadRifle(db, jr, jr, scope.value());
        if (!rifle) {
            return rifle.error();
        }
        out.rifle_id = rifle.value();
        return sqlite_manager::Ok();
    }

    auto bullet = ReadBullet(db, doc.at("bullet"));
    if (!bullet) {
        return bullet.error();
    }
    std::string caliber = format == kLegacyProfileFormat ? doc.at("rifle").value("caliber", "") : "";
    if (caliber.empty()) {
        caliber = doc.at("bullet").value("caliber", "");
    }
    auto cartridge = ReadCartridge(db, doc.at("cartridge"), bullet.value(), caliber);
    if (!cartridge) {
        return cartridge.error();
    }
    out.cartridge_id = cartridge.value();
    if (format == kCartridgeFormat) {
        return sqlite_manager::Ok();
    }

    // Legacy profile: the rifle with the profile's zero, and their pair.
    auto scope = ReadScope(db, doc);
    if (!scope) {
        return scope.error();
    }
    const json& jp = doc.at("profile");
    auto rifle = ReadRifle(db, doc.at("rifle"), jp, scope.value());
    if (!rifle) {
        return rifle.error();
    }
    out.rifle_id = rifle.value();
    ProfileRecord p;
    p.name = jp.value("name", "");
    p.rifle_id = out.rifle_id;
    p.cartridge_id = out.cartridge_id;
    p.zero_offset_up_m = jp.value("zero_offset_up_m", 0.0);
    p.zero_offset_right_m = jp.value("zero_offset_right_m", 0.0);
    p.velocity_scale = jp.value("velocity_scale", 1.0);
    p.drag_scale = jp.value("drag_scale", 1.0);
    if (auto id = Repository<ProfileRecord>(db).Save(p); !id) {
        return id.error();
    }
    out.profile_id = p.id;
    return sqlite_manager::Ok();
}

} // namespace

Result<std::string> ExportRifleJson(Database& db, Id rifle_id) {
    auto r = Require<RifleRecord>(db, rifle_id, "rifle");
    if (!r) {
        return r.error();
    }
    const RifleRecord& rr = r.value();
    json doc;
    doc["format"] = kRifleFormat;
    doc["version"] = kVersion;
    doc["rifle"] = {{"name", rr.name},
                    {"caliber", rr.caliber},
                    {"barrel_length_m", rr.barrel_length_m},
                    {"twist_m", rr.twist_m},
                    {"sight_height_m", rr.sight_height_m},
                    {"notes", rr.notes},
                    {"zero_range_m", rr.zero_range_m},
                    {"zero_atmosphere", ToJson(rr.zero_atmosphere)},
                    {"zero_powder_temp_k", rr.zero_powder_temp_k}};
    doc["scope"] = nullptr;
    if (rr.scope_id) {
        auto scope = ScopeJson(db, *rr.scope_id);
        if (!scope) {
            return scope.error();
        }
        doc["scope"] = std::move(scope).value();
    }
    return doc.dump(2);
}

Result<std::string> ExportCartridgeJson(Database& db, Id cartridge_id) {
    auto c = Require<CartridgeRecord>(db, cartridge_id, "cartridge");
    if (!c) {
        return c.error();
    }
    const CartridgeRecord& cr = c.value();
    auto bullet = BulletJson(db, cr.bullet_id);
    if (!bullet) {
        return bullet.error();
    }
    json velocity_points = json::array();
    for (const auto& [temp, v] : cr.velocity_points) {
        velocity_points.push_back({temp, v});
    }
    json doc;
    doc["format"] = kCartridgeFormat;
    doc["version"] = kVersion;
    doc["cartridge"] = {{"name", cr.name},
                        {"caliber", cr.caliber},
                        {"muzzle_velocity_mps", cr.muzzle_velocity_mps},
                        {"reference_powder_temp_k", cr.reference_powder_temp_k},
                        {"powder_sensitivity_per_k", cr.powder_sensitivity_per_k},
                        {"barrel_length_m", cr.barrel_length_m},
                        {"source", cr.source},
                        {"notes", cr.notes},
                        {"velocity_points", velocity_points}};
    doc["bullet"] = std::move(bullet).value();
    return doc.dump(2);
}

Result<Imported> ImportShareJson(Database& db, const std::string& text) {
    const json doc = json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded() || !doc.is_object()) {
        return BadFile("it is not JSON");
    }
    const std::string format = doc.value("format", "");
    if (format != kRifleFormat && format != kCartridgeFormat && format != kLegacyProfileFormat) {
        return BadFile("unknown format");
    }
    if (doc.value("version", 0) > kVersion) {
        return BadFile("made by a newer version of the app");
    }
    auto txn = sqlite_manager::Transaction::Begin(db.connection());
    if (!txn) {
        return txn.error();
    }
    Imported out;
    try {
        if (Status s = ImportInto(db, doc, format, out); !s) {
            return s.error();
        }
    } catch (const json::exception& e) {
        return BadFile(e.what());
    }
    if (auto s = txn.value().Commit(); !s) {
        return s.error();
    }
    return out;
}

} // namespace ballistics::applogic
