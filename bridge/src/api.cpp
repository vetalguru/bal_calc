#include <ballistics/bridge/api.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <ballistics/applogic/armory.h>
#include <ballistics/applogic/importers.h>
#include <ballistics/applogic/library.h>
#include <ballistics/applogic/profile_io.h>
#include <ballistics/applogic/reticle.h>
#include <ballistics/applogic/session.h>
#include <ballistics/applogic/truing.h>
#include <ballistics/applogic/wez.h>
#include <ballistics/atmosphere.h>
#include <ballistics/storage/database.h>
#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>
#include <ballistics/units.h>
#include <ballistics/version.h>

// Methods (arguments → result):
//
//   open {path}                       → {databasePath}
//   seed {version, files:[{name, content}]} → {imported, skipped, problems}
//   info                              → {engineVersion, sqliteVersion, databasePath}
//   state                             → {rifles, cartridges, currentRifleId,
//                                        currentCartridgeId, currentProfileId,
//                                        currentPair, angleUnit, holdMode, language,
//                                        tableFromM, tableToM, tableStepM, conditions}
//   select {rifleId?, cartridgeId?}   → state
//   setConditions {any condition keys} → conditions
//   setSettings {angleUnit?, holdMode?, language?, tableFromM?, tableToM?, tableStepM?}
//                                     → state
//   solution                          → {ok, error, rangeM, elevation, windage, ...}
//   rangeTable                        → {ok, error, hasScope, computeMs, rows}
//   trajectoryCurve {maxRangeM, points} → as rangeTable
//   rifleForm {id} / saveRifle {form} / deleteRifle {id}
//   cartridgeForm {id} / saveCartridge {form} / deleteCartridge {id}
//   cartridgeFormWithBullet {form, bulletId} / libraryCartridges {filter}
//   cartridgeFormFromLibrary {id}
//   setZeroOffset {upCm, rightCm} / addSample {rifleName, cartridgeName}
//   shots / logShot {rangeM, elevation, hasWindage, windage, notes}
//   deleteShot {id} / setShotUsed {id, used}
//   computeTruing / applyTruing / resetTruing
//   computeDsf / applyDsf / setDsf {points:[{mach, factor}]} / resetDsf
//   wez {settings?, toM, stepM}     → {settings, ok, error, rows, atTarget, parts, shots50/80/95}
//   bcCalculator {mode: "chronograph"|"hit", table, vNearMps, vFarMps, distanceM,
//                 rangeM, elevation} → {ok, error, bc, table}
//   reticles / libraryBullets {filter} / bulletForm {id} / saveBullet {form}
//   deleteBullet {id}
//   exportJson {kind: "rifle"|"cartridge", id} → {json, fileName}
//   importShared {text}               → state
//   importFiles {files:[{name, content}]} → {imported, problems:[{file, message}]}
//   stationPressure {qnhHpa, altitudeM} → hPa
//   compareCurves {maxRangeM, points, pairs:[{rifleId, cartridgeId}]} → [table + label]
//   pairOptions                     → [{rifleId, rifleName, cartridges:[{id, name}]}]
//
// save* return {id}; delete*, set* and log return state or {} as noted in
// the handlers below.

namespace ballistics::bridge {

namespace {

namespace al = ballistics::applogic;
namespace bs = ballistics::storage;
namespace u = ballistics::units;
using nlohmann::json;
using al::Id;

constexpr const char* kCurrentProfileKey = "ui.current_profile"; // before v3: the selection
constexpr const char* kCurrentRifleKey = "ui.current_rifle";
constexpr const char* kCurrentCartridgeKey = "ui.current_cartridge";
constexpr const char* kAngleUnitKey = "ui.angle_unit";
constexpr const char* kLanguageKey = "ui.language";
constexpr std::size_t kMaxExtraWindZones = 2; // three wind zones in all
constexpr const char* kTargetSpeedUnitKey = "ui.target_speed_unit";
constexpr const char* kHoldModeKey = "ui.hold_mode";
constexpr const char* kTableFromKey = "ui.table.from_m";
constexpr const char* kTableToKey = "ui.table.to_m";
constexpr const char* kTableStepKey = "ui.table.step_m";

// A failure reported to the caller as {"ok": false, "error": message}.
struct Failure : std::runtime_error {
    using std::runtime_error::runtime_error;
};

template <typename T>
T Must(bs::Result<T> r) {
    if (!r) {
        throw Failure(r.error().message);
    }
    return std::move(r).value();
}

void Must(const bs::Status& s) {
    if (!s) {
        throw Failure(s.error().message);
    }
}

double NowUnix() {
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}

double Num(const json& j, const char* key, double fallback = 0.0) {
    return j.contains(key) && j.at(key).is_number() ? j.at(key).get<double>() : fallback;
}
Id IdOf(const json& j, const char* key = "id") {
    return j.contains(key) && j.at(key).is_number() ? j.at(key).get<Id>() : 0;
}
std::string Str(const json& j, const char* key, const std::string& fallback = "") {
    return j.contains(key) && j.at(key).is_string() ? j.at(key).get<std::string>() : fallback;
}
bool Bool(const json& j, const char* key, bool fallback = false) {
    return j.contains(key) && j.at(key).is_boolean() ? j.at(key).get<bool>() : fallback;
}

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

// WEZ settings in the app's keys and units.
struct WezKey {
    const char* key;
    double al::WezSettings::*value;
};
const WezKey kWezKeys[] = {
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
};

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

// Invalid UTF-8 (a hand-edited import, say) is replaced, never thrown.
std::string Dump(const json& j) { return j.dump(-1, ' ', false, json::error_handler_t::replace); }

Id FirstId(const json& list) { return list.empty() ? 0 : list.front().at("id").get<Id>(); }
bool Contains(const json& list, Id id) {
    return std::any_of(list.begin(), list.end(),
                       [id](const json& v) { return v.at("id").get<Id>() == id; });
}

} // namespace

struct Api::Impl {
    bs::Database db;
    bool open = false;
    std::string db_path;

    // Selection and the pair derived from it.
    json rifles = json::array();
    json cartridges = json::array();
    Id rifle_id = 0;
    Id cartridge_id = 0;
    Id profile_id = 0;

    // Settings (persisted).
    std::string angle_unit = "mrad";
    std::string hold_mode = "dial_elevation";
    std::string language; // "" = system
    double table_from_m = 100.0;
    double table_to_m = 1000.0;
    double table_step_m = 50.0;

    // Current conditions in UI terms (persisted as the session).
    double temperature_c = 15.0;
    double pressure_hpa = 1013.25;
    double altitude_m = 0.0;
    double humidity_pct = 50.0;
    bool powder_follows_air = true;
    double powder_c = 15.0;
    double wind_speed = 0.0;
    double wind_from_deg = 90.0;
    double wind_until_m = 0.0;       // end of the first zone when there are more
    std::vector<al::WindInput> wind_zones; // the zones after the first, in order
    double wind_gust_mps = 0.0;
    double target_speed_mps = 0.0;
    double target_heading_deg = 90.0;
    std::string target_speed_unit = "kmh"; // how the app shows it: "kmh" or "mps"
    double look_angle_deg = 0.0;
    double cant_deg = 0.0;
    bool coriolis = false;
    double latitude_deg = 50.0;
    bool use_azimuth = false;
    double azimuth_deg = 0.0;
    double target_range_m = 300.0;
    double magnification = 0.0;
    bool use_density_altitude = false;
    double density_altitude_m = 0.0;
    double target_height_cm = 20.0;
    double weather_at_unix = 0.0;

    al::TruingResult last_truing;
    al::DsfResult last_dsf;

    using Handler = std::function<json(Impl&, const json&)>;
    static const std::map<std::string, Handler>& Handlers();

    void RequireOpen() const {
        if (!open) {
            throw Failure("The database is not open.");
        }
    }

    al::AngleUnit Unit() const {
        return angle_unit == "moa" ? al::AngleUnit::kMoa : al::AngleUnit::kMrad;
    }
    double UnitRad() const { return angle_unit == "moa" ? u::MoaToRad(1.0) : u::MradToRad(1.0); }

    // ---- Settings and session ------------------------------------------------

    std::optional<std::string> Setting(const char* key) {
        auto v = bs::GetSetting(db, key);
        return v && v.value() ? v.value() : std::nullopt;
    }
    void Put(const char* key, const std::string& value) { bs::SetSetting(db, key, value).ok(); }

    void LoadSettings() {
        angle_unit = Setting(kAngleUnitKey).value_or(angle_unit);
        hold_mode = Setting(kHoldModeKey).value_or(hold_mode);
        language = Setting(kLanguageKey).value_or(language);
        target_speed_unit = Setting(kTargetSpeedUnitKey).value_or(target_speed_unit);
        for (const auto& [key, value] : {std::pair{kTableFromKey, &table_from_m},
                                         std::pair{kTableToKey, &table_to_m},
                                         std::pair{kTableStepKey, &table_step_m}}) {
            if (auto v = Setting(key)) {
                try {
                    *value = std::stod(*v);
                } catch (const std::exception&) {
                }
            }
        }
        if (auto session = al::LoadSession(db)) {
            ApplySession(session.value());
        }
    }

    al::SessionConditions Session() const {
        al::SessionConditions s;
        s.temperature_c = temperature_c;
        s.pressure_hpa = pressure_hpa;
        s.altitude_m = altitude_m;
        s.humidity_pct = humidity_pct;
        if (!powder_follows_air) {
            s.powder_c = powder_c;
        }
        // The first zone is always there (a calm one costs nothing), so its
        // direction survives a zero speed and the gust has a zone to go to.
        s.winds.push_back({wind_speed, wind_from_deg, wind_zones.empty() ? 0.0 : wind_until_m});
        for (std::size_t i = 0; i < wind_zones.size(); ++i) {
            al::WindInput w = wind_zones[i];
            if (i + 1 == wind_zones.size()) {
                w.until_m = 0.0; // the last zone goes to the end
            }
            s.winds.push_back(w);
        }
        s.wind_gust_mps = wind_gust_mps;
        s.target_speed_mps = target_speed_mps;
        s.target_heading_deg = target_heading_deg;
        s.look_angle_deg = look_angle_deg;
        s.cant_deg = cant_deg;
        if (coriolis) {
            s.latitude_deg = latitude_deg;
            if (use_azimuth) {
                s.azimuth_deg = azimuth_deg;
            }
        }
        s.target_range_m = target_range_m;
        s.magnification = magnification;
        if (use_density_altitude) {
            s.density_altitude_m = density_altitude_m;
        }
        s.target_height_cm = target_height_cm;
        s.weather_at_unix = weather_at_unix;
        return s;
    }

    void ApplySession(const al::SessionConditions& s) {
        temperature_c = s.temperature_c;
        pressure_hpa = s.pressure_hpa;
        altitude_m = s.altitude_m;
        humidity_pct = s.humidity_pct;
        powder_follows_air = !s.powder_c.has_value();
        powder_c = s.powder_c.value_or(s.temperature_c);
        if (!s.winds.empty()) {
            wind_speed = s.winds.front().speed_mps;
            wind_from_deg = s.winds.front().from_deg;
            wind_until_m = s.winds.front().until_m;
        }
        wind_zones.assign(s.winds.size() > 1 ? s.winds.begin() + 1 : s.winds.end(), s.winds.end());
        wind_gust_mps = s.wind_gust_mps;
        target_speed_mps = s.target_speed_mps;
        target_heading_deg = s.target_heading_deg;
        look_angle_deg = s.look_angle_deg;
        cant_deg = s.cant_deg;
        coriolis = s.latitude_deg.has_value();
        latitude_deg = s.latitude_deg.value_or(latitude_deg);
        use_azimuth = s.azimuth_deg.has_value();
        azimuth_deg = s.azimuth_deg.value_or(azimuth_deg);
        target_range_m = s.target_range_m;
        magnification = s.magnification;
        use_density_altitude = s.density_altitude_m.has_value();
        density_altitude_m = s.density_altitude_m.value_or(density_altitude_m);
        target_height_cm = s.target_height_cm;
        weather_at_unix = s.weather_at_unix;
    }

    json Conditions() const {
        return {{"temperatureC", temperature_c},   {"pressureHpa", pressure_hpa},
                {"altitudeM", altitude_m},         {"humidityPct", humidity_pct},
                {"powderFollowsAir", powder_follows_air}, {"powderC", powder_c},
                {"windSpeed", wind_speed},         {"windFromDeg", wind_from_deg},
                {"lookAngleDeg", look_angle_deg},  {"cantDeg", cant_deg},
                {"coriolis", coriolis},            {"latitudeDeg", latitude_deg},
                {"useAzimuth", use_azimuth},       {"azimuthDeg", azimuth_deg},
                {"targetRangeM", target_range_m},  {"magnification", magnification},
                {"useDensityAltitude", use_density_altitude},
                {"densityAltitudeM", density_altitude_m},
                {"targetHeightCm", target_height_cm},
                {"windUntilM", wind_until_m},
                {"windZones", ZonesJson()},
                {"windGustMps", wind_gust_mps},
                {"targetSpeedMps", target_speed_mps},
                {"targetHeadingDeg", target_heading_deg},
                {"targetSpeedUnit", target_speed_unit}};
    }

    json ZonesJson() const {
        json zones = json::array();
        for (const al::WindInput& w : wind_zones) {
            zones.push_back({{"speedMps", w.speed_mps}, {"fromDeg", w.from_deg}, {"untilM", w.until_m}});
        }
        return zones;
    }

    void SetConditions(const json& a) {
        const auto num = [&a](const char* key, double& field) { field = Num(a, key, field); };
        const auto flag = [&a](const char* key, bool& field) { field = Bool(a, key, field); };
        const auto air = std::make_tuple(temperature_c, pressure_hpa, altitude_m, humidity_pct,
                                         use_density_altitude, density_altitude_m);
        num("temperatureC", temperature_c);
        num("pressureHpa", pressure_hpa);
        num("altitudeM", altitude_m);
        num("humidityPct", humidity_pct);
        flag("powderFollowsAir", powder_follows_air);
        num("powderC", powder_c);
        num("windSpeed", wind_speed);
        num("windFromDeg", wind_from_deg);
        num("lookAngleDeg", look_angle_deg);
        num("cantDeg", cant_deg);
        flag("coriolis", coriolis);
        num("latitudeDeg", latitude_deg);
        flag("useAzimuth", use_azimuth);
        num("azimuthDeg", azimuth_deg);
        num("targetRangeM", target_range_m);
        num("magnification", magnification);
        flag("useDensityAltitude", use_density_altitude);
        num("densityAltitudeM", density_altitude_m);
        num("targetHeightCm", target_height_cm);
        num("windUntilM", wind_until_m);
        num("windGustMps", wind_gust_mps);
        num("targetSpeedMps", target_speed_mps);
        num("targetHeadingDeg", target_heading_deg);
        if (const std::string u = Str(a, "targetSpeedUnit"); (u == "kmh" || u == "mps") && u != target_speed_unit) {
            target_speed_unit = u;
            Put(kTargetSpeedUnitKey, u);
        }
        if (a.contains("windZones") && a.at("windZones").is_array()) {
            wind_zones.clear();
            for (const json& z : a.at("windZones")) {
                if (wind_zones.size() == kMaxExtraWindZones) {
                    break;
                }
                wind_zones.push_back({Num(z, "speedMps"), Num(z, "fromDeg", 90.0), Num(z, "untilM")});
            }
        }
        // The air was entered now: the solution warns when it gets old.
        if (air != std::make_tuple(temperature_c, pressure_hpa, altitude_m, humidity_pct,
                                   use_density_altitude, density_altitude_m)) {
            weather_at_unix = NowUnix();
        }
        al::SaveSession(db, Session()).ok();
    }

    // ---- Armory and selection ------------------------------------------------

    void ReloadArmory() {
        rifles = json::array();
        std::string caliber;
        for (const al::RifleSummary& r : Must(al::ListRifles(db))) {
            rifles.push_back({{"id", r.id}, {"name", r.name}, {"caliber", r.caliber}});
            if (r.id == rifle_id) {
                caliber = r.caliber;
            }
        }
        cartridges = json::array();
        for (const al::CartridgeSummary& c : Must(al::ListCartridges(db, caliber))) {
            cartridges.push_back(ToJson(c, al::SameCaliber(c.caliber, caliber)));
        }
    }

    // Keeps the selection valid and the pair in step with it.
    void UpdatePair() {
        if (!Contains(rifles, rifle_id)) {
            rifle_id = FirstId(rifles);
        }
        if (!Contains(cartridges, cartridge_id)) {
            cartridge_id = FirstId(cartridges);
        }
        Put(kCurrentRifleKey, std::to_string(rifle_id));
        Put(kCurrentCartridgeKey, std::to_string(cartridge_id));
        Id pair = 0;
        if (rifle_id != 0 && cartridge_id != 0) {
            pair = Must(al::EnsureProfile(db, rifle_id, cartridge_id));
        }
        if (pair != profile_id) {
            profile_id = pair;
            last_truing = {};
        }
    }

    void Select(Id rifle, Id cartridge) {
        const bool rifle_changed = rifle != rifle_id;
        rifle_id = rifle;
        cartridge_id = cartridge;
        if (rifle_changed) {
            ReloadArmory(); // cartridges of the new calibre first
        }
        UpdatePair();
    }

    // After the lists changed: reorder for the current rifle, keep valid.
    void Refresh() {
        ReloadArmory();
        UpdatePair();
    }

    json CurrentPair() {
        if (profile_id == 0) {
            return json::object();
        }
        auto p = bs::LoadProfile(db, profile_id);
        if (!p) {
            return json::object();
        }
        return {{"rifleName", p.value().rifle.name},
                {"cartridgeName", p.value().cartridge.name},
                {"zeroRangeM", p.value().rifle.zero_range_m},
                {"offsetUpCm", p.value().profile.zero_offset_up_m * 100.0},
                {"offsetRightCm", p.value().profile.zero_offset_right_m * 100.0}};
    }

    json State() {
        return {{"rifles", rifles},
                {"cartridges", cartridges},
                {"currentRifleId", rifle_id},
                {"currentCartridgeId", cartridge_id},
                {"currentProfileId", profile_id},
                {"currentPair", CurrentPair()},
                {"angleUnit", angle_unit},
                {"holdMode", hold_mode},
                {"language", language},
                {"tableFromM", table_from_m},
                {"tableToM", table_to_m},
                {"tableStepM", table_step_m},
                {"conditions", Conditions()}};
    }

    void Open(const std::string& path) {
        if (open) {
            throw Failure("The database is already open.");
        }
        Must(db.Open(path));
        open = true;
        db_path = path;
        LoadSettings();
        const auto setting_id = [this](const char* key) -> Id {
            const auto v = Setting(key);
            try {
                return v ? std::stoll(*v) : 0;
            } catch (const std::exception&) {
                return 0;
            }
        };
        rifle_id = setting_id(kCurrentRifleKey);
        cartridge_id = setting_id(kCurrentCartridgeKey);
        if (rifle_id == 0 && cartridge_id == 0) {
            // Upgraded from a version with profiles: keep the profile chosen there.
            auto p = bs::Repository<bs::ProfileRecord>(db).Get(setting_id(kCurrentProfileKey));
            if (p && p.value()) {
                rifle_id = p.value()->rifle_id;
                cartridge_id = p.value()->cartridge_id;
            }
        }
        profile_id = 0;
        Refresh();
    }

    // ---- Solution ------------------------------------------------------------

    void AddReticle(const bs::LoadedProfile& p, const al::SolutionSummary& r, json& out) {
        out["hasReticle"] = false;
        if (!r.ok || !p.scope) {
            return;
        }
        const bs::ScopeRecord& scope = *p.scope;
        const double zoom = magnification > 0.0 ? magnification : scope.max_magnification;
        const al::ReticleHold hold =
            al::ComputeReticleHold(r.elevation * UnitRad(), r.windage * UnitRad(), scope, zoom,
                                   al::HoldModeFromString(hold_mode));
        out["holdMode"] = hold_mode;
        out["dialElevationClicks"] = hold.dial_elevation_clicks;
        out["dialWindageClicks"] = hold.dial_windage_clicks;
        out["targetX"] = hold.target_x;
        out["targetY"] = hold.target_y;
        out["subtensionScale"] = hold.scale;
        out["focalPlane"] = scope.focal_plane;
        out["minMagnification"] = scope.min_magnification;
        out["maxMagnification"] = scope.max_magnification;
        out["magnification"] = zoom;
        if (scope.reticle_id) {
            auto ret = bs::Repository<bs::ReticleRecord>(db).Get(*scope.reticle_id);
            if (ret && ret.value()) {
                out["hasReticle"] = true;
                out["reticleName"] = ret.value()->name;
                out["reticleUnits"] = ret.value()->units;
                out["reticleDefinition"] = ret.value()->definition;
            }
        }
    }

    json Solution() {
        const auto start = std::chrono::steady_clock::now();
        json out;
        if (profile_id == 0) {
            out = {{"ok", false}, {"error", "Choose a rifle and a cartridge."}};
        } else if (auto p = bs::LoadProfile(db, profile_id); !p) {
            out = {{"ok", false}, {"error", p.error().message}};
        } else {
            const al::SolutionSummary r = al::Summarize(p.value(), Session(), Unit(), NowUnix());
            out = {{"ok", r.ok},
                   {"error", r.error},
                   {"rangeM", r.range_m},
                   {"elevation", r.elevation},
                   {"windage", r.windage},
                   {"elevationClicks", r.elevation_clicks},
                   {"windageClicks", r.windage_clicks},
                   {"hasScope", p.value().scope.has_value()},
                   {"dropCm", r.drop_cm},
                   {"windageCm", r.windage_cm},
                   {"velocity", r.velocity_mps},
                   {"energy", r.energy_j},
                   {"time", r.time_s},
                   {"mach", r.mach},
                   {"muzzleVelocity", r.muzzle_velocity_mps},
                   {"stability", r.stability},
                   {"velocityScale", p.value().profile.velocity_scale},
                   {"dragScale", p.value().profile.drag_scale},
                   {"dsf", DsfJson(p.value().profile.dsf)},
                   {"spinDriftCm", r.spin_drift_cm},
                   {"subsonic", r.subsonic},
                   {"transonicRangeM", r.transonic_range_m},
                   {"apexCm", r.apex_cm},
                   {"apexRangeM", r.apex_range_m},
                   {"pointBlankNearM", r.point_blank_near_m},
                   {"pointBlankFarM", r.point_blank_far_m},
                   {"densityAltitudeM", r.density_altitude_m},
                   {"pressureHpa", r.pressure_hpa},
                   {"hasGust", r.has_gust},
                   {"gustWindage", r.gust_windage},
                   {"gustWindageClicks", r.gust_windage_clicks},
                   {"gustWindageCm", r.gust_windage_cm},
                   {"hasLead", r.has_lead},
                   {"lead", r.lead},
                   {"leadClicks", r.lead_clicks},
                   {"leadCm", r.lead_cm},
                   {"leadTotalWindage", r.lead_total_windage},
                   {"leadTotalWindageClicks", r.lead_total_windage_clicks},
                   {"leadRangeM", r.lead_range_m},
                   {"leadElevation", r.lead_elevation},
                   {"leadElevationClicks", r.lead_elevation_clicks}};
            json warnings = json::array();
            for (const al::Warning& w : r.warnings) {
                warnings.push_back({{"code", w.code}, {"value", w.value}});
            }
            out["warnings"] = warnings;
            AddReticle(p.value(), r, out);
        }
        out["computeMs"] = std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - start)
                               .count();
        return out;
    }

    // Curves of other rifle + cartridge pairs in the current conditions (at
    // most four), each as a range table with its label.
    json CompareCurves(const json& a) {
        const double max_range = std::clamp(Num(a, "maxRangeM", 1000.0), 10.0, 3000.0);
        const int points = std::clamp(static_cast<int>(Num(a, "points", 250)), 10, 1000);
        json out = json::array();
        for (const json& pair : a.value("pairs", json::array())) {
            if (out.size() == 4) {
                break;
            }
            const Id rifle = pair.value("rifleId", Id{0});
            const Id cartridge = pair.value("cartridgeId", Id{0});
            json curve = {{"ok", false}, {"error", ""}, {"rows", json::array()}, {"label", ""}};
            const auto profile = al::EnsureProfile(db, rifle, cartridge);
            if (!profile) {
                curve["error"] = profile.error().message;
            } else if (auto p = bs::LoadProfile(db, profile.value()); !p) {
                curve["error"] = p.error().message;
            } else {
                curve = ToJson(al::BuildRangeTable(p.value(), Session(), Unit(), 0.0, max_range,
                                                   max_range / points),
                               p.value().scope.has_value());
                curve["label"] = p.value().rifle.name + " · " + p.value().cartridge.name;
            }
            curve["rifleId"] = rifle;
            curve["cartridgeId"] = cartridge;
            out.push_back(curve);
        }
        return out;
    }

    // Every rifle with the cartridges of its calibre: what can be compared.
    json PairOptions() {
        const auto cartridges = Must(al::ListCartridges(db));
        json out = json::array();
        for (const al::RifleSummary& r : Must(al::ListRifles(db))) {
            json list = json::array();
            for (const al::CartridgeSummary& c : cartridges) {
                if (al::SameCaliber(c.caliber, r.caliber)) {
                    list.push_back({{"id", c.id}, {"name", c.name}});
                }
            }
            out.push_back({{"rifleId", r.id}, {"rifleName", r.name}, {"cartridges", list}});
        }
        return out;
    }

    json Table(double from_m, double to_m, double step_m) {
        const auto start = std::chrono::steady_clock::now();
        if (profile_id == 0) {
            return {{"ok", false}, {"error", "Choose a rifle and a cartridge."}, {"rows", json::array()}};
        }
        auto p = bs::LoadProfile(db, profile_id);
        if (!p) {
            return {{"ok", false}, {"error", p.error().message}, {"rows", json::array()}};
        }
        json out = ToJson(al::BuildRangeTable(p.value(), Session(), Unit(), from_m, to_m, step_m),
                          p.value().scope.has_value());
        out["computeMs"] = std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - start)
                               .count();
        return out;
    }

    // ---- Shot log and truing -------------------------------------------------

    json Shots() {
        json out = json::array();
        if (profile_id == 0) {
            return out;
        }
        for (const bs::DopeRecord& d : Must(al::ListShots(db, profile_id))) {
            out.push_back(
                {{"id", d.id},
                 {"rangeM", d.range_m},
                 {"observed", al::FromRad(d.observed_elevation_rad, Unit())},
                 {"predicted", d.predicted_elevation_rad
                                   ? json(al::FromRad(*d.predicted_elevation_rad, Unit()))
                                   : json(nullptr)},
                 {"hasWindage", d.observed_windage_rad.has_value()},
                 {"observedWindage", al::FromRad(d.observed_windage_rad.value_or(0.0), Unit())},
                 {"shotAt", d.shot_at},
                 {"used", d.use_for_truing},
                 {"notes", d.notes},
                 {"temperatureC", u::KToC(d.atmosphere.temperature_k)}});
        }
        return out;
    }

    json Dsf() {
        last_dsf = al::ComputeDsf(db, profile_id);
        const al::DsfResult& r = last_dsf;
        json shots = json::array();
        for (const al::DsfShot& s : r.shots) {
            shots.push_back({{"shotId", s.shot_id},
                             {"rangeM", s.range_m},
                             {"mach", s.mach},
                             {"observed", al::FromRad(s.observed_rad, Unit())},
                             {"before", al::FromRad(s.predicted_before_rad, Unit())},
                             {"after", al::FromRad(s.predicted_after_rad, Unit())},
                             {"used", s.used},
                             {"limited", s.limited}});
        }
        return {{"ok", r.ok},
                {"error", r.error},
                {"points", DsfJson(r.points)},
                {"shots", shots},
                {"rmsBefore", al::FromRad(r.rms_before_rad, Unit())},
                {"rmsAfter", al::FromRad(r.rms_after_rad, Unit())}};
    }

    json Truing() {
        last_truing = al::ComputeTruing(db, profile_id);
        const al::TruingResult& r = last_truing;
        json points = json::array();
        for (const auto& p : r.points) {
            points.push_back({{"rangeM", p.range_m},
                              {"observed", al::FromRad(p.observed_rad, Unit())},
                              {"before", al::FromRad(p.predicted_before_rad, Unit())},
                              {"after", al::FromRad(p.predicted_after_rad, Unit())}});
        }
        return {{"ok", r.ok},
                {"error", r.error},
                {"velocityScale", r.velocity_scale},
                {"dragScale", r.drag_scale},
                {"dragFitted", r.drag_fitted},
                {"rmsBefore", al::FromRad(r.rms_before_rad, Unit())},
                {"rmsAfter", al::FromRad(r.rms_after_rad, Unit())},
                {"velocityBefore", r.muzzle_velocity_before_mps},
                {"velocityAfter", r.muzzle_velocity_after_mps},
                {"points", points}};
    }

    void RequirePair() const {
        if (profile_id == 0) {
            throw Failure("Choose a rifle and a cartridge.");
        }
    }

    // ---- Sharing -------------------------------------------------------------

    std::string ExportFileName(const std::string& kind, Id id) const {
        const json& list = kind == "rifle" ? rifles : cartridges;
        for (const json& v : list) {
            if (v.at("id").get<Id>() == id) {
                std::string name = v.at("name").get<std::string>();
                for (char& c : name) {
                    if (std::string("\\/:*?\"<>|").find(c) != std::string::npos) {
                        c = '_';
                    }
                }
                return name + ".balcalc.json";
            }
        }
        return kind + ".balcalc.json";
    }
};

const std::map<std::string, Api::Impl::Handler>& Api::Impl::Handlers() {
    using I = Api::Impl;
    static const std::map<std::string, Handler> handlers = {
        {"open",
         [](I& s, const json& a) -> json {
             s.Open(Str(a, "path"));
             return {{"databasePath", s.db_path}};
         }},
        {"seed",
         [](I& s, const json& a) -> json {
             s.RequireOpen();
             std::vector<al::SeedFile> files;
             if (a.contains("files")) {
                 for (const json& f : a.at("files")) {
                     files.push_back({Str(f, "name"), Str(f, "content")});
                 }
             }
             const al::SeedReport r =
                 Must(al::SeedLibrary(s.db, files, static_cast<int>(Num(a, "version", 1))));
             s.Refresh();
             return {{"imported", r.imported}, {"skipped", r.skipped}, {"problems", r.problems}};
         }},
        {"info",
         [](I& s, const json&) -> json {
             return {{"engineVersion", ballistics::version()},
                     {"sqliteVersion", bs::SqliteVersion()},
                     {"databasePath", s.db_path}};
         }},
        {"state", [](I& s, const json&) -> json { return s.State(); }},
        {"select",
         [](I& s, const json& a) -> json {
             s.Select(a.contains("rifleId") ? IdOf(a, "rifleId") : s.rifle_id,
                      a.contains("cartridgeId") ? IdOf(a, "cartridgeId") : s.cartridge_id);
             return s.State();
         }},
        {"setConditions",
         [](I& s, const json& a) -> json {
             s.SetConditions(a);
             return s.Conditions();
         }},
        {"setSettings",
         [](I& s, const json& a) -> json {
             if (a.contains("angleUnit")) {
                 const std::string unit = Str(a, "angleUnit");
                 if (unit == "mrad" || unit == "moa") {
                     s.angle_unit = unit;
                     s.Put(kAngleUnitKey, unit);
                     s.last_truing = {};
                 }
             }
             if (a.contains("holdMode")) {
                 s.hold_mode = al::ToString(al::HoldModeFromString(Str(a, "holdMode")));
                 s.Put(kHoldModeKey, s.hold_mode);
             }
             if (a.contains("language")) {
                 s.language = Str(a, "language");
                 s.Put(kLanguageKey, s.language);
             }
             for (const auto& [name, key, field] :
                  {std::tuple{"tableFromM", kTableFromKey, &s.table_from_m},
                   std::tuple{"tableToM", kTableToKey, &s.table_to_m},
                   std::tuple{"tableStepM", kTableStepKey, &s.table_step_m}}) {
                 if (a.contains(name)) {
                     *field = Num(a, name, *field);
                     s.Put(key, std::to_string(*field));
                 }
             }
             return s.State();
         }},
        {"solution", [](I& s, const json&) -> json { return s.Solution(); }},
        {"rangeTable",
         [](I& s, const json&) -> json {
             return s.Table(s.table_from_m, s.table_to_m, s.table_step_m);
         }},
        {"compareCurves", [](I& s, const json& a) -> json { return s.CompareCurves(a); }},
        {"pairOptions", [](I& s, const json&) -> json { return s.PairOptions(); }},
        {"trajectoryCurve",
         [](I& s, const json& a) -> json {
             const double max_range = Num(a, "maxRangeM", 1000.0);
             const int points = std::clamp(static_cast<int>(Num(a, "points", 250)), 10, 1000);
             return s.Table(0.0, max_range, max_range / points);
         }},
        // Rifles
        {"rifleForm",
         [](I& s, const json& a) -> json {
             const Id id = IdOf(a);
             return ToJson(id == 0 ? al::RifleForm{} : Must(al::LoadRifleForm(s.db, id)));
         }},
        {"saveRifle",
         [](I& s, const json& a) -> json {
             const Id id = Must(al::SaveRifleForm(s.db, RifleFrom(a.value("form", json::object()))));
             s.rifle_id = 0; // the cartridge order follows the (maybe new) calibre
             s.ReloadArmory();
             s.Select(id, s.cartridge_id);
             return {{"id", id}};
         }},
        {"deleteRifle",
         [](I& s, const json& a) -> json {
             Must(al::DeleteRifle(s.db, IdOf(a)));
             s.Refresh();
             return s.State();
         }},
        // Cartridges
        {"cartridgeForm",
         [](I& s, const json& a) -> json {
             const Id id = IdOf(a);
             return ToJson(id == 0 ? al::CartridgeForm{} : Must(al::LoadCartridgeForm(s.db, id)));
         }},
        {"saveCartridge",
         [](I& s, const json& a) -> json {
             const Id id =
                 Must(al::SaveCartridgeForm(s.db, CartridgeFrom(a.value("form", json::object()))));
             s.ReloadArmory();
             s.Select(s.rifle_id, id);
             return {{"id", id}};
         }},
        {"deleteCartridge",
         [](I& s, const json& a) -> json {
             Must(al::DeleteCartridge(s.db, IdOf(a)));
             s.Refresh();
             return s.State();
         }},
        {"cartridgeFormWithBullet",
         [](I& s, const json& a) -> json {
             return ToJson(Must(al::WithLibraryBullet(
                 s.db, CartridgeFrom(a.value("form", json::object())), IdOf(a, "bulletId"))));
         }},
        {"libraryCartridges",
         [](I& s, const json& a) -> json {
             json out = json::array();
             for (const auto& c : Must(al::ListLibraryCartridges(s.db, Str(a, "filter")))) {
                 out.push_back(ToJson(c, false));
             }
             return out;
         }},
        {"cartridgeFormFromLibrary",
         [](I& s, const json& a) -> json {
             return ToJson(Must(al::CartridgeFromLibrary(s.db, IdOf(a))));
         }},
        {"setZeroOffset",
         [](I& s, const json& a) -> json {
             s.RequirePair();
             Must(al::SetZeroOffset(s.db, s.profile_id, Num(a, "upCm"), Num(a, "rightCm")));
             return s.CurrentPair();
         }},
        {"addSample",
         [](I& s, const json& a) -> json {
             const Id pair = Must(al::CreateSampleProfile(
                 s.db, Str(a, "rifleName", "Sample .308 Win"),
                 Str(a, "cartridgeName", "Sample SMK 175 gr")));
             const auto p = Must(bs::Repository<bs::ProfileRecord>(s.db).Get(pair));
             if (p) {
                 s.rifle_id = 0;
                 s.ReloadArmory();
                 s.Select(p->rifle_id, p->cartridge_id);
             }
             return s.State();
         }},
        // Shot log and truing
        {"shots", [](I& s, const json&) -> json { return s.Shots(); }},
        {"logShot",
         [](I& s, const json& a) -> json {
             s.RequirePair();
             std::optional<double> wind;
             if (Bool(a, "hasWindage")) {
                 wind = Num(a, "windage") * s.UnitRad();
             }
             const Id id = Must(al::LogShot(s.db, s.profile_id, s.Session(), Num(a, "rangeM"),
                                            Num(a, "elevation") * s.UnitRad(), wind,
                                            Str(a, "notes")));
             return {{"id", id}};
         }},
        {"deleteShot",
         [](I& s, const json& a) -> json {
             Must(al::DeleteShot(s.db, IdOf(a)));
             return json::object();
         }},
        {"setShotUsed",
         [](I& s, const json& a) -> json {
             Must(al::SetShotUsedForTruing(s.db, IdOf(a), Bool(a, "used", true)));
             return json::object();
         }},
        {"computeTruing", [](I& s, const json&) -> json { return s.Truing(); }},
        {"applyTruing",
         [](I& s, const json&) -> json {
             Must(al::ApplyTruing(s.db, s.profile_id, s.last_truing));
             s.last_truing = {};
             return json::object();
         }},
        {"resetTruing",
         [](I& s, const json&) -> json {
             Must(al::ResetTruing(s.db, s.profile_id));
             return json::object();
         }},
        {"wez",
         [](I& s, const json& a) -> json {
             al::WezSettings w = Must(al::LoadWezSettings(s.db));
             if (a.contains("settings")) {
                 Update(w, a.at("settings"));
                 Must(al::SaveWezSettings(s.db, w));
             }
             json out = {{"settings", ToJson(w)}};
             if (s.profile_id == 0) {
                 out["ok"] = false;
                 out["error"] = "Choose a rifle and a cartridge.";
                 return out;
             }
             const al::WezResult r = al::ComputeWez(Must(bs::LoadProfile(s.db, s.profile_id)),
                                                    s.Session(), w, Num(a, "toM", 1000.0),
                                                    Num(a, "stepM", 50.0));
             json rows = json::array();
             for (const al::WezRow& row : r.rows) {
                 rows.push_back(ToJson(row));
             }
             json parts = json::array();
             for (const Spread::Part& p : r.parts) {
                 parts.push_back({{"source", p.source}, {"upCm", p.up_m * 100.0}, {"rightCm", p.right_m * 100.0}});
             }
             out.update({{"ok", r.ok},
                         {"error", r.error},
                         {"rows", rows},
                         {"atTarget", ToJson(r.at_target)},
                         {"parts", parts},
                         {"shots50", r.shots_50},
                         {"shots80", r.shots_80},
                         {"shots95", r.shots_95}});
             return out;
         }},
        {"bcCalculator",
         [](I& s, const json& a) -> json {
             const std::string table = Str(a, "table", "G7");
             al::BcResult r;
             if (Str(a, "mode") == "hit") {
                 if (s.profile_id == 0) {
                     throw Failure("Choose a rifle and a cartridge.");
                 }
                 r = al::BcFromHit(Must(bs::LoadProfile(s.db, s.profile_id)), table, Num(a, "rangeM"),
                                   Num(a, "elevation") * s.UnitRad(), s.Session());
             } else {
                 r = al::BcFromChronograph(table, Num(a, "vNearMps"), Num(a, "vFarMps"),
                                           Num(a, "distanceM"), s.Session());
             }
             return {{"ok", r.ok}, {"error", r.error}, {"bc", r.bc}, {"table", table}};
         }},
        {"computeDsf", [](I& s, const json&) -> json { return s.Dsf(); }},
        {"applyDsf",
         [](I& s, const json&) -> json {
             if (!s.last_dsf.ok) {
                 throw Failure("Nothing to apply.");
             }
             Must(al::SetDsf(s.db, s.profile_id, s.last_dsf.points));
             s.last_dsf = {};
             return json::object();
         }},
        {"setDsf",
         [](I& s, const json& a) -> json {
             std::vector<DsfPoint> points;
             for (const json& p : a.value("points", json::array())) {
                 points.push_back({Num(p, "mach"), Num(p, "factor", 1.0)});
             }
             Must(al::SetDsf(s.db, s.profile_id, std::move(points)));
             return json::object();
         }},
        {"resetDsf",
         [](I& s, const json&) -> json {
             Must(al::SetDsf(s.db, s.profile_id, {}));
             return json::object();
         }},
        // Library
        {"reticles",
         [](I& s, const json&) -> json {
             json out = json::array();
             for (const auto& r : Must(bs::Repository<bs::ReticleRecord>(s.db).List())) {
                 out.push_back({{"id", r.id}, {"name", r.name}, {"units", r.units}});
             }
             return out;
         }},
        {"libraryBullets",
         [](I& s, const json& a) -> json {
             json out = json::array();
             for (const al::BulletSummary& b : Must(al::ListLibraryBullets(s.db, Str(a, "filter")))) {
                 out.push_back({{"id", b.id},
                                {"name", b.name},
                                {"manufacturer", b.manufacturer},
                                {"caliber", b.caliber},
                                {"massGr", b.mass_gr},
                                {"diameterIn", b.diameter_in},
                                {"dragKind", b.drag_kind},
                                {"dragTable", b.drag_table},
                                {"bc", b.bc},
                                {"bcBands", b.bc_bands},
                                {"source", b.source}});
             }
             return out;
         }},
        {"bulletForm",
         [](I& s, const json& a) -> json {
             const Id id = IdOf(a);
             return ToJson(id == 0 ? al::BulletForm{} : Must(al::LoadBulletForm(s.db, id)));
         }},
        {"saveBullet",
         [](I& s, const json& a) -> json {
             const Id id = Must(al::SaveBulletForm(s.db, BulletFrom(a.value("form", json::object()))));
             s.ReloadArmory(); // cartridges show their bullet
             return {{"id", id}};
         }},
        {"deleteBullet",
         [](I& s, const json& a) -> json {
             Must(al::DeleteBullet(s.db, IdOf(a)));
             return json::object();
         }},
        // Sharing
        {"exportJson",
         [](I& s, const json& a) -> json {
             const std::string kind = Str(a, "kind");
             const Id id = IdOf(a);
             if (kind != "rifle" && kind != "cartridge") {
                 throw Failure("Unknown export kind: " + kind);
             }
             const std::string text = Must(kind == "rifle" ? al::ExportRifleJson(s.db, id)
                                                           : al::ExportCartridgeJson(s.db, id));
             return {{"json", text}, {"fileName", s.ExportFileName(kind, id)}};
         }},
        {"importShared",
         [](I& s, const json& a) -> json {
             const al::Imported im = Must(al::ImportShareJson(s.db, Str(a, "text")));
             // What the file brought in becomes current.
             const Id rifle = im.rifle_id != 0 ? im.rifle_id : s.rifle_id;
             const Id cartridge = im.cartridge_id != 0 ? im.cartridge_id : s.cartridge_id;
             s.rifle_id = 0;
             s.ReloadArmory();
             s.Select(rifle, cartridge);
             return s.State();
         }},
        {"importFiles",
         [](I& s, const json& a) -> json {
             int imported = 0;
             json problems = json::array();
             if (a.contains("files")) {
                 for (const json& f : a.at("files")) {
                     const std::string name = Str(f, "name");
                     auto id = al::ImportFile(s.db, name, Str(f, "content"));
                     if (id) {
                         ++imported;
                     } else {
                         problems.push_back({{"file", name}, {"message", id.error().message}});
                     }
                 }
             }
             s.Refresh();
             return {{"imported", imported}, {"problems", problems}};
         }},
        {"stationPressure",
         [](I&, const json& a) -> json {
             return StationPressureFromSeaLevel(Num(a, "qnhHpa") * 100.0, Num(a, "altitudeM")) /
                    100.0;
         }},
    };
    return handlers;
}

Api::Api() : impl_(std::make_unique<Impl>()) {}
Api::~Api() = default;

std::string Api::Call(const std::string& method, const std::string& args_json) {
    try {
        const auto& handlers = Impl::Handlers();
        const auto handler = handlers.find(method);
        if (handler == handlers.end()) {
            throw Failure("Unknown method: " + method);
        }
        json args = args_json.empty() ? json::object() : json::parse(args_json);
        if (!args.is_object()) {
            throw Failure("Arguments must be a JSON object.");
        }
        if (method != "open" && method != "info") {
            impl_->RequireOpen();
        }
        json result = handler->second(*impl_, args);
        return Dump({{"ok", true}, {"result", std::move(result)}});
    } catch (const Failure& e) {
        return Dump({{"ok", false}, {"error", e.what()}});
    } catch (const json::exception& e) {
        return Dump({{"ok", false}, {"error", std::string("Bad arguments: ") + e.what()}});
    } catch (const std::exception& e) {
        return Dump({{"ok", false}, {"error", e.what()}});
    }
}

} // namespace ballistics::bridge
