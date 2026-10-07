#include <ballistics/applogic/reticle.h>
#include <ballistics/units.h>

#include <cmath>

namespace ballistics::applogic {

HoldMode HoldModeFromString(const std::string& s) {
    if (s == "hold") {
        return HoldMode::kHoldAll;
    }
    if (s == "dial") {
        return HoldMode::kDialAll;
    }
    return HoldMode::kDialElevation;
}

const char* ToString(HoldMode mode) {
    switch (mode) {
        case HoldMode::kHoldAll:
            return "hold";
        case HoldMode::kDialAll:
            return "dial";
        case HoldMode::kDialElevation:
            break;
    }
    return "dial_elevation";
}

double SubtensionScale(const storage::ScopeRecord& scope, double magnification) {
    if (scope.focal_plane != "sfp" || !(scope.sfp_reference_magnification > 0.0) ||
        !(magnification > 0.0)) {
        return 1.0;
    }
    return scope.sfp_reference_magnification / magnification;
}

ReticleHold ComputeReticleHold(double elevation_rad, double windage_rad,
                               const storage::ScopeRecord& scope, double magnification,
                               HoldMode mode) {
    ReticleHold h;
    h.scale = SubtensionScale(scope, magnification);

    auto dial = [](double angle, double click, double& clicks, double& dialled) {
        if (click > 0.0) {
            clicks = std::round(angle / click);
            dialled = clicks * click;
        } else {
            clicks = 0.0;
            dialled = angle;  // no turret data: treat as dialled exactly
        }
    };
    if (mode != HoldMode::kHoldAll) {
        dial(elevation_rad, scope.click_vertical_rad, h.dial_elevation_clicks,
             h.dial_elevation_rad);
    }
    if (mode == HoldMode::kDialAll) {
        dial(windage_rad, scope.click_horizontal_rad, h.dial_windage_clicks, h.dial_windage_rad);
    }
    // What is left is held; a mark of nominal n mrad covers n * scale.
    const double hold_up = elevation_rad - h.dial_elevation_rad;
    const double hold_right = windage_rad - h.dial_windage_rad;
    h.target_x = -units::RadToMrad(hold_right) / h.scale;
    h.target_y = -units::RadToMrad(hold_up) / h.scale;
    return h;
}

}  // namespace ballistics::applogic
