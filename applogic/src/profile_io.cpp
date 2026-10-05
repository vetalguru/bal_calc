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

constexpr const char* kFormat = "balcalc-profile";
constexpr int kVersion = 1;

Error BadFile(const std::string& what) {
    return Error(ErrorCode::kFormat, 0, "Not a valid profile file: " + what);
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

std::string UniqueProfileName(Database& db, const std::string& name) {
    auto all = Repository<ProfileRecord>(db).List();
    std::set<std::string> taken;
    if (all) {
        for (const auto& p : all.value()) {
            taken.insert(p.name);
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

} // namespace

Result<std::string> ExportProfileJson(Database& db, Id profile_id) {
    auto p = Require<ProfileRecord>(db, profile_id, "profile");
    if (!p) {
        return p.error();
    }
    auto r = Require<RifleRecord>(db, p.value().rifle_id, "rifle");
    auto c = Require<CartridgeRecord>(db, p.value().cartridge_id, "cartridge");
    if (!r) {
        return r.error();
    }
    if (!c) {
        return c.error();
    }
    auto b = Require<BulletRecord>(db, c.value().bullet_id, "bullet");
    if (!b) {
        return b.error();
    }

    const ProfileRecord& pr = p.value();
    json doc;
    doc["format"] = kFormat;
    doc["version"] = kVersion;
    doc["profile"] = {{"name", pr.name},
                      {"zero_range_m", pr.zero_range_m},
                      {"zero_offset_up_m", pr.zero_offset_up_m},
                      {"zero_offset_right_m", pr.zero_offset_right_m},
                      {"zero_atmosphere", ToJson(pr.zero_atmosphere)},
                      {"zero_powder_temp_k", pr.zero_powder_temp_k},
                      {"velocity_scale", pr.velocity_scale},
                      {"drag_scale", pr.drag_scale}};
    const RifleRecord& rr = r.value();
    doc["rifle"] = {{"name", rr.name},
                    {"caliber", rr.caliber},
                    {"barrel_length_m", rr.barrel_length_m},
                    {"twist_m", rr.twist_m},
                    {"sight_height_m", rr.sight_height_m},
                    {"notes", rr.notes}};
    doc["scope"] = nullptr;
    if (pr.scope_id) {
        auto s = Require<ScopeRecord>(db, *pr.scope_id, "scope");
        if (!s) {
            return s.error();
        }
        const ScopeRecord& sr = s.value();
        doc["scope"] = {{"name", sr.name},
                        {"click_units", sr.click_units},
                        {"click_vertical_rad", sr.click_vertical_rad},
                        {"click_horizontal_rad", sr.click_horizontal_rad},
                        {"min_magnification", sr.min_magnification},
                        {"max_magnification", sr.max_magnification},
                        {"notes", sr.notes},
                        {"reticle", nullptr}};
        if (sr.reticle_id) {
            auto ret = Require<ReticleRecord>(db, *sr.reticle_id, "reticle");
            if (!ret) {
                return ret.error();
            }
            const ReticleRecord& t = ret.value();
            doc["scope"]["reticle"] = {{"name", t.name},
                                       {"units", t.units},
                                       {"focal_plane", t.focal_plane},
                                       {"reference_magnification", t.reference_magnification},
                                       {"definition", t.definition},
                                       {"source", t.source}};
        }
    }
    const CartridgeRecord& cr = c.value();
    json velocity_points = json::array();
    for (const auto& [temp, v] : cr.velocity_points) {
        velocity_points.push_back({temp, v});
    }
    doc["cartridge"] = {{"name", cr.name},
                        {"muzzle_velocity_mps", cr.muzzle_velocity_mps},
                        {"reference_powder_temp_k", cr.reference_powder_temp_k},
                        {"powder_sensitivity_per_k", cr.powder_sensitivity_per_k},
                        {"barrel_length_m", cr.barrel_length_m},
                        {"source", cr.source},
                        {"notes", cr.notes},
                        {"velocity_points", velocity_points}};
    const BulletRecord& br = b.value();
    json bands = json::array();
    for (const BcPoint& band : br.bc_bands) {
        bands.push_back({band.velocity_mps, band.bc_lb_in2});
    }
    doc["bullet"] = {{"name", br.name},
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
        doc["bullet"]["curve"] = {{"name", curve.value().name},
                                  {"source", curve.value().source},
                                  {"points", points}};
    }
    return doc.dump(2);
}

Result<Id> ImportProfileJson(Database& db, const std::string& text) {
    const json doc = json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded() || !doc.is_object()) {
        return BadFile("it is not JSON");
    }
    if (doc.value("format", "") != kFormat) {
        return BadFile("unknown format");
    }
    if (doc.value("version", 0) > kVersion) {
        return BadFile("made by a newer version of the app");
    }

    auto txn = sqlite_manager::Transaction::Begin(db.connection());
    if (!txn) {
        return txn.error();
    }
    ProfileRecord p;
    try {
        // Bullet (and its curve).
        const json& jb = doc.at("bullet");
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
            b.id = *same;
        } else if (auto id = Repository<BulletRecord>(db).Save(b); !id) {
            return id.error();
        }

        // Cartridge.
        const json& jc = doc.at("cartridge");
        CartridgeRecord c;
        c.name = jc.at("name").get<std::string>();
        c.bullet_id = b.id;
        c.muzzle_velocity_mps = jc.at("muzzle_velocity_mps").get<double>();
        c.reference_powder_temp_k = jc.value("reference_powder_temp_k", 288.15);
        c.powder_sensitivity_per_k = jc.value("powder_sensitivity_per_k", 0.0);
        c.barrel_length_m = jc.value("barrel_length_m", 0.0);
        c.source = jc.value("source", "");
        c.notes = jc.value("notes", "");
        for (const json& vp : jc.value("velocity_points", json::array())) {
            c.velocity_points.emplace_back(vp.at(0).get<double>(), vp.at(1).get<double>());
        }
        if (auto id = Repository<CartridgeRecord>(db).Save(c); !id) {
            return id.error();
        }

        // Rifle.
        const json& jr = doc.at("rifle");
        RifleRecord r;
        r.name = jr.at("name").get<std::string>();
        r.caliber = jr.value("caliber", "");
        r.barrel_length_m = jr.value("barrel_length_m", 0.0);
        r.twist_m = jr.value("twist_m", 0.0);
        r.sight_height_m = jr.at("sight_height_m").get<double>();
        r.notes = jr.value("notes", "");
        if (auto id = Repository<RifleRecord>(db).Save(r); !id) {
            return id.error();
        }

        // Scope (and reticle).
        std::optional<Id> scope_id;
        if (doc.contains("scope") && !doc.at("scope").is_null()) {
            const json& js = doc.at("scope");
            ScopeRecord s;
            s.name = js.at("name").get<std::string>();
            s.click_units = js.value("click_units", "mrad");
            s.click_vertical_rad = js.at("click_vertical_rad").get<double>();
            s.click_horizontal_rad = js.at("click_horizontal_rad").get<double>();
            s.min_magnification = js.value("min_magnification", 0.0);
            s.max_magnification = js.value("max_magnification", 0.0);
            s.notes = js.value("notes", "");
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
            scope_id = s.id;
        }

        // Profile.
        const json& jp = doc.at("profile");
        p.name = UniqueProfileName(db, jp.at("name").get<std::string>());
        p.rifle_id = r.id;
        p.scope_id = scope_id;
        p.cartridge_id = c.id;
        p.zero_range_m = jp.at("zero_range_m").get<double>();
        p.zero_offset_up_m = jp.value("zero_offset_up_m", 0.0);
        p.zero_offset_right_m = jp.value("zero_offset_right_m", 0.0);
        p.zero_atmosphere = AtmosphereFrom(jp.at("zero_atmosphere"));
        p.zero_powder_temp_k = jp.value("zero_powder_temp_k", p.zero_atmosphere.temperature_k);
        p.velocity_scale = jp.value("velocity_scale", 1.0);
        p.drag_scale = jp.value("drag_scale", 1.0);
        if (auto id = Repository<ProfileRecord>(db).Save(p); !id) {
            return id.error();
        }
    } catch (const json::exception& e) {
        return BadFile(e.what());
    }
    if (auto s = txn.value().Commit(); !s) {
        return s.error();
    }
    return p.id;
}

} // namespace ballistics::applogic
