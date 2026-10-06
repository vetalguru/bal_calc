#ifndef BALLISTICS_ATMOSPHERE_H
#define BALLISTICS_ATMOSPHERE_H

namespace ballistics {

// Measured air at the firing point (SI).
struct Atmosphere {
    double altitude_m = 0.0;         // above mean sea level
    double pressure_pa = 101325.0;   // absolute (station) pressure, not QNH
    double temperature_k = 288.15;
    double humidity = 0.0;           // relative humidity, 0..1
};

// How the speed of sound is computed from the air state.
enum class SoundSpeedModel {
    // Ideal-gas mixture of dry air and water vapour: sqrt(gamma R T / M)
    // with humidity-dependent gamma and molar mass. Default.
    kHumidAir,
    // Dry air only, c = 20.0467 * sqrt(T) (the classic ballistic formula).
    kDryAir,
};

// Local air properties the drag model needs.
struct AirState {
    double density_kg_m3 = 0.0;
    double speed_of_sound_mps = 0.0;
};

// ICAO standard atmosphere at a geometric altitude (troposphere, < 11 km).
Atmosphere StandardAtmosphere(double altitude_m);

// Station (absolute) pressure at `altitude_m` from a sea-level-reduced
// pressure (QNH), per the ICAO standard lapse.
double StationPressureFromSeaLevel(double qnh_pa, double altitude_m);

// Moist-air density by the CIPM-2007 equation (Picard et al., Metrologia
// 45, 2008), for temperature in K, pressure in Pa and humidity 0..1.
double AirDensity(double temperature_k, double pressure_pa, double humidity);

// Mole fraction of water vapour in moist air (CIPM-2007 saturation vapour
// pressure and enhancement factor).
double WaterVaporMoleFraction(double temperature_k, double pressure_pa, double humidity);

double SpeedOfSound(double temperature_k, double pressure_pa, double humidity,
                    SoundSpeedModel model = SoundSpeedModel::kHumidAir);

// The same, from the water-vapour mole fraction instead of humidity.
double AirDensityFromMoleFraction(double temperature_k, double pressure_pa, double xv);
double SpeedOfSoundFromMoleFraction(double temperature_k, double xv,
                                    SoundSpeedModel model = SoundSpeedModel::kHumidAir);

// Density altitude: the ICAO standard-atmosphere altitude where dry air
// has the density of `air` (moist air, CIPM-2007).
double DensityAltitude(const Atmosphere& air);

// Station pressure that gives air at `temperature_k` and `humidity` the
// density of the standard atmosphere at `density_altitude_m`.
double StationPressureFromDensityAltitude(double density_altitude_m, double temperature_k,
                                          double humidity);

// Air along the trajectory: the firing-point atmosphere, extrapolated to
// other altitudes with the standard lapse rate (-6.5 K/km), the barometric
// formula and the water-vapour content of the firing-point air (capped at
// saturation).
class AtmosphereModel final {
public:
    explicit AtmosphereModel(const Atmosphere& base,
                             SoundSpeedModel sound = SoundSpeedModel::kHumidAir);

    const Atmosphere& base() const { return base_; }

    // Air at the firing point.
    const AirState& AtBase() const { return base_state_; }

    // Air at `altitude_m` above mean sea level.
    AirState At(double altitude_m) const;

private:
    Atmosphere base_;
    SoundSpeedModel sound_;
    double base_vapor_; // water vapour mole fraction at the firing point
    AirState base_state_;
};

} // namespace ballistics

#endif // BALLISTICS_ATMOSPHERE_H
