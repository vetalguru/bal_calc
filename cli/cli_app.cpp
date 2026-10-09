#include "cli_app.h"

#include <ballistics/applogic/armory.h>
#include <ballistics/applogic/importers.h>
#include <ballistics/atmosphere.h>
#include <ballistics/effects.h>
#include <ballistics/solver.h>
#include <ballistics/storage/database.h>
#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>
#include <ballistics/units.h>
#include <ballistics/version.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace balcli {

namespace {

namespace al = ballistics::applogic;
namespace bs = ballistics::storage;
namespace u = ballistics::units;
using ballistics::Shot;
using ballistics::Trajectory;
using ballistics::TrajectoryPoint;

constexpr const char* kUsage = R"(bal-cli - ballistic calculator

Usage: bal-cli [--db FILE] COMMAND [options]

Commands:
  version                 engine and SQLite versions
  demo                    add a demo rifle and cartridge (M24, M118LR) to --db
  rifles, cartridges      list the rifles / the user's cartridges in --db
  profiles                list the rifle + cartridge pairs in --db
  import FILE...          import .ammo/.drg/.reticle/.json files into --db
  seed DIR                import the starter library (data/seed) into --db
  table --rifle ID --cartridge ID  (or --profile ID of a pair)
                          range card for a stored rifle and cartridge
  quick                   range card without a database:
      --bc BC --drag G1|G7|...  --v0 M/S  [--mass-gr GR --diam-in IN --len-in IN]
      [--twist-in IN (negative = left)] [--sight-cm CM] [--zero-m M]

Conditions (table, quick):
  --temp C  --pressure HPA (station) | --qnh HPA  --alt M  --humidity PCT
  --powder-temp C  --look DEG  --cant DEG  --lat DEG  --azimuth DEG
  --wind SPEED@DIR[:UNTIL]   m/s, from DIR degrees (0 = headwind, 90 = from
                             the right) or clock (3h, 10h30), zone end in m;
                             repeat for zones
Output:
  --from M (0) --to M (1000) --step M (100)  --units mrad|moa (mrad)  --csv
)";

// Command line split into flags and repeated --wind values.
struct Args {
    std::string command;
    std::vector<std::string> files;  // positional arguments after the command
    std::map<std::string, std::string> opts;
    std::vector<std::string> winds;
    bool csv = false;
};

bool ParseArgs(const std::vector<std::string>& in, Args& a, std::ostream& err) {
    for (std::size_t i = 0; i < in.size(); ++i) {
        const std::string& s = in[i];
        if (s == "--csv") {
            a.csv = true;
        } else if (s == "-h" || s == "--help") {
            a.command = "help";
        } else if (s.rfind("--", 0) == 0) {
            if (i + 1 >= in.size()) {
                err << "missing value for " << s << "\n";
                return false;
            }
            if (s == "--wind") {
                a.winds.push_back(in[++i]);
            } else {
                a.opts[s.substr(2)] = in[++i];
            }
        } else if (a.command.empty()) {
            a.command = s;
        } else {
            a.files.push_back(s);
        }
    }
    return true;
}

bool Number(const std::string& text, double& value) {
    try {
        std::size_t used = 0;
        value = std::stod(text, &used);
        return used == text.size();
    } catch (...) {
        return false;
    }
}

// Reads an optional numeric flag; false (with a message) if it is malformed.
bool Opt(const Args& a, const char* name, std::optional<double>& value, std::ostream& err) {
    const auto it = a.opts.find(name);
    if (it == a.opts.end()) {
        return true;
    }
    double v = 0.0;
    if (!Number(it->second, v)) {
        err << "--" << name << ": not a number: " << it->second << "\n";
        return false;
    }
    value = v;
    return true;
}

// Conditions from flags, with ICAO sea level as the default.
bool ReadConditions(const Args& a, bs::ConditionsRecord& c, std::ostream& err) {
    std::optional<double> temp, pressure, qnh, alt, humidity, powder, look, cant, lat, az;
    if (!Opt(a, "temp", temp, err) || !Opt(a, "pressure", pressure, err) ||
        !Opt(a, "qnh", qnh, err) || !Opt(a, "alt", alt, err) ||
        !Opt(a, "humidity", humidity, err) || !Opt(a, "powder-temp", powder, err) ||
        !Opt(a, "look", look, err) || !Opt(a, "cant", cant, err) || !Opt(a, "lat", lat, err) ||
        !Opt(a, "azimuth", az, err)) {
        return false;
    }
    const double altitude = alt.value_or(0.0);
    c.atmosphere = ballistics::StandardAtmosphere(altitude);
    if (temp) {
        c.atmosphere.temperature_k = u::CToK(*temp);
    }
    if (pressure) {
        c.atmosphere.pressure_pa = u::HpaToPa(*pressure);
    } else if (qnh) {
        c.atmosphere.pressure_pa =
            ballistics::StationPressureFromSeaLevel(u::HpaToPa(*qnh), altitude);
    }
    c.atmosphere.humidity = humidity.value_or(0.0) / 100.0;
    if (powder) {
        c.powder_temp_k = u::CToK(*powder);
    }
    c.look_angle_rad = u::DegToRad(look.value_or(0.0));
    c.cant_rad = u::DegToRad(cant.value_or(0.0));
    if (lat) {
        c.latitude_rad = u::DegToRad(*lat);
    }
    if (az) {
        c.azimuth_rad = u::DegToRad(*az);
    }
    for (const std::string& w : a.winds) {
        WindSpec spec;
        if (!ParseWind(w, spec)) {
            err << "--wind: expected SPEED@DIR[:UNTIL], got " << w << "\n";
            return false;
        }
        c.winds.push_back({spec.until_m, spec.speed_mps, u::DegToRad(spec.from_deg), 0.0});
    }
    return true;
}

struct TableSpec {
    double from = 0.0, to = 1000.0, step = 100.0;
    bool moa = false;
};

bool ReadTableSpec(const Args& a, TableSpec& t, std::ostream& err) {
    std::optional<double> from, to, step;
    if (!Opt(a, "from", from, err) || !Opt(a, "to", to, err) || !Opt(a, "step", step, err)) {
        return false;
    }
    t.from = from.value_or(0.0);
    t.to = to.value_or(1000.0);
    t.step = step.value_or(100.0);
    if (!(t.step > 0.0) || t.to < t.from || t.to > 5000.0) {
        err << "need 0 < --step and --from <= --to <= 5000\n";
        return false;
    }
    const auto units = a.opts.find("units");
    if (units != a.opts.end()) {
        if (units->second != "mrad" && units->second != "moa") {
            err << "--units: mrad or moa\n";
            return false;
        }
        t.moa = units->second == "moa";
    }
    return true;
}

std::string Fixed(double v, int decimals) {
    std::array<char, 64> buf{};
    std::snprintf(buf.data(), buf.size(), "%.*f", decimals, v);
    return buf.data();
}

void PrintTable(const Trajectory& traj, const TableSpec& spec, const bs::ScopeRecord* scope,
                bool csv, std::ostream& out) {
    const char* unit = spec.moa ? "MOA" : "MRAD";
    auto angle = [&](double rad) { return spec.moa ? u::RadToMoa(rad) : u::RadToMrad(rad); };

    std::vector<std::string> head = {"Range m", std::string("Elev ") + unit,
                                     std::string("Wind ") + unit};
    if (scope) {
        head.insert(head.end(), {"Elev clk", "Wind clk"});
    }
    head.insert(head.end(), {"Drop cm", "Windage cm", "V m/s", "Mach", "E J", "TOF s"});

    std::vector<std::vector<std::string>> rows;
    // An integer count, the range computed from it: no rounding error builds up.
    const auto r_steps = static_cast<int>(std::floor((spec.to - spec.from) / spec.step + 1e-9));
    for (int k = 0; k <= r_steps; ++k) {
        const double r = spec.from + spec.step * static_cast<double>(k);
        const auto pt = traj.AtSlantRange(r);
        if (!pt) {
            break;
        }
        std::vector<std::string> row = {Fixed(r, 0)};
        row.push_back(r > 0.0 ? Fixed(angle(pt->hold_elevation_rad), 2) : "-");
        row.push_back(r > 0.0 ? Fixed(angle(pt->hold_windage_rad), 2) : "-");
        if (scope) {
            row.push_back(
                r > 0.0 ? Fixed(bs::ToClicks(pt->hold_elevation_rad, scope->click_vertical_rad), 0)
                        : "-");
            row.push_back(
                r > 0.0 ? Fixed(bs::ToClicks(pt->hold_windage_rad, scope->click_horizontal_rad), 0)
                        : "-");
        }
        row.push_back(Fixed(pt->drop_m * 100.0, 1));
        row.push_back(Fixed(pt->windage_m * 100.0, 1));
        row.push_back(Fixed(pt->speed_mps, 1));
        row.push_back(Fixed(pt->mach, 3));
        row.push_back(Fixed(pt->energy_j, 0));
        row.push_back(Fixed(pt->time_s, 3));
        rows.push_back(std::move(row));
    }

    if (csv) {
        for (std::size_t i = 0; i < head.size(); ++i) {
            out << (i ? "," : "") << head[i];
        }
        out << "\n";
        for (const auto& row : rows) {
            for (std::size_t i = 0; i < row.size(); ++i) {
                out << (i ? "," : "") << row[i];
            }
            out << "\n";
        }
        return;
    }
    std::vector<std::size_t> width(head.size());
    for (std::size_t i = 0; i < head.size(); ++i) {
        width[i] = head[i].size();
        for (const auto& row : rows) {
            width[i] = std::max(width[i], row[i].size());
        }
    }
    auto line = [&](const std::vector<std::string>& cells) {
        for (std::size_t i = 0; i < cells.size(); ++i) {
            out << (i ? "  " : "") << std::string(width[i] - cells[i].size(), ' ') << cells[i];
        }
        out << "\n";
    };
    line(head);
    for (const auto& row : rows) {
        line(row);
    }
}

void PrintSummary(const Shot& shot, const Trajectory& traj, const ballistics::ZeroResult& zero,
                  std::ostream& out) {
    out << "V0 " << Fixed(shot.muzzle_velocity_mps, 1) << " m/s, air "
        << Fixed(u::KToC(shot.atmosphere.temperature_k), 1) << " C "
        << Fixed(shot.atmosphere.pressure_pa / 100.0, 1) << " hPa "
        << Fixed(shot.atmosphere.humidity * 100.0, 0) << " %, zero elevation "
        << Fixed(u::RadToMrad(zero.elevation_rad), 3) << " MRAD";
    if (traj.stability() > 0.0) {
        out << ", Sg " << Fixed(traj.stability(), 2);
        if (traj.stability() < 1.0) {
            out << " (UNSTABLE)";
        } else if (traj.stability() < 1.4) {
            out << " (marginal)";
        }
    }
    out << "\n\n";
}

int CmdTable(const Args& a, std::ostream& out, std::ostream& err) {
    const auto db_path = a.opts.find("db");
    std::optional<double> profile_id, rifle_id, cartridge_id;
    if (!Opt(a, "profile", profile_id, err) || !Opt(a, "rifle", rifle_id, err) ||
        !Opt(a, "cartridge", cartridge_id, err)) {
        return 2;
    }
    if (db_path == a.opts.end() || !(profile_id || (rifle_id && cartridge_id))) {
        err << "table needs --db FILE and --rifle ID --cartridge ID (or --profile ID)\n";
        return 2;
    }
    bs::ConditionsRecord cond;
    TableSpec spec;
    if (!ReadConditions(a, cond, err) || !ReadTableSpec(a, spec, err)) {
        return 2;
    }
    bs::Database db;
    if (auto s = db.Open(db_path->second); !s) {
        err << "cannot open " << db_path->second << ": " << s.error().message << "\n";
        return 1;
    }
    bs::Id id = static_cast<bs::Id>(profile_id.value_or(0.0));
    if (!profile_id) {
        auto pair = al::EnsureProfile(db, static_cast<bs::Id>(*rifle_id),
                                      static_cast<bs::Id>(*cartridge_id));
        if (!pair) {
            err << pair.error().message << "\n";
            return 1;
        }
        id = pair.value();
    }
    auto loaded = bs::LoadProfile(db, id);
    if (!loaded) {
        err << loaded.error().message << "\n";
        return 1;
    }
    auto sol = bs::Solve(loaded.value(), cond, spec.to + 1.0);
    if (!sol) {
        err << sol.error().message << "\n";
        return 1;
    }
    out << loaded.value().rifle.name << " / " << loaded.value().cartridge.name << "\n";
    PrintSummary(sol.value().shot, sol.value().trajectory, sol.value().zero, out);
    const std::optional<bs::ScopeRecord>& scope = loaded.value().scope;
    PrintTable(sol.value().trajectory, spec, scope ? &*scope : nullptr, a.csv, out);
    return 0;
}

int CmdQuick(const Args& a, std::ostream& out, std::ostream& err) {
    std::optional<double> bc, v0, mass, diam, len, twist, sight, zero;
    if (!Opt(a, "bc", bc, err) || !Opt(a, "v0", v0, err) || !Opt(a, "mass-gr", mass, err) ||
        !Opt(a, "diam-in", diam, err) || !Opt(a, "len-in", len, err) ||
        !Opt(a, "twist-in", twist, err) || !Opt(a, "sight-cm", sight, err) ||
        !Opt(a, "zero-m", zero, err)) {
        return 2;
    }
    if (!bc || !v0) {
        err << "quick needs --bc and --v0\n";
        return 2;
    }
    bs::LoadedProfile p;
    p.bullet.name = "quick";
    p.bullet.drag_kind = bs::kDragKindBc;
    const auto drag = a.opts.find("drag");
    p.bullet.drag_table = drag != a.opts.end() ? drag->second : "G7";
    p.bullet.bc = bc;
    p.bullet.mass_kg = u::GrainToKg(mass.value_or(0.0));
    p.bullet.diameter_m = u::InchToM(diam.value_or(0.0));
    p.bullet.length_m = u::InchToM(len.value_or(0.0));
    p.cartridge.muzzle_velocity_mps = *v0;
    p.rifle.twist_m = u::InchToM(twist.value_or(0.0));
    p.rifle.sight_height_m = sight.value_or(5.0) / 100.0;
    p.rifle.zero_range_m = zero.value_or(100.0);

    bs::ConditionsRecord cond;
    TableSpec spec;
    if (!ReadConditions(a, cond, err) || !ReadTableSpec(a, spec, err)) {
        return 2;
    }
    // Quick mode zeroes in the same air it shoots in.
    p.rifle.zero_atmosphere = cond.atmosphere;
    p.rifle.zero_powder_temp_k = cond.powder_temp_k.value_or(cond.atmosphere.temperature_k);
    p.cartridge.reference_powder_temp_k = p.rifle.zero_powder_temp_k;
    auto sol = bs::Solve(p, cond, spec.to + 1.0);
    if (!sol) {
        err << sol.error().message << "\n";
        return 1;
    }
    PrintSummary(sol.value().shot, sol.value().trajectory, sol.value().zero, out);
    PrintTable(sol.value().trajectory, spec, nullptr, a.csv, out);
    return 0;
}

// Lists rifles, the user's cartridges or the pairs.
int CmdList(const Args& a, std::ostream& out, std::ostream& err) {
    const auto db_path = a.opts.find("db");
    if (db_path == a.opts.end()) {
        err << a.command << " needs --db FILE\n";
        return 2;
    }
    bs::Database db;
    if (auto s = db.Open(db_path->second); !s) {
        err << "cannot open " << db_path->second << ": " << s.error().message << "\n";
        return 1;
    }
    if (a.command == "rifles") {
        auto list = bs::Repository<bs::RifleRecord>(db).List();
        if (!list) {
            err << list.error().message << "\n";
            return 1;
        }
        for (const auto& r : list.value()) {
            out << r.id << "  " << r.name << "  " << r.caliber << "  (zero "
                << Fixed(r.zero_range_m, 0) << " m)\n";
        }
        return 0;
    }
    if (a.command == "cartridges") {
        auto list = al::ListCartridges(db);
        if (!list) {
            err << list.error().message << "\n";
            return 1;
        }
        for (const auto& c : list.value()) {
            out << c.id << "  " << c.name << "  " << c.caliber << "  "
                << Fixed(c.muzzle_velocity_mps, 0) << " m/s  " << c.bullet_name << "\n";
        }
        return 0;
    }
    auto list = bs::Repository<bs::ProfileRecord>(db).List();
    if (!list) {
        err << list.error().message << "\n";
        return 1;
    }
    for (const auto& p : list.value()) {
        const auto r = bs::Repository<bs::RifleRecord>(db).Get(p.rifle_id);
        const auto c = bs::Repository<bs::CartridgeRecord>(db).Get(p.cartridge_id);
        const bs::RifleRecord* rifle = bs::Found(r);
        const bs::CartridgeRecord* cartridge = bs::Found(c);
        if (rifle && cartridge) {
            out << p.id << "  " << rifle->name << " / " << cartridge->name << "  (rifle "
                << p.rifle_id << ", cartridge " << p.cartridge_id << ")\n";
        }
    }
    return 0;
}

int CmdDemo(const Args& a, std::ostream& out, std::ostream& err) {
    const auto db_path = a.opts.find("db");
    if (db_path == a.opts.end()) {
        err << "demo needs --db FILE\n";
        return 2;
    }
    bs::Database db;
    if (auto s = db.Open(db_path->second); !s) {
        err << "cannot open " << db_path->second << ": " << s.error().message << "\n";
        return 1;
    }
    bs::BulletRecord b;
    b.name = "Sierra MatchKing 175 gr HPBT";
    b.manufacturer = "Sierra";
    b.caliber = ".308";
    b.diameter_m = u::InchToM(0.308);
    b.mass_kg = u::GrainToKg(175.0);
    b.length_m = u::InchToM(1.240);
    b.drag_kind = bs::kDragKindMultiBc;
    b.drag_table = "G1";
    b.bc_bands = {
        {u::FpsToMps(2800.0), 0.505}, {u::FpsToMps(1800.0), 0.496}, {u::FpsToMps(1500.0), 0.485}};
    b.source = "demo";
    bs::CartridgeRecord c;
    c.name = "M118LR (demo)";
    c.caliber = ".308";
    c.muzzle_velocity_mps = 790.0;
    c.powder_sensitivity_per_k = 0.0008;
    bs::RifleRecord r;
    r.name = "M24 (demo)";
    r.caliber = ".308";
    r.twist_m = u::InchToM(11.25);
    r.sight_height_m = 0.05;
    r.zero_range_m = 100.0;
    bs::ScopeRecord s;
    s.name = "0.1 MRAD scope (demo)";
    s.click_vertical_rad = s.click_horizontal_rad = u::MradToRad(0.1);

    if (auto id = bs::Repository<bs::BulletRecord>(db).Save(b); !id) {
        err << id.error().message << "\n";
        return 1;
    }
    c.bullet_id = b.id;
    if (!bs::Repository<bs::CartridgeRecord>(db).Save(c) ||
        !bs::Repository<bs::ScopeRecord>(db).Save(s)) {
        err << "could not save the demo records\n";
        return 1;
    }
    r.scope_id = s.id;
    if (!bs::Repository<bs::RifleRecord>(db).Save(r)) {
        err << "could not save the demo records\n";
        return 1;
    }
    auto pair = al::EnsureProfile(db, r.id, c.id);
    if (!pair) {
        err << pair.error().message << "\n";
        return 1;
    }
    out << "demo rifle " << r.id << ", cartridge " << c.id << " created (profile " << pair.value()
        << ")\n";
    return 0;
}

std::string ReadFile(const std::filesystem::path& path, bool& ok) {
    std::ifstream in(path, std::ios::binary);
    ok = static_cast<bool>(in);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

bool OpenDb(const Args& a, bs::Database& db, std::ostream& err) {
    const auto db_path = a.opts.find("db");
    if (db_path == a.opts.end()) {
        err << a.command << " needs --db FILE\n";
        return false;
    }
    if (auto s = db.Open(db_path->second); !s) {
        err << "cannot open " << db_path->second << ": " << s.error().message << "\n";
        return false;
    }
    return true;
}

int CmdImport(const Args& a, std::ostream& out, std::ostream& err) {
    if (a.files.empty()) {
        err << "import needs at least one FILE\n";
        return 2;
    }
    bs::Database db;
    if (!OpenDb(a, db, err)) {
        return 1;
    }
    int failed = 0;
    for (const std::string& file : a.files) {
        bool ok = false;
        const std::string content = ReadFile(file, ok);
        if (!ok) {
            err << file << ": cannot read\n";
            ++failed;
            continue;
        }
        const auto id = ballistics::applogic::ImportFile(
            db, std::filesystem::path(file).filename().string(), content);
        if (id) {
            out << file << ": imported (id " << id.value() << ")\n";
        } else {
            err << file << ": " << id.error().message << "\n";
            ++failed;
        }
    }
    return failed == 0 ? 0 : 1;
}

int CmdSeed(const Args& a, std::ostream& out, std::ostream& err) {
    if (a.files.size() != 1) {
        err << "seed needs the data/seed directory\n";
        return 2;
    }
    bs::Database db;
    if (!OpenDb(a, db, err)) {
        return 1;
    }
    std::vector<ballistics::applogic::SeedFile> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(a.files[0], ec)) {
        const auto ext = entry.path().extension().string();
        if (!entry.is_regular_file() ||
            (ext != ".ammo" && ext != ".drg" && ext != ".reticle" && ext != ".json")) {
            continue;
        }
        bool ok = false;
        std::string content = ReadFile(entry.path(), ok);
        if (ok) {
            files.push_back({entry.path().filename().string(), std::move(content)});
        }
    }
    if (ec || files.empty()) {
        err << a.files[0] << ": no data files found\n";
        return 1;
    }
    // A version above any the app uses: re-run always, existing records are kept.
    const auto report = ballistics::applogic::SeedLibrary(db, files, 1000000);
    if (!report) {
        err << report.error().message << "\n";
        return 1;
    }
    out << report.value().imported << " imported, " << report.value().skipped << " skipped\n";
    for (const std::string& p : report.value().problems) {
        err << p << "\n";
    }
    return 0;
}

}  // namespace

bool ParseWind(const std::string& text, WindSpec& wind) {
    const auto at = text.find('@');
    if (at == std::string::npos) {
        return false;
    }
    std::string dir = text.substr(at + 1);
    const auto colon = dir.find(':');
    if (colon != std::string::npos) {
        if (!Number(dir.substr(colon + 1), wind.until_m) || !(wind.until_m > 0.0)) {
            return false;
        }
        dir = dir.substr(0, colon);
    }
    if (!Number(text.substr(0, at), wind.speed_mps) || wind.speed_mps < 0.0) {
        return false;
    }
    const auto h = dir.find('h');
    if (h != std::string::npos) {
        double hours = 0.0, minutes = 0.0;
        if (!Number(dir.substr(0, h), hours) ||
            (h + 1 < dir.size() && !Number(dir.substr(h + 1), minutes))) {
            return false;
        }
        wind.from_deg = std::fmod((hours + minutes / 60.0) * 30.0, 360.0);
        return true;
    }
    return Number(dir, wind.from_deg);
}

int Run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
    Args a;
    if (!ParseArgs(args, a, err)) {
        err << kUsage;
        return 2;
    }
    if (a.command.empty() || a.command == "help") {
        out << kUsage;
        return a.command.empty() ? 2 : 0;
    }
    if (!a.files.empty() && a.command != "import" && a.command != "seed") {
        err << "unexpected argument: " << a.files.front() << "\n" << kUsage;
        return 2;
    }
    if (a.command == "version") {
        out << "ballistics " << ballistics::version() << " (SQLite " << bs::SqliteVersion()
            << ")\n";
        return 0;
    }
    if (a.command == "table") {
        return CmdTable(a, out, err);
    }
    if (a.command == "quick") {
        return CmdQuick(a, out, err);
    }
    if (a.command == "profiles" || a.command == "rifles" || a.command == "cartridges") {
        return CmdList(a, out, err);
    }
    if (a.command == "demo") {
        return CmdDemo(a, out, err);
    }
    if (a.command == "import") {
        return CmdImport(a, out, err);
    }
    if (a.command == "seed") {
        return CmdSeed(a, out, err);
    }
    err << "unknown command: " << a.command << "\n" << kUsage;
    return 2;
}

}  // namespace balcli
