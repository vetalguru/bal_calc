#ifndef BALLISTICS_SOLVER_H
#define BALLISTICS_SOLVER_H

#include <optional>
#include <vector>

#include <ballistics/atmosphere.h>
#include <ballistics/drag.h>
#include <ballistics/vec3.h>

namespace ballistics {

inline constexpr double kStandardGravity = 9.80665; // m/s^2

// Everything needed to fly one shot (SI units, angles in radians).
//
// Geometry: the muzzle is the origin. The line of sight (LOS) starts at
// the sight, `sight_height_m` above the bore (perpendicular to the LOS),
// and points `look_angle_rad` above the horizon (uphill positive). The
// bore points `elevation_rad` above and `windage_rad` right of the LOS.
struct Shot {
    DragModel drag;
    double muzzle_velocity_mps = 0.0;
    double mass_kg = 0.0; // optional, only for energy output

    double sight_height_m = 0.0;
    double look_angle_rad = 0.0;
    double elevation_rad = 0.0;
    double windage_rad = 0.0;

    Atmosphere atmosphere;
    SoundSpeedModel sound_speed = SoundSpeedModel::kHumidAir;
    double gravity_mps2 = kStandardGravity;
};

struct SolverOptions {
    double relative_tolerance = 1e-10;
    double position_tolerance_m = 1e-7;
    double velocity_tolerance_mps = 1e-7;
    double min_speed_mps = 30.0;  // stop below this speed
    double max_time_s = 60.0;     // stop after this flight time
    double max_drop_m = 10000.0;  // stop this far below the muzzle
};

enum class StopReason {
    kRangeReached,
    kMinSpeed,
    kMaxTime,
    kMaxDrop,
};

// State of the projectile at one moment, plus its offsets from the LOS.
struct TrajectoryPoint {
    double time_s = 0.0;
    Vec3 position;          // m, shooter frame (see vec3.h)
    Vec3 velocity;          // m/s
    double speed_mps = 0.0;
    double mach = 0.0;
    double energy_j = 0.0;  // 0 when Shot::mass_kg is not set

    double slant_range_m = 0.0; // distance along the LOS
    double drop_m = 0.0;        // offset from the LOS, up positive
    double windage_m = 0.0;     // offset from the LOS, right positive

    // Sight corrections to hit this point: dial/hold up and right are
    // positive, i.e. hold = -offset / slant range.
    double hold_elevation_rad = 0.0;
    double hold_windage_rad = 0.0;
};

// A computed flight with dense output: any moment or LOS distance can be
// sampled to integrator accuracy (quintic Hermite interpolation).
class Trajectory final {
public:
    StopReason stop_reason() const { return stop_reason_; }
    // Farthest LOS distance the flight covered.
    double max_slant_range_m() const;
    std::size_t step_count() const { return nodes_.empty() ? 0 : nodes_.size() - 1; }

    // Point where the projectile crosses the plane perpendicular to the
    // LOS at `slant_range_m`; nullopt if the flight ended before it.
    std::optional<TrajectoryPoint> AtSlantRange(double slant_range_m) const;
    std::optional<TrajectoryPoint> AtTime(double time_s) const;

    // Points every `step_m` from `step_m` (or 0 with `include_zero`) up to
    // `max_m` inclusive, stopping early where the flight ended.
    std::vector<TrajectoryPoint> Table(double step_m, double max_m, bool include_zero = true) const;

    struct Node {
        double t;
        Vec3 p, v, a;
    };

private:
    friend Trajectory Fly(const Shot&, double, const SolverOptions&);

    TrajectoryPoint Interpolate(std::size_t segment, double t) const;
    TrajectoryPoint MakePoint(double t, const Vec3& p, const Vec3& v) const;
    double SlantRange(const Vec3& p) const;

    std::vector<Node> nodes_;
    StopReason stop_reason_ = StopReason::kRangeReached;
    Vec3 sight_;     // sight position
    Vec3 los_;       // unit vector along the LOS
    Vec3 los_up_;    // unit vector perpendicular to LOS, in the vertical plane
    double mass_kg_ = 0.0;
    AtmosphereModel air_{Atmosphere{}};
};

// Integrates the shot until the projectile passes `max_slant_range_m`
// along the LOS (or a stop condition in `options` triggers).
Trajectory Fly(const Shot& shot, double max_slant_range_m, const SolverOptions& options = {});

struct ZeroResult {
    bool converged = false;
    double elevation_rad = 0.0; // bore elevation relative to the LOS
    int iterations = 0;
};

// Finds the bore elevation (relative to the LOS) that puts the impact on
// the LOS at `zero_range_m`, offset by `offset_up_m` (positive = impact
// above the aim point). The shot's own elevation is ignored.
ZeroResult FindZero(Shot shot, double zero_range_m, double offset_up_m = 0.0,
                    const SolverOptions& options = {});

} // namespace ballistics

#endif // BALLISTICS_SOLVER_H
