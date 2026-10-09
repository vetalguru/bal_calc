#include <array>

#include "internal.h"

namespace ballistics::bridge {

namespace {

// WEZ settings in the app's keys and units.
struct WezKey {
    const char* key;
    double al::WezSettings::* value;
};
const std::array<WezKey, 15> kWezKeys = {{
    {"rangeM", &al::WezSettings::range_m},
    {"windSpeedMps", &al::WezSettings::wind_speed_mps},
    {"windDirectionDeg", &al::WezSettings::wind_direction_deg},
    {"muzzleVelocityMps", &al::WezSettings::muzzle_velocity_mps},
    {"bcPercent", &al::WezSettings::bc_percent},
    {"temperatureC", &al::WezSettings::temperature_c},
    {"pressureHpa", &al::WezSettings::pressure_hpa},
    {"humidityPct", &al::WezSettings::humidity_pct},
    {"lookAngleDeg", &al::WezSettings::look_angle_deg},
    {"cantDeg", &al::WezSettings::cant_deg},
    {"azimuthDeg", &al::WezSettings::azimuth_deg},
    {"latitudeDeg", &al::WezSettings::latitude_deg},
    {"groupMoa", &al::WezSettings::group_moa},
    {"targetWidthCm", &al::WezSettings::target_width_cm},
    {"targetHeightCm", &al::WezSettings::target_height_cm},
}};
}  // namespace

// ---- Forms <-> JSON ---------------------------------------------------------

json ToJson(const al::RifleForm& f) {
    return {{"rifleId", f.rifle_id},
            {"name", f.name},
            {"caliber", f.caliber},
            {"sightHeightCm", f.sight_height_cm},
            {"twistIn", f.twist_in},
            {"twistLeft", f.twist_left},
            {"clickUnits", f.click_units},
            {"clickValue", f.click_value},
            {"reticleId", f.reticle_id},
            {"focalPlane", f.focal_plane},
            {"sfpReferenceMagnification", f.sfp_reference_magnification},
            {"minMagnification", f.min_magnification},
            {"maxMagnification", f.max_magnification},
            {"zeroRangeM", f.zero_range_m},
            {"zeroTemperatureC", f.zero_temperature_c},
            {"zeroPressureHpa", f.zero_pressure_hpa},
            {"zeroAltitudeM", f.zero_altitude_m},
            {"zeroHumidityPct", f.zero_humidity_pct},
            {"zeroPowderC", f.zero_powder_c}};
}

al::RifleForm RifleFrom(const json& m) {
    al::RifleForm f;
    f.rifle_id = IdOf(m, "rifleId");
    f.name = Str(m, "name");
    f.caliber = Str(m, "caliber");
    f.sight_height_cm = Num(m, "sightHeightCm", f.sight_height_cm);
    f.twist_in = Num(m, "twistIn", f.twist_in);
    f.twist_left = Bool(m, "twistLeft");
    f.click_units = Str(m, "clickUnits", f.click_units);
    f.click_value = Num(m, "clickValue", f.click_value);
    f.reticle_id = IdOf(m, "reticleId");
    f.focal_plane = Str(m, "focalPlane", "ffp");
    f.sfp_reference_magnification = Num(m, "sfpReferenceMagnification");
    f.min_magnification = Num(m, "minMagnification");
    f.max_magnification = Num(m, "maxMagnification");
    f.zero_range_m = Num(m, "zeroRangeM", f.zero_range_m);
    f.zero_temperature_c = Num(m, "zeroTemperatureC", f.zero_temperature_c);
    f.zero_pressure_hpa = Num(m, "zeroPressureHpa", f.zero_pressure_hpa);
    f.zero_altitude_m = Num(m, "zeroAltitudeM", f.zero_altitude_m);
    f.zero_humidity_pct = Num(m, "zeroHumidityPct", f.zero_humidity_pct);
    f.zero_powder_c = Num(m, "zeroPowderC", f.zero_powder_c);
    return f;
}

json ToJson(const al::CartridgeForm& f) {
    return {{"cartridgeId", f.cartridge_id},
            {"copyOf", f.copy_of},
            {"libraryBulletId", f.library_bullet_id},
            {"name", f.name},
            {"caliber", f.caliber},
            {"bulletName", f.bullet_name},
            {"dragTable", f.drag_table},
            {"bc", f.bc},
            {"massGr", f.mass_gr},
            {"diameterIn", f.diameter_in},
            {"lengthIn", f.length_in},
            {"muzzleVelocity", f.muzzle_velocity_mps},
            {"powderReferenceC", f.powder_reference_c},
            {"powderSensitivity", f.powder_sensitivity_pct_per_c}};
}

al::CartridgeForm CartridgeFrom(const json& m) {
    al::CartridgeForm f;
    f.cartridge_id = IdOf(m, "cartridgeId");
    f.copy_of = IdOf(m, "copyOf");
    f.library_bullet_id = IdOf(m, "libraryBulletId");
    f.name = Str(m, "name");
    f.caliber = Str(m, "caliber");
    f.bullet_name = Str(m, "bulletName");
    f.drag_table = Str(m, "dragTable", f.drag_table);
    f.bc = Num(m, "bc");
    f.mass_gr = Num(m, "massGr");
    f.diameter_in = Num(m, "diameterIn");
    f.length_in = Num(m, "lengthIn");
    f.muzzle_velocity_mps = Num(m, "muzzleVelocity");
    f.powder_reference_c = Num(m, "powderReferenceC", f.powder_reference_c);
    f.powder_sensitivity_pct_per_c = Num(m, "powderSensitivity");
    return f;
}

json ToJson(const al::CartridgeSummary& c, bool matches) {
    return {{"id", c.id},
            {"name", c.name},
            {"caliber", c.caliber},
            {"bulletName", c.bullet_name},
            {"muzzleVelocity", c.muzzle_velocity_mps},
            {"matches", matches}};
}

json ToJson(const al::BulletForm& f) {
    json bands = json::array();
    for (const al::BcBand& b : f.bands) {
        bands.push_back({{"velocity", b.velocity_mps}, {"bc", b.bc}});
    }
    return {{"id", f.id},
            {"name", f.name},
            {"manufacturer", f.manufacturer},
            {"caliber", f.caliber},
            {"massGr", f.mass_gr},
            {"diameterIn", f.diameter_in},
            {"lengthIn", f.length_in},
            {"dragTable", f.drag_table},
            {"bc", f.bc},
            {"bands", bands},
            {"notes", f.notes},
            {"source", f.source},
            {"hasCustomCurve", f.has_custom_curve}};
}

al::BulletForm BulletFrom(const json& m) {
    al::BulletForm f;
    f.id = IdOf(m);
    f.name = Str(m, "name");
    f.manufacturer = Str(m, "manufacturer");
    f.caliber = Str(m, "caliber");
    f.mass_gr = Num(m, "massGr");
    f.diameter_in = Num(m, "diameterIn");
    f.length_in = Num(m, "lengthIn");
    f.drag_table = Str(m, "dragTable", f.drag_table);
    f.bc = Num(m, "bc");
    if (m.contains("bands") && m.at("bands").is_array()) {
        for (const json& b : m.at("bands")) {
            f.bands.push_back({Num(b, "velocity"), Num(b, "bc")});
        }
    }
    f.notes = Str(m, "notes");
    f.source = Str(m, "source", al::kSourceLibrary);
    f.has_custom_curve = Bool(m, "hasCustomCurve");
    return f;
}

json ToJson(const al::WezSettings& w) {
    json out = {{"targetKind", w.target_kind}};
    for (const WezKey& k : kWezKeys) {
        out[k.key] = w.*k.value;
    }
    return out;
}

void Update(al::WezSettings& w, const json& a) {
    for (const WezKey& k : kWezKeys) {
        w.*k.value = Num(a, k.key, w.*k.value);
    }
    const std::string kind = Str(a, "targetKind", w.target_kind);
    if (kind == "rectangle" || kind == "ellipse" || kind == "figure") {
        w.target_kind = kind;
    }
}

json ToJson(const al::WezRow& r) {
    return {{"rangeM", r.range_m},
            {"probability", r.probability},
            {"sigmaUpCm", r.sigma_up_cm},
            {"sigmaRightCm", r.sigma_right_cm}};
}

json DsfJson(const std::vector<DsfPoint>& points) {
    json out = json::array();
    for (const DsfPoint& p : points) {
        out.push_back({{"mach", p.mach}, {"factor", p.factor}});
    }
    return out;
}

json ToJson(const al::RangeTable& t, bool has_scope) {
    json rows = json::array();
    for (const al::RangeRow& r : t.rows) {
        rows.push_back({{"rangeM", r.range_m},
                        {"elevation", r.elevation},
                        {"windage", r.windage},
                        {"elevationClicks", r.elevation_clicks},
                        {"windageClicks", r.windage_clicks},
                        {"dropCm", r.drop_cm},
                        {"windageCm", r.windage_cm},
                        {"velocity", r.velocity_mps},
                        {"mach", r.mach},
                        {"energy", r.energy_j},
                        {"time", r.time_s},
                        {"lead", r.lead},
                        {"leadClicks", r.lead_clicks},
                        {"leadCm", r.lead_cm},
                        {"spinDriftCm", r.spin_drift_cm},
                        {"coriolisDriftCm", r.coriolis_drift_cm},
                        {"coriolisLiftCm", r.coriolis_lift_cm}});
    }
    return {{"ok", t.ok}, {"error", t.error}, {"hasScope", has_scope}, {"rows", rows}};
}

}  // namespace ballistics::bridge
