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
};

storage::ConditionsRecord ToConditions(const SessionConditions& s);

// Persisted in app settings so the app reopens where it was left.
Result<SessionConditions> LoadSession(storage::Database& db);
Status SaveSession(storage::Database& db, const SessionConditions& s);

enum class AngleUnit { kMrad, kMoa };

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
};

SolutionSummary Summarize(const storage::LoadedProfile& profile, const SessionConditions& s,
                          AngleUnit unit);

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
