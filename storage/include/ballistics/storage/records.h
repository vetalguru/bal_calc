#ifndef BALLISTICS_STORAGE_RECORDS_H
#define BALLISTICS_STORAGE_RECORDS_H

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <ballistics/drag.h>
#include <ballistics/solver.h>

// Rows of the application database (see src/schema.cpp). Quantities are
// SI; `id` 0 means "not stored yet". Child rows (BC bands, curve points,
// wind zones, velocity points) live inside their parent record and are
// saved and loaded with it.
namespace ballistics::storage {

using Id = std::int64_t;

struct DragCurveRecord {
    Id id = 0;
    std::string name;
    std::string source;
    std::string notes;
    std::vector<DragPoint> points;
};

// How a bullet's drag is described.
inline constexpr const char* kDragKindBc = "bc";            // one BC vs drag_table
inline constexpr const char* kDragKindMultiBc = "multi_bc"; // BC bands vs drag_table
inline constexpr const char* kDragKindCurve = "curve";      // own Cd(M) curve

struct BulletRecord {
    Id id = 0;
    std::string name;
    std::string manufacturer;
    std::string caliber;
    double diameter_m = 0.0;
    double mass_kg = 0.0;
    double length_m = 0.0;
    std::string drag_kind = kDragKindBc;
    std::string drag_table = "G7"; // DragTableName()
    std::optional<double> bc;      // lb/in^2, for kDragKindBc
    std::optional<Id> curve_id;    // for kDragKindCurve
    double form_factor = 1.0;
    std::string source;
    std::string notes;
    std::vector<BcPoint> bc_bands; // for kDragKindMultiBc
};

struct CartridgeRecord {
    Id id = 0;
    std::string name;
    std::string caliber; // e.g. ".308 Win"; matched against the rifle's
    Id bullet_id = 0;
    double muzzle_velocity_mps = 0.0;
    double reference_powder_temp_k = 288.15;
    double powder_sensitivity_per_k = 0.0; // relative, e.g. 0.001 = 0.1 %/K
    double barrel_length_m = 0.0;          // barrel the velocity was measured with
    std::string source;
    std::string notes;
    // Measured (powder temperature K, velocity m/s); overrides the coefficient.
    std::vector<std::pair<double, double>> velocity_points;
};

struct RifleRecord {
    Id id = 0;
    std::string name;
    std::string caliber;
    double barrel_length_m = 0.0;
    double twist_m = 0.0; // right-hand positive, left-hand negative, 0 unknown
    double sight_height_m = 0.0;
    std::string notes;
    // The rifle's scope and zero (the cartridge it is fired with may shift
    // the point of impact, see ProfileRecord).
    std::optional<Id> scope_id;
    double zero_range_m = 100.0;
    Atmosphere zero_atmosphere = StandardAtmosphere(0.0);
    double zero_powder_temp_k = 288.15;
};

struct ReticleRecord {
    Id id = 0;
    std::string name;
    std::string units = "mrad";      // "mrad" | "moa"
    std::string focal_plane = "ffp"; // "ffp" | "sfp"
    double reference_magnification = 0.0; // SFP: magnification the marks are true at
    std::string definition;               // drawing (JSON), see the reticle module
    std::string source;
};

struct ScopeRecord {
    Id id = 0;
    std::string name;
    std::string click_units = "mrad"; // "mrad" | "moa" | "smoa" | "cm100m"
    double click_vertical_rad = 0.0;
    double click_horizontal_rad = 0.0;
    std::optional<Id> reticle_id;
    double min_magnification = 0.0;
    double max_magnification = 0.0;
    std::string notes;
    // Second focal plane reticles subtend their nominal values only at
    // `sfp_reference_magnification` (usually the maximum).
    std::string focal_plane = "ffp"; // "ffp" | "sfp"
    double sfp_reference_magnification = 0.0;
};

// A rifle + cartridge pair: what a solution is computed for. Holds what
// depends on both - the point-of-impact shift of this cartridge relative to
// the rifle's zero, truing and (in dope_log) the shot log.
struct ProfileRecord {
    Id id = 0;
    std::string name;
    Id rifle_id = 0;
    Id cartridge_id = 0;
    double zero_offset_up_m = 0.0;
    double zero_offset_right_m = 0.0;
    double velocity_scale = 1.0; // truing
    double drag_scale = 1.0;     // truing
    std::vector<DsfPoint> dsf;   // truing of the transonic part, by Mach (empty = none)
    std::string created_at;      // set by the database
    std::optional<std::string> last_used_at;
};

struct ConditionsRecord {
    Id id = 0;
    std::string name;
    Atmosphere atmosphere = StandardAtmosphere(0.0);
    std::optional<double> powder_temp_k; // default: air temperature
    std::optional<double> latitude_rad;
    std::optional<double> azimuth_rad;
    double look_angle_rad = 0.0;
    double cant_rad = 0.0;
    std::string created_at; // set by the database
    std::vector<WindZone> winds;
};

// One observed shot: the correction that actually hit at `range_m`.
struct DopeRecord {
    Id id = 0;
    Id profile_id = 0;
    std::string shot_at; // set by the database when empty
    double range_m = 0.0;
    double observed_elevation_rad = 0.0;
    std::optional<double> observed_windage_rad;
    std::optional<double> predicted_elevation_rad;
    std::optional<double> predicted_windage_rad;
    Atmosphere atmosphere = StandardAtmosphere(0.0);
    std::optional<double> powder_temp_k;
    double look_angle_rad = 0.0;
    bool use_for_truing = true;
    std::string notes;
};

} // namespace ballistics::storage

#endif // BALLISTICS_STORAGE_RECORDS_H
