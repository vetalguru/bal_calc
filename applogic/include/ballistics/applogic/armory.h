#ifndef BALLISTICS_APPLOGIC_ARMORY_H
#define BALLISTICS_APPLOGIC_ARMORY_H

#include <ballistics/storage/database.h>
#include <ballistics/storage/records.h>

#include <string>
#include <vector>

// The armory screens: rifles (with their scope and zero) and cartridges
// (with their bullet) as two independent lists, edited in the units shooters
// type (grains, inches, cm, C, hPa). A solution is computed for a rifle +
// cartridge pair (storage::ProfileRecord), created on first use.
namespace ballistics::applogic {

using storage::Id;
using storage::Result;
using storage::Status;

// Scope adjustment units.
inline constexpr const char* kClickMrad = "mrad";      // value in MRAD, e.g. 0.1
inline constexpr const char* kClickMoa = "moa";        // true MOA, e.g. 0.25
inline constexpr const char* kClickSmoa = "smoa";      // inch per 100 yd (shooter's MOA)
inline constexpr const char* kClickCm100m = "cm100m";  // cm per 100 m

// Angle of one click, rad; 0 for unknown units or non-positive values.
double ClickToRad(const std::string& units, double value);
double RadToClick(const std::string& units, double rad);

// Same calibre for sorting: compares the leading number (".308 Win" and
// "308 Winchester" match, "6.5 Creedmoor" and "6.5x47" match).
bool SameCaliber(const std::string& a, const std::string& b);

// ---- Rifles ---------------------------------------------------------------

struct RifleForm {
    Id rifle_id = 0;  // 0 = new rifle

    std::string name;
    std::string caliber;
    double sight_height_cm = 5.0;
    double twist_in = 10.0;  // 0 = unknown (no spin effects)
    bool twist_left = false;
    // Barrel length: factory velocities are recalculated for it (0 = unknown).
    double barrel_in = 0.0;
    // Description; empty and 0 are "not stated".
    std::string maker;
    std::string model;
    int rifling_grooves = 0;
    std::string rifling_type;   // "" | "conventional" | "polygonal" | "5r" | "other"
    std::string muzzle_device;  // "" | "none" | "brake" | "compensator" | "flash_hider"
    // Where the device moves the point of impact, up and right positive, in
    // its units; it counts when the device is on now but was off at the
    // zero, or the other way round.
    double muzzle_poi_up = 0.0;
    double muzzle_poi_right = 0.0;
    std::string muzzle_poi_units = kClickMoa;  // "moa" | "mrad"
    bool muzzle_device_on = true;
    bool zeroed_with_device = false;
    // The suppressor, on its own (it may go over the muzzle device), with
    // the same kind of shift.
    bool has_suppressor = false;
    std::string suppressor_name;
    double suppressor_poi_up = 0.0;
    double suppressor_poi_right = 0.0;
    std::string suppressor_poi_units = kClickMoa;  // "moa" | "mrad"
    bool suppressor_on = true;
    bool zeroed_with_suppressor = false;

    // Mount: the rail (base) slope in its own units, and the rings.
    double rail_slope = 0.0;
    std::string rail_slope_units = kClickMoa;  // "moa" | "mrad"
    double ring_height_mm = 0.0;

    // Scope
    std::string click_units = kClickMrad;
    double click_value = 0.1;             // vertical
    double click_value_horizontal = 0.0;  // 0 = as the vertical one
    // Each turret's full travel, in the click units (0 = not stated).
    double elevation_travel = 0.0;
    double windage_travel = 0.0;
    std::string scope_maker;
    std::string scope_model;
    double tube_diameter_mm = 0.0;
    double objective_mm = 0.0;
    Id reticle_id = 0;                // 0 = none
    std::string focal_plane = "ffp";  // "ffp" | "sfp"
    double sfp_reference_magnification = 0.0;
    double min_magnification = 0.0;
    double max_magnification = 0.0;

    // Zero
    double zero_range_m = 100.0;
    double zero_temperature_c = 15.0;
    double zero_pressure_hpa = 1013.25;  // station pressure
    double zero_altitude_m = 0.0;
    double zero_humidity_pct = 50.0;
    double zero_powder_c = 15.0;
};

struct RifleSummary {
    Id id = 0;
    std::string name;
    std::string caliber;
};

// Problems a user must fix before saving, as a readable sentence; empty if
// the form is complete.
std::string Validate(const RifleForm& form);
Result<std::vector<RifleSummary>> ListRifles(storage::Database& db);
Result<RifleForm> LoadRifleForm(storage::Database& db, Id rifle_id);
// Creates or updates the rifle and its scope. Returns the rifle id.
Result<Id> SaveRifleForm(storage::Database& db, const RifleForm& form);
// Deletes the rifle, its scope and its pairs (with their shot logs).
Status DeleteRifle(storage::Database& db, Id rifle_id);

// ---- Cartridges -----------------------------------------------------------

struct CartridgeForm {
    Id cartridge_id = 0;  // 0 = new cartridge
    // A new cartridge copied from this (library) cartridge keeps what the
    // form does not show: measured velocities, notes.
    Id copy_of = 0;

    std::string name;
    std::string caliber;

    // Bullet. With `library_bullet_id` set the cartridge uses that library
    // bullet as is (the fields below only display it); with 0 the fields
    // describe the cartridge's own bullet.
    Id library_bullet_id = 0;
    std::string bullet_name;
    std::string drag_table = "G7";
    double bc = 0.0;
    double mass_gr = 0.0;
    double diameter_in = 0.0;
    double length_in = 0.0;

    double muzzle_velocity_mps = 0.0;
    // The barrel that velocity was measured in (a maker's test barrel); the
    // calculation recalculates it for the rifle's barrel. 0 = this rifle's.
    double barrel_in = 0.0;
    double powder_reference_c = 15.0;
    double powder_sensitivity_pct_per_c = 0.0;  // % of V0 per C
};

struct CartridgeSummary {
    Id id = 0;
    std::string name;
    std::string caliber;
    std::string bullet_name;
    double muzzle_velocity_mps = 0.0;
    double barrel_in = 0.0;  // the velocity's test barrel, 0 = not stated
};

std::string Validate(const CartridgeForm& form);
// The user's cartridges; with `caliber` set those of that calibre first.
Result<std::vector<CartridgeSummary>> ListCartridges(storage::Database& db,
                                                     const std::string& caliber = "");
// Factory loads from the starter library and imported .ammo files,
// filtered by a case-insensitive substring of name or calibre.
Result<std::vector<CartridgeSummary>> ListLibraryCartridges(storage::Database& db,
                                                            const std::string& filter = "");
// A new cartridge form copied from a library cartridge.
Result<CartridgeForm> CartridgeFromLibrary(storage::Database& db, Id library_cartridge_id);
// The form with a library bullet chosen (fields filled for display).
Result<CartridgeForm> WithLibraryBullet(storage::Database& db, CartridgeForm form, Id bullet_id);
Result<CartridgeForm> LoadCartridgeForm(storage::Database& db, Id cartridge_id);
// Creates or updates the cartridge and its own bullet. Returns its id.
Result<Id> SaveCartridgeForm(storage::Database& db, const CartridgeForm& form);
// Deletes the cartridge, its own bullet and its pairs (with shot logs).
Status DeleteCartridge(storage::Database& db, Id cartridge_id);

// ---- Pairs ----------------------------------------------------------------

// The pair's profile id, created (no offset, no truing) on first use.
Result<Id> EnsureProfile(storage::Database& db, Id rifle_id, Id cartridge_id);
// Where this cartridge hits relative to the rifle's zero, at the zero range.
Status SetZeroOffset(storage::Database& db, Id profile_id, double up_cm, double right_cm);

// A ready-to-try rifle and cartridge: .308 Win, 1:10 twist, 0.1 MRAD scope,
// 100 m zero; Sierra MatchKing 175 gr from the library when it is there
// (an own copy otherwise). Returns the pair's profile id.
Result<Id> CreateSampleProfile(storage::Database& db, const std::string& rifle_name,
                               const std::string& cartridge_name);

}  // namespace ballistics::applogic

#endif  // BALLISTICS_APPLOGIC_ARMORY_H
