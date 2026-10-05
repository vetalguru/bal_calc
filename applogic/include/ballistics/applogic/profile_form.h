#ifndef BALLISTICS_APPLOGIC_PROFILE_FORM_H
#define BALLISTICS_APPLOGIC_PROFILE_FORM_H

#include <string>

#include <ballistics/storage/database.h>
#include <ballistics/storage/records.h>

// The "profile" screen edits one rifle + scope + cartridge + bullet as a
// single form in the units shooters type (grains, inches, cm, C, hPa).
namespace ballistics::applogic {

using storage::Id;
using storage::Result;
using storage::Status;

// Scope adjustment units.
inline constexpr const char* kClickMrad = "mrad";     // value in MRAD, e.g. 0.1
inline constexpr const char* kClickMoa = "moa";       // true MOA, e.g. 0.25
inline constexpr const char* kClickSmoa = "smoa";     // inch per 100 yd (shooter's MOA)
inline constexpr const char* kClickCm100m = "cm100m"; // cm per 100 m

// Angle of one click, rad; 0 for unknown units or non-positive values.
double ClickToRad(const std::string& units, double value);
double RadToClick(const std::string& units, double rad);

struct ProfileForm {
    Id profile_id = 0; // 0 = new profile

    std::string name;
    std::string caliber;

    // Rifle
    double sight_height_cm = 5.0;
    double twist_in = 10.0; // 0 = unknown (no spin effects)
    bool twist_left = false;

    // Scope
    std::string click_units = kClickMrad;
    double click_value = 0.1;
    Id reticle_id = 0;              // 0 = none
    std::string focal_plane = "ffp"; // "ffp" | "sfp"
    double sfp_reference_magnification = 0.0;
    double min_magnification = 0.0;
    double max_magnification = 0.0;

    // Bullet. With `library_bullet_id` set the profile uses that library
    // bullet as is (the fields below only display it); with 0 the fields
    // describe the profile's own bullet.
    Id library_bullet_id = 0;
    std::string bullet_name;
    std::string drag_table = "G7";
    double bc = 0.0;
    double mass_gr = 0.0;
    double diameter_in = 0.0;
    double length_in = 0.0;

    // Cartridge
    double muzzle_velocity_mps = 0.0;
    double powder_reference_c = 15.0;
    double powder_sensitivity_pct_per_c = 0.0; // % of V0 per C

    // Zero
    double zero_range_m = 100.0;
    double zero_offset_up_cm = 0.0;
    double zero_offset_right_cm = 0.0;
    double zero_temperature_c = 15.0;
    double zero_pressure_hpa = 1013.25; // station pressure
    double zero_altitude_m = 0.0;
    double zero_humidity_pct = 50.0;
    double zero_powder_c = 15.0;
};

// Problems a user must fix before saving, as a readable sentence; empty if
// the form is complete.
std::string Validate(const ProfileForm& form);

// The form with a library bullet chosen (fields filled for display).
Result<ProfileForm> WithLibraryBullet(storage::Database& db, ProfileForm form, Id bullet_id);

// Loads a stored profile into a form.
Result<ProfileForm> LoadProfileForm(storage::Database& db, Id profile_id);

// Saves the form: creates or updates the profile and the rifle, scope,
// cartridge and bullet it owns, in one transaction. Returns the profile id.
Result<Id> SaveProfileForm(storage::Database& db, const ProfileForm& form);

// A ready-to-try profile: .308 Win, 1:10 twist, 0.1 MRAD scope, 100 m zero,
// Sierra MatchKing 175 gr from the library when it is there (an own copy of
// its published bands otherwise). Returns the new profile id.
Result<Id> CreateSampleProfile(storage::Database& db, const std::string& name);

// Deletes a profile and the records it owns (those no other profile uses).
Status DeleteProfile(storage::Database& db, Id profile_id);

} // namespace ballistics::applogic

#endif // BALLISTICS_APPLOGIC_PROFILE_FORM_H
