// Rifles and cartridges as files to share.
#include "impl.h"

namespace ballistics::bridge {

using namespace detail;

std::string Api::Impl::ExportFileName(const std::string& kind, Id id) const {
    const json& list = kind == "rifle" ? rifles : cartridges;
    for (const json& v : list) {
        if (v.at("id").get<Id>() == id) {
            std::string name = v.at("name").get<std::string>();
            for (char& c : name) {
                if (std::string("\\/:*?\"<>|").find(c) != std::string::npos) {
                    c = '_';
                }
            }
            return name + ".balcalc.json";
        }
    }
    return kind + ".balcalc.json";
}

void Api::Impl::AddSharingHandlers(HandlerMap& h) {
    using I = Api::Impl;
    h.insert({
        // Sharing
        {"exportJson",
         [](I& s, const json& a) -> json {
             const std::string kind = Str(a, "kind");
             const Id id = IdOf(a);
             if (kind != "rifle" && kind != "cartridge") {
                 throw Failure("Unknown export kind: " + kind);
             }
             const std::string text = Must(kind == "rifle" ? al::ExportRifleJson(s.db, id)
                                                           : al::ExportCartridgeJson(s.db, id));
             return {{"json", text}, {"fileName", s.ExportFileName(kind, id)}};
         }},
        {"importShared",
         [](I& s, const json& a) -> json {
             const al::Imported im = Must(al::ImportShareJson(s.db, Str(a, "text")));
             // What the file brought in becomes current.
             const Id rifle = im.rifle_id != 0 ? im.rifle_id : s.rifle_id;
             const Id cartridge = im.cartridge_id != 0 ? im.cartridge_id : s.cartridge_id;
             s.rifle_id = 0;
             s.ReloadArmory();
             s.Select(rifle, cartridge);
             return s.State();
         }},
    });
}

} // namespace ballistics::bridge
