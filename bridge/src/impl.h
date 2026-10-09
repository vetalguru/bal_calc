// The bridge's session (Api::Impl): its state, and the methods each topic
// file defines (session.cpp, armory.cpp, ...), with its JSON methods.
#ifndef BALLISTICS_BRIDGE_IMPL_H
#define BALLISTICS_BRIDGE_IMPL_H

#include <ballistics/applogic/armory.h>
#include <ballistics/applogic/importers.h>
#include <ballistics/applogic/library.h>
#include <ballistics/applogic/profile_io.h>
#include <ballistics/applogic/reticle.h>
#include <ballistics/applogic/session.h>
#include <ballistics/applogic/truing.h>
#include <ballistics/applogic/wez.h>
#include <ballistics/atmosphere.h>
#include <ballistics/bridge/api.h>
#include <ballistics/effects.h>
#include <ballistics/storage/database.h>
#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>
#include <ballistics/units.h>
#include <ballistics/version.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "internal.h"

namespace ballistics::bridge {

struct Api::Impl {
    bs::Database db;
    bool open = false;
    std::string db_path;

    // Read-only catalogs from the seed files (published_scopes.json,
    // published_rifles.json): "Choose from the library" in the rifle editor.
    json scope_catalog = json::array();
    json rifle_catalog = json::array();

    // Selection and the pair derived from it.
    json rifles = json::array();
    json cartridges = json::array();
    Id rifle_id = 0;
    Id cartridge_id = 0;
    Id profile_id = 0;

    // Settings (persisted).
    std::string angle_unit = "mrad";
    std::string hold_mode = "dial_elevation";
    std::string language;  // "" = system
    // Interface preferences of the app (theme, screen, display format): the
    // core keeps them for it, as one JSON object.
    json ui_prefs = json::object();
    double table_from_m = 100.0;
    double table_to_m = 1000.0;
    double table_step_m = 50.0;

    // Current conditions in UI terms (persisted as the session).
    double temperature_c = 15.0;
    double pressure_hpa = 1013.25;
    double altitude_m = 0.0;
    double humidity_pct = 50.0;
    bool powder_follows_air = true;
    double powder_c = 15.0;
    double wind_speed = 0.0;
    double wind_from_deg = 0.0;             // 12 o'clock: from the target
    double wind_until_m = 0.0;              // end of the first zone when there are more
    std::vector<al::WindInput> wind_zones;  // the zones after the first, in order
    double wind_gust_mps = 0.0;
    double target_speed_mps = 0.0;
    double target_heading_deg = 90.0;
    std::string target_speed_unit = "kmh";  // how the app shows it: "kmh" or "mps"
    double look_angle_deg = 0.0;
    double cant_deg = 0.0;
    bool coriolis = false;
    double latitude_deg = 50.0;
    bool use_azimuth = false;
    double azimuth_deg = 0.0;
    double target_range_m = 300.0;
    double magnification = 0.0;
    bool use_density_altitude = false;
    double density_altitude_m = 0.0;
    double target_height_cm = 20.0;
    double weather_at_unix = 0.0;

    al::TruingResult last_truing;
    al::DsfResult last_dsf;

    using Handler = std::function<json(Impl&, const json&)>;
    using HandlerMap = std::map<std::string, Handler>;
    static const HandlerMap& Handlers();
    // Each topic file adds its JSON methods (Api::Call looks them up).
    static void AddSessionHandlers(HandlerMap& h);
    static void AddArmoryHandlers(HandlerMap& h);
    static void AddTargetsHandlers(HandlerMap& h);
    static void AddSolutionHandlers(HandlerMap& h);
    static void AddTruingHandlers(HandlerMap& h);
    static void AddPhotosHandlers(HandlerMap& h);
    static void AddLibraryHandlers(HandlerMap& h);
    static void AddSharingHandlers(HandlerMap& h);

    void RequireOpen() const {
        if (!open) {
            throw Failure("The database is not open.");
        }
    }

    [[nodiscard]] al::AngleUnit Unit() const {
        return angle_unit == "moa" ? al::AngleUnit::kMoa : al::AngleUnit::kMrad;
    }
    [[nodiscard]] double UnitRad() const {
        return angle_unit == "moa" ? u::MoaToRad(1.0) : u::MradToRad(1.0);
    }

    // ---- Settings and session ------------------------------------------------

    std::optional<std::string> Setting(const char* key);
    void Put(const char* key, const std::string& value);

    void LoadSettings();

    [[nodiscard]] al::SessionConditions Session() const;

    void ApplySession(const al::SessionConditions& s);

    [[nodiscard]] json Conditions() const;

    [[nodiscard]] json ZonesJson() const;

    void SetConditions(const json& a);

    // ---- Armory and selection ------------------------------------------------

    void ReloadArmory();

    // Keeps the selection valid and the pair in step with it.
    void UpdatePair();

    void Select(Id rifle, Id cartridge);

    // A seed file that is one of the read-only catalogs: kept in memory.
    bool TakeCatalog(const std::string& content);

    // ---- Targets: up to 20 named ranges with their angle and wind ----------

    json TargetsStored();

    // The session with a target's range, angle and wind (one zone).
    [[nodiscard]] al::SessionConditions SessionFor(const json& t) const;

    // Every target with its corrections and where to hold it on the reticle
    // with the turrets as set for the current target (the hold mode).
    json Targets();

    json SaveTargets(const json& list);

    // Makes a target current: its range, angle and wind go into the conditions.
    json SelectTarget(std::size_t index);

    // ---- Situations: a rifle, a cartridge and the conditions, by name -------

    json SituationsStored();

    bool Exists(Id rifle, Id cartridge);

    json Situations();

    json SaveSituation(const std::string& name);

    json ApplySituation(const std::string& name);

    json DeleteSituation(const std::string& name);

    // After the lists changed: reorder for the current rifle, keep valid.
    void Refresh();

    json CurrentPair();

    json State();

    void Open(const std::string& path);

    // ---- Solution ------------------------------------------------------------

    void AddReticle(const bs::LoadedProfile& p, const al::SolutionSummary& r, json& out);

    json Solution();

    // Curves of other rifle + cartridge pairs in the current conditions (at
    // most four), each as a range table with its label.
    json CompareCurves(const json& a);

    // Every rifle with the cartridges of its calibre: what can be compared.
    json PairOptions();

    json Table(double from_m, double to_m, double step_m, const json& wind_speeds = json::array());

    // ---- Shot log and truing -------------------------------------------------

    json Shots();

    json Dsf();

    json Truing();

    void RequirePair() const;

    // ---- Sharing -------------------------------------------------------------

    [[nodiscard]] std::string ExportFileName(const std::string& kind, Id id) const;
};

}  // namespace ballistics::bridge

#endif  // BALLISTICS_BRIDGE_IMPL_H
