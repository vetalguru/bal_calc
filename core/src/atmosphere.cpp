#include <ballistics/atmosphere.h>

#include <algorithm>
#include <cmath>

namespace ballistics {

namespace {

constexpr double kGasConstant = 8.314472;       // J/(mol K), CIPM-2007
constexpr double kMolarMassDryAir = 28.96546e-3; // kg/mol, CIPM-2007 (400 ppm CO2)
constexpr double kMolarMassWater = 18.01528e-3;  // kg/mol
constexpr double kZeroCelsius = 273.15;

// ICAO standard atmosphere (troposphere).
constexpr double kSeaLevelTemperature = 288.15; // K
constexpr double kSeaLevelPressure = 101325.0;  // Pa
constexpr double kLapseRate = -0.0065;          // K/m
constexpr double kPressureExponent = 5.255876;  // g0 M / (R |L|)
constexpr double kLowestTemperature = 183.0;    // K, model floor (-90 C)

// Isobaric heat capacities in units of R (ideal gas): dry air is
// diatomic-dominated (7/2), water vapour ~4.0 over the ambient range.
constexpr double kCpDryAirOverR = 3.5;
constexpr double kCpWaterOverR = 4.0;

double SaturationVaporPressure(double t_k) {
    constexpr double kA = 1.2378847e-5;
    constexpr double kB = -1.9121316e-2;
    constexpr double kC = 33.93711047;
    constexpr double kD = -6.3431645e3;
    return std::exp(kA * t_k * t_k + kB * t_k + kC + kD / t_k);
}

double EnhancementFactor(double p_pa, double t_c) {
    return 1.00062 + 3.14e-8 * p_pa + 5.6e-7 * t_c * t_c;
}

double Compressibility(double p_pa, double t_k, double xv) {
    const double t = t_k - kZeroCelsius;
    constexpr double kA0 = 1.58123e-6;
    constexpr double kA1 = -2.9331e-8;
    constexpr double kA2 = 1.1043e-10;
    constexpr double kB0 = 5.707e-6;
    constexpr double kB1 = -2.051e-8;
    constexpr double kC0 = 1.9898e-4;
    constexpr double kC1 = -2.376e-6;
    constexpr double kD = 1.83e-11;
    constexpr double kE = -0.765e-8;
    const double pt = p_pa / t_k;
    return 1.0 - pt * (kA0 + kA1 * t + kA2 * t * t + (kB0 + kB1 * t) * xv + (kC0 + kC1 * t) * xv * xv) +
           pt * pt * (kD + kE * xv * xv);
}

} // namespace

Atmosphere StandardAtmosphere(double altitude_m) {
    Atmosphere a;
    a.altitude_m = altitude_m;
    a.temperature_k = kSeaLevelTemperature + kLapseRate * altitude_m;
    a.pressure_pa = StationPressureFromSeaLevel(kSeaLevelPressure, altitude_m);
    a.humidity = 0.0;
    return a;
}

double StationPressureFromSeaLevel(double qnh_pa, double altitude_m) {
    return qnh_pa *
           std::pow(1.0 + kLapseRate * altitude_m / kSeaLevelTemperature, kPressureExponent);
}

double WaterVaporMoleFraction(double temperature_k, double pressure_pa, double humidity) {
    const double rh = std::clamp(humidity, 0.0, 1.0);
    if (rh == 0.0 || pressure_pa <= 0.0) {
        return 0.0;
    }
    const double f = EnhancementFactor(pressure_pa, temperature_k - kZeroCelsius);
    return std::min(rh * f * SaturationVaporPressure(temperature_k) / pressure_pa, 1.0);
}

double AirDensityFromMoleFraction(double temperature_k, double pressure_pa, double xv) {
    const double z = Compressibility(pressure_pa, temperature_k, xv);
    return pressure_pa * kMolarMassDryAir / (z * kGasConstant * temperature_k) *
           (1.0 - xv * (1.0 - kMolarMassWater / kMolarMassDryAir));
}

double SpeedOfSoundFromMoleFraction(double temperature_k, double xv, SoundSpeedModel model) {
    if (model == SoundSpeedModel::kDryAir) {
        return 20.0467 * std::sqrt(temperature_k);
    }
    const double molar_mass = kMolarMassDryAir * (1.0 - xv) + kMolarMassWater * xv;
    const double cp = kCpDryAirOverR * (1.0 - xv) + kCpWaterOverR * xv;
    const double gamma = cp / (cp - 1.0);
    return std::sqrt(gamma * kGasConstant * temperature_k / molar_mass);
}

double AirDensity(double temperature_k, double pressure_pa, double humidity) {
    return AirDensityFromMoleFraction(
        temperature_k, pressure_pa, WaterVaporMoleFraction(temperature_k, pressure_pa, humidity));
}

double SpeedOfSound(double temperature_k, double pressure_pa, double humidity,
                    SoundSpeedModel model) {
    return SpeedOfSoundFromMoleFraction(
        temperature_k, WaterVaporMoleFraction(temperature_k, pressure_pa, humidity), model);
}

AtmosphereModel::AtmosphereModel(const Atmosphere& base, SoundSpeedModel sound)
    : base_(base),
      sound_(sound),
      base_vapor_(WaterVaporMoleFraction(base.temperature_k, base.pressure_pa, base.humidity)) {
    base_state_.density_kg_m3 =
        AirDensityFromMoleFraction(base.temperature_k, base.pressure_pa, base_vapor_);
    base_state_.speed_of_sound_mps =
        SpeedOfSoundFromMoleFraction(base.temperature_k, base_vapor_, sound);
}

AirState AtmosphereModel::At(double altitude_m) const {
    const double dh = altitude_m - base_.altitude_m;
    if (dh == 0.0) {
        return base_state_;
    }
    const double t = std::max(base_.temperature_k + kLapseRate * dh, kLowestTemperature);
    const double p = base_.pressure_pa * std::pow(t / base_.temperature_k, kPressureExponent);
    // The vapour content of the air mass is kept (not its relative
    // humidity), capped at saturation where the air is cooler.
    const double xv = std::min(base_vapor_, WaterVaporMoleFraction(t, p, 1.0));
    AirState s;
    s.density_kg_m3 = AirDensityFromMoleFraction(t, p, xv);
    s.speed_of_sound_mps = SpeedOfSoundFromMoleFraction(t, xv, sound_);
    return s;
}

} // namespace ballistics
