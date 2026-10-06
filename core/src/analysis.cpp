#include <ballistics/analysis.h>

#include <algorithm>
#include <cmath>

namespace ballistics {

namespace {

constexpr double kScanStepM = 1.0;    // coarse scan along the LOS
constexpr double kRangeToleranceM = 0.01;

// Height above the LOS at a LOS distance; the muzzle (0) is the sight
// height below the LOS.
std::optional<double> Height(const Trajectory& t, double r) {
    const auto p = t.AtSlantRange(r);
    if (!p) {
        return std::nullopt;
    }
    return p->drop_m;
}

// Range in [lo, hi] where `inside` changes from `lo_inside` (bisection).
template <typename Inside>
double Boundary(double lo, double hi, Inside inside) {
    const bool lo_inside = inside(lo);
    while (hi - lo > kRangeToleranceM) {
        const double mid = 0.5 * (lo + hi);
        (inside(mid) == lo_inside ? lo : hi) = mid;
    }
    return 0.5 * (lo + hi);
}

} // namespace

Apex MaxOrdinate(const Trajectory& trajectory, double max_slant_range_m) {
    const double end = std::min(max_slant_range_m, trajectory.max_slant_range_m());
    Apex best;
    best.height_m = -1e9;
    for (double r = 0.0; r <= end; r += kScanStepM) {
        if (const auto h = Height(trajectory, r); h && *h > best.height_m) {
            best = {r, *h};
        }
    }
    // Golden-section search around the best sample: the height is smooth
    // and has one maximum there.
    double a = std::max(0.0, best.slant_range_m - kScanStepM);
    double b = std::min(end, best.slant_range_m + kScanStepM);
    constexpr double kInvPhi = 0.6180339887498949;
    while (b - a > kRangeToleranceM) {
        const double c = b - kInvPhi * (b - a);
        const double d = a + kInvPhi * (b - a);
        if (Height(trajectory, c).value_or(-1e9) > Height(trajectory, d).value_or(-1e9)) {
            b = d;
        } else {
            a = c;
        }
    }
    const double r = 0.5 * (a + b);
    if (const auto h = Height(trajectory, r); h && *h > best.height_m) {
        best = {r, *h};
    }
    return best;
}

std::optional<PointBlank> PointBlankRange(const Trajectory& trajectory, double half_height_m,
                                          double max_slant_range_m) {
    const double end = std::min(max_slant_range_m, trajectory.max_slant_range_m());
    const auto inside = [&](double r) {
        const auto h = Height(trajectory, r);
        return h && std::abs(*h) <= half_height_m;
    };
    std::optional<PointBlank> out;
    double prev = 0.0;
    for (double r = 0.0; r <= end; r += kScanStepM) {
        const bool in = inside(r);
        if (!out && in) {
            out = PointBlank{r > 0.0 ? Boundary(prev, r, inside) : 0.0, end};
        } else if (out && !in) {
            out->far_m = Boundary(prev, r, inside);
            break;
        }
        prev = r;
    }
    return out;
}

std::optional<Lead> MovingTargetLead(const Trajectory& trajectory, double range_m,
                                     double crossing_mps, double radial_mps) {
    Lead lead;
    lead.range_m = range_m;
    for (int i = 0; i < 50; ++i) {
        const auto p = trajectory.AtSlantRange(lead.range_m);
        if (!p) {
            return std::nullopt;
        }
        const double next = range_m + radial_mps * p->time_s;
        const bool done = std::abs(p->time_s - lead.time_s) < 1e-9;
        lead.time_s = p->time_s;
        if (done) {
            break;
        }
        lead.range_m = next;
    }
    lead.lateral_m = crossing_mps * lead.time_s;
    lead.hold_rad = std::atan2(lead.lateral_m, lead.range_m);
    return lead;
}

} // namespace ballistics
