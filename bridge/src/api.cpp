#include <ballistics/bridge/api.h>

#include <memory>
#include <string>
#include <utility>

#include "impl.h"

// Methods (arguments → result):
//
//   open {path}                       → {databasePath}
//   seed {version, files:[{name, content}]} → {imported, skipped, problems}
//   seedVersion                       → {version} (0: never seeded)
//   info                              → {engineVersion, sqliteVersion, databasePath}
//   state                             → {rifles, cartridges, currentRifleId,
//                                        currentCartridgeId, currentProfileId,
//                                        currentPair, angleUnit, holdMode, language,
//                                        tableFromM, tableToM, tableStepM, conditions}
//   select {rifleId?, cartridgeId?}   → state
//   setConditions {any condition keys} → conditions
//   setSettings {angleUnit?, holdMode?, language?, tableFromM?, tableToM?, tableStepM?, prefs?
//   (merged into the interface preferences)}
//                                     → state
//   solution                          → {ok, error, rangeM, elevation, windage, ...}
//   rangeTable {windSpeeds?}          → {ok, error, hasScope, computeMs, rows, windSpeeds}; rows
//   carry windages for each speed trajectoryCurve {maxRangeM, points} → as rangeTable rifleForm
//   {id} / saveRifle {form} / deleteRifle {id} cartridgeForm {id} / saveCartridge {form} /
//   deleteCartridge {id} cartridgeFormWithBullet {form, bulletId} / libraryCartridges {filter}
//   cartridgeFormFromLibrary {id}
//   setZeroOffset {upCm, rightCm} / addSample {rifleName, cartridgeName}
//   shots / logShot {rangeM, elevation, hasWindage, windage, notes}
//   deleteShot {id} / setShotUsed {id, used}
//   computeTruing / applyTruing / resetTruing
//   computeDsf / applyDsf / setDsf {points:[{mach, factor}]} / resetDsf
//   wez {settings?, toM, stepM}     → {settings, ok, error, rows, atTarget, parts, shots50/80/95}
//   bcCalculator {mode: "chronograph"|"hit", table, vNearMps, vFarMps, distanceM,
//                 rangeM, elevation} → {ok, error, bc, table}
//   reticles / libraryBullets {filter} / bulletForm {id} / saveBullet {form}
//   deleteBullet {id}
//   exportJson {kind: "rifle"|"cartridge", id} → {json, fileName}
//   importShared {text}               → state
//   importFiles {files:[{name, content}]} → {imported, problems:[{file, message}]}
//   stationPressure {qnhHpa, altitudeM} → hPa
//   stability {twistIn, massGr, diameterIn, lengthIn, velocityMps} → {sg} (standard air)
//   photos {kind} → {id: base64} / photo {kind, id} → base64 / setPhoto {kind, id, image} (empty
//   removes) targets → [{name, rangeM, lookAngleDeg, windSpeed, windFromDeg, ok, elevation,
//   windage, *Clicks, holdX, holdY}] saveTargets {targets} → targets / selectTarget {index} → state
//   situations / saveSituation {name} / applySituation {name} → state / deleteSituation {name}
//   compareCurves {maxRangeM, points, pairs:[{rifleId, cartridgeId}]} → [table + label]
//   pairOptions                     → [{rifleId, rifleName, cartridges:[{id, name}]}]
//
// save* return {id}; delete*, set* and log return state or {} as noted in
// the handlers below.

namespace ballistics::bridge {

const Api::Impl::HandlerMap& Api::Impl::Handlers() {
    static const HandlerMap handlers = [] {
        HandlerMap h;
        AddSessionHandlers(h);
        AddArmoryHandlers(h);
        AddTargetsHandlers(h);
        AddSolutionHandlers(h);
        AddTruingHandlers(h);
        AddPhotosHandlers(h);
        AddLibraryHandlers(h);
        AddSharingHandlers(h);
        return h;
    }();
    return handlers;
}

Api::Api() : impl_(std::make_unique<Impl>()) {}
Api::~Api() = default;

std::string Api::Call(const std::string& method, const std::string& args_json) {
    try {
        const auto& handlers = Impl::Handlers();
        const auto handler = handlers.find(method);
        if (handler == handlers.end()) {
            throw Failure("Unknown method: " + method);
        }
        json args = args_json.empty() ? json::object() : json::parse(args_json);
        if (!args.is_object()) {
            throw Failure("Arguments must be a JSON object.");
        }
        if (method != "open" && method != "info") {
            impl_->RequireOpen();
        }
        json result = handler->second(*impl_, args);
        return Dump({{"ok", true}, {"result", std::move(result)}});
    } catch (const Failure& e) {
        return Dump({{"ok", false}, {"error", e.what()}});
    } catch (const json::exception& e) {
        return Dump({{"ok", false}, {"error", std::string("Bad arguments: ") + e.what()}});
    } catch (const std::exception& e) {
        return Dump({{"ok", false}, {"error", e.what()}});
    }
}

}  // namespace ballistics::bridge
