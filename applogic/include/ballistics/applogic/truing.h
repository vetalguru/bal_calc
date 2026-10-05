#ifndef BALLISTICS_APPLOGIC_TRUING_H
#define BALLISTICS_APPLOGIC_TRUING_H

#include <optional>
#include <string>
#include <vector>

#include <ballistics/applogic/session.h>
#include <ballistics/storage/database.h>
#include <ballistics/storage/records.h>

// Shot log and truing: fit the profile's muzzle-velocity and drag scales
// to the corrections that actually hit.
namespace ballistics::applogic {

using storage::DopeRecord;

Result<std::vector<DopeRecord>> ListShots(storage::Database& db, Id profile_id);

// Logs a shot taken in `conditions` at `range_m` with the elevation (and
// optionally windage) correction that hit, up/right positive, rad. The
// current prediction is stored alongside.
Result<Id> LogShot(storage::Database& db, Id profile_id, const SessionConditions& conditions,
                   double range_m, double observed_elevation_rad,
                   std::optional<double> observed_windage_rad = std::nullopt,
                   const std::string& notes = "");
Status DeleteShot(storage::Database& db, Id shot_id);
Status SetShotUsedForTruing(storage::Database& db, Id shot_id, bool used);

struct TruingPoint {
    Id shot_id = 0;
    double range_m = 0.0;
    double observed_rad = 0.0;
    double predicted_before_rad = 0.0; // with the profile's current scales
    double predicted_after_rad = 0.0;  // with the fitted scales
};

struct TruingResult {
    bool ok = false;
    std::string error;
    double velocity_scale = 1.0;
    double drag_scale = 1.0;
    bool drag_fitted = false; // false: only velocity could be told apart
    double rms_before_rad = 0.0;
    double rms_after_rad = 0.0;
    double muzzle_velocity_before_mps = 0.0; // at the cartridge's reference powder temperature
    double muzzle_velocity_after_mps = 0.0;
    std::vector<TruingPoint> points;
};

// Fits the scales to the shots marked for truing. One shot (or shots too
// close in range to separate velocity from drag) fits velocity only.
TruingResult ComputeTruing(storage::Database& db, Id profile_id);

Status ApplyTruing(storage::Database& db, Id profile_id, const TruingResult& result);
Status ResetTruing(storage::Database& db, Id profile_id);

} // namespace ballistics::applogic

#endif // BALLISTICS_APPLOGIC_TRUING_H
