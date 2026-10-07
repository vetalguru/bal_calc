#include <ballistics/effects.h>
#include <ballistics/units.h>

#include <algorithm>
#include <cmath>

namespace ballistics {

double LocalGravity(double latitude_rad, double altitude_m) {
    constexpr double kEquatorGravity = 9.7803253359;
    constexpr double kK = 0.00193185265241;
    constexpr double kE2 = 0.00669437999013;
    constexpr double kFreeAirGradient = 3.086e-6;  // 1/s^2
    const double s2 = std::sin(latitude_rad) * std::sin(latitude_rad);
    const double g0 = kEquatorGravity * (1.0 + kK * s2) / std::sqrt(1.0 - kE2 * s2);
    return g0 - kFreeAirGradient * altitude_m;
}

double MillerStability(double mass_kg, double diameter_m, double length_m, double twist_m,
                       double velocity_mps, double temperature_k, double pressure_pa) {
    if (!(mass_kg > 0.0 && diameter_m > 0.0 && length_m > 0.0 && twist_m != 0.0 &&
          velocity_mps > 0.0 && pressure_pa > 0.0)) {
        return 0.0;
    }
    const double grains = units::KgToGrain(mass_kg);
    const double d_in = units::MToInch(diameter_m);
    const double twist_cal = std::fabs(twist_m) / diameter_m;
    const double len_cal = length_m / diameter_m;
    const double sg =
        30.0 * grains /
        (twist_cal * twist_cal * d_in * d_in * d_in * len_cal * (1.0 + len_cal * len_cal));
    const double fv = std::cbrt(units::MpsToFps(velocity_mps) / 2800.0);
    const double temp_f = units::KToF(temperature_k);
    const double pressure_inhg = pressure_pa / units::kPaPerInHg;
    const double fa = ((temp_f + 460.0) / (59.0 + 460.0)) * (29.92 / pressure_inhg);
    return sg * fv * fa;
}

double LitzSpinDrift(double stability, double time_s, double twist_m) {
    if (stability <= 0.0 || twist_m == 0.0 || time_s <= 0.0) {
        return 0.0;
    }
    const double inches = 1.25 * (stability + 1.2) * std::pow(time_s, 1.83);
    return (twist_m > 0.0 ? 1.0 : -1.0) * units::InchToM(inches);
}

double AerodynamicJump(double stability, double length_m, double diameter_m, double twist_m,
                       double crosswind_from_right_mps) {
    if (stability <= 0.0 || twist_m == 0.0 || diameter_m <= 0.0) {
        return 0.0;
    }
    constexpr double kMpsPerMph = 0.44704;
    const double moa_per_mph = 0.01 * stability - 0.0024 * (length_m / diameter_m) + 0.032;
    const double moa = moa_per_mph * crosswind_from_right_mps / kMpsPerMph;
    // Right-hand twist: wind from the right -> low.
    return -(twist_m > 0.0 ? 1.0 : -1.0) * units::MoaToRad(moa);
}

double MuzzleVelocityAt(const PowderSensitivity& powder, double powder_temperature_k) {
    const auto& t = powder.table;
    if (t.size() >= 2) {
        auto sorted = t;
        std::sort(sorted.begin(), sorted.end());
        std::size_t i = 0;
        while (i + 2 < sorted.size() && powder_temperature_k > sorted[i + 1].first) {
            ++i;
        }
        const auto& [t0, v0] = sorted[i];
        const auto& [t1, v1] = sorted[i + 1];
        if (t1 == t0) {
            return v0;
        }
        return v0 + (v1 - v0) * (powder_temperature_k - t0) / (t1 - t0);
    }
    return powder.reference_velocity_mps *
           (1.0 +
            powder.fraction_per_kelvin * (powder_temperature_k - powder.reference_temperature_k));
}

}  // namespace ballistics
