#include <ballistics/bridge/api.h>

#include "internal.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
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
#include <ballistics/effects.h>
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
//   setSettings {angleUnit?, holdMode?, language?, tableFromM?, tableToM?, tableStepM?, prefs? (merged into the interface preferences)}
//                                     → state
//   solution                          → {ok, error, rangeM, elevation, windage, ...}
//   rangeTable {windSpeeds?}          → {ok, error, hasScope, computeMs, rows, windSpeeds}; rows carry windages for each speed
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
//   stability {twistIn, massGr, diameterIn, lengthIn, velocityMps} → {sg} (standard air)
//   photos {kind} → {id: base64} / photo {kind, id} → base64 / setPhoto {kind, id, image} (empty removes)
//   targets → [{name, rangeM, lookAngleDeg, windSpeed, windFromDeg, ok, elevation, windage, *Clicks, holdX, holdY}]
//   saveTargets {targets} → targets / selectTarget {index} → state
//   situations / saveSituation {name} / applySituation {name} → state / deleteSituation {name}
//   compareCurves {maxRangeM, points, pairs:[{rifleId, cartridgeId}]} → [table + label]
//   pairOptions                     → [{rifleId, rifleName, cartridges:[{id, name}]}]
//
// save* return {id}; delete*, set* and log return state or {} as noted in
// the handlers below.

namespace ballistics::bridge {

using namespace detail;

struct Api::Impl {
    bs::Database db;
    bool open = false;
    std::string db_path;

    // Read-only catalogs from the seed files (published_scopes.json,
    // published_rifles.json): "Choose from the library" in the rifle editor.
    json scope_catalog = json::array();
    json rifle_catalog = json::array();

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
    // Interface preferences of the app (theme, screen, display format): the
    // core keeps them for it, as one JSON object.
    json ui_prefs = json::object();
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
        if (auto text = Setting(kUiPrefsKey)) {
            json prefs = json::parse(*text, nullptr, false);
            if (prefs.is_object()) {
                ui_prefs = prefs;
            }
        }
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

    // A seed file that is one of the read-only catalogs: kept in memory.
    bool TakeCatalog(const std::string& content) {
        const std::string head = content.substr(0, 200);
        const bool is_scopes = head.find("\"balcalc-scopes\"") != std::string::npos;
        const bool is_rifles = head.find("\"balcalc-rifles\"") != std::string::npos;
        if (!is_scopes && !is_rifles) {
            return false;
        }
        const json doc = json::parse(content, nullptr, false);
        const char* key = is_scopes ? "scopes" : "rifles";
        if (!doc.is_discarded() && doc.contains(key) && doc.at(key).is_array()) {
            (is_scopes ? scope_catalog : rifle_catalog) = doc.at(key);
        }
        return true;
    }

    // ---- Targets: up to 20 named ranges with their angle and wind ----------

    json TargetsStored() {
        const auto text = Setting(kTargetsKey);
        json list = text ? json::parse(*text, nullptr, false) : json::array();
        return list.is_array() ? list : json::array();
    }

    // The session with a target's range, angle and wind (one zone).
    al::SessionConditions SessionFor(const json& t) const {
        al::SessionConditions s = Session();
        s.target_range_m = t.value("rangeM", s.target_range_m);
        s.look_angle_deg = t.value("lookAngleDeg", 0.0);
        s.winds = {al::WindInput{t.value("windSpeed", 0.0), t.value("windFromDeg", 90.0), 0.0}};
        s.wind_gust_mps = 0.0;
        s.target_speed_mps = 0.0;
        return s;
    }

    // Every target with its corrections and where to hold it on the reticle
    // with the turrets as set for the current target (the hold mode).
    json Targets() {
        json out = json::array();
        const json list = TargetsStored();
        std::optional<bs::LoadedProfile> p;
        if (profile_id != 0) {
            if (auto loaded = bs::LoadProfile(db, profile_id)) {
                p = std::move(loaded).value();
            }
        }
        double dial_e = 0.0;
        double dial_w = 0.0;
        double zoom = 0.0;
        if (p && p->scope) {
            zoom = magnification > 0.0 ? magnification : p->scope->max_magnification;
            const al::SolutionSummary now = al::Summarize(*p, Session(), Unit());
            if (now.ok) {
                const al::ReticleHold h = al::ComputeReticleHold(now.elevation * UnitRad(), now.windage * UnitRad(),
                                                                 *p->scope, zoom, al::HoldModeFromString(hold_mode));
                dial_e = h.dial_elevation_rad;
                dial_w = h.dial_windage_rad;
            }
        }
        for (const json& t : list) {
            json item = {{"name", t.value("name", "")},
                         {"rangeM", t.value("rangeM", 0.0)},
                         {"lookAngleDeg", t.value("lookAngleDeg", 0.0)},
                         {"windSpeed", t.value("windSpeed", 0.0)},
                         {"windFromDeg", t.value("windFromDeg", 90.0)},
                         {"ok", false}};
            if (p) {
                const al::SolutionSummary r = al::Summarize(*p, SessionFor(t), Unit());
                item["ok"] = r.ok;
                if (r.ok) {
                    item["elevation"] = r.elevation;
                    item["windage"] = r.windage;
                    item["elevationClicks"] = r.elevation_clicks;
                    item["windageClicks"] = r.windage_clicks;
                    // Held, not dialled: the rest after the turrets, in reticle mrad.
                    const double e = r.elevation * UnitRad() - dial_e;
                    const double w = r.windage * UnitRad() - dial_w;
                    double x = -u::RadToMrad(w);
                    double y = -u::RadToMrad(e);
                    if (p->scope) {
                        const al::ReticleHold h = al::ComputeReticleHold(e, w, *p->scope, zoom, al::HoldMode::kHoldAll);
                        x = h.target_x;
                        y = h.target_y;
                    }
                    item["holdX"] = x;
                    item["holdY"] = y;
                }
            }
            out.push_back(item);
        }
        return out;
    }

    json SaveTargets(const json& list) {
        if (!list.is_array()) {
            throw Failure("Targets must be a list.");
        }
        if (list.size() > kMaxTargets) {
            throw Failure("At most 20 targets.");
        }
        json clean = json::array();
        for (const json& t : list) {
            const std::string name = Trim(Str(t, "name"));
            const double range = Num(t, "rangeM");
            if (name.empty()) {
                throw Failure("Enter a name for each target.");
            }
            if (range < 10.0 || range > 3000.0) {
                throw Failure("A target range must be between 10 and 3000 m.");
            }
            clean.push_back({{"name", name},
                             {"rangeM", range},
                             {"lookAngleDeg", std::clamp(Num(t, "lookAngleDeg"), -60.0, 60.0)},
                             {"windSpeed", std::clamp(Num(t, "windSpeed"), 0.0, 40.0)},
                             {"windFromDeg", Num(t, "windFromDeg", 90.0)}});
        }
        Put(kTargetsKey, clean.dump());
        return Targets();
    }

    // Makes a target current: its range, angle and wind go into the conditions.
    json SelectTarget(std::size_t index) {
        const json list = TargetsStored();
        if (index >= list.size()) {
            throw Failure("No such target.");
        }
        const json& t = list.at(index);
        json c = Conditions();
        c["targetRangeM"] = t.value("rangeM", 300.0);
        c["lookAngleDeg"] = t.value("lookAngleDeg", 0.0);
        c["windSpeed"] = t.value("windSpeed", 0.0);
        c["windFromDeg"] = t.value("windFromDeg", 90.0);
        c["windZones"] = json::array();
        SetConditions(c);
        return State();
    }

    // ---- Situations: a rifle, a cartridge and the conditions, by name -------

    json SituationsStored() {
        const auto text = Setting(kSituationsKey);
        if (!text) {
            return json::array();
        }
        json list = json::parse(*text, nullptr, false);
        return list.is_array() ? list : json::array();
    }

    bool Exists(Id rifle, Id cartridge) {
        const auto r = bs::Repository<bs::RifleRecord>(db).Get(rifle);
        const auto c = bs::Repository<bs::CartridgeRecord>(db).Get(cartridge);
        return r && r.value() && c && c.value();
    }

    json Situations() {
        json out = json::array();
        for (const json& s : SituationsStored()) {
            const Id rifle = s.value("rifleId", Id{0});
            const Id cartridge = s.value("cartridgeId", Id{0});
            const bool ok = Exists(rifle, cartridge);
            const auto r = bs::Repository<bs::RifleRecord>(db).Get(rifle);
            const auto c = bs::Repository<bs::CartridgeRecord>(db).Get(cartridge);
            out.push_back({{"name", s.value("name", "")},
                           {"rifleName", ok ? r.value()->name : ""},
                           {"cartridgeName", ok ? c.value()->name : ""},
                           {"rangeM", s.value("conditions", json::object()).value("targetRangeM", 0.0)},
                           {"available", ok}});
        }
        return out;
    }

    json SaveSituation(const std::string& name) {
        if (name.empty()) {
            throw Failure("Enter a name for the situation.");
        }
        if (profile_id == 0) {
            throw Failure("Choose a rifle and a cartridge.");
        }
        json list = json::array();
        for (const json& s : SituationsStored()) {
            if (s.value("name", "") != name) {
                list.push_back(s);
            }
        }
        list.push_back({{"name", name},
                        {"rifleId", rifle_id},
                        {"cartridgeId", cartridge_id},
                        {"holdMode", hold_mode},
                        {"conditions", Conditions()}});
        Put(kSituationsKey, list.dump());
        return Situations();
    }

    json ApplySituation(const std::string& name) {
        for (const json& s : SituationsStored()) {
            if (s.value("name", "") != name) {
                continue;
            }
            const Id rifle = s.value("rifleId", Id{0});
            const Id cartridge = s.value("cartridgeId", Id{0});
            if (!Exists(rifle, cartridge)) {
                throw Failure("The rifle or cartridge of this situation was deleted.");
            }
            Select(rifle, cartridge);
            SetConditions(s.value("conditions", json::object()));
            hold_mode = al::ToString(al::HoldModeFromString(s.value("holdMode", hold_mode)));
            Put(kHoldModeKey, hold_mode);
            return State();
        }
        throw Failure("No situation of this name.");
    }

    json DeleteSituation(const std::string& name) {
        json list = json::array();
        for (const json& s : SituationsStored()) {
            if (s.value("name", "") != name) {
                list.push_back(s);
            }
        }
        Put(kSituationsKey, list.dump());
        return Situations();
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
                {"prefs", ui_prefs},
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
                   // One click in the angle unit (0 without a scope): for rounding to clicks.
                   {"clickElevation", p.value().scope ? al::FromRad(p.value().scope->click_vertical_rad, Unit()) : 0.0},
                   {"clickWindage", p.value().scope ? al::FromRad(p.value().scope->click_horizontal_rad, Unit()) : 0.0},
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
        const auto all_cartridges = Must(al::ListCartridges(db));
        json out = json::array();
        for (const al::RifleSummary& r : Must(al::ListRifles(db))) {
            json list = json::array();
            for (const al::CartridgeSummary& c : all_cartridges) {
                if (al::SameCaliber(c.caliber, r.caliber)) {
                    list.push_back({{"id", c.id}, {"name", c.name}});
                }
            }
            out.push_back({{"rifleId", r.id}, {"rifleName", r.name}, {"cartridges", list}});
        }
        return out;
    }

    json Table(double from_m, double to_m, double step_m, const json& wind_speeds = json::array()) {
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
        // Wind columns: the windage for other wind speeds, from the same
        // direction (one zone), each computed in full.
        json speeds = json::array();
        if (wind_speeds.is_array()) {
            for (const json& v : wind_speeds) {
                if (v.is_number() && speeds.size() < 6) {
                    speeds.push_back(std::clamp(v.get<double>(), 0.0, 40.0));
                }
            }
        }
        out["windSpeeds"] = speeds;
        if (!speeds.empty() && out.value("ok", false)) {
            const al::SessionConditions base = Session();
            const double from = base.winds.empty() ? 90.0 : base.winds.front().from_deg;
            json& rows = out["rows"];
            for (json& row : rows) {
                row["windages"] = json::array();
                row["windageClicksAt"] = json::array();
            }
            for (const json& v : speeds) {
                al::SessionConditions s = base;
                s.winds = {al::WindInput{v.get<double>(), from, 0.0}};
                const al::RangeTable t = al::BuildRangeTable(p.value(), s, Unit(), from_m, to_m, step_m);
                for (std::size_t i = 0; i < rows.size() && i < t.rows.size(); ++i) {
                    rows[i]["windages"].push_back(t.rows[i].windage);
                    rows[i]["windageClicksAt"].push_back(t.rows[i].windage_clicks);
                }
            }
        }
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
                     if (s.TakeCatalog(Str(f, "content"))) {
                         continue; // a catalog, not a library record
                     }
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
        {"targets", [](I& s, const json&) -> json { return s.Targets(); }},
        {"saveTargets", [](I& s, const json& a) -> json { return s.SaveTargets(a.value("targets", json::array())); }},
        {"selectTarget",
         [](I& s, const json& a) -> json {
             const double i = Num(a, "index", -1.0);
             return s.SelectTarget(i < 0.0 ? kMaxTargets : static_cast<std::size_t>(i));
         }},
        {"situations", [](I& s, const json&) -> json { return s.Situations(); }},
        {"saveSituation", [](I& s, const json& a) -> json { return s.SaveSituation(Trim(Str(a, "name"))); }},
        {"applySituation", [](I& s, const json& a) -> json { return s.ApplySituation(Str(a, "name")); }},
        {"deleteSituation", [](I& s, const json& a) -> json { return s.DeleteSituation(Str(a, "name")); }},
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
             if (a.contains("prefs") && a.at("prefs").is_object()) {
                 s.ui_prefs.merge_patch(a.at("prefs")); // a null value removes a key
                 s.Put(kUiPrefsKey, s.ui_prefs.dump());
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
         [](I& s, const json& a) -> json {
             return s.Table(s.table_from_m, s.table_to_m, s.table_step_m, a.value("windSpeeds", json::array()));
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
        // Gyroscopic stability of a bullet in a barrel (Miller) at standard
        // air, for the editors: 0 when an input is missing.
        {"stability",
         [](I&, const json& a) -> json {
             const double sg = ballistics::MillerStability(
                 u::GrainToKg(Num(a, "massGr")), u::InchToM(Num(a, "diameterIn")), u::InchToM(Num(a, "lengthIn")),
                 u::InchToM(Num(a, "twistIn")), Num(a, "velocityMps"), 288.15, 101325.0);
             return {{"sg", sg}};
         }},
        // Pictures of rifles and cartridges
        {"photos",
         [](I& s, const json& a) -> json {
             const std::string kind = PhotoKind(a);
             json out = json::object();
             for (const Id id : Must(bs::PhotoOwners(s.db, kind))) {
                 if (auto image = Must(bs::GetPhoto(s.db, kind, id))) {
                     out[std::to_string(id)] = ToBase64(*image);
                 }
             }
             return out;
         }},
        {"photo",
         [](I& s, const json& a) -> json {
             const auto image = Must(bs::GetPhoto(s.db, PhotoKind(a), IdOf(a)));
             return image ? ToBase64(*image) : std::string{};
         }},
        {"setPhoto",
         [](I& s, const json& a) -> json {
             const std::string kind = PhotoKind(a);
             const Id id = IdOf(a);
             const bool exists = kind == "rifle"
                                     ? Must(bs::Repository<bs::RifleRecord>(s.db).Get(id)).has_value()
                                     : Must(bs::Repository<bs::CartridgeRecord>(s.db).Get(id)).has_value();
             if (!exists) {
                 throw Failure("Save the record first.");
             }
             const std::vector<std::uint8_t> image = FromBase64(Str(a, "image"));
             if (image.size() > kMaxPhotoBytes) {
                 throw Failure("The picture is too large.");
             }
             Must(bs::SetPhoto(s.db, kind, id, image));
             return json::object();
         }},
        {"cartridgeFormWithBullet",
         [](I& s, const json& a) -> json {
             return ToJson(Must(al::WithLibraryBullet(
                 s.db, CartridgeFrom(a.value("form", json::object())), IdOf(a, "bulletId"))));
         }},
        {"libraryScopes", [](I& s, const json& a) -> json { return Matching(s.scope_catalog, Str(a, "filter")); }},
        {"libraryRifles", [](I& s, const json& a) -> json { return Matching(s.rifle_catalog, Str(a, "filter")); }},
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
