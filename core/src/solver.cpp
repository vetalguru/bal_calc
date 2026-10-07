#include <ballistics/effects.h>
#include <ballistics/solver.h>

#include <algorithm>
#include <cmath>

namespace ballistics {

namespace {

// Projectile state: position and velocity.
struct State {
    Vec3 p, v;
};

State operator+(const State& s, const State& d) { return {s.p + d.p, s.v + d.v}; }
State operator*(double k, const State& s) { return {k * s.p, k * s.v}; }

// Air velocity of a wind zone in the shooter's frame.
Vec3 WindVector(const WindZone& w) {
    return {-w.speed_mps * std::cos(w.from_rad), w.vertical_mps,
            -w.speed_mps * std::sin(w.from_rad)};
}

// Earth's rotation vector in the shooter's frame (x towards the LOS
// azimuth, y up, z right). Without an azimuth only the local vertical
// component - the one behind horizontal drift - is kept.
Vec3 EarthRotation(const Shot& shot) {
    if (!shot.latitude_rad) {
        return {};
    }
    const double lat = *shot.latitude_rad;
    if (!shot.azimuth_rad) {
        return {0.0, kEarthAngularVelocity * std::sin(lat), 0.0};
    }
    const double az = *shot.azimuth_rad;
    return {kEarthAngularVelocity * std::cos(lat) * std::cos(az),
            kEarthAngularVelocity * std::sin(lat),
            -kEarthAngularVelocity * std::cos(lat) * std::sin(az)};
}

// Right-hand side of the point-mass equations of motion: gravity, drag
// against the moving air, Coriolis.
class Dynamics {
   public:
    Dynamics(const Shot& shot, const AtmosphereModel& air) : shot_(shot), air_(air) {
        for (const WindZone& w : shot.winds) {
            zones_.push_back({w.until_range_m, WindVector(w)});
        }
        std::stable_sort(zones_.begin(), zones_.end(),
                         [](const Zone& a, const Zone& b) { return a.until < b.until; });
        omega2_ = 2.0 * EarthRotation(shot);
    }

    Vec3 Wind(double x) const {
        for (const Zone& z : zones_) {
            if (x < z.until) {
                return z.air;
            }
        }
        return zones_.empty() ? Vec3{} : zones_.back().air;
    }

    Vec3 Acceleration(const State& s) const {
        const AirState air = air_.At(shot_.atmosphere.altitude_m + s.p.y);
        const Vec3 v_air = s.v - Wind(s.p.x);
        const double speed = v_air.Norm();
        const double mach = speed / air.speed_of_sound_mps;
        const double k = air.density_kg_m3 * shot_.drag.Coefficient(mach) * speed;
        return Vec3{0.0, -shot_.gravity_mps2, 0.0} - k * v_air - Cross(omega2_, s.v);
    }

    State Derivative(const State& s) const { return {s.v, Acceleration(s)}; }

   private:
    struct Zone {
        double until;
        Vec3 air;
    };
    const Shot& shot_;
    const AtmosphereModel& air_;
    std::vector<Zone> zones_;
    Vec3 omega2_;
};

// Dormand-Prince 5(4) tableau (the system is autonomous, so the c_i
// nodes are not needed).
constexpr double kA21 = 1.0 / 5;
constexpr double kA31 = 3.0 / 40, kA32 = 9.0 / 40;
constexpr double kA41 = 44.0 / 45, kA42 = -56.0 / 15, kA43 = 32.0 / 9;
constexpr double kA51 = 19372.0 / 6561, kA52 = -25360.0 / 2187, kA53 = 64448.0 / 6561,
                 kA54 = -212.0 / 729;
constexpr double kA61 = 9017.0 / 3168, kA62 = -355.0 / 33, kA63 = 46732.0 / 5247, kA64 = 49.0 / 176,
                 kA65 = -5103.0 / 18656;
constexpr double kB1 = 35.0 / 384, kB3 = 500.0 / 1113, kB4 = 125.0 / 192, kB5 = -2187.0 / 6784,
                 kB6 = 11.0 / 84;
// Error weights: 5th-order minus embedded 4th-order solution.
constexpr double kE1 = 71.0 / 57600, kE3 = -71.0 / 16695, kE4 = 71.0 / 1920,
                 kE5 = -17253.0 / 339200, kE6 = 22.0 / 525, kE7 = -1.0 / 40;

double ErrorRatio(const State& err, const State& y0, const State& y1, const SolverOptions& o) {
    auto ratio = [&](double e, double a, double b, double atol) {
        return std::fabs(e) / (atol + o.relative_tolerance * std::max(std::fabs(a), std::fabs(b)));
    };
    double r = 0.0;
    r = std::max(r, ratio(err.p.x, y0.p.x, y1.p.x, o.position_tolerance_m));
    r = std::max(r, ratio(err.p.y, y0.p.y, y1.p.y, o.position_tolerance_m));
    r = std::max(r, ratio(err.p.z, y0.p.z, y1.p.z, o.position_tolerance_m));
    r = std::max(r, ratio(err.v.x, y0.v.x, y1.v.x, o.velocity_tolerance_mps));
    r = std::max(r, ratio(err.v.y, y0.v.y, y1.v.y, o.velocity_tolerance_mps));
    r = std::max(r, ratio(err.v.z, y0.v.z, y1.v.z, o.velocity_tolerance_mps));
    return r;
}

// Quintic Hermite basis on [0, 1] for value, first and second derivative
// at both ends.
struct Quintic {
    double h00, h10, h20, h01, h11, h21;
    explicit Quintic(double s) {
        const double s2 = s * s, s3 = s2 * s, s4 = s3 * s, s5 = s4 * s;
        h00 = 1 - 10 * s3 + 15 * s4 - 6 * s5;
        h10 = s - 6 * s3 + 8 * s4 - 3 * s5;
        h20 = 0.5 * (s2 - 3 * s3 + 3 * s4 - s5);
        h01 = 10 * s3 - 15 * s4 + 6 * s5;
        h11 = -4 * s3 + 7 * s4 - 3 * s5;
        h21 = 0.5 * (s3 - 2 * s4 + s5);
    }
};

}  // namespace

double Trajectory::SlantRange(const Vec3& p) const { return Dot(p - sight_, los_); }

double Trajectory::max_slant_range_m() const {
    return nodes_.empty() ? 0.0 : SlantRange(nodes_.back().p);
}

TrajectoryPoint Trajectory::MakePoint(double t, const Vec3& p, const Vec3& v) const {
    TrajectoryPoint pt;
    pt.time_s = t;
    pt.position = p;
    pt.velocity = v;
    pt.speed_mps = v.Norm();
    pt.mach = pt.speed_mps / air_.At(air_.base().altitude_m + p.y).speed_of_sound_mps;
    pt.energy_j = 0.5 * mass_kg_ * pt.speed_mps * pt.speed_mps;

    const Vec3 rel = p - sight_;
    pt.slant_range_m = Dot(rel, los_);
    pt.drop_m = Dot(rel, los_up_);
    if (spin_drift_) {
        pt.spin_drift_m = LitzSpinDrift(stability_, t, twist_m_);
    }
    pt.windage_m = rel.z + pt.spin_drift_m;
    if (pt.slant_range_m > 0.0) {
        pt.hold_elevation_rad = std::atan2(-pt.drop_m, pt.slant_range_m);
        pt.hold_windage_rad = std::atan2(-pt.windage_m, pt.slant_range_m);
    }
    return pt;
}

TrajectoryPoint Trajectory::Interpolate(std::size_t i, double t) const {
    const Node& n0 = nodes_[i];
    const Node& n1 = nodes_[i + 1];
    const double h = n1.t - n0.t;
    const double s = (t - n0.t) / h;
    const Quintic q(s);
    const Vec3 p = q.h00 * n0.p + (q.h10 * h) * n0.v + (q.h20 * h * h) * n0.a + q.h01 * n1.p +
                   (q.h11 * h) * n1.v + (q.h21 * h * h) * n1.a;
    // Velocity: derivative of the quintic position interpolant.
    const double s2 = s * s, s3 = s2 * s, s4 = s3 * s;
    const double d00 = (-30 * s2 + 60 * s3 - 30 * s4) / h;
    const double d10 = 1 - 18 * s2 + 32 * s3 - 15 * s4;
    const double d20 = 0.5 * h * (2 * s - 9 * s2 + 12 * s3 - 5 * s4);
    const double d01 = (30 * s2 - 60 * s3 + 30 * s4) / h;
    const double d11 = -12 * s2 + 28 * s3 - 15 * s4;
    const double d21 = 0.5 * h * (3 * s2 - 8 * s3 + 5 * s4);
    const Vec3 v = d00 * n0.p + d10 * n0.v + d20 * n0.a + d01 * n1.p + d11 * n1.v + d21 * n1.a;
    return MakePoint(t, p, v);
}

std::optional<TrajectoryPoint> Trajectory::AtTime(double time_s) const {
    if (nodes_.size() < 2 || time_s < nodes_.front().t || time_s > nodes_.back().t) {
        return std::nullopt;
    }
    auto it = std::upper_bound(nodes_.begin(), nodes_.end(), time_s,
                               [](double t, const Node& n) { return t < n.t; });
    std::size_t i = static_cast<std::size_t>(it - nodes_.begin());
    i = std::clamp<std::size_t>(i, 1, nodes_.size() - 1) - 1;
    return Interpolate(i, time_s);
}

std::optional<TrajectoryPoint> Trajectory::AtSlantRange(double slant_range_m) const {
    if (nodes_.size() < 2) {
        return std::nullopt;
    }
    // First segment whose end reaches the requested distance.
    std::size_t i = 0;
    while (i + 1 < nodes_.size() && SlantRange(nodes_[i + 1].p) < slant_range_m) {
        ++i;
    }
    if (i + 1 >= nodes_.size()) {
        return std::nullopt;
    }
    if (SlantRange(nodes_[i].p) > slant_range_m) {
        return std::nullopt;  // before the start of the flight
    }

    // Safeguarded Newton on the interpolant: s(t) is monotone in a segment.
    double lo = nodes_[i].t;
    double hi = nodes_[i + 1].t;
    double t = lo + (hi - lo) * 0.5;
    for (int iter = 0; iter < 60; ++iter) {
        const TrajectoryPoint pt = Interpolate(i, t);
        const double f = pt.slant_range_m - slant_range_m;
        if (std::fabs(f) < 1e-11) {
            return pt;
        }
        if (f > 0.0) {
            hi = t;
        } else {
            lo = t;
        }
        const double rate = Dot(pt.velocity, los_);
        double next = rate > 0.0 ? t - f / rate : 0.5 * (lo + hi);
        if (!(next > lo && next < hi)) {
            next = 0.5 * (lo + hi);
        }
        t = next;
    }
    return Interpolate(i, t);
}

std::vector<TrajectoryPoint> Trajectory::Table(double step_m, double max_m,
                                               bool include_zero) const {
    std::vector<TrajectoryPoint> out;
    if (!(step_m > 0.0)) {
        return out;
    }
    const auto count = static_cast<long>(std::floor(max_m / step_m + 1e-9));
    for (long k = include_zero ? 0 : 1; k <= count; ++k) {
        auto pt = AtSlantRange(static_cast<double>(k) * step_m);
        if (!pt) {
            break;
        }
        out.push_back(*pt);
    }
    return out;
}

Trajectory Fly(const Shot& shot, double max_slant_range_m, const SolverOptions& options) {
    Trajectory traj;
    traj.mass_kg_ = shot.mass_kg;
    traj.air_ = AtmosphereModel(shot.atmosphere, shot.sound_speed);

    // LOS frame: forward, up (vertical plane), right.
    const double cl = std::cos(shot.look_angle_rad);
    const double sl = std::sin(shot.look_angle_rad);
    const Vec3 forward{cl, sl, 0.0};
    const Vec3 up{-sl, cl, 0.0};
    const Vec3 right{0.0, 0.0, 1.0};
    traj.los_ = forward;
    traj.los_up_ = up;

    // Rifle frame: rotated about the LOS by the cant (top to the right).
    const double cc = std::cos(shot.cant_rad);
    const double sc = std::sin(shot.cant_rad);
    const Vec3 rifle_up = cc * up + sc * right;
    const Vec3 rifle_right = cc * right - sc * up;
    traj.sight_ = shot.sight_height_m * rifle_up;

    // Spin: stability at the muzzle, crosswind jump from the first zone.
    traj.twist_m_ = shot.twist_m;
    traj.stability_ = MillerStability(shot.mass_kg, shot.bullet_diameter_m, shot.bullet_length_m,
                                      shot.twist_m, shot.muzzle_velocity_mps,
                                      shot.atmosphere.temperature_k, shot.atmosphere.pressure_pa);
    traj.spin_drift_ = shot.spin_drift && traj.stability_ > 0.0;
    if (shot.aerodynamic_jump && traj.stability_ > 0.0 && !shot.winds.empty()) {
        const WindZone* first = &shot.winds.front();
        for (const WindZone& w : shot.winds) {
            if (w.until_range_m < first->until_range_m) {
                first = &w;
            }
        }
        traj.jump_rad_ =
            AerodynamicJump(traj.stability_, shot.bullet_length_m, shot.bullet_diameter_m,
                            shot.twist_m, first->speed_mps * std::sin(first->from_rad));
    }

    const double elevation = shot.elevation_rad + traj.jump_rad_;
    const double cw = std::cos(shot.windage_rad);
    const Vec3 bore = (std::cos(elevation) * cw) * forward + (std::sin(elevation) * cw) * rifle_up +
                      std::sin(shot.windage_rad) * rifle_right;

    const Dynamics dyn(shot, traj.air_);
    State y{Vec3{}, shot.muzzle_velocity_mps * bore};
    State k1 = dyn.Derivative(y);
    double t = 0.0;
    traj.nodes_.push_back({t, y.p, y.v, k1.v});

    double h = 1e-4;
    constexpr double kMinStep = 1e-9;
    while (true) {
        if (traj.SlantRange(y.p) > max_slant_range_m) {
            traj.stop_reason_ = StopReason::kRangeReached;
            break;
        }
        if (y.v.Norm() < options.min_speed_mps) {
            traj.stop_reason_ = StopReason::kMinSpeed;
            break;
        }
        if (t >= options.max_time_s) {
            traj.stop_reason_ = StopReason::kMaxTime;
            break;
        }
        if (y.p.y < -options.max_drop_m) {
            traj.stop_reason_ = StopReason::kMaxDrop;
            break;
        }

        h = std::min(h, options.max_time_s - t + kMinStep);
        const State k2 = dyn.Derivative(y + (h * kA21) * k1);
        const State k3 = dyn.Derivative(y + h * (kA31 * k1 + kA32 * k2));
        const State k4 = dyn.Derivative(y + h * (kA41 * k1 + kA42 * k2 + kA43 * k3));
        const State k5 = dyn.Derivative(y + h * (kA51 * k1 + kA52 * k2 + kA53 * k3 + kA54 * k4));
        const State k6 =
            dyn.Derivative(y + h * (kA61 * k1 + kA62 * k2 + kA63 * k3 + kA64 * k4 + kA65 * k5));
        const State y1 = y + h * (kB1 * k1 + kB3 * k3 + kB4 * k4 + kB5 * k5 + kB6 * k6);
        const State k7 = dyn.Derivative(y1);
        const State err = h * (kE1 * k1 + kE3 * k3 + kE4 * k4 + kE5 * k5 + kE6 * k6 + kE7 * k7);

        const double ratio = ErrorRatio(err, y, y1, options);
        if (ratio <= 1.0 || h <= kMinStep) {
            t += h;
            y = y1;
            k1 = k7;  // first-same-as-last
            traj.nodes_.push_back({t, y.p, y.v, k1.v});
        }
        const double factor = ratio == 0.0 ? 5.0 : 0.9 * std::pow(ratio, -0.2);
        h = std::max(kMinStep, h * std::clamp(factor, 0.2, 5.0));
    }
    return traj;
}

ZeroResult FindZero(Shot shot, double zero_range_m, double offset_up_m,
                    const SolverOptions& options, double offset_right_m) {
    ZeroResult result;
    shot.windage_rad = 0.0;
    // Miss (up, right) at the zero range for the current angles.
    auto miss = [&](double elevation) -> std::optional<TrajectoryPoint> {
        shot.elevation_rad = elevation;
        return Fly(shot, zero_range_m + 1.0, options).AtSlantRange(zero_range_m);
    };

    for (int pass = 0; pass < 3; ++pass) {
        // Secant on elevation; the first step uses d(drop)/d(elevation) ~ range.
        double e0 = shot.elevation_rad;
        auto p0 = miss(e0);
        if (!p0) {
            return result;
        }
        double f0 = p0->drop_m - offset_up_m;
        bool elevation_ok = std::fabs(f0) < 1e-9;
        double e1 = elevation_ok ? e0 : e0 - f0 / zero_range_m;
        for (int i = 0; i < 30 && !elevation_ok; ++i) {
            ++result.iterations;
            const auto p1 = miss(e1);
            if (!p1) {
                return result;
            }
            const double f1 = p1->drop_m - offset_up_m;
            if (std::fabs(f1) < 1e-9) {
                elevation_ok = true;
                break;
            }
            const double slope = (f1 - f0) / (e1 - e0);
            const double e2 = slope != 0.0 ? e1 - f1 / slope : e1 - f1 / zero_range_m;
            e0 = e1;
            f0 = f1;
            e1 = e2;
        }
        if (!elevation_ok) {
            result.elevation_rad = e1;
            return result;
        }
        shot.elevation_rad = e1;

        // Windage: the horizontal miss is linear in the bore windage.
        const auto p = Fly(shot, zero_range_m + 1.0, options).AtSlantRange(zero_range_m);
        if (!p) {
            return result;
        }
        const double horizontal = p->windage_m - offset_right_m;
        if (std::fabs(horizontal) < 1e-9) {
            result.converged = true;
            result.elevation_rad = shot.elevation_rad;
            result.windage_rad = shot.windage_rad;
            return result;
        }
        shot.windage_rad -= horizontal / zero_range_m;
    }
    result.elevation_rad = shot.elevation_rad;
    result.windage_rad = shot.windage_rad;
    return result;
}

}  // namespace ballistics
