#ifndef BALLISTICS_BC_H
#define BALLISTICS_BC_H

#include <ballistics/atmosphere.h>
#include <ballistics/drag.h>

#include <functional>
#include <optional>

// Ballistic coefficient from measurements: the BC (against a standard
// table) with which the model reproduces what was measured.

namespace ballistics {

// Range of BCs searched (lb/in^2).
inline constexpr double kMinFittedBc = 0.02;
inline constexpr double kMaxFittedBc = 2.0;

// The BC at which `rising(bc)` (a quantity that grows with the BC, such as
// the velocity kept or minus the drop) equals `target`, by bisection in
// kMinFittedBc..kMaxFittedBc. No value from `rising` (a bullet too draggy
// to get there) counts as below the target. nullopt when no BC in range
// reaches the target.
std::optional<double> FitBc(const std::function<std::optional<double>(double)>& rising,
                            double target);

// Two chronograph readings `distance_m` apart on a level line in `air`:
// `v_near_mps` at the near one, `v_far_mps` at the far one. nullopt when
// no BC in range gives that slowdown.
std::optional<double> BcFromVelocities(DragTableId table, double v_near_mps, double v_far_mps,
                                       double distance_m, const Atmosphere& air);

}  // namespace ballistics

#endif  // BALLISTICS_BC_H
