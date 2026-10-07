// The target card and the saved situations.
#include "impl.h"

namespace ballistics::bridge {

using namespace detail;

json Api::Impl::TargetsStored() {
    const auto text = Setting(kTargetsKey);
    json list = text ? json::parse(*text, nullptr, false) : json::array();
    return list.is_array() ? list : json::array();
}

// The session with a target's range, angle and wind (one zone).
al::SessionConditions Api::Impl::SessionFor(const json& t) const {
    al::SessionConditions s = Session();
    s.target_range_m = t.value("rangeM", s.target_range_m);
    s.look_angle_deg = t.value("lookAngleDeg", 0.0);
    s.winds = {al::WindInput{t.value("windSpeed", 0.0), t.value("windFromDeg", 0.0), 0.0}};
    s.wind_gust_mps = 0.0;
    s.target_speed_mps = 0.0;
    return s;
}

// Every target with its corrections and where to hold it on the reticle
// with the turrets as set for the current target (the hold mode).
json Api::Impl::Targets() {
    json out = json::array();
    const json list = TargetsStored();
    std::optional<bs::LoadedProfile> p;
    if (profile_id != 0) {
        if (auto loaded = bs::LoadProfile(db, profile_id)) {
            p = std::move(loaded).value();
        }
    }
    double dial_e = 0.0;
    double dial_w = 0.0;
    double zoom = 0.0;
    if (p && p->scope) {
        zoom = magnification > 0.0 ? magnification : p->scope->max_magnification;
        const al::SolutionSummary now = al::Summarize(*p, Session(), Unit());
        if (now.ok) {
            const al::ReticleHold h =
                al::ComputeReticleHold(now.elevation * UnitRad(), now.windage * UnitRad(),
                                       *p->scope, zoom, al::HoldModeFromString(hold_mode));
            dial_e = h.dial_elevation_rad;
            dial_w = h.dial_windage_rad;
        }
    }
    for (const json& t : list) {
        json item = {{"name", t.value("name", "")},
                     {"rangeM", t.value("rangeM", 0.0)},
                     {"lookAngleDeg", t.value("lookAngleDeg", 0.0)},
                     {"windSpeed", t.value("windSpeed", 0.0)},
                     {"windFromDeg", t.value("windFromDeg", 0.0)},
                     {"ok", false}};
        if (p) {
            const al::SolutionSummary r = al::Summarize(*p, SessionFor(t), Unit());
            item["ok"] = r.ok;
            if (r.ok) {
                item["elevation"] = r.elevation;
                item["windage"] = r.windage;
                item["elevationClicks"] = r.elevation_clicks;
                item["windageClicks"] = r.windage_clicks;
                // Held, not dialled: the rest after the turrets, in reticle mrad.
                const double e = r.elevation * UnitRad() - dial_e;
                const double w = r.windage * UnitRad() - dial_w;
                double x = -u::RadToMrad(w);
                double y = -u::RadToMrad(e);
                if (p->scope) {
                    const al::ReticleHold h =
                        al::ComputeReticleHold(e, w, *p->scope, zoom, al::HoldMode::kHoldAll);
                    x = h.target_x;
                    y = h.target_y;
                }
                item["holdX"] = x;
                item["holdY"] = y;
            }
        }
        out.push_back(item);
    }
    return out;
}

json Api::Impl::SaveTargets(const json& list) {
    if (!list.is_array()) {
        throw Failure("Targets must be a list.");
    }
    if (list.size() > kMaxTargets) {
        throw Failure("At most 20 targets.");
    }
    json clean = json::array();
    for (const json& t : list) {
        const std::string name = Trim(Str(t, "name"));
        const double range = Num(t, "rangeM");
        if (name.empty()) {
            throw Failure("Enter a name for each target.");
        }
        if (range < 10.0 || range > 3000.0) {
            throw Failure("A target range must be between 10 and 3000 m.");
        }
        clean.push_back({{"name", name},
                         {"rangeM", range},
                         {"lookAngleDeg", std::clamp(Num(t, "lookAngleDeg"), -60.0, 60.0)},
                         {"windSpeed", std::clamp(Num(t, "windSpeed"), 0.0, 40.0)},
                         {"windFromDeg", Num(t, "windFromDeg", 0.0)}});
    }
    Put(kTargetsKey, clean.dump());
    return Targets();
}

// Makes a target current: its range, angle and wind go into the conditions.
json Api::Impl::SelectTarget(std::size_t index) {
    const json list = TargetsStored();
    if (index >= list.size()) {
        throw Failure("No such target.");
    }
    const json& t = list.at(index);
    json c = Conditions();
    c["targetRangeM"] = t.value("rangeM", 300.0);
    c["lookAngleDeg"] = t.value("lookAngleDeg", 0.0);
    c["windSpeed"] = t.value("windSpeed", 0.0);
    c["windFromDeg"] = t.value("windFromDeg", 0.0);
    c["windZones"] = json::array();
    SetConditions(c);
    return State();
}

json Api::Impl::SituationsStored() {
    const auto text = Setting(kSituationsKey);
    if (!text) {
        return json::array();
    }
    json list = json::parse(*text, nullptr, false);
    return list.is_array() ? list : json::array();
}

bool Api::Impl::Exists(Id rifle, Id cartridge) {
    const auto r = bs::Repository<bs::RifleRecord>(db).Get(rifle);
    const auto c = bs::Repository<bs::CartridgeRecord>(db).Get(cartridge);
    return r && r.value() && c && c.value();
}

json Api::Impl::Situations() {
    json out = json::array();
    for (const json& s : SituationsStored()) {
        const Id rifle = s.value("rifleId", Id{0});
        const Id cartridge = s.value("cartridgeId", Id{0});
        const bool ok = Exists(rifle, cartridge);
        const auto r = bs::Repository<bs::RifleRecord>(db).Get(rifle);
        const auto c = bs::Repository<bs::CartridgeRecord>(db).Get(cartridge);
        out.push_back({{"name", s.value("name", "")},
                       {"rifleName", ok ? r.value()->name : ""},
                       {"cartridgeName", ok ? c.value()->name : ""},
                       {"rangeM", s.value("conditions", json::object()).value("targetRangeM", 0.0)},
                       {"available", ok}});
    }
    return out;
}

json Api::Impl::SaveSituation(const std::string& name) {
    if (name.empty()) {
        throw Failure("Enter a name for the situation.");
    }
    if (profile_id == 0) {
        throw Failure("Choose a rifle and a cartridge.");
    }
    json list = json::array();
    for (const json& s : SituationsStored()) {
        if (s.value("name", "") != name) {
            list.push_back(s);
        }
    }
    list.push_back({{"name", name},
                    {"rifleId", rifle_id},
                    {"cartridgeId", cartridge_id},
                    {"holdMode", hold_mode},
                    {"conditions", Conditions()}});
    Put(kSituationsKey, list.dump());
    return Situations();
}

json Api::Impl::ApplySituation(const std::string& name) {
    for (const json& s : SituationsStored()) {
        if (s.value("name", "") != name) {
            continue;
        }
        const Id rifle = s.value("rifleId", Id{0});
        const Id cartridge = s.value("cartridgeId", Id{0});
        if (!Exists(rifle, cartridge)) {
            throw Failure("The rifle or cartridge of this situation was deleted.");
        }
        Select(rifle, cartridge);
        SetConditions(s.value("conditions", json::object()));
        hold_mode = al::ToString(al::HoldModeFromString(s.value("holdMode", hold_mode)));
        Put(kHoldModeKey, hold_mode);
        return State();
    }
    throw Failure("No situation of this name.");
}

json Api::Impl::DeleteSituation(const std::string& name) {
    json list = json::array();
    for (const json& s : SituationsStored()) {
        if (s.value("name", "") != name) {
            list.push_back(s);
        }
    }
    Put(kSituationsKey, list.dump());
    return Situations();
}

void Api::Impl::AddTargetsHandlers(HandlerMap& h) {
    using I = Api::Impl;
    h.insert({
        {"targets", [](I& s, const json&) -> json { return s.Targets(); }},
        {"saveTargets",
         [](I& s, const json& a) -> json {
             return s.SaveTargets(a.value("targets", json::array()));
         }},
        {"selectTarget",
         [](I& s, const json& a) -> json {
             const double i = Num(a, "index", -1.0);
             return s.SelectTarget(i < 0.0 ? kMaxTargets : static_cast<std::size_t>(i));
         }},
        {"situations", [](I& s, const json&) -> json { return s.Situations(); }},
        {"saveSituation",
         [](I& s, const json& a) -> json { return s.SaveSituation(Trim(Str(a, "name"))); }},
        {"applySituation",
         [](I& s, const json& a) -> json { return s.ApplySituation(Str(a, "name")); }},
        {"deleteSituation",
         [](I& s, const json& a) -> json { return s.DeleteSituation(Str(a, "name")); }},
    });
}

}  // namespace ballistics::bridge
