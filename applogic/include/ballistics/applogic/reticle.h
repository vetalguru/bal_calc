#ifndef BALLISTICS_APPLOGIC_RETICLE_H
#define BALLISTICS_APPLOGIC_RETICLE_H

#include <string>

#include <ballistics/storage/records.h>

// Where to put the target in the reticle for a firing solution.
namespace ballistics::applogic {

// How the correction is applied.
enum class HoldMode {
    kDialElevation, // dial elevation on the turret, hold windage (default)
    kHoldAll,       // hold both on the reticle, turrets stay at zero
    kDialAll,       // dial both
};

HoldMode HoldModeFromString(const std::string& s); // "dial_elevation" | "hold" | "dial"
const char* ToString(HoldMode mode);

// How many times larger a reticle mark looks than its nominal value: 1 for
// FFP; reference / current magnification for SFP (marks grow when zoomed
// out). 1 when the magnifications are unknown.
double SubtensionScale(const storage::ScopeRecord& scope, double magnification);

struct ReticleHold {
    // Turret settings (whole clicks; 0 for the axes that are held).
    double dial_elevation_clicks = 0.0;
    double dial_windage_clicks = 0.0;
    double dial_elevation_rad = 0.0;
    double dial_windage_rad = 0.0;
    // Where to put the target, in the reticle's own drawing units (nominal
    // mrad of the reticle, y up, aiming point at 0,0). Holding up means the
    // target goes below the centre, hence the negative signs.
    double target_x = 0.0;
    double target_y = 0.0;
    double scale = 1.0; // SubtensionScale used
};

// Splits a correction (up/right positive, rad) into turret clicks and a
// reticle hold. Dialled values are whole clicks; the rounding remainder is
// left to the hold.
ReticleHold ComputeReticleHold(double elevation_rad, double windage_rad,
                               const storage::ScopeRecord& scope, double magnification,
                               HoldMode mode);

} // namespace ballistics::applogic

#endif // BALLISTICS_APPLOGIC_RETICLE_H
