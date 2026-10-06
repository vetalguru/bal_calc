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

} // namespace ballistics

#endif // BALLISTICS_ANALYSIS_H
