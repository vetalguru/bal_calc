#include <ballistics/analysis.h>
#include <ballistics/applogic/session.h>
#include <ballistics/storage/repository.h>
#include <ballistics/units.h>
#include <sqlite_manager/transaction.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>
#include <utility>

namespace ballistics::applogic {

namespace {

// The session is kept as "session.<key>" settings, numbers as text.
constexpr const char* kPrefix = "session.";

std::string Num(double v) {
    std::array<char, 40> buf{};
    std::snprintf(buf.data(), buf.size(), "%.17g", v);
    return buf.data();
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

// Speeds of a moving target across the line of fire (right positive) and
// along it (away positive).
std::pair<double, double> TargetVelocity(const SessionConditions& s) {
    const double h = units::DegToRad(s.target_heading_deg);
    return {s.target_speed_mps * std::sin(h), s.target_speed_mps * std::cos(h)};
}

// How much further than the range a target going away can take the
// bullet: 5 s of its motion.
double LeadReachM(const SessionConditions& s) { return std::max(0.0, s.target_speed_mps) * 5.0; }

}  // namespace

storage::ConditionsRecord ToConditions(const SessionConditions& s) {
    storage::ConditionsRecord c;
    c.atmosphere = {s.altitude_m, s.pressure_hpa * 100.0, units::CToK(s.temperature_c),
                    s.humidity_pct / 100.0};
    if (s.density_altitude_m) {
        c.atmosphere.pressure_pa = StationPressureFromDensityAltitude(
            *s.density_altitude_m, c.atmosphere.temperature_k, c.atmosphere.humidity);
    }
    if (s.powder_c) {
        c.powder_temp_k = units::CToK(*s.powder_c);
    }
    for (const WindInput& w : s.winds) {
        c.winds.push_back(
            {w.until_m > 0.0 ? w.until_m : 1e6, w.speed_mps, units::DegToRad(w.from_deg), 0.0});
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
    const std::array<Field, 13> fields = {{{"temperature_c", &s.temperature_c},
                                           {"pressure_hpa", &s.pressure_hpa},
                                           {"altitude_m", &s.altitude_m},
                                           {"humidity_pct", &s.humidity_pct},
                                           {"look_angle_deg", &s.look_angle_deg},
                                           {"cant_deg", &s.cant_deg},
                                           {"target_range_m", &s.target_range_m},
                                           {"magnification", &s.magnification},
                                           {"target_height_cm", &s.target_height_cm},
                                           {"wind_gust_mps", &s.wind_gust_mps},
                                           {"target_speed_mps", &s.target_speed_mps},
                                           {"target_heading_deg", &s.target_heading_deg},
                                           {"weather_at", &s.weather_at_unix}}};
    for (const Field& f : fields) {
        auto v = get(f.key);
        if (!v) {
            return v.error();
        }
        double d = 0.0;
        if (const auto& text = v.value(); text && ParseNum(*text, d)) {
            *f.value = d;
        }
    }
    struct OptField {
        const char* key;
        std::optional<double>* value;
    };
    const std::array<OptField, 4> opts = {{{"powder_c", &s.powder_c},
                                           {"latitude_deg", &s.latitude_deg},
                                           {"azimuth_deg", &s.azimuth_deg},
                                           {"density_altitude_m", &s.density_altitude_m}}};
    for (const OptField& f : opts) {
        auto v = get(f.key);
        if (!v) {
            return v.error();
        }
        double d = 0.0;
        if (const auto& text = v.value(); text && ParseNum(*text, d)) {
            *f.value = d;
        }
    }
    auto winds = get("winds");
    if (!winds) {
        return winds.error();
    }
    if (const auto& text = winds.value()) {
        s.winds = WindsFromText(*text);
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
          {"density_altitude_m", opt(s.density_altitude_m)},
          {"target_height_cm", Num(s.target_height_cm)},
          {"wind_gust_mps", Num(s.wind_gust_mps)},
          {"target_speed_mps", Num(s.target_speed_mps)},
          {"target_heading_deg", Num(s.target_heading_deg)},
          {"weather_at", Num(s.weather_at_unix)},
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
                          AngleUnit unit, double now_unix) {
    SolutionSummary out;
    out.range_m = s.target_range_m;
    if (!(s.target_range_m > 0.0)) {
        out.error = "Enter a target range.";
        return out;
    }
    // Far enough for the point-blank range of a big target, too.
    constexpr double kPointBlankReachM = 1000.0;
    const storage::ConditionsRecord conditions = ToConditions(s);
    auto sol = storage::Solve(profile, conditions,
                              std::max(s.target_range_m + LeadReachM(s), kPointBlankReachM) + 1.0);
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
    // An integer count, the range computed from it: no rounding error builds up.
    const auto r_steps = static_cast<int>(std::floor((s.target_range_m - 10.0) / 10.0 + 1e-9));
    for (int k = 0; k <= r_steps; ++k) {
        const double r = 10.0 + 10.0 * static_cast<double>(k);
        const auto q = traj.AtSlantRange(r);
        if (q && q->mach < 1.2) {
            out.transonic_range_m = r;
            break;
        }
    }

    if (s.wind_gust_mps > 0.0) {
        // The same shot with the second speed in the nearest zone (a still
        // shooter's zone becomes a zone to the end of the range).
        SessionConditions gust = s;
        if (gust.winds.empty()) {
            gust.winds.push_back({0.0, 90.0, 0.0});
        }
        gust.winds.front().speed_mps = s.wind_gust_mps;
        gust.wind_gust_mps = 0.0;
        if (auto g = storage::Solve(profile, ToConditions(gust), s.target_range_m + 1.0)) {
            if (const auto gp = g.value().trajectory.AtSlantRange(s.target_range_m)) {
                out.has_gust = true;
                out.gust_windage = FromRad(gp->hold_windage_rad, unit);
                out.gust_windage_cm = gp->windage_m * 100.0;
                if (profile.scope) {
                    out.gust_windage_clicks = storage::ToClicks(
                        gp->hold_windage_rad, profile.scope->click_horizontal_rad);
                }
            }
        }
    }

    if (s.target_speed_mps > 0.0) {
        const auto [crossing, radial] = TargetVelocity(s);
        const auto lead = MovingTargetLead(traj, s.target_range_m, crossing, radial);
        const auto meet = lead ? traj.AtSlantRange(lead->range_m) : std::nullopt;
        if (meet) {
            out.has_lead = true;
            out.lead = FromRad(lead->hold_rad, unit);
            out.lead_cm = lead->lateral_m * 100.0;
            out.lead_range_m = lead->range_m;
            const double total = meet->hold_windage_rad + lead->hold_rad;
            out.lead_total_windage = FromRad(total, unit);
            out.lead_elevation = FromRad(meet->hold_elevation_rad, unit);
            if (profile.scope) {
                const double h = profile.scope->click_horizontal_rad;
                out.lead_clicks = storage::ToClicks(lead->hold_rad, h);
                out.lead_total_windage_clicks = storage::ToClicks(total, h);
                out.lead_elevation_clicks =
                    storage::ToClicks(meet->hold_elevation_rad, profile.scope->click_vertical_rad);
            }
        }
    }

    const Apex apex = MaxOrdinate(traj, s.target_range_m);
    out.apex_cm = apex.height_m * 100.0;
    out.apex_range_m = apex.slant_range_m;
    if (const auto pbr = PointBlankRange(traj, s.target_height_cm / 200.0, kPointBlankReachM)) {
        out.point_blank_near_m = pbr->near_m;
        out.point_blank_far_m = pbr->far_m;
    }
    const Atmosphere& air = conditions.atmosphere;
    out.density_altitude_m = DensityAltitude(air);
    out.pressure_hpa = air.pressure_pa / 100.0;

    if (out.stability > 0.0 && out.stability < 1.0) {
        out.warnings.push_back({kWarnUnstable, out.stability});
    } else if (out.stability > 0.0 && out.stability < kMarginalStability) {
        out.warnings.push_back({kWarnLowStability, out.stability});
    }
    if (pt->mach < 1.0) {
        out.warnings.push_back({kWarnSubsonic, pt->mach});
    } else if (pt->mach < 1.2) {
        out.warnings.push_back({kWarnTransonic, pt->mach});
    }
    const Atmosphere& zero = profile.rifle.zero_atmosphere;
    const double dt = air.temperature_k - zero.temperature_k;
    if (std::abs(dt) > kZeroTemperatureLimitC) {
        out.warnings.push_back({kWarnZeroTemperature, dt});
    }
    const double dp = (air.pressure_pa - zero.pressure_pa) / 100.0;
    if (std::abs(dp) > kZeroPressureLimitHpa) {
        out.warnings.push_back({kWarnZeroPressure, dp});
    }
    if (now_unix > 0.0 && s.weather_at_unix > 0.0) {
        const double hours = (now_unix - s.weather_at_unix) / 3600.0;
        if (hours > kStaleWeatherHours) {
            out.warnings.push_back({kWarnStaleWeather, hours});
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
    auto sol = storage::Solve(profile, ToConditions(s), to_m + LeadReachM(s) + 1.0);
    const auto [crossing, radial] = TargetVelocity(s);
    if (!sol) {
        table.error = sol.error().message;
        return table;
    }
    const Trajectory& traj = sol.value().trajectory;
    // Earth rotation on its own: the same shot without it.
    std::optional<storage::Solution> still;
    if (s.latitude_deg) {
        SessionConditions no_rotation = s;
        no_rotation.latitude_deg.reset();
        no_rotation.azimuth_deg.reset();
        if (auto r =
                storage::Solve(profile, ToConditions(no_rotation), to_m + LeadReachM(s) + 1.0)) {
            still = std::move(r.value());
        }
    }
    const auto count = static_cast<std::int64_t>(std::floor((to_m - from_m) / step_m + 1e-9));
    for (std::int64_t k = 0; k <= count; ++k) {
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
        row.spin_drift_cm = pt->spin_drift_m * 100.0;
        if (still) {
            if (const auto q = still->trajectory.AtSlantRange(r)) {
                row.coriolis_drift_cm = (pt->windage_m - q->windage_m) * 100.0;
                row.coriolis_lift_cm = (pt->drop_m - q->drop_m) * 100.0;
            }
        }
        if (s.target_speed_mps > 0.0 && r > 0.0) {
            if (const auto lead = MovingTargetLead(traj, r, crossing, radial)) {
                row.lead = FromRad(lead->hold_rad, unit);
                row.lead_cm = lead->lateral_m * 100.0;
                if (profile.scope) {
                    row.lead_clicks =
                        storage::ToClicks(lead->hold_rad, profile.scope->click_horizontal_rad);
                }
            }
        }
        table.rows.push_back(row);
    }
    table.ok = true;
    return table;
}

}  // namespace ballistics::applogic
