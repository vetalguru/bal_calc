#include <ballistics/bc.h>

#include <ballistics/solver.h>

namespace ballistics {

std::optional<double> FitBc(const std::function<std::optional<double>(double)>& rising,
                            double target) {
    double lo = kMinFittedBc;
    double hi = kMaxFittedBc;
    const auto flo = rising(lo);
    const auto fhi = rising(hi);
    if (!fhi || target > *fhi || (flo && target < *flo)) {
        return std::nullopt;
    }
    for (int i = 0; i < 80 && hi - lo > 1e-7; ++i) {
        const double mid = 0.5 * (lo + hi);
        const auto fm = rising(mid);
        (!fm || *fm < target ? lo : hi) = mid;
    }
    return 0.5 * (lo + hi);
}

std::optional<double> BcFromVelocities(DragTableId table, double v_near_mps, double v_far_mps,
                                       double distance_m, const Atmosphere& air) {
    if (!(distance_m > 0.0) || !(v_near_mps > v_far_mps) || !(v_far_mps > 0.0)) {
        return std::nullopt;
    }
    // The bullet as it passes the near chronograph: over a level stretch its
    // speed hardly depends on gravity, so fly it from there.
    return FitBc(
        [&](double bc) -> std::optional<double> {
            Shot shot;
            shot.drag = DragModel::FromBc(table, bc);
            shot.muzzle_velocity_mps = v_near_mps;
            shot.atmosphere = air;
            shot.spin_drift = false;
            shot.aerodynamic_jump = false;
            const auto p = Fly(shot, distance_m + 1.0).AtSlantRange(distance_m);
            if (!p) {
                return std::nullopt;
            }
            return p->speed_mps;
        },
        v_far_mps);
}

} // namespace ballistics
