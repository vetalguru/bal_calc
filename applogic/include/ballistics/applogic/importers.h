#ifndef BALLISTICS_APPLOGIC_IMPORTERS_H
#define BALLISTICS_APPLOGIC_IMPORTERS_H

#include <string>
#include <vector>

#include <ballistics/storage/database.h>
#include <ballistics/storage/records.h>

// Readers for third-party data files and the bundled starter library:
//
//  *.ammo    - BallisticCalculator (LGPL) cartridge: bullet + muzzle velocity
//  *.drg     - JBM / Exterior Ballistics drag function: "TYPE, name, mass kg,
//              diameter m, length m, Radar Data" then "Cd Mach" rows
//  *.reticle - BallisticCalculator reticle drawing (lines, circles, paths,
//              text, BDC marks in MOA or mil)
namespace ballistics::applogic {

using storage::Id;
using storage::Result;
using storage::Status;

// Bullet sources of imported data (BulletRecord::source, etc.).
inline constexpr const char* kSourceAmmoFile = "import:ammo";
inline constexpr const char* kSourceDrgFile = "import:drg";
inline constexpr const char* kSourceReticleFile = "import:reticle";
inline constexpr const char* kSourcePublished = "published";

struct AmmoFile {
    storage::BulletRecord bullet;       // id 0
    storage::CartridgeRecord cartridge; // bullet_id 0
};
Result<AmmoFile> ParseAmmo(const std::string& xml);

struct DrgFile {
    std::string kind; // "CFM", "BRL", ...
    std::string name;
    double mass_kg = 0.0;
    double diameter_m = 0.0;
    double length_m = 0.0;
    std::vector<DragPoint> points; // Mach ascending, duplicates removed
};
// Fails for "Encoded Data" files (obfuscated, not readable).
Result<DrgFile> ParseDrg(const std::string& text);

// The reticle as a ReticleRecord whose `definition` is the drawing as JSON
// in milliradians: {"size":[w,h],"zero":[x,y],"elements":[...],"bdc":[...]}
// with elements {"t":"line",x1,y1,x2,y2,w}, {"t":"circle",x,y,r,w,fill},
// {"t":"path",fill,"d":[["M",x,y],["L",x,y],["A",x,y,r,cw,major]]},
// {"t":"text",x,y,h,s}; y is up, the aiming point is (0, 0).
Result<storage::ReticleRecord> ParseReticle(const std::string& xml);

// Importers: parse and store as library records. Return the new id (the
// bullet for .ammo/.drg, the reticle for .reticle).
Result<Id> ImportAmmo(storage::Database& db, const std::string& xml);
Result<Id> ImportDrg(storage::Database& db, const std::string& text);
Result<Id> ImportReticle(storage::Database& db, const std::string& xml);

// Imports a file by its extension (.ammo, .drg, .reticle, .json profile).
Result<Id> ImportFile(storage::Database& db, const std::string& file_name,
                      const std::string& content);

// Bundled starter data.
struct SeedFile {
    std::string name;    // file name, decides the format
    std::string content;
};
struct SeedReport {
    int imported = 0;
    int skipped = 0; // already present or unreadable (e.g. encoded .drg)
    std::vector<std::string> problems;
};
// Imports `files` once per `seed_version` (remembered in the settings);
// records that already exist (same name and source) are skipped.
Result<SeedReport> SeedLibrary(storage::Database& db, const std::vector<SeedFile>& files,
                               int seed_version);

} // namespace ballistics::applogic

#endif // BALLISTICS_APPLOGIC_IMPORTERS_H
