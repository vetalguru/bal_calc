// Opening the database, the settings and the conditions of the session.
#include "impl.h"

namespace ballistics::bridge {

using namespace detail;

std::optional<std::string> Api::Impl::Setting(const char* key) {
    auto v = bs::GetSetting(db, key);
    return v && v.value() ? v.value() : std::nullopt;
}

void Api::Impl::Put(const char* key, const std::string& value) { bs::SetSetting(db, key, value).ok(); }

void Api::Impl::LoadSettings() {
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
    // The default wind used to come from 3 o'clock; a calm wind still
    // there is that default, not a choice: it moves to 12 once.
    if (!Setting(kWindNoonKey)) {
        if (wind_speed == 0.0 && wind_from_deg == 90.0) {
            wind_from_deg = 0.0;
            al::SaveSession(db, Session()).ok();
        }
        Put(kWindNoonKey, "1");
    }
}

al::SessionConditions Api::Impl::Session() const {
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

void Api::Impl::ApplySession(const al::SessionConditions& s) {
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

json Api::Impl::Conditions() const {
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

json Api::Impl::ZonesJson() const {
    json zones = json::array();
    for (const al::WindInput& w : wind_zones) {
        zones.push_back({{"speedMps", w.speed_mps}, {"fromDeg", w.from_deg}, {"untilM", w.until_m}});
    }
    return zones;
}

void Api::Impl::SetConditions(const json& a) {
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

json Api::Impl::State() {
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

void Api::Impl::Open(const std::string& path) {
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

void Api::Impl::AddSessionHandlers(HandlerMap& h) {
    using I = Api::Impl;
    h.insert({
        {"open",
         [](I& s, const json& a) -> json {
             s.Open(Str(a, "path"));
             return {{"databasePath", s.db_path}};
         }},
        {"info",
         [](I& s, const json&) -> json {
             return {{"engineVersion", ballistics::version()},
                     {"sqliteVersion", bs::SqliteVersion()},
                     {"databasePath", s.db_path}};
         }},
        {"state", [](I& s, const json&) -> json { return s.State(); }},
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
        {"stationPressure",
         [](I&, const json& a) -> json {
             return StationPressureFromSeaLevel(Num(a, "qnhHpa") * 100.0, Num(a, "altitudeM")) /
                    100.0;
         }},
    });
}

} // namespace ballistics::bridge
