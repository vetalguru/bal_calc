#ifndef BALLISTICS_WEZ_H
#define BALLISTICS_WEZ_H

#include <ballistics/solver.h>

#include <cstdint>
#include <string>
#include <vector>

// Weapon employment zone: how likely a shot is to hit, given how well the
// inputs are known. Linearised: every error source shifts the point of
// impact in proportion to its size, the shifts add up as independent
// normal errors (with the rifle's own dispersion) into an ellipse, and the
// probability is the share of that ellipse inside the target.

namespace ballistics {

// One standard deviation (1 sigma) of each input; 0 = known exactly.
struct ErrorSources {
    double range_m = 0.0;             // distance to the target
    double wind_speed_mps = 0.0;      // crosswind not accounted for
    double wind_direction_rad = 0.0;  // of the wind that was entered
    double muzzle_velocity_mps = 0.0;
    double drag_fraction = 0.0;  // BC: 0.02 = 2 %
    double temperature_k = 0.0;
    double pressure_pa = 0.0;
    double humidity = 0.0;  // 0..1
    double look_angle_rad = 0.0;
    double cant_rad = 0.0;
    double azimuth_rad = 0.0;   // with Coriolis on
    double latitude_rad = 0.0;  // with Coriolis on
    // The rifle and shooter: 1 sigma per axis, as an angle.
    double dispersion_rad = 0.0;
};

// A 5-shot group's extreme spread is about 3.07 sigma (per axis, circular
// normal); this turns a group size into ErrorSources::dispersion_rad.
inline constexpr double kGroupSpreadPerSigma = 3.07;

// Where shots land around the aim point at one range (m, up and right).
struct Spread {
    double sigma_up_m = 0.0;
    double sigma_right_m = 0.0;
    double correlation = 0.0;  // of up and right
    struct Part {
        std::string source;  // "range", "windSpeed", ... (ErrorSources fields)
        double up_m = 0.0;
        double right_m = 0.0;
    };
    std::vector<Part> parts;  // 1 sigma of each source, largest first
};

// The spreads of a shot along its range. `shot` is the shot as fired
// (bore angles set, wind, Earth rotation); its flights with each error at
// +-1 sigma are computed once, up to `max_slant_range_m`.
class WezModel final {
   public:
    WezModel(const Shot& shot, const ErrorSources& errors, double max_slant_range_m);

    // nullopt-like: ok() false where the nominal flight did not get there.
    Spread At(double slant_range_m, bool* ok = nullptr) const;

   private:
    struct Perturbed {
        std::string source;
        Trajectory plus;
        Trajectory minus;
    };
    Trajectory nominal_;
    std::vector<Perturbed> perturbed_;
    ErrorSources errors_;
};

// A target as seen from the shooter, centred on the aim point (m).
struct Target {
    enum class Kind : std::uint8_t {
        kRectangle,  // width x height
        kEllipse,    // width x height (a circle when equal)
        kFigure,     // a chest-and-head silhouette width x height (see .cpp)
    };
    Kind kind = Kind::kRectangle;
    double width_m = 0.5;
    double height_m = 0.5;
};

// Probability (0..1) that a shot with this spread, aimed at the centre,
// lands inside the target.
double HitProbability(const Spread& spread, const Target& target);

// Shots needed to hit at least once with probability `confidence`, each hit
// with `p` (independent); 0 when p is 0.
int ShotsToHit(double p, double confidence);

}  // namespace ballistics

#endif  // BALLISTICS_WEZ_H
