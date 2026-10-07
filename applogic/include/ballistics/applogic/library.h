#ifndef BALLISTICS_APPLOGIC_LIBRARY_H
#define BALLISTICS_APPLOGIC_LIBRARY_H

#include <ballistics/storage/database.h>
#include <ballistics/storage/records.h>

#include <string>
#include <vector>

// The bullet library screen: browse, search, edit bullets in the units
// shooters use, including velocity-banded BCs.
namespace ballistics::applogic {

using storage::Id;
using storage::Result;
using storage::Status;

// Where a bullet came from (BulletRecord::source).
inline constexpr const char* kSourceUser = "user";        // typed in a profile
inline constexpr const char* kSourceLibrary = "library";  // added in the library

struct BulletSummary {
    Id id = 0;
    std::string name;
    std::string manufacturer;
    std::string caliber;
    double mass_gr = 0.0;
    double diameter_in = 0.0;
    std::string drag_kind;   // storage::kDragKind*
    std::string drag_table;  // "G1", "G7", ...
    double bc = 0.0;         // single BC, or the first (fastest) band
    int bc_bands = 0;
    std::string source;
};

// Library bullets (all but those private to a profile), filtered by a
// case-insensitive substring of name, manufacturer or caliber.
Result<std::vector<BulletSummary>> ListLibraryBullets(storage::Database& db,
                                                      const std::string& filter = "");

struct BcBand {
    double velocity_mps = 0.0;
    double bc = 0.0;
};

struct BulletForm {
    Id id = 0;
    std::string name;
    std::string manufacturer;
    std::string caliber;
    double mass_gr = 0.0;
    double diameter_in = 0.0;
    double length_in = 0.0;
    std::string drag_table = "G7";
    // One BC, or BCs by velocity (Sierra-style) when `bands` is not empty.
    double bc = 0.0;
    std::vector<BcBand> bands;
    std::string notes;
    std::string source = kSourceLibrary;
    bool has_custom_curve = false;  // own Cd(M) curve: drag fields read-only
};

std::string Validate(const BulletForm& form);
Result<BulletForm> LoadBulletForm(storage::Database& db, Id bullet_id);
Result<Id> SaveBulletForm(storage::Database& db, const BulletForm& form);
// Fails with a readable message while a cartridge uses the bullet.
Status DeleteBullet(storage::Database& db, Id bullet_id);

}  // namespace ballistics::applogic

#endif  // BALLISTICS_APPLOGIC_LIBRARY_H
