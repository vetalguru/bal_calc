#ifndef BALLISTICS_APPLOGIC_PROFILE_IO_H
#define BALLISTICS_APPLOGIC_PROFILE_IO_H

#include <ballistics/storage/database.h>
#include <ballistics/storage/records.h>

#include <string>

// Rifles and cartridges as self-contained JSON documents for backup and
// sharing:
//
//   { "format": "balcalc-rifle", "version": 1,
//     "rifle": {..., "zero_range_m", "zero_atmosphere": {...}, ...},
//     "scope": {..., "reticle": {...} | null} | null }
//
//   { "format": "balcalc-cartridge", "version": 1,
//     "cartridge": {..., "velocity_points": [[K, m/s], ...]},
//     "bullet":    {..., "bc_bands": [[m/s, bc], ...],
//                   "curve": {"name": ..., "points": [[mach, cd], ...]} | null} }
//
// Files of the earlier "balcalc-profile" v1 format (rifle, scope, cartridge
// and bullet in one, zero in "profile") still import, as a rifle, a
// cartridge and their pair.
//
// All quantities are SI (m, kg, m/s, K, Pa, rad), BCs in lb/in^2, as in
// the database.
namespace ballistics::applogic {

using storage::Id;
using storage::Result;

Result<std::string> ExportRifleJson(storage::Database& db, Id rifle_id);
Result<std::string> ExportCartridgeJson(storage::Database& db, Id cartridge_id);

// What an import created (0 = nothing of that kind).
struct Imported {
    Id rifle_id = 0;
    Id cartridge_id = 0;
    Id profile_id = 0;  // pair, from a "balcalc-profile" file
};

// Imports a rifle, a cartridge or a legacy profile as new records. The
// bullet is reused when the library already has an identical one. A taken
// name gets a " (2)", " (3)", ... suffix.
Result<Imported> ImportShareJson(storage::Database& db, const std::string& json);

}  // namespace ballistics::applogic

#endif  // BALLISTICS_APPLOGIC_PROFILE_IO_H
