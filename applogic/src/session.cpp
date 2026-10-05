#include <ballistics/applogic/session.h>

#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>

#include <sqlite_manager/transaction.h>

#include <ballistics/storage/repository.h>
#include <ballistics/units.h>

namespace ballistics::applogic {

namespace {

// The session is kept as "session.<key>" settings, numbers as text.
constexpr const char* kPrefix = "session.";

std::string Num(double v) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

bool ParseNum(const std::string& s, double& v) {
    try {
        std::size_t used = 0;
        v = std::stod(s, &used);
        return used == s.size();
    } catch (...) {
        return false;
    }
}

std::string WindsToText(const std::vector<WindInput>& winds) {
    std::string out;
    for (const WindInput& w : winds) {
        out += (out.empty() ? "" : ";") + Num(w.speed_mps) + "," + Num(w.from_deg) + "," +
               Num(w.until_m);
    }
    return out;
}

std::vector<WindInput> WindsFromText(const std::string& text) {
    std::vector<WindInput> winds;
    std::stringstream zones(text);
    for (std::string zone; std::getline(zones, zone, ';');) {
        std::stringstream fields(zone);
        std::string a, b, c;
        WindInput w;
        if (std::getline(fields, a, ',') && std::getline(fields, b, ',') &&
            std::getline(fields, c, ',') && ParseNum(a, w.speed_mps) && ParseNum(b, w.from_deg) &&
            ParseNum(c, w.until_m)) {
            winds.push_back(w);
        }
    }
    return winds;
}

} // namespace

storage::ConditionsRecord ToConditions(const SessionConditions& s) {
    storage::ConditionsRecord c;
    c.atmosphere = {s.altitude_m, s.pressure_hpa * 100.0, units::CToK(s.temperature_c),
                    s.humidity_pct / 100.0};
    if (s.powder_c) {
        c.powder_temp_k = units::CToK(*s.powder_c);
    }
    for (const WindInput& w : s.winds) {
        c.winds.push_back({w.until_m > 0.0 ? w.until_m : 1e6, w.speed_mps,
                           units::DegToRad(w.from_deg), 0.0});
    }
    c.look_angle_rad = units::DegToRad(s.look_angle_deg);
    c.cant_rad = units::DegToRad(s.cant_deg);
    if (s.latitude_deg) {
        c.latitude_rad = units::DegToRad(*s.latitude_deg);
        if (s.azimuth_deg) {
            c.azimuth_rad = units::DegToRad(*s.azimuth_deg);
        }
    }
    return c;
}

Result<SessionConditions> LoadSession(storage::Database& db) {
    SessionConditions s;
    auto get = [&](const char* key) -> Result<std::optional<std::string>> {
        return storage::GetSetting(db, std::string(kPrefix) + key);
    };
    struct Field {
        const char* key;
        double* value;
    };
    const Field fields[] = {{"temperature_c", &s.temperature_c},
                            {"pressure_hpa", &s.pressure_hpa},
                            {"altitude_m", &s.altitude_m},
                            {"humidity_pct", &s.humidity_pct},
                            {"look_angle_deg", &s.look_angle_deg},
                            {"cant_deg", &s.cant_deg},
                            {"target_range_m", &s.target_range_m},
                            {"magnification", &s.magnification}};
    for (const Field& f : fields) {
        auto v = get(f.key);
        if (!v) {
            return v.error();
        }
        double d = 0.0;
        if (v.value() && ParseNum(*v.value(), d)) {
            *f.value = d;
        }
    }
    struct OptField {
        const char* key;
        std::optional<double>* value;
    };
    const OptField opts[] = {{"powder_c", &s.powder_c},
                             {"latitude_deg", &s.latitude_deg},
                             {"azimuth_deg", &s.azimuth_deg}};
    for (const OptField& f : opts) {
        auto v = get(f.key);
        if (!v) {
            return v.error();
        }
        double d = 0.0;
        if (v.value() && ParseNum(*v.value(), d)) {
            *f.value = d;
        }
    }
    auto winds = get("winds");
    if (!winds) {
        return winds.error();
    }
    if (winds.value()) {
        s.winds = WindsFromText(*winds.value());
    }
    return s;
}

Status SaveSession(storage::Database& db, const SessionConditions& s) {
    auto txn = sqlite_manager::Transaction::Begin(db.connection());
    if (!txn) {
        return txn.error();
    }
    auto set = [&](const char* key, const std::string& value) {
        return storage::SetSetting(db, std::string(kPrefix) + key, value);
    };
    auto opt = [](const std::optional<double>& v) { return v ? Num(*v) : std::string(); };
    for (const auto& [key, value] :
         {std::pair<const char*, std::string>{"temperature_c", Num(s.temperature_c)},
          {"pressure_hpa", Num(s.pressure_hpa)},
          {"altitude_m", Num(s.altitude_m)},
          {"humidity_pct", Num(s.humidity_pct)},
          {"look_angle_deg", Num(s.look_angle_deg)},
          {"cant_deg", Num(s.cant_deg)},
          {"target_range_m", Num(s.target_range_m)},
          {"magnification", Num(s.magnification)},
          {"powder_c", opt(s.powder_c)},
          {"latitude_deg", opt(s.latitude_deg)},
          {"azimuth_deg", opt(s.azimuth_deg)},
          {"winds", WindsToText(s.winds)}}) {
        if (Status st = set(key, value); !st) {
            return st;
        }
    }
    return txn.value().Commit();
}

double FromRad(double rad, AngleUnit unit) {
    return unit == AngleUnit::kMoa ? units::RadToMoa(rad) : units::RadToMrad(rad);
}

SolutionSummary Summarize(const storage::LoadedProfile& profile, const SessionConditions& s,
                          AngleUnit unit) {
    SolutionSummary out;
    out.range_m = s.target_range_m;
    if (!(s.target_range_m > 0.0)) {
        out.error = "Enter a target range.";
        return out;
    }
    auto sol = storage::Solve(profile, ToConditions(s), s.target_range_m + 1.0);
    if (!sol) {
        out.error = sol.error().message;
        return out;
    }
    const Trajectory& traj = sol.value().trajectory;
    const auto pt = traj.AtSlantRange(s.target_range_m);
    if (!pt) {
        out.error = "The bullet does not reach this range.";
        return out;
    }
    out.ok = true;
    out.elevation = FromRad(pt->hold_elevation_rad, unit);
    out.windage = FromRad(pt->hold_windage_rad, unit);
    if (profile.scope) {
        out.elevation_clicks =
            storage::ToClicks(pt->hold_elevation_rad, profile.scope->click_vertical_rad);
        out.windage_clicks =
            storage::ToClicks(pt->hold_windage_rad, profile.scope->click_horizontal_rad);
    }
    out.drop_cm = pt->drop_m * 100.0;
    out.windage_cm = pt->windage_m * 100.0;
    out.velocity_mps = pt->speed_mps;
    out.energy_j = pt->energy_j;
    out.time_s = pt->time_s;
    out.mach = pt->mach;
    out.muzzle_velocity_mps = sol.value().shot.muzzle_velocity_mps;
    out.stability = traj.stability();
    out.spin_drift_cm = pt->spin_drift_m * 100.0;
    out.subsonic = pt->mach < 1.0;
    for (double r = 10.0; r <= s.target_range_m; r += 10.0) {
        const auto q = traj.AtSlantRange(r);
        if (q && q->mach < 1.2) {
            out.transonic_range_m = r;
            break;
        }
    }
    return out;
}

RangeTable BuildRangeTable(const storage::LoadedProfile& profile, const SessionConditions& s,
                           AngleUnit unit, double from_m, double to_m, double step_m) {
    RangeTable table;
    if (!(step_m > 0.0) || from_m < 0.0 || to_m < from_m || (to_m - from_m) / step_m > 2000.0) {
        table.error = "Check the table range and step.";
        return table;
    }
    auto sol = storage::Solve(profile, ToConditions(s), to_m + 1.0);
    if (!sol) {
        table.error = sol.error().message;
        return table;
    }
    const Trajectory& traj = sol.value().trajectory;
    const auto count = static_cast<long>(std::floor((to_m - from_m) / step_m + 1e-9));
    for (long k = 0; k <= count; ++k) {
        const double r = from_m + static_cast<double>(k) * step_m;
        const auto pt = traj.AtSlantRange(r);
        if (!pt) {
            break;
        }
        RangeRow row;
        row.range_m = r;
        if (r > 0.0) {
            row.elevation = FromRad(pt->hold_elevation_rad, unit);
            row.windage = FromRad(pt->hold_windage_rad, unit);
            if (profile.scope) {
                row.elevation_clicks =
                    storage::ToClicks(pt->hold_elevation_rad, profile.scope->click_vertical_rad);
                row.windage_clicks =
                    storage::ToClicks(pt->hold_windage_rad, profile.scope->click_horizontal_rad);
            }
        }
        row.drop_cm = pt->drop_m * 100.0;
        row.windage_cm = pt->windage_m * 100.0;
        row.velocity_mps = pt->speed_mps;
        row.mach = pt->mach;
        row.energy_j = pt->energy_j;
        row.time_s = pt->time_s;
        table.rows.push_back(row);
    }
    table.ok = true;
    return table;
}

} // namespace ballistics::applogic
