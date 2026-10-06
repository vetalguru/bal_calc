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

// Drag scale factor (DSF) truing: the transonic part of the trajectory.
// Shots slower than kDsfMaxMach at the target each get a DSF point at
// their Mach, fitted from the nearest shot out (each point changes the
// flight to every farther one); the table keeps factor 1 from
// kDsfAnchorMach up, so the supersonic part stays as velocity and drag
// truing left it. Shots closer than kDsfMinMachStep to the previous point
// are skipped.
inline constexpr double kDsfMaxMach = 1.3;
inline constexpr double kDsfAnchorMach = 1.4;
inline constexpr double kDsfMinMachStep = 0.08;
inline constexpr double kDsfMinFactor = 0.5;
inline constexpr double kDsfMaxFactor = 2.0;
// A transonic shot still missing by more than this after the fit is one the
// DSF cannot explain (DsfShot::limited).
inline constexpr double kDsfMissTolerance = 0.05e-3; // rad

struct DsfShot {
    Id shot_id = 0;
    double range_m = 0.0;
    double mach = 0.0;                 // at the target, before the fit
    double observed_rad = 0.0;
    double predicted_before_rad = 0.0; // with the profile's current DSF
    double predicted_after_rad = 0.0;  // with the fitted one
    bool used = false;                 // gave a DSF point
    // Still misses by more than kDsfMissTolerance after the fit: the DSF
    // alone cannot explain this hit (true the velocity and drag first).
    bool limited = false;
};

struct DsfResult {
    bool ok = false;
    std::string error;
    std::vector<DsfPoint> points; // sorted by Mach
    std::vector<DsfShot> shots;   // every shot marked for truing
    double rms_before_rad = 0.0;
    double rms_after_rad = 0.0;
};

DsfResult ComputeDsf(storage::Database& db, Id profile_id);
// Replaces the profile's DSF table (empty = none) after checking it: Mach
// 0..5, factors kDsfMinFactor..kDsfMaxFactor, no two points at one Mach.
Status SetDsf(storage::Database& db, Id profile_id, std::vector<DsfPoint> points);

// BC calculator: the BC against a standard table ("G1", "G7", ...) that
// reproduces a measurement. Failures are a sentence for the shooter.
struct BcResult {
    bool ok = false;
    std::string error;
    double bc = 0.0; // lb/in^2
};

// From two chronograph readings `distance_m` apart, in the session's air.
BcResult BcFromChronograph(const std::string& table, double v_near_mps, double v_far_mps,
                           double distance_m, const SessionConditions& s);

// From the elevation that hit at `range_m` with this rifle and cartridge in
// the session's conditions: the bullet's drag becomes one BC against
// `table`, everything else (zero, powder, truing) stays as the app solves.
BcResult BcFromHit(const storage::LoadedProfile& profile, const std::string& table,
                   double range_m, double elevation_rad, const SessionConditions& s);

} // namespace ballistics::applogic

#endif // BALLISTICS_APPLOGIC_TRUING_H
