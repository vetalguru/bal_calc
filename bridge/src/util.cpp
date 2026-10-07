#include <cctype>
#include <chrono>
#include <sstream>

#include "internal.h"

namespace ballistics::bridge::detail {

double NowUnix() {
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

double Num(const json& j, const char* key, double fallback) {
    return j.contains(key) && j.at(key).is_number() ? j.at(key).get<double>() : fallback;
}
Id IdOf(const json& j, const char* key) {
    return j.contains(key) && j.at(key).is_number() ? j.at(key).get<Id>() : 0;
}
std::string Str(const json& j, const char* key, const std::string& fallback) {
    return j.contains(key) && j.at(key).is_string() ? j.at(key).get<std::string>() : fallback;
}
std::string Trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    const auto last = s.find_last_not_of(" \t\r\n");
    return first == std::string::npos ? std::string{} : s.substr(first, last - first + 1);
}
bool Bool(const json& j, const char* key, bool fallback) {
    return j.contains(key) && j.at(key).is_boolean() ? j.at(key).get<bool>() : fallback;
}

std::string Lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// Catalog items whose maker, model and calibre contain every word of the filter.
json Matching(const json& catalog, const std::string& filter) {
    std::vector<std::string> words;
    std::istringstream in(Lower(filter));
    for (std::string w; in >> w;) {
        words.push_back(w);
    }
    json out = json::array();
    for (const json& item : catalog) {
        const std::string text = Lower(item.value("maker", "") + " " + item.value("model", "") +
                                       " " + item.value("caliber", ""));
        bool all = true;
        for (const std::string& w : words) {
            all = all && text.find(w) != std::string::npos;
        }
        if (all) {
            out.push_back(item);
        }
    }
    return out;
}

// Pictures travel through the JSON as base64 (RFC 4648, with padding).
std::string ToBase64(const std::vector<std::uint8_t>& data) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < data.size(); i += 3) {
        const std::uint32_t n = (std::uint32_t{data[i]} << 16) |
                                (i + 1 < data.size() ? std::uint32_t{data[i + 1]} << 8 : 0) |
                                (i + 2 < data.size() ? std::uint32_t{data[i + 2]} : 0);
        out += kAlphabet[(n >> 18) & 63];
        out += kAlphabet[(n >> 12) & 63];
        out += i + 1 < data.size() ? kAlphabet[(n >> 6) & 63] : '=';
        out += i + 2 < data.size() ? kAlphabet[n & 63] : '=';
    }
    return out;
}

std::vector<std::uint8_t> FromBase64(const std::string& text) {
    std::vector<std::uint8_t> out;
    std::uint32_t bits = 0;
    int count = 0;
    for (const char c : text) {
        int v = -1;
        if (c >= 'A' && c <= 'Z') {
            v = c - 'A';
        } else if (c >= 'a' && c <= 'z') {
            v = c - 'a' + 26;
        } else if (c >= '0' && c <= '9') {
            v = c - '0' + 52;
        } else if (c == '+' || c == '-') {
            v = 62;
        } else if (c == '/' || c == '_') {
            v = 63;
        } else if (c == '=' || std::isspace(static_cast<unsigned char>(c))) {
            continue;
        } else {
            throw Failure("The picture is damaged.");
        }
        bits = (bits << 6) | static_cast<std::uint32_t>(v);
        if (++count == 4) {
            out.push_back(static_cast<std::uint8_t>(bits >> 16));
            out.push_back(static_cast<std::uint8_t>(bits >> 8));
            out.push_back(static_cast<std::uint8_t>(bits));
            bits = 0;
            count = 0;
        }
    }
    if (count == 2) {
        out.push_back(static_cast<std::uint8_t>(bits >> 4));
    } else if (count == 3) {
        out.push_back(static_cast<std::uint8_t>(bits >> 10));
        out.push_back(static_cast<std::uint8_t>(bits >> 2));
    }
    return out;
}

// "rifle" or "cartridge": what may have a picture.
std::string PhotoKind(const json& a) {
    const std::string kind = Str(a, "kind");
    if (kind != "rifle" && kind != "cartridge") {
        throw Failure("Unknown kind of record.");
    }
    return kind;
}

// Invalid UTF-8 (a hand-edited import, say) is replaced, never thrown.
std::string Dump(const json& j) { return j.dump(-1, ' ', false, json::error_handler_t::replace); }

Id FirstId(const json& list) { return list.empty() ? 0 : list.front().at("id").get<Id>(); }
bool Contains(const json& list, Id id) {
    return std::any_of(list.begin(), list.end(),
                       [id](const json& v) { return v.at("id").get<Id>() == id; });
}

}  // namespace ballistics::bridge::detail
