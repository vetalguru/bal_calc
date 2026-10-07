// The starter library, bullets, factory cartridges, catalogs and imports.
#include "impl.h"

namespace ballistics::bridge {

using namespace detail;

// A seed file that is one of the read-only catalogs: kept in memory.
bool Api::Impl::TakeCatalog(const std::string& content) {
    const std::string head = content.substr(0, 200);
    const bool is_scopes = head.find("\"balcalc-scopes\"") != std::string::npos;
    const bool is_rifles = head.find("\"balcalc-rifles\"") != std::string::npos;
    if (!is_scopes && !is_rifles) {
        return false;
    }
    const json doc = json::parse(content, nullptr, false);
    const char* key = is_scopes ? "scopes" : "rifles";
    if (!doc.is_discarded() && doc.contains(key) && doc.at(key).is_array()) {
        (is_scopes ? scope_catalog : rifle_catalog) = doc.at(key);
    }
    return true;
}

void Api::Impl::AddLibraryHandlers(HandlerMap& h) {
    using I = Api::Impl;
    h.insert({
        {"seed",
         [](I& s, const json& a) -> json {
             s.RequireOpen();
             std::vector<al::SeedFile> files;
             if (a.contains("files")) {
                 for (const json& f : a.at("files")) {
                     if (s.TakeCatalog(Str(f, "content"))) {
                         continue; // a catalog, not a library record
                     }
                     files.push_back({Str(f, "name"), Str(f, "content")});
                 }
             }
             const al::SeedReport r =
                 Must(al::SeedLibrary(s.db, files, static_cast<int>(Num(a, "version", 1))));
             s.Refresh();
             return {{"imported", r.imported}, {"skipped", r.skipped}, {"problems", r.problems}};
         }},
        {"libraryScopes", [](I& s, const json& a) -> json { return Matching(s.scope_catalog, Str(a, "filter")); }},
        {"libraryRifles", [](I& s, const json& a) -> json { return Matching(s.rifle_catalog, Str(a, "filter")); }},
        {"libraryCartridges",
         [](I& s, const json& a) -> json {
             json out = json::array();
             for (const auto& c : Must(al::ListLibraryCartridges(s.db, Str(a, "filter")))) {
                 out.push_back(ToJson(c, false));
             }
             return out;
         }},
        {"cartridgeFormFromLibrary",
         [](I& s, const json& a) -> json {
             return ToJson(Must(al::CartridgeFromLibrary(s.db, IdOf(a))));
         }},
        // Library
        {"reticles",
         [](I& s, const json&) -> json {
             json out = json::array();
             for (const auto& r : Must(bs::Repository<bs::ReticleRecord>(s.db).List())) {
                 out.push_back({{"id", r.id}, {"name", r.name}, {"units", r.units}});
             }
             return out;
         }},
        {"libraryBullets",
         [](I& s, const json& a) -> json {
             json out = json::array();
             for (const al::BulletSummary& b : Must(al::ListLibraryBullets(s.db, Str(a, "filter")))) {
                 out.push_back({{"id", b.id},
                                {"name", b.name},
                                {"manufacturer", b.manufacturer},
                                {"caliber", b.caliber},
                                {"massGr", b.mass_gr},
                                {"diameterIn", b.diameter_in},
                                {"dragKind", b.drag_kind},
                                {"dragTable", b.drag_table},
                                {"bc", b.bc},
                                {"bcBands", b.bc_bands},
                                {"source", b.source}});
             }
             return out;
         }},
        {"bulletForm",
         [](I& s, const json& a) -> json {
             const Id id = IdOf(a);
             return ToJson(id == 0 ? al::BulletForm{} : Must(al::LoadBulletForm(s.db, id)));
         }},
        {"saveBullet",
         [](I& s, const json& a) -> json {
             const Id id = Must(al::SaveBulletForm(s.db, BulletFrom(a.value("form", json::object()))));
             s.ReloadArmory(); // cartridges show their bullet
             return {{"id", id}};
         }},
        {"deleteBullet",
         [](I& s, const json& a) -> json {
             Must(al::DeleteBullet(s.db, IdOf(a)));
             return json::object();
         }},
        {"importFiles",
         [](I& s, const json& a) -> json {
             int imported = 0;
             json problems = json::array();
             if (a.contains("files")) {
                 for (const json& f : a.at("files")) {
                     const std::string name = Str(f, "name");
                     auto id = al::ImportFile(s.db, name, Str(f, "content"));
                     if (id) {
                         ++imported;
                     } else {
                         problems.push_back({{"file", name}, {"message", id.error().message}});
                     }
                 }
             }
             s.Refresh();
             return {{"imported", imported}, {"problems", problems}};
         }},
    });
}

} // namespace ballistics::bridge
