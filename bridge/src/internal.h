// Shared by the bridge's sources (not installed): JSON helpers, settings
// keys and the conversions between applogic forms and the JSON the app sees.
#ifndef BALLISTICS_BRIDGE_INTERNAL_H
#define BALLISTICS_BRIDGE_INTERNAL_H

#include <ballistics/applogic/armory.h>
#include <ballistics/applogic/library.h>
#include <ballistics/applogic/session.h>
#include <ballistics/applogic/wez.h>
#include <ballistics/storage/database.h>
#include <ballistics/units.h>

#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ballistics::bridge::detail {

namespace al = ballistics::applogic;
namespace bs = ballistics::storage;
namespace u = ballistics::units;
using al::Id;
using nlohmann::json;

inline constexpr const char* kCurrentProfileKey = "ui.current_profile";  // before v3: the selection
inline constexpr const char* kCurrentRifleKey = "ui.current_rifle";
inline constexpr const char* kCurrentCartridgeKey = "ui.current_cartridge";
inline constexpr const char* kAngleUnitKey = "ui.angle_unit";
inline constexpr const char* kLanguageKey = "ui.language";
inline constexpr const char* kUiPrefsKey = "ui.prefs";
inline constexpr std::size_t kMaxExtraWindZones = 2;  // three wind zones in all
inline constexpr const char* kTargetSpeedUnitKey = "ui.target_speed_unit";
inline constexpr const char* kHoldModeKey = "ui.hold_mode";
inline constexpr const char* kSituationsKey = "ui.situations";
inline constexpr const char* kTargetsKey = "ui.targets";
inline constexpr std::size_t kMaxTargets = 20;
// The app sends pictures shrunk to about 640 px; this only stops mistakes.
inline constexpr std::size_t kMaxPhotoBytes = 2u << 20;
inline constexpr const char* kTableFromKey = "ui.table.from_m";
inline constexpr const char* kTableToKey = "ui.table.to_m";
inline constexpr const char* kTableStepKey = "ui.table.step_m";

// A failure reported to the caller as {"ok": false, "error": message}.
struct Failure : std::runtime_error {
    using std::runtime_error::runtime_error;
};

template <typename T>
T Must(bs::Result<T> r) {
    if (!r) {
        throw Failure(r.error().message);
    }
    return std::move(r).value();
}

inline void Must(const bs::Status& s) {
    if (!s) {
        throw Failure(s.error().message);
    }
}

// ---- JSON arguments and helpers --------------------------------------------

double NowUnix();
double Num(const json& j, const char* key, double fallback = 0.0);
Id IdOf(const json& j, const char* key = "id");
std::string Str(const json& j, const char* key, const std::string& fallback = "");
std::string Trim(const std::string& s);
bool Bool(const json& j, const char* key, bool fallback = false);
std::string Lower(std::string s);

// Catalog items whose maker, model and calibre contain every word of the filter.
json Matching(const json& catalog, const std::string& filter);

// Pictures travel through the JSON as base64 (RFC 4648, with padding).
std::string ToBase64(const std::vector<std::uint8_t>& data);
std::vector<std::uint8_t> FromBase64(const std::string& text);

// "rifle" or "cartridge": what may have a picture.
std::string PhotoKind(const json& a);

// ---- Forms <-> JSON ---------------------------------------------------------

json ToJson(const al::RifleForm& f);
al::RifleForm RifleFrom(const json& m);
json ToJson(const al::CartridgeForm& f);
al::CartridgeForm CartridgeFrom(const json& m);
json ToJson(const al::CartridgeSummary& c, bool matches);
json ToJson(const al::BulletForm& f);
al::BulletForm BulletFrom(const json& m);
json ToJson(const al::WezSettings& w);
void Update(al::WezSettings& w, const json& a);
json ToJson(const al::WezRow& r);
json DsfJson(const std::vector<DsfPoint>& points);
json ToJson(const al::RangeTable& t, bool has_scope);

// Invalid UTF-8 (a hand-edited import, say) is replaced, never thrown.
std::string Dump(const json& j);
Id FirstId(const json& list);
bool Contains(const json& list, Id id);

}  // namespace ballistics::bridge::detail

#endif  // BALLISTICS_BRIDGE_INTERNAL_H
