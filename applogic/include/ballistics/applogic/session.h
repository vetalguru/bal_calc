#ifndef BALLISTICS_APPLOGIC_SESSION_H
#define BALLISTICS_APPLOGIC_SESSION_H

#include <optional>
#include <string>
#include <vector>

#include <ballistics/storage/database.h>
#include <ballistics/storage/solution.h>

// The current shooting situation (what the conditions screen edits) and
// the numbers the solution screen shows.
namespace ballistics::applogic {

using storage::Id;
using storage::Result;
using storage::Status;

struct WindInput {
    double speed_mps = 0.0;
    double from_deg = 90.0; // 0 = headwind, 90 = from the right
    double until_m = 0.0;   // 0 = to the end of the range
};

struct SessionConditions {
    double temperature_c = 15.0;
    double pressure_hpa = 1013.25; // station pressure
    double altitude_m = 0.0;
    double humidity_pct = 50.0;
    std::optional<double> powder_c; // default: air temperature
    std::vector<WindInput> winds;   // in zone order
    double look_angle_deg = 0.0;
    double cant_deg = 0.0;
    std::optional<double> latitude_deg; // Coriolis on when set
    std::optional<double> azimuth_deg;
    double target_range_m = 300.0;
    double magnification = 0.0; // current zoom (SFP holds), 0 = unknown
    // Typed instead of the pressure: the pressure then follows from it, the
    // temperature and the humidity.
    std::optional<double> density_altitude_m;
    double target_height_cm = 20.0; // for the point-blank range
    // Wind bracket: a second speed of the first (nearest) wind zone, e.g.
    // the gusts; 0 = off. The solution then gives the windage for both.
    double wind_gust_mps = 0.0;
    double weather_at_unix = 0.0;   // when the air was last entered, 0 = unknown
    // A moving target: its speed and where it heads, like the wind but the
    // way it goes: 0 = away, 90 = to the right across the line of fire,
    // 180 = toward the shooter, 270 = to the left. Speed 0 = standing.
    double target_speed_mps = 0.0;
    double target_heading_deg = 90.0;
};

storage::ConditionsRecord ToConditions(const SessionConditions& s);

// Persisted in app settings so the app reopens where it was left.
Result<SessionConditions> LoadSession(storage::Database& db);
Status SaveSession(storage::Database& db, const SessionConditions& s);

enum class AngleUnit { kMrad, kMoa };

// Something the shooter should know about this solution. `code` is stable
// (the app translates it); `value` is the number the message shows.
struct Warning {
    std::string code;
    double value = 0.0;
};

// Warning codes.
inline constexpr const char* kWarnUnstable = "unstable";          // Sg < 1.0; value = Sg
inline constexpr const char* kWarnLowStability = "lowStability";  // Sg < 1.3; value = Sg
inline constexpr const char* kWarnSubsonic = "subsonic";          // at the target; value = Mach
inline constexpr const char* kWarnTransonic = "transonic";        // Mach < 1.2 at the target
inline constexpr const char* kWarnZeroTemperature = "zeroTemperature"; // value = C off the zero
inline constexpr const char* kWarnZeroPressure = "zeroPressure";       // value = hPa off the zero
inline constexpr const char* kWarnStaleWeather = "staleWeather";       // value = hours since entered

// Thresholds of the warnings (one place, see Summarize).
inline constexpr double kMarginalStability = 1.3;
inline constexpr double kZeroTemperatureLimitC = 15.0;
inline constexpr double kZeroPressureLimitHpa = 50.0;
inline constexpr double kStaleWeatherHours = 24.0;

struct SolutionSummary {
    bool ok = false;
    std::string error;

    double range_m = 0.0;
    // Corrections (dial/hold up and right positive) in the chosen unit and
    // in clicks of the profile's scope (0 without a scope).
    double elevation = 0.0;
    double windage = 0.0;
    double elevation_clicks = 0.0;
    double windage_clicks = 0.0;
    double drop_cm = 0.0;
    double windage_cm = 0.0;

    double velocity_mps = 0.0;
    double energy_j = 0.0;
    double time_s = 0.0;
    double mach = 0.0;
    double muzzle_velocity_mps = 0.0;
    double stability = 0.0;    // 0 when twist/length unknown
    double spin_drift_cm = 0.0;
    bool subsonic = false;     // at the target
    double transonic_range_m = 0.0; // first range below Mach 1.2, 0 if none
    // Highest point above the line of sight on the way to the target.
    double apex_cm = 0.0;
    double apex_range_m = 0.0;
    // Point-blank range for a target `target_height_cm` tall, aimed at its
    // centre (0/0 if the bullet never stays within it).
    double point_blank_near_m = 0.0;
    double point_blank_far_m = 0.0;
    double density_altitude_m = 0.0;
    double pressure_hpa = 0.0; // station pressure used (follows a typed density altitude)
    // Windage with the second wind speed (has_gust when it is set).
    bool has_gust = false;
    double gust_windage = 0.0;
    double gust_windage_clicks = 0.0;
    double gust_windage_cm = 0.0;
    // A moving target (has_lead): the extra aim in the direction it moves,
    // the windage with it, and where the bullet meets the target (its
    // elevation, when the target comes closer or goes away meanwhile).
    bool has_lead = false;
    double lead = 0.0; // right positive, like windage
    double lead_clicks = 0.0;
    double lead_cm = 0.0; // how far the target moves during the flight
    double lead_total_windage = 0.0;
    double lead_total_windage_clicks = 0.0;
    double lead_range_m = 0.0;
    double lead_elevation = 0.0;
    double lead_elevation_clicks = 0.0;
    std::vector<Warning> warnings;
};

// `now_unix` (seconds) enables the stale-weather warning; 0 skips it.
SolutionSummary Summarize(const storage::LoadedProfile& profile, const SessionConditions& s,
                          AngleUnit unit, double now_unix = 0.0);

double FromRad(double rad, AngleUnit unit);

// One line of the range card.
struct RangeRow {
    double range_m = 0.0;
    double elevation = 0.0; // in the chosen angle unit, up positive
    double windage = 0.0;   // right positive
    double elevation_clicks = 0.0;
    double windage_clicks = 0.0;
    double drop_cm = 0.0;
    double windage_cm = 0.0;
    double velocity_mps = 0.0;
    double mach = 0.0;
    double energy_j = 0.0;
    double time_s = 0.0;
    double lead = 0.0; // for a moving target (see SessionConditions), 0 otherwise
    double lead_clicks = 0.0;
};

struct RangeTable {
    bool ok = false;
    std::string error;
    std::vector<RangeRow> rows; // stops early where the bullet stops
};

// Range card from `from_m` to `to_m` every `step_m` (one flight). The
// target range of `s` is ignored.
RangeTable BuildRangeTable(const storage::LoadedProfile& profile, const SessionConditions& s,
                           AngleUnit unit, double from_m, double to_m, double step_m);

} // namespace ballistics::applogic

#endif // BALLISTICS_APPLOGIC_SESSION_H
