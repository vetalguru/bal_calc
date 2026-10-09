#ifndef BALLISTICS_SOLVER_H
#define BALLISTICS_SOLVER_H

#include <ballistics/atmosphere.h>
#include <ballistics/drag.h>
#include <ballistics/vec3.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace ballistics {

inline constexpr double kStandardGravity = 9.80665;  // m/s^2

// Wind over a stretch of the range. Zones apply in order of
// `until_range_m` (horizontal distance from the muzzle); the last zone
// extends to infinity.
struct WindZone {
    double until_range_m = 0.0;
    double speed_mps = 0.0;
    // Direction the wind blows FROM, clockwise from the line of fire:
    // 0 = from the target (headwind, 12 o'clock), pi/2 = from the right
    // (3 o'clock), pi = from behind, 3pi/2 = from the left.
    double from_rad = 0.0;
    double vertical_mps = 0.0;  // updraft positive
};

// Everything needed to fly one shot (SI units, angles in radians).
//
// Geometry: the muzzle is the origin. The line of sight (LOS) starts at
// the sight, `sight_height_m` above the bore in the rifle's own "up"
// (perpendicular to the LOS, rotated by `cant_rad`), and points
// `look_angle_rad` above the horizon (uphill positive). The bore points
// `elevation_rad` above and `windage_rad` right of the LOS, both in the
// rifle's frame, so a canted rifle turns elevation partly into windage.
struct Shot {
    DragModel drag;
    double muzzle_velocity_mps = 0.0;
    double mass_kg = 0.0;            // energy output, spin effects
    double bullet_diameter_m = 0.0;  // spin effects
    double bullet_length_m = 0.0;    // spin effects
    // Barrel twist length, right-hand positive, left-hand negative,
    // 0 = no spin effects.
    double twist_m = 0.0;

    double sight_height_m = 0.0;
    double look_angle_rad = 0.0;
    double elevation_rad = 0.0;
    double windage_rad = 0.0;
    double cant_rad = 0.0;  // clockwise (top to the right) positive

    Atmosphere atmosphere;
    SoundSpeedModel sound_speed = SoundSpeedModel::kHumidAir;
    double gravity_mps2 = kStandardGravity;
    std::vector<WindZone> winds;

    // Earth rotation (Coriolis/Eotvos), off without a latitude. Without an
    // azimuth only the horizontal (latitude) component is applied.
    std::optional<double> latitude_rad;
    std::optional<double> azimuth_rad;  // bearing of the LOS, clockwise from north

    bool spin_drift = true;        // needs twist, diameter, length, mass
    bool aerodynamic_jump = true;  // likewise
};

struct SolverOptions {
    double relative_tolerance = 1e-10;
    double position_tolerance_m = 1e-7;
    double velocity_tolerance_mps = 1e-7;
    double min_speed_mps = 30.0;  // stop below this speed
    double max_time_s = 60.0;     // stop after this flight time
    double max_drop_m = 10000.0;  // stop this far below the muzzle
};

enum class StopReason : std::uint8_t {
    kRangeReached,
    kMinSpeed,
    kMaxTime,
    kMaxDrop,
};

// State of the projectile at one moment, plus its offsets from the LOS.
struct TrajectoryPoint {
    double time_s = 0.0;
    Vec3 position;  // m, shooter frame (see vec3.h)
    Vec3 velocity;  // m/s
    double speed_mps = 0.0;
    double mach = 0.0;
    double energy_j = 0.0;  // 0 when Shot::mass_kg is not set

    double slant_range_m = 0.0;  // distance along the LOS
    double drop_m = 0.0;         // offset from the LOS, up positive
    double windage_m = 0.0;      // offset from the LOS, right positive (incl. spin drift)
    double spin_drift_m = 0.0;   // the spin-drift part of windage_m

    // Sight corrections to hit this point: dial/hold up and right are
    // positive, i.e. hold = -offset / slant range.
    double hold_elevation_rad = 0.0;
    double hold_windage_rad = 0.0;
};

// A computed flight with dense output: any moment or LOS distance can be
// sampled to integrator accuracy (quintic Hermite interpolation).
class Trajectory final {
   public:
    [[nodiscard]] StopReason stop_reason() const { return stop_reason_; }
    // Gyroscopic stability at the muzzle (Miller), 0 if not computed.
    [[nodiscard]] double stability() const { return stability_; }
    // Vertical aerodynamic jump applied at the muzzle, rad.
    [[nodiscard]] double aerodynamic_jump_rad() const { return jump_rad_; }
    // Farthest LOS distance the flight covered.
    [[nodiscard]] double max_slant_range_m() const;
    [[nodiscard]] std::size_t step_count() const { return nodes_.empty() ? 0 : nodes_.size() - 1; }

    // Point where the projectile crosses the plane perpendicular to the
    // LOS at `slant_range_m`; nullopt if the flight ended before it.
    [[nodiscard]] std::optional<TrajectoryPoint> AtSlantRange(double slant_range_m) const;
    [[nodiscard]] std::optional<TrajectoryPoint> AtTime(double time_s) const;

    // Points every `step_m` from `step_m` (or 0 with `include_zero`) up to
    // `max_m` inclusive, stopping early where the flight ended.
    [[nodiscard]] std::vector<TrajectoryPoint> Table(double step_m, double max_m,
                                                     bool include_zero = true) const;

    struct Node {
        double t;
        Vec3 p, v, a;
    };

   private:
    friend Trajectory Fly(const Shot&, double, const SolverOptions&);

    [[nodiscard]] TrajectoryPoint Interpolate(std::size_t segment, double t) const;
    [[nodiscard]] TrajectoryPoint MakePoint(double t, const Vec3& p, const Vec3& v) const;
    [[nodiscard]] double SlantRange(const Vec3& p) const;

    std::vector<Node> nodes_;
    StopReason stop_reason_ = StopReason::kRangeReached;
    Vec3 sight_;   // sight position
    Vec3 los_;     // unit vector along the LOS
    Vec3 los_up_;  // unit vector perpendicular to LOS, in the vertical plane
    double mass_kg_ = 0.0;
    double stability_ = 0.0;
    double twist_m_ = 0.0;
    bool spin_drift_ = false;
    double jump_rad_ = 0.0;
    AtmosphereModel air_{Atmosphere{}};
};

// Integrates the shot until the projectile passes `max_slant_range_m`
// along the LOS (or a stop condition in `options` triggers).
Trajectory Fly(const Shot& shot, double max_slant_range_m, const SolverOptions& options = {});

struct ZeroResult {
    bool converged = false;
    double elevation_rad = 0.0;  // bore elevation relative to the LOS
    double windage_rad = 0.0;    // bore windage relative to the LOS
    int iterations = 0;
};

// Finds the bore elevation and windage (relative to the LOS) that put the
// impact `offset_up_m` above and `offset_right_m` right of the aim point
// at `zero_range_m`. Horizontal effects present at the zero range (spin
// drift, wind, Coriolis) are absorbed into the windage, as when zeroing a
// real rifle. The shot's own elevation and windage are ignored.
ZeroResult FindZero(Shot shot, double zero_range_m, double offset_up_m = 0.0,
                    const SolverOptions& options = {}, double offset_right_m = 0.0);

}  // namespace ballistics

#endif  // BALLISTICS_SOLVER_H
