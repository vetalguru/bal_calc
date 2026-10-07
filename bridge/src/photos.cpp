// Pictures of rifles and cartridges.
#include "impl.h"

namespace ballistics::bridge {

using namespace detail;

void Api::Impl::AddPhotosHandlers(HandlerMap& h) {
    using I = Api::Impl;
    h.insert({
        // Pictures of rifles and cartridges
        {"photos",
         [](I& s, const json& a) -> json {
             const std::string kind = PhotoKind(a);
             json out = json::object();
             for (const Id id : Must(bs::PhotoOwners(s.db, kind))) {
                 if (auto image = Must(bs::GetPhoto(s.db, kind, id))) {
                     out[std::to_string(id)] = ToBase64(*image);
                 }
             }
             return out;
         }},
        {"photo",
         [](I& s, const json& a) -> json {
             const auto image = Must(bs::GetPhoto(s.db, PhotoKind(a), IdOf(a)));
             return image ? ToBase64(*image) : std::string{};
         }},
        {"setPhoto",
         [](I& s, const json& a) -> json {
             const std::string kind = PhotoKind(a);
             const Id id = IdOf(a);
             const bool exists = kind == "rifle"
                                     ? Must(bs::Repository<bs::RifleRecord>(s.db).Get(id)).has_value()
                                     : Must(bs::Repository<bs::CartridgeRecord>(s.db).Get(id)).has_value();
             if (!exists) {
                 throw Failure("Save the record first.");
             }
             const std::vector<std::uint8_t> image = FromBase64(Str(a, "image"));
             if (image.size() > kMaxPhotoBytes) {
                 throw Failure("The picture is too large.");
             }
             Must(bs::SetPhoto(s.db, kind, id, image));
             return json::object();
         }},
    });
}

} // namespace ballistics::bridge
