#ifndef BALLISTICS_APPLOGIC_PROFILE_IO_H
#define BALLISTICS_APPLOGIC_PROFILE_IO_H

#include <string>

#include <ballistics/storage/database.h>
#include <ballistics/storage/records.h>

// Profiles as self-contained JSON documents for backup and sharing:
//
//   { "format": "balcalc-profile", "version": 1,
//     "profile":   {...}, "rifle": {...}, "scope": {...} | null,
//     "cartridge": {..., "velocity_points": [[K, m/s], ...]},
//     "bullet":    {..., "bc_bands": [[m/s, bc], ...],
//                   "curve": {"name": ..., "points": [[mach, cd], ...]} | null} }
//
// All quantities are SI (m, kg, m/s, K, Pa, rad), BCs in lb/in^2, as in
// the database.
namespace ballistics::applogic {

using storage::Id;
using storage::Result;

Result<std::string> ExportProfileJson(storage::Database& db, Id profile_id);

// Creates a new profile with its own rifle, scope and cartridge. The bullet
// is reused when the library already has an identical one. A taken name
// gets a " (2)", " (3)", ... suffix. Returns the new profile id.
Result<Id> ImportProfileJson(storage::Database& db, const std::string& json);

} // namespace ballistics::applogic

#endif // BALLISTICS_APPLOGIC_PROFILE_IO_H
