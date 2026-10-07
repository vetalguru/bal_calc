// The firing solution, its reticle hold, tables, curves and the hit chance.
#include "impl.h"

namespace ballistics::bridge {

using namespace detail;

void Api::Impl::AddReticle(const bs::LoadedProfile& p, const al::SolutionSummary& r, json& out) {
    out["hasReticle"] = false;
    if (!r.ok || !p.scope) {
        return;
    }
    const bs::ScopeRecord& scope = *p.scope;
    const double zoom = magnification > 0.0 ? magnification : scope.max_magnification;
    const al::ReticleHold hold =
        al::ComputeReticleHold(r.elevation * UnitRad(), r.windage * UnitRad(), scope, zoom,
                               al::HoldModeFromString(hold_mode));
    out["holdMode"] = hold_mode;
    out["dialElevationClicks"] = hold.dial_elevation_clicks;
    out["dialWindageClicks"] = hold.dial_windage_clicks;
    out["targetX"] = hold.target_x;
    out["targetY"] = hold.target_y;
    out["subtensionScale"] = hold.scale;
    out["focalPlane"] = scope.focal_plane;
    out["minMagnification"] = scope.min_magnification;
    out["maxMagnification"] = scope.max_magnification;
    out["magnification"] = zoom;
    if (scope.reticle_id) {
        auto ret = bs::Repository<bs::ReticleRecord>(db).Get(*scope.reticle_id);
        if (ret && ret.value()) {
            out["hasReticle"] = true;
            out["reticleName"] = ret.value()->name;
            out["reticleUnits"] = ret.value()->units;
            out["reticleDefinition"] = ret.value()->definition;
        }
    }
}

json Api::Impl::Solution() {
    const auto start = std::chrono::steady_clock::now();
    json out;
    if (profile_id == 0) {
        out = {{"ok", false}, {"error", "Choose a rifle and a cartridge."}};
    } else if (auto p = bs::LoadProfile(db, profile_id); !p) {
        out = {{"ok", false}, {"error", p.error().message}};
    } else {
        const al::SolutionSummary r = al::Summarize(p.value(), Session(), Unit(), NowUnix());
        out = {{"ok", r.ok},
               {"error", r.error},
               {"rangeM", r.range_m},
               {"elevation", r.elevation},
               {"windage", r.windage},
               {"elevationClicks", r.elevation_clicks},
               {"windageClicks", r.windage_clicks},
               {"hasScope", p.value().scope.has_value()},
               // One click in the angle unit (0 without a scope): for rounding to clicks.
               {"clickElevation",
                p.value().scope ? al::FromRad(p.value().scope->click_vertical_rad, Unit()) : 0.0},
               {"clickWindage",
                p.value().scope ? al::FromRad(p.value().scope->click_horizontal_rad, Unit()) : 0.0},
               {"dropCm", r.drop_cm},
               {"windageCm", r.windage_cm},
               {"velocity", r.velocity_mps},
               {"energy", r.energy_j},
               {"time", r.time_s},
               {"mach", r.mach},
               {"muzzleVelocity", r.muzzle_velocity_mps},
               {"stability", r.stability},
               {"velocityScale", p.value().profile.velocity_scale},
               {"dragScale", p.value().profile.drag_scale},
               {"dsf", DsfJson(p.value().profile.dsf)},
               {"spinDriftCm", r.spin_drift_cm},
               {"subsonic", r.subsonic},
               {"transonicRangeM", r.transonic_range_m},
               {"apexCm", r.apex_cm},
               {"apexRangeM", r.apex_range_m},
               {"pointBlankNearM", r.point_blank_near_m},
               {"pointBlankFarM", r.point_blank_far_m},
               {"densityAltitudeM", r.density_altitude_m},
               {"pressureHpa", r.pressure_hpa},
               {"hasGust", r.has_gust},
               {"gustWindage", r.gust_windage},
               {"gustWindageClicks", r.gust_windage_clicks},
               {"gustWindageCm", r.gust_windage_cm},
               {"hasLead", r.has_lead},
               {"lead", r.lead},
               {"leadClicks", r.lead_clicks},
               {"leadCm", r.lead_cm},
               {"leadTotalWindage", r.lead_total_windage},
               {"leadTotalWindageClicks", r.lead_total_windage_clicks},
               {"leadRangeM", r.lead_range_m},
               {"leadElevation", r.lead_elevation},
               {"leadElevationClicks", r.lead_elevation_clicks}};
        json warnings = json::array();
        for (const al::Warning& w : r.warnings) {
            warnings.push_back({{"code", w.code}, {"value", w.value}});
        }
        out["warnings"] = warnings;
        AddReticle(p.value(), r, out);
    }
    out["computeMs"] =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return out;
}

// Curves of other rifle + cartridge pairs in the current conditions (at
// most four), each as a range table with its label.
json Api::Impl::CompareCurves(const json& a) {
    const double max_range = std::clamp(Num(a, "maxRangeM", 1000.0), 10.0, 3000.0);
    const int points = std::clamp(static_cast<int>(Num(a, "points", 250)), 10, 1000);
    json out = json::array();
    for (const json& pair : a.value("pairs", json::array())) {
        if (out.size() == 4) {
            break;
        }
        const Id rifle = pair.value("rifleId", Id{0});
        const Id cartridge = pair.value("cartridgeId", Id{0});
        json curve = {{"ok", false}, {"error", ""}, {"rows", json::array()}, {"label", ""}};
        const auto profile = al::EnsureProfile(db, rifle, cartridge);
        if (!profile) {
            curve["error"] = profile.error().message;
        } else if (auto p = bs::LoadProfile(db, profile.value()); !p) {
            curve["error"] = p.error().message;
        } else {
            curve = ToJson(al::BuildRangeTable(p.value(), Session(), Unit(), 0.0, max_range,
                                               max_range / points),
                           p.value().scope.has_value());
            curve["label"] = p.value().rifle.name + " · " + p.value().cartridge.name;
        }
        curve["rifleId"] = rifle;
        curve["cartridgeId"] = cartridge;
        out.push_back(curve);
    }
    return out;
}

// Every rifle with the cartridges of its calibre: what can be compared.
json Api::Impl::PairOptions() {
    const auto all_cartridges = Must(al::ListCartridges(db));
    json out = json::array();
    for (const al::RifleSummary& r : Must(al::ListRifles(db))) {
        json list = json::array();
        for (const al::CartridgeSummary& c : all_cartridges) {
            if (al::SameCaliber(c.caliber, r.caliber)) {
                list.push_back({{"id", c.id}, {"name", c.name}});
            }
        }
        out.push_back({{"rifleId", r.id}, {"rifleName", r.name}, {"cartridges", list}});
    }
    return out;
}

json Api::Impl::Table(double from_m, double to_m, double step_m, const json& wind_speeds) {
    const auto start = std::chrono::steady_clock::now();
    if (profile_id == 0) {
        return {
            {"ok", false}, {"error", "Choose a rifle and a cartridge."}, {"rows", json::array()}};
    }
    auto p = bs::LoadProfile(db, profile_id);
    if (!p) {
        return {{"ok", false}, {"error", p.error().message}, {"rows", json::array()}};
    }
    json out = ToJson(al::BuildRangeTable(p.value(), Session(), Unit(), from_m, to_m, step_m),
                      p.value().scope.has_value());
    // Wind columns: the windage for other wind speeds, from the same
    // direction (one zone), each computed in full.
    json speeds = json::array();
    if (wind_speeds.is_array()) {
        for (const json& v : wind_speeds) {
            if (v.is_number() && speeds.size() < 6) {
                speeds.push_back(std::clamp(v.get<double>(), 0.0, 40.0));
            }
        }
    }
    out["windSpeeds"] = speeds;
    if (!speeds.empty() && out.value("ok", false)) {
        const al::SessionConditions base = Session();
        const double from = base.winds.empty() ? 90.0 : base.winds.front().from_deg;
        json& rows = out["rows"];
        for (json& row : rows) {
            row["windages"] = json::array();
            row["windageClicksAt"] = json::array();
        }
        for (const json& v : speeds) {
            al::SessionConditions s = base;
            s.winds = {al::WindInput{v.get<double>(), from, 0.0}};
            const al::RangeTable t =
                al::BuildRangeTable(p.value(), s, Unit(), from_m, to_m, step_m);
            for (std::size_t i = 0; i < rows.size() && i < t.rows.size(); ++i) {
                rows[i]["windages"].push_back(t.rows[i].windage);
                rows[i]["windageClicksAt"].push_back(t.rows[i].windage_clicks);
            }
        }
    }
    out["computeMs"] =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return out;
}

void Api::Impl::AddSolutionHandlers(HandlerMap& h) {
    using I = Api::Impl;
    h.insert({
        {"solution", [](I& s, const json&) -> json { return s.Solution(); }},
        {"rangeTable",
         [](I& s, const json& a) -> json {
             return s.Table(s.table_from_m, s.table_to_m, s.table_step_m,
                            a.value("windSpeeds", json::array()));
         }},
        {"compareCurves", [](I& s, const json& a) -> json { return s.CompareCurves(a); }},
        {"pairOptions", [](I& s, const json&) -> json { return s.PairOptions(); }},
        {"trajectoryCurve",
         [](I& s, const json& a) -> json {
             const double max_range = Num(a, "maxRangeM", 1000.0);
             const int points = std::clamp(static_cast<int>(Num(a, "points", 250)), 10, 1000);
             return s.Table(0.0, max_range, max_range / points);
         }},
        {"wez",
         [](I& s, const json& a) -> json {
             al::WezSettings w = Must(al::LoadWezSettings(s.db));
             if (a.contains("settings")) {
                 Update(w, a.at("settings"));
                 Must(al::SaveWezSettings(s.db, w));
             }
             json out = {{"settings", ToJson(w)}};
             if (s.profile_id == 0) {
                 out["ok"] = false;
                 out["error"] = "Choose a rifle and a cartridge.";
                 return out;
             }
             const al::WezResult r =
                 al::ComputeWez(Must(bs::LoadProfile(s.db, s.profile_id)), s.Session(), w,
                                Num(a, "toM", 1000.0), Num(a, "stepM", 50.0));
             json rows = json::array();
             for (const al::WezRow& row : r.rows) {
                 rows.push_back(ToJson(row));
             }
             json parts = json::array();
             for (const Spread::Part& p : r.parts) {
                 parts.push_back({{"source", p.source},
                                  {"upCm", p.up_m * 100.0},
                                  {"rightCm", p.right_m * 100.0}});
             }
             out.update({{"ok", r.ok},
                         {"error", r.error},
                         {"rows", rows},
                         {"atTarget", ToJson(r.at_target)},
                         {"parts", parts},
                         {"shots50", r.shots_50},
                         {"shots80", r.shots_80},
                         {"shots95", r.shots_95}});
             return out;
         }},
    });
}

}  // namespace ballistics::bridge
