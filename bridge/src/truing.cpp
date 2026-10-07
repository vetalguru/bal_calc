// The shot log, truing, the DSF table and the BC calculator.
#include "impl.h"

namespace ballistics::bridge {

using namespace detail;

json Api::Impl::Shots() {
    json out = json::array();
    if (profile_id == 0) {
        return out;
    }
    for (const bs::DopeRecord& d : Must(al::ListShots(db, profile_id))) {
        out.push_back(
            {{"id", d.id},
             {"rangeM", d.range_m},
             {"observed", al::FromRad(d.observed_elevation_rad, Unit())},
             {"predicted", d.predicted_elevation_rad
                               ? json(al::FromRad(*d.predicted_elevation_rad, Unit()))
                               : json(nullptr)},
             {"hasWindage", d.observed_windage_rad.has_value()},
             {"observedWindage", al::FromRad(d.observed_windage_rad.value_or(0.0), Unit())},
             {"shotAt", d.shot_at},
             {"used", d.use_for_truing},
             {"notes", d.notes},
             {"temperatureC", u::KToC(d.atmosphere.temperature_k)}});
    }
    return out;
}

json Api::Impl::Dsf() {
    last_dsf = al::ComputeDsf(db, profile_id);
    const al::DsfResult& r = last_dsf;
    json shots = json::array();
    for (const al::DsfShot& s : r.shots) {
        shots.push_back({{"shotId", s.shot_id},
                         {"rangeM", s.range_m},
                         {"mach", s.mach},
                         {"observed", al::FromRad(s.observed_rad, Unit())},
                         {"before", al::FromRad(s.predicted_before_rad, Unit())},
                         {"after", al::FromRad(s.predicted_after_rad, Unit())},
                         {"used", s.used},
                         {"limited", s.limited}});
    }
    return {{"ok", r.ok},
            {"error", r.error},
            {"points", DsfJson(r.points)},
            {"shots", shots},
            {"rmsBefore", al::FromRad(r.rms_before_rad, Unit())},
            {"rmsAfter", al::FromRad(r.rms_after_rad, Unit())}};
}

json Api::Impl::Truing() {
    last_truing = al::ComputeTruing(db, profile_id);
    const al::TruingResult& r = last_truing;
    json points = json::array();
    for (const auto& p : r.points) {
        points.push_back({{"rangeM", p.range_m},
                          {"observed", al::FromRad(p.observed_rad, Unit())},
                          {"before", al::FromRad(p.predicted_before_rad, Unit())},
                          {"after", al::FromRad(p.predicted_after_rad, Unit())}});
    }
    return {{"ok", r.ok},
            {"error", r.error},
            {"velocityScale", r.velocity_scale},
            {"dragScale", r.drag_scale},
            {"dragFitted", r.drag_fitted},
            {"rmsBefore", al::FromRad(r.rms_before_rad, Unit())},
            {"rmsAfter", al::FromRad(r.rms_after_rad, Unit())},
            {"velocityBefore", r.muzzle_velocity_before_mps},
            {"velocityAfter", r.muzzle_velocity_after_mps},
            {"points", points}};
}

void Api::Impl::RequirePair() const {
    if (profile_id == 0) {
        throw Failure("Choose a rifle and a cartridge.");
    }
}

void Api::Impl::AddTruingHandlers(HandlerMap& h) {
    using I = Api::Impl;
    h.insert({
        // Shot log and truing
        {"shots", [](I& s, const json&) -> json { return s.Shots(); }},
        {"logShot",
         [](I& s, const json& a) -> json {
             s.RequirePair();
             std::optional<double> wind;
             if (Bool(a, "hasWindage")) {
                 wind = Num(a, "windage") * s.UnitRad();
             }
             const Id id = Must(al::LogShot(s.db, s.profile_id, s.Session(), Num(a, "rangeM"),
                                            Num(a, "elevation") * s.UnitRad(), wind,
                                            Str(a, "notes")));
             return {{"id", id}};
         }},
        {"deleteShot",
         [](I& s, const json& a) -> json {
             Must(al::DeleteShot(s.db, IdOf(a)));
             return json::object();
         }},
        {"setShotUsed",
         [](I& s, const json& a) -> json {
             Must(al::SetShotUsedForTruing(s.db, IdOf(a), Bool(a, "used", true)));
             return json::object();
         }},
        {"computeTruing", [](I& s, const json&) -> json { return s.Truing(); }},
        {"applyTruing",
         [](I& s, const json&) -> json {
             Must(al::ApplyTruing(s.db, s.profile_id, s.last_truing));
             s.last_truing = {};
             return json::object();
         }},
        {"resetTruing",
         [](I& s, const json&) -> json {
             Must(al::ResetTruing(s.db, s.profile_id));
             return json::object();
         }},
        {"bcCalculator",
         [](I& s, const json& a) -> json {
             const std::string table = Str(a, "table", "G7");
             al::BcResult r;
             if (Str(a, "mode") == "hit") {
                 if (s.profile_id == 0) {
                     throw Failure("Choose a rifle and a cartridge.");
                 }
                 r = al::BcFromHit(Must(bs::LoadProfile(s.db, s.profile_id)), table, Num(a, "rangeM"),
                                   Num(a, "elevation") * s.UnitRad(), s.Session());
             } else {
                 r = al::BcFromChronograph(table, Num(a, "vNearMps"), Num(a, "vFarMps"),
                                           Num(a, "distanceM"), s.Session());
             }
             return {{"ok", r.ok}, {"error", r.error}, {"bc", r.bc}, {"table", table}};
         }},
        {"computeDsf", [](I& s, const json&) -> json { return s.Dsf(); }},
        {"applyDsf",
         [](I& s, const json&) -> json {
             if (!s.last_dsf.ok) {
                 throw Failure("Nothing to apply.");
             }
             Must(al::SetDsf(s.db, s.profile_id, s.last_dsf.points));
             s.last_dsf = {};
             return json::object();
         }},
        {"setDsf",
         [](I& s, const json& a) -> json {
             std::vector<DsfPoint> points;
             for (const json& p : a.value("points", json::array())) {
                 points.push_back({Num(p, "mach"), Num(p, "factor", 1.0)});
             }
             Must(al::SetDsf(s.db, s.profile_id, std::move(points)));
             return json::object();
         }},
        {"resetDsf",
         [](I& s, const json&) -> json {
             Must(al::SetDsf(s.db, s.profile_id, {}));
             return json::object();
         }},
    });
}

} // namespace ballistics::bridge
