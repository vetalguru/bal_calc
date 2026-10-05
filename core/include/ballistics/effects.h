#ifndef BALLISTICS_EFFECTS_H
#define BALLISTICS_EFFECTS_H

#include <utility>
#include <vector>

// Secondary effects that the point-mass equations do not produce on their
// own, as empirical corrections (SI units, angles in radians).
namespace ballistics {

// Earth's rotation rate, rad/s.
inline constexpr double kEarthAngularVelocity = 7.2921159e-5;

// Normal gravity on the WGS-84 ellipsoid (Somigliana) at a geodetic
// latitude, reduced to `altitude_m` with the free-air gradient.
double LocalGravity(double latitude_rad, double altitude_m);

// Miller gyroscopic stability factor Sg, corrected for muzzle velocity
// and air density (Miller 2005, as used by Litz/JBM). `twist_m` is the
// barrel twist length (sign ignored). Returns 0 if any input is missing.
double MillerStability(double mass_kg, double diameter_m, double length_m, double twist_m,
                       double velocity_mps, double temperature_k, double pressure_pa);

// Spin drift (yaw of repose) after `time_s` of flight, Litz's empirical
// fit: 1.25 (Sg + 1.2) t^1.83 inches. Positive to the right for a
// right-hand twist (`twist_m` > 0), to the left for a left-hand one.
double LitzSpinDrift(double stability, double time_s, double twist_m);

// Vertical aerodynamic jump caused by a crosswind at the muzzle (Litz):
// (0.01 Sg - 0.0024 L/d + 0.032) MOA per mph. `crosswind_from_right_mps`
// is the crosswind component blowing from the right. With a right-hand
// twist a wind from the right throws the shot low. Returns the elevation
// change of the departure angle, up positive.
double AerodynamicJump(double stability, double length_m, double diameter_m, double twist_m,
                       double crosswind_from_right_mps);

// Muzzle velocity dependence on powder temperature.
struct PowderSensitivity {
    double reference_velocity_mps = 0.0;
    double reference_temperature_k = 288.15;
    // Relative change per kelvin, e.g. 0.001 = 0.1 %/C. Used when `table`
    // has fewer than two points.
    double fraction_per_kelvin = 0.0;
    // Measured (powder temperature K, velocity m/s) pairs; when given,
    // velocity is interpolated (and linearly extrapolated) from them.
    std::vector<std::pair<double, double>> table;
};

double MuzzleVelocityAt(const PowderSensitivity& powder, double powder_temperature_k);

} // namespace ballistics

#endif // BALLISTICS_EFFECTS_H
