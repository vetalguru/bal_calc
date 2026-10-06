#ifndef BALLISTICS_APPLOGIC_WEZ_H
#define BALLISTICS_APPLOGIC_WEZ_H

// Hit probability for the current rifle, cartridge and conditions: what
// the shooter enters (in their units) and the curve over the range.

#include <string>
#include <vector>

#include <ballistics/applogic/session.h>
#include <ballistics/storage/database.h>
#include <ballistics/storage/solution.h>
#include <ballistics/wez.h>

namespace ballistics::applogic {

// The errors (1 sigma) and the target, as entered; kept in the settings.
struct WezSettings {
    double range_m = 5.0;             // a laser rangefinder
    double wind_speed_mps = 1.0;
    double wind_direction_deg = 10.0;
    double muzzle_velocity_mps = 4.0; // SD of the load
    double bc_percent = 1.0;
    double temperature_c = 2.0;
    double pressure_hpa = 2.0;
    double humidity_pct = 10.0;
    double look_angle_deg = 0.5;
    double cant_deg = 1.0;
    double azimuth_deg = 5.0;
    double latitude_deg = 0.5;
    double group_moa = 1.0; // 5-shot group of the rifle and shooter
    std::string target_kind = "rectangle"; // "rectangle", "ellipse", "figure"
    double target_width_cm = 50.0;
    double target_height_cm = 50.0;
};

Result<WezSettings> LoadWezSettings(storage::Database& db);
Status SaveWezSettings(storage::Database& db, const WezSettings& w);

ErrorSources ToErrorSources(const WezSettings& w);
Target ToTarget(const WezSettings& w);

struct WezRow {
    double range_m = 0.0;
    double probability = 0.0; // 0..1
    double sigma_up_cm = 0.0;
    double sigma_right_cm = 0.0;
};

struct WezResult {
    bool ok = false;
    std::string error;
    std::vector<WezRow> rows;        // step_m apart up to to_m
    WezRow at_target;                // at the session's target range
    std::vector<Spread::Part> parts; // at the target range, largest first
    int shots_50 = 0;                // shots to hit once at the target with
    int shots_80 = 0;                // 50, 80 and 95 % confidence
    int shots_95 = 0;
};

WezResult ComputeWez(const storage::LoadedProfile& profile, const SessionConditions& s,
                     const WezSettings& w, double to_m, double step_m);

} // namespace ballistics::applogic

#endif // BALLISTICS_APPLOGIC_WEZ_H
