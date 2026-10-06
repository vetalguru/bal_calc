#include <ballistics/applogic/importers.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <regex>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>
#include <sqlite_manager/transaction.h>
#include <tinyxml2.h>

#include <ballistics/applogic/library.h>
#include <ballistics/applogic/profile_io.h>
#include <ballistics/storage/repository.h>
#include <ballistics/units.h>

namespace ballistics::applogic {

namespace {

using nlohmann::json;
using sqlite_manager::Error;
using sqlite_manager::ErrorCode;
using storage::BulletRecord;
using storage::CartridgeRecord;
using storage::Database;
using storage::DragCurveRecord;
using storage::Repository;
using storage::ReticleRecord;

Error Bad(const std::string& what) { return Error(ErrorCode::kFormat, 0, what); }

// A transaction unless the caller already has one open (SQLite does not
// nest them); Commit() is then a no-op and the caller commits.
class Scope {
public:
    explicit Scope(Database& db) {
        if (!db.connection().InTransaction()) {
            auto t = sqlite_manager::Transaction::Begin(db.connection());
            if (t) {
                txn_ = std::move(t).value();
            } else {
                error_ = t.error();
            }
        }
    }
    const std::optional<Error>& error() const { return error_; }
    Status Commit() { return txn_.IsActive() ? txn_.Commit() : sqlite_manager::Ok(); }

private:
    sqlite_manager::Transaction txn_;
    std::optional<Error> error_;
};

std::string Trim(std::string s) {
    const auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool EndsWith(const std::string& s, const std::string& tail) {
    return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

// "11.28000000g" -> {11.28, "g"}; false when there is no leading number.
bool SplitQuantity(const char* text, double& value, std::string& unit) {
    if (text == nullptr) {
        return false;
    }
    char* end = nullptr;
    value = std::strtod(text, &end);
    if (end == text) {
        return false;
    }
    unit = Lower(Trim(end));
    return std::isfinite(value);
}

bool ToKg(const char* text, double& kg) {
    double v = 0.0;
    std::string u;
    if (!SplitQuantity(text, v, u)) {
        return false;
    }
    if (u == "gr" || u == "grain") {
        kg = units::GrainToKg(v);
    } else if (u == "g") {
        kg = v / 1000.0;
    } else if (u == "kg") {
        kg = v;
    } else if (u == "lb") {
        kg = v * units::kKgPerPound;
    } else {
        return false;
    }
    return true;
}

bool ToMeters(const char* text, double& m) {
    double v = 0.0;
    std::string u;
    if (!SplitQuantity(text, v, u)) {
        return false;
    }
    if (u == "in" || u == "inch") {
        m = units::InchToM(v);
    } else if (u == "mm") {
        m = v / 1000.0;
    } else if (u == "cm") {
        m = v / 100.0;
    } else if (u == "m") {
        m = v;
    } else if (u == "ft") {
        m = units::FootToM(v);
    } else if (u == "yd") {
        m = units::YardToM(v);
    } else {
        return false;
    }
    return true;
}

bool ToMps(const char* text, double& mps) {
    double v = 0.0;
    std::string u;
    if (!SplitQuantity(text, v, u)) {
        return false;
    }
    if (u == "ft/s" || u == "fps") {
        mps = units::FpsToMps(v);
    } else if (u == "m/s" || u == "mps") {
        mps = v;
    } else {
        return false;
    }
    return true;
}

// Angle in milliradians from "1.5mil" / "3moa" / "0.5mrad".
bool ToMrad(const char* text, double& mrad) {
    double v = 0.0;
    std::string u;
    if (!SplitQuantity(text, v, u)) {
        return false;
    }
    if (u == "mil" || u == "mrad") {
        mrad = v;
    } else if (u == "moa") {
        mrad = units::RadToMrad(units::MoaToRad(v));
    } else if (u == "rad") {
        mrad = v * 1000.0;
    } else if (u == "deg") {
        mrad = units::RadToMrad(units::DegToRad(v));
    } else {
        return false;
    }
    return true;
}

double Mrad(const tinyxml2::XMLElement* e, const char* attr, double fallback = 0.0) {
    double v = fallback;
    if (const char* a = e->Attribute(attr)) {
        ToMrad(a, v);
    }
    return v;
}

bool Flag(const tinyxml2::XMLElement* e, const char* attr) {
    const char* a = e->Attribute(attr);
    return a != nullptr && Lower(a) == "true";
}

template <typename T>
bool Exists(Database& db, const std::string& name, const std::string& source) {
    auto list = Repository<T>(db).List(name);
    if (!list) {
        return false;
    }
    for (const auto& r : list.value()) {
        if (r.name == name && r.source == source) {
            return true;
        }
    }
    return false;
}

// Bore diameter from a caliber label: ".308", "7.62x39", "45 ACP",
// "12 GA"; 0 if it cannot be told.
double DiameterFromCaliber(const std::string& caliber) {
    double v = 0.0;
    std::string rest;
    const std::string trimmed = Trim(caliber);
    const char* text = trimmed.c_str();
    if (*text == '.') {
        // ".308" style
        return SplitQuantity(text, v, rest) && v > 0.0 ? units::InchToM(v) : 0.0;
    }
    if (!SplitQuantity(text, v, rest) || !(v > 0.0)) {
        return 0.0;
    }
    if (rest.find("ga") != std::string::npos) {
        // Shotgun gauges (bore diameter, inches).
        constexpr std::pair<int, double> kGauges[] = {{10, 0.775}, {12, 0.729}, {16, 0.662},
                                                      {20, 0.615}, {28, 0.550}, {410, 0.410}};
        for (const auto& [gauge, inches] : kGauges) {
            if (static_cast<int>(v) == gauge) {
                return units::InchToM(inches);
            }
        }
        return 0.0;
    }
    if (v < 1.0) {
        return units::InchToM(v); // "0.308"
    }
    if (v <= 30.0) {
        return v / 1000.0; // millimetres: "7.62x39", "9mm"
    }
    if (v < 100.0) {
        return units::InchToM(v / 100.0); // "45 ACP", "50 BMG"
    }
    return units::InchToM(v / 1000.0); // "308 Win", "410"
}

// Caliber label from a diameter, e.g. 0.00782 m -> ".308".
std::string CaliberFromDiameter(double d) {
    std::ostringstream s;
    const double inches = units::MToInch(d);
    s << "." << static_cast<int>(std::lround(inches * 1000.0));
    return s.str();
}

// --- Published bullets (balcalc-bullets JSON) ------------------------------

constexpr const char* kBulletsFormat = "balcalc-bullets";

Result<Id> ImportBulletsJson(Database& db, const json& doc, int* imported, int* skipped) {
    Id last = 0;
    for (const json& j : doc.at("bullets")) {
        BulletRecord b;
        b.name = j.at("name").get<std::string>();
        b.manufacturer = j.value("manufacturer", "");
        b.caliber = j.value("caliber", "");
        b.mass_kg = units::GrainToKg(j.at("weight_gr").get<double>());
        b.diameter_m = units::InchToM(j.at("diameter_in").get<double>());
        b.length_m = units::InchToM(j.value("length_in", 0.0));
        b.source = kSourcePublished;
        b.notes = j.value("reference", "");
        // Bands (BCs by velocity) describe the bullet better than one BC.
        const char* bands = j.contains("g7_bands") ? "g7_bands" : j.contains("g1_bands") ? "g1_bands" : nullptr;
        if (bands != nullptr) {
            b.drag_table = bands[1] == '7' ? "G7" : "G1";
            b.drag_kind = storage::kDragKindMultiBc;
            for (const json& band : j.at(bands)) {
                b.bc_bands.push_back({units::FpsToMps(band.at(0).get<double>()), band.at(1).get<double>()});
            }
        } else if (j.contains("g7")) {
            b.drag_table = "G7";
            b.bc = j.at("g7").get<double>();
        } else {
            b.drag_table = "G1";
            b.bc = j.at("g1").get<double>();
        }
        if (j.contains("g1") && j.contains("g7")) {
            b.notes += (b.notes.empty() ? "" : "\n") + std::string("G1 BC ") +
                       std::to_string(j.at("g1").get<double>());
        }
        if (Exists<BulletRecord>(db, b.name, b.source)) {
            ++*skipped;
            continue;
        }
        auto id = Repository<BulletRecord>(db).Save(b);
        if (!id) {
            return id.error();
        }
        last = id.value();
        ++*imported;
    }
    return last;
}

} // namespace

Result<AmmoFile> ParseAmmo(const std::string& xml) {
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) {
        return Bad("Not an .ammo file: invalid XML.");
    }
    const tinyxml2::XMLElement* e = doc.FirstChildElement("ammo-info-ex");
    if (e == nullptr) {
        return Bad("Not an .ammo file: no <ammo-info-ex> element.");
    }
    AmmoFile f;
    BulletRecord& b = f.bullet;
    b.name = e->Attribute("name") ? e->Attribute("name") : "";
    b.caliber = e->Attribute("caliber") ? e->Attribute("caliber") : "";
    b.manufacturer = e->Attribute("source") ? e->Attribute("source") : "";
    b.source = kSourceAmmoFile;
    if (const char* type = e->Attribute("bullet-type"); type && *type) {
        b.notes = std::string("Type: ") + type;
    }
    if (b.name.empty()) {
        return Bad("The .ammo file has no name.");
    }
    if (!ToKg(e->Attribute("bullet-weight"), b.mass_kg)) {
        return Bad(b.name + ": bullet weight is missing or has unknown units.");
    }
    if (!ToMeters(e->Attribute("bullet-diameter"), b.diameter_m)) {
        // A few files omit it; fall back to the caliber text (".308", "7.62x51").
        b.diameter_m = DiameterFromCaliber(b.caliber);
        if (!(b.diameter_m > 0.0)) {
            return Bad(b.name + ": bullet diameter is missing.");
        }
    }
    if (const char* len = e->Attribute("bullet-length")) {
        ToMeters(len, b.length_m);
    }
    const char* table = e->Attribute("table");
    b.drag_table = table ? table : "G1";
    double bc = 0.0;
    if (e->QueryDoubleAttribute("bc", &bc) != tinyxml2::XML_SUCCESS || !(bc > 0.0)) {
        return Bad(b.name + ": BC is missing.");
    }
    b.bc = bc;

    CartridgeRecord& c = f.cartridge;
    c.name = b.name;
    c.source = kSourceAmmoFile;
    if (!ToMps(e->Attribute("muzzle-velocity"), c.muzzle_velocity_mps)) {
        return Bad(b.name + ": muzzle velocity is missing or has unknown units.");
    }
    if (const char* barrel = e->Attribute("barrel-length")) {
        ToMeters(barrel, c.barrel_length_m);
    }
    return f;
}

Result<DrgFile> ParseDrg(const std::string& text) {
    std::istringstream in(text);
    std::string header;
    if (!std::getline(in, header)) {
        return Bad("Empty .drg file.");
    }
    if (header.find("Encoded Data") != std::string::npos) {
        return Bad("This .drg file is encoded and cannot be read.");
    }
    // Header: TYPE, name (may contain commas), mass, diameter, length[,
    // data type]. The three numbers are the last ones on the line, whatever
    // separates them (one Lapua file has "0.007830. 0.03370").
    const auto comma = header.find(',');
    if (comma == std::string::npos) {
        return Bad("Not a .drg file: unexpected header.");
    }
    const std::string rest = header.substr(comma + 1);
    static const std::regex kNumber(R"([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)");
    std::vector<std::smatch> numbers;
    for (auto it = std::sregex_iterator(rest.begin(), rest.end(), kNumber); it != std::sregex_iterator();
         ++it) {
        numbers.push_back(*it);
    }
    if (numbers.size() < 3) {
        return Bad("Not a .drg file: unexpected header.");
    }
    DrgFile f;
    f.kind = Trim(header.substr(0, comma));
    const std::smatch& first = numbers[numbers.size() - 3];
    f.name = Trim(rest.substr(0, static_cast<std::size_t>(first.position(0))));
    while (!f.name.empty() && (f.name.back() == ',' || std::isspace(static_cast<unsigned char>(f.name.back())))) {
        f.name.pop_back();
    }
    f.mass_kg = std::atof(numbers[numbers.size() - 3].str().c_str());
    f.diameter_m = std::atof(numbers[numbers.size() - 2].str().c_str());
    f.length_m = std::atof(numbers[numbers.size() - 1].str().c_str());
    if (f.name.empty() || !(f.mass_kg > 0.0 && f.diameter_m > 0.0)) {
        return Bad("Not a .drg file: name, mass and diameter are required.");
    }

    for (std::string line; std::getline(in, line);) {
        std::istringstream ls(line);
        double cd = 0.0, mach = 0.0;
        if (ls >> cd >> mach) {
            f.points.push_back({mach, cd});
        }
    }
    std::sort(f.points.begin(), f.points.end(),
              [](const DragPoint& a, const DragPoint& b) { return a.mach < b.mach; });
    f.points.erase(std::unique(f.points.begin(), f.points.end(),
                               [](const DragPoint& a, const DragPoint& b) { return a.mach == b.mach; }),
                   f.points.end());
    if (f.points.size() < 2) {
        return Bad(f.name + ": not enough drag points.");
    }
    return f;
}

Result<ReticleRecord> ParseReticle(const std::string& xml) {
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) {
        return Bad("Not a .reticle file: invalid XML.");
    }
    const tinyxml2::XMLElement* r = doc.FirstChildElement("reticle");
    if (r == nullptr) {
        return Bad("Not a .reticle file: no <reticle> element.");
    }
    ReticleRecord rec;
    rec.name = r->Attribute("name") ? r->Attribute("name") : "Reticle";
    rec.source = kSourceReticleFile;
    rec.focal_plane = "ffp";
    {
        const char* sx = r->Attribute("size-x");
        rec.units = (sx && Lower(sx).find("moa") != std::string::npos) ? "moa" : "mrad";
    }
    json def;
    def["size"] = {Mrad(r, "size-x"), Mrad(r, "size-y")};
    def["zero"] = {Mrad(r, "zero-x"), Mrad(r, "zero-y")};
    json elements = json::array();
    if (const auto* list = r->FirstChildElement("elements")) {
        for (const auto* e = list->FirstChildElement(); e; e = e->NextSiblingElement()) {
            const std::string tag = e->Name();
            if (tag == "reticle-line") {
                elements.push_back({{"t", "line"},
                                    {"x1", Mrad(e, "start-x")},
                                    {"y1", Mrad(e, "start-y")},
                                    {"x2", Mrad(e, "end-x")},
                                    {"y2", Mrad(e, "end-y")},
                                    {"w", Mrad(e, "line-width")}});
            } else if (tag == "reticle-circle") {
                elements.push_back({{"t", "circle"},
                                    {"x", Mrad(e, "center-x")},
                                    {"y", Mrad(e, "center-y")},
                                    {"r", Mrad(e, "radius")},
                                    {"w", Mrad(e, "line-width")},
                                    {"fill", Flag(e, "fill")}});
            } else if (tag == "reticle-text") {
                elements.push_back({{"t", "text"},
                                    {"x", Mrad(e, "position-x")},
                                    {"y", Mrad(e, "position-y")},
                                    {"h", Mrad(e, "text-height")},
                                    {"s", e->Attribute("text") ? e->Attribute("text") : ""}});
            } else if (tag == "reticle-path") {
                json d = json::array();
                if (const auto* steps = e->FirstChildElement("elements")) {
                    for (const auto* s = steps->FirstChildElement(); s; s = s->NextSiblingElement()) {
                        const std::string st = s->Name();
                        const double x = Mrad(s, "position-x");
                        const double y = Mrad(s, "position-y");
                        if (st == "reticle-path-move-to") {
                            d.push_back({"M", x, y});
                        } else if (st == "reticle-path-line-to") {
                            d.push_back({"L", x, y});
                        } else if (st == "reticle-path-arc") {
                            d.push_back({"A", x, y, Mrad(s, "radius"), Flag(s, "clockwise"),
                                         Flag(s, "major-arc")});
                        }
                    }
                }
                elements.push_back({{"t", "path"},
                                    {"fill", Flag(e, "fill")},
                                    {"w", Mrad(e, "line-width", 0.05)},
                                    {"d", d}});
            }
        }
    }
    def["elements"] = elements;
    json bdc = json::array();
    if (const auto* list = r->FirstChildElement("bdc")) {
        for (const auto* e = list->FirstChildElement("bdc"); e; e = e->NextSiblingElement("bdc")) {
            bdc.push_back({{"x", Mrad(e, "position-x")},
                           {"y", Mrad(e, "position-y")},
                           {"offset", Mrad(e, "text-offset")},
                           {"h", Mrad(e, "text-height")}});
        }
    }
    def["bdc"] = bdc;
    rec.definition = def.dump();
    return rec;
}

Result<Id> ImportAmmo(Database& db, const std::string& xml) {
    auto f = ParseAmmo(xml);
    if (!f) {
        return f.error();
    }
    Scope txn(db);
    if (txn.error()) {
        return *txn.error();
    }
    AmmoFile a = std::move(f).value();
    if (auto id = Repository<BulletRecord>(db).Save(a.bullet); !id) {
        return id.error();
    }
    a.cartridge.bullet_id = a.bullet.id;
    if (auto id = Repository<CartridgeRecord>(db).Save(a.cartridge); !id) {
        return id.error();
    }
    if (auto s = txn.Commit(); !s) {
        return s.error();
    }
    return a.bullet.id;
}

Result<Id> ImportDrg(Database& db, const std::string& text) {
    auto f = ParseDrg(text);
    if (!f) {
        return f.error();
    }
    Scope txn(db);
    if (txn.error()) {
        return *txn.error();
    }
    DragCurveRecord curve;
    curve.name = f.value().name;
    curve.source = kSourceDrgFile;
    curve.notes = f.value().kind + " radar drag function";
    curve.points = f.value().points;
    if (auto id = Repository<DragCurveRecord>(db).Save(curve); !id) {
        return id.error();
    }
    BulletRecord b;
    b.name = f.value().name;
    b.caliber = CaliberFromDiameter(f.value().diameter_m);
    b.manufacturer = f.value().name.find("Lapua") != std::string::npos ? "Lapua" : "";
    b.diameter_m = f.value().diameter_m;
    b.mass_kg = f.value().mass_kg;
    b.length_m = f.value().length_m;
    b.drag_kind = storage::kDragKindCurve;
    b.drag_table = "";
    b.curve_id = curve.id;
    b.source = kSourceDrgFile;
    b.notes = "Doppler radar drag curve (" + std::to_string(curve.points.size()) + " points)";
    if (auto id = Repository<BulletRecord>(db).Save(b); !id) {
        return id.error();
    }
    if (auto s = txn.Commit(); !s) {
        return s.error();
    }
    return b.id;
}

Result<Id> ImportReticle(Database& db, const std::string& xml) {
    auto r = ParseReticle(xml);
    if (!r) {
        return r.error();
    }
    ReticleRecord rec = std::move(r).value();
    return Repository<ReticleRecord>(db).Save(rec);
}

Result<Id> ImportFile(Database& db, const std::string& file_name, const std::string& content) {
    const std::string name = Lower(file_name);
    if (EndsWith(name, ".ammo")) {
        return ImportAmmo(db, content);
    }
    if (EndsWith(name, ".drg")) {
        return ImportDrg(db, content);
    }
    if (EndsWith(name, ".reticle")) {
        return ImportReticle(db, content);
    }
    if (EndsWith(name, ".json")) {
        const json doc = json::parse(content, nullptr, false);
        if (!doc.is_discarded() && doc.is_object() && doc.value("format", "") == kBulletsFormat) {
            int imported = 0, skipped = 0;
            try {
                return ImportBulletsJson(db, doc, &imported, &skipped);
            } catch (const json::exception& e) {
                return Bad(std::string("Bullet list: ") + e.what());
            }
        }
        auto imported = ImportShareJson(db, content);
        if (!imported) {
            return imported.error();
        }
        return imported.value().rifle_id != 0 ? imported.value().rifle_id
                                              : imported.value().cartridge_id;
    }
    return Bad("Unknown file type: " + file_name);
}

Result<SeedReport> SeedLibrary(Database& db, const std::vector<SeedFile>& files, int seed_version) {
    constexpr const char* kKey = "seed.version";
    SeedReport report;
    auto done = storage::GetSetting(db, kKey);
    if (!done) {
        return done.error();
    }
    if (done.value() && std::atoi(done.value()->c_str()) >= seed_version) {
        return report;
    }
    // One transaction for the whole seed: far fewer disk syncs on phones.
    Scope txn(db);
    if (txn.error()) {
        return *txn.error();
    }
    for (const SeedFile& f : files) {
        const std::string name = Lower(f.name);
        // Skip what an earlier seed already brought in.
        if (EndsWith(name, ".ammo")) {
            auto parsed = ParseAmmo(f.content);
            if (parsed && Exists<BulletRecord>(db, parsed.value().bullet.name, kSourceAmmoFile)) {
                ++report.skipped;
                continue;
            }
        } else if (EndsWith(name, ".drg")) {
            auto parsed = ParseDrg(f.content);
            if (!parsed) {
                ++report.skipped; // encoded or unreadable
                continue;
            }
            if (Exists<BulletRecord>(db, parsed.value().name, kSourceDrgFile)) {
                ++report.skipped;
                continue;
            }
        } else if (EndsWith(name, ".reticle")) {
            auto parsed = ParseReticle(f.content);
            if (parsed && Exists<ReticleRecord>(db, parsed.value().name, kSourceReticleFile)) {
                ++report.skipped;
                continue;
            }
        } else if (EndsWith(name, ".json")) {
            const json doc = json::parse(f.content, nullptr, false);
            if (!doc.is_discarded() && doc.value("format", "") == kBulletsFormat) {
                try {
                    auto r = ImportBulletsJson(db, doc, &report.imported, &report.skipped);
                    if (!r) {
                        report.problems.push_back(f.name + ": " + r.error().message);
                    }
                } catch (const json::exception& e) {
                    report.problems.push_back(f.name + ": " + e.what());
                }
                continue;
            }
        }
        auto id = ImportFile(db, f.name, f.content);
        if (id) {
            ++report.imported;
        } else {
            ++report.skipped;
            report.problems.push_back(f.name + ": " + id.error().message);
        }
    }
    if (auto s = storage::SetSetting(db, kKey, std::to_string(seed_version)); !s) {
        return s.error();
    }
    if (auto s = txn.Commit(); !s) {
        return s.error();
    }
    return report;
}

} // namespace ballistics::applogic
