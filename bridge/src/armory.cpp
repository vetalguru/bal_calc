// The rifle and cartridge chosen, their forms, the pair they make.
#include "impl.h"

namespace ballistics::bridge {

using namespace detail;

void Api::Impl::ReloadArmory() {
    rifles = json::array();
    std::string caliber;
    for (const al::RifleSummary& r : Must(al::ListRifles(db))) {
        rifles.push_back({{"id", r.id}, {"name", r.name}, {"caliber", r.caliber}});
        if (r.id == rifle_id) {
            caliber = r.caliber;
        }
    }
    cartridges = json::array();
    for (const al::CartridgeSummary& c : Must(al::ListCartridges(db, caliber))) {
        cartridges.push_back(ToJson(c, al::SameCaliber(c.caliber, caliber)));
    }
}

// Keeps the selection valid and the pair in step with it.
void Api::Impl::UpdatePair() {
    if (!Contains(rifles, rifle_id)) {
        rifle_id = FirstId(rifles);
    }
    if (!Contains(cartridges, cartridge_id)) {
        cartridge_id = FirstId(cartridges);
    }
    Put(kCurrentRifleKey, std::to_string(rifle_id));
    Put(kCurrentCartridgeKey, std::to_string(cartridge_id));
    Id pair = 0;
    if (rifle_id != 0 && cartridge_id != 0) {
        pair = Must(al::EnsureProfile(db, rifle_id, cartridge_id));
    }
    if (pair != profile_id) {
        profile_id = pair;
        last_truing = {};
    }
}

void Api::Impl::Select(Id rifle, Id cartridge) {
    const bool rifle_changed = rifle != rifle_id;
    rifle_id = rifle;
    cartridge_id = cartridge;
    if (rifle_changed) {
        ReloadArmory(); // cartridges of the new calibre first
    }
    UpdatePair();
}

// After the lists changed: reorder for the current rifle, keep valid.
void Api::Impl::Refresh() {
    ReloadArmory();
    UpdatePair();
}

json Api::Impl::CurrentPair() {
    if (profile_id == 0) {
        return json::object();
    }
    auto p = bs::LoadProfile(db, profile_id);
    if (!p) {
        return json::object();
    }
    return {{"rifleName", p.value().rifle.name},
            {"cartridgeName", p.value().cartridge.name},
            {"zeroRangeM", p.value().rifle.zero_range_m},
            {"offsetUpCm", p.value().profile.zero_offset_up_m * 100.0},
            {"offsetRightCm", p.value().profile.zero_offset_right_m * 100.0}};
}

void Api::Impl::AddArmoryHandlers(HandlerMap& h) {
    using I = Api::Impl;
    h.insert({
        {"select",
         [](I& s, const json& a) -> json {
             s.Select(a.contains("rifleId") ? IdOf(a, "rifleId") : s.rifle_id,
                      a.contains("cartridgeId") ? IdOf(a, "cartridgeId") : s.cartridge_id);
             return s.State();
         }},
        // Rifles
        {"rifleForm",
         [](I& s, const json& a) -> json {
             const Id id = IdOf(a);
             return ToJson(id == 0 ? al::RifleForm{} : Must(al::LoadRifleForm(s.db, id)));
         }},
        {"saveRifle",
         [](I& s, const json& a) -> json {
             const Id id = Must(al::SaveRifleForm(s.db, RifleFrom(a.value("form", json::object()))));
             s.rifle_id = 0; // the cartridge order follows the (maybe new) calibre
             s.ReloadArmory();
             s.Select(id, s.cartridge_id);
             return {{"id", id}};
         }},
        {"deleteRifle",
         [](I& s, const json& a) -> json {
             Must(al::DeleteRifle(s.db, IdOf(a)));
             s.Refresh();
             return s.State();
         }},
        // Cartridges
        {"cartridgeForm",
         [](I& s, const json& a) -> json {
             const Id id = IdOf(a);
             return ToJson(id == 0 ? al::CartridgeForm{} : Must(al::LoadCartridgeForm(s.db, id)));
         }},
        {"saveCartridge",
         [](I& s, const json& a) -> json {
             const Id id =
                 Must(al::SaveCartridgeForm(s.db, CartridgeFrom(a.value("form", json::object()))));
             s.ReloadArmory();
             s.Select(s.rifle_id, id);
             return {{"id", id}};
         }},
        {"deleteCartridge",
         [](I& s, const json& a) -> json {
             Must(al::DeleteCartridge(s.db, IdOf(a)));
             s.Refresh();
             return s.State();
         }},
        // Gyroscopic stability of a bullet in a barrel (Miller) at standard
        // air, for the editors: 0 when an input is missing.
        {"stability",
         [](I&, const json& a) -> json {
             const double sg = ballistics::MillerStability(
                 u::GrainToKg(Num(a, "massGr")), u::InchToM(Num(a, "diameterIn")), u::InchToM(Num(a, "lengthIn")),
                 u::InchToM(Num(a, "twistIn")), Num(a, "velocityMps"), 288.15, 101325.0);
             return {{"sg", sg}};
         }},
        {"cartridgeFormWithBullet",
         [](I& s, const json& a) -> json {
             return ToJson(Must(al::WithLibraryBullet(
                 s.db, CartridgeFrom(a.value("form", json::object())), IdOf(a, "bulletId"))));
         }},
        {"setZeroOffset",
         [](I& s, const json& a) -> json {
             s.RequirePair();
             Must(al::SetZeroOffset(s.db, s.profile_id, Num(a, "upCm"), Num(a, "rightCm")));
             return s.CurrentPair();
         }},
        {"addSample",
         [](I& s, const json& a) -> json {
             const Id pair = Must(al::CreateSampleProfile(
                 s.db, Str(a, "rifleName", "Sample .308 Win"),
                 Str(a, "cartridgeName", "Sample SMK 175 gr")));
             const auto p = Must(bs::Repository<bs::ProfileRecord>(s.db).Get(pair));
             if (p) {
                 s.rifle_id = 0;
                 s.ReloadArmory();
                 s.Select(p->rifle_id, p->cartridge_id);
             }
             return s.State();
         }},
    });
}

} // namespace ballistics::bridge
