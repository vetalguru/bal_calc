#ifndef BALLISTICS_DRAG_H
#define BALLISTICS_DRAG_H

#include <vector>

namespace ballistics {

// One sample of a drag function: drag coefficient at a Mach number.
struct DragPoint {
    double mach = 0.0;
    double cd = 0.0;
};

// Standard reference projectiles.
enum class DragTableId {
    kG1,  // flat base (most published BCs)
    kG2,  // Aberdeen J projectile
    kG5,  // short boat-tail
    kG6,  // flat base, secant ogive
    kG7,  // long boat-tail (modern long-range bullets)
    kG8,  // flat base, 10 cal secant ogive
    kGI,  // Ingalls
    kGS,  // sphere
    kRA4, // .22 LR
};

const char* DragTableName(DragTableId id);
std::vector<DragPoint> StandardDragTable(DragTableId id);

// Monotone piecewise-cubic (PCHIP, Fritsch-Carlson) Cd(Mach) curve: C1,
// no overshoot between table points. Mach outside the table is clamped
// to the end points.
class DragCurve final {
public:
    DragCurve() = default;
    // `points` must have >= 2 entries with strictly increasing Mach.
    explicit DragCurve(std::vector<DragPoint> points);

    double Cd(double mach) const;
    bool empty() const { return x_.empty(); }
    const std::vector<DragPoint>& points() const { return points_; }

private:
    std::vector<DragPoint> points_;
    std::vector<double> x_, a_, b_, c_, d_;
};

// A published BC valid around one velocity (e.g. Sierra's velocity bands).
struct BcPoint {
    double velocity_mps = 0.0;
    double bc_lb_in2 = 0.0;
};

// Drag of a particular projectile. Retardation is
//   a = rho * K(M) * v^2,   K = (pi / 8) * Cd_ref(M) / BC,
// with BC in kg/m^2 (sectional density / form factor).
class DragModel final {
public:
    // Published BC (lb/in^2) against a standard table.
    static DragModel FromBc(DragTableId table, double bc_lb_in2);

    // Several BCs against one standard table, each at a velocity. The BC is
    // interpolated linearly in Mach (velocities are converted at the
    // standard 15 C speed of sound, 340.29 m/s) and held constant beyond
    // the first/last point. One point is the same as FromBc.
    static DragModel FromMultiBc(DragTableId table, std::vector<BcPoint> points);

    // Projectile-specific Cd(M) curve (e.g. Doppler-radar measured); the
    // BC is the sectional density m/d^2 divided by `form_factor`.
    static DragModel FromCurve(std::vector<DragPoint> curve, double mass_kg, double diameter_m,
                               double form_factor = 1.0);

    // The same model with all drag multiplied by `factor` (truing).
    DragModel Scaled(double factor) const;

    // K(M) in m^2/kg; multiply by density and v^2 for the deceleration.
    double Coefficient(double mach) const;

    double bc_kg_m2() const { return bc_kg_m2_; }
    const DragCurve& curve() const { return curve_; }

private:
    DragCurve curve_;
    double bc_kg_m2_ = 0.0;
};

} // namespace ballistics

#endif // BALLISTICS_DRAG_H
