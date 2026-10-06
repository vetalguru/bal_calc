#ifndef BALLISTICS_ANALYSIS_H
#define BALLISTICS_ANALYSIS_H

#include <optional>

#include <ballistics/solver.h>

// Numbers read off a computed flight: the highest point of the trajectory
// and the point-blank range.

namespace ballistics {

// Highest point of the trajectory above the line of sight.
struct Apex {
    double slant_range_m = 0.0; // where along the LOS
    double height_m = 0.0;      // above the LOS (negative if never above it)
};

// The apex between the muzzle and `max_slant_range_m` (or where the flight
// ended), located to about 1 cm of range.
Apex MaxOrdinate(const Trajectory& trajectory, double max_slant_range_m);

// Stretch of the range where the bullet stays within `half_height_m` of
// the line of sight, aiming at the centre of a target that tall.
struct PointBlank {
    double near_m = 0.0; // where the bullet rises into the band (0 at the muzzle)
    double far_m = 0.0;  // where it leaves the band
};

// nullopt if the bullet never enters the band before `max_slant_range_m`.
// A bullet still inside the band at the end gives far_m = the range covered.
std::optional<PointBlank> PointBlankRange(const Trajectory& trajectory, double half_height_m,
                                          double max_slant_range_m);

// Where a bullet meets a target moving at a constant velocity: `crossing_mps`
// across the line of sight (right positive) and `radial_mps` along it (away
// positive), from `range_m` at the moment of the shot.
struct Lead {
    double range_m = 0.0;   // LOS distance of the meeting point
    double time_s = 0.0;    // flight time to it
    double lateral_m = 0.0; // how far the target has moved across the LOS
    // The extra aim in the direction of the motion, right positive (the
    // angle of `lateral_m` seen from the muzzle at `range_m`).
    double hold_rad = 0.0;
};

// Fixed-point iteration on the flight time (it converges in a few steps,
// since a target is far slower than the bullet). nullopt if the bullet
// does not reach the meeting point.
std::optional<Lead> MovingTargetLead(const Trajectory& trajectory, double range_m,
                                     double crossing_mps, double radial_mps = 0.0);

} // namespace ballistics

#endif // BALLISTICS_ANALYSIS_H
