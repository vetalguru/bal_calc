// bal-cli edge cases: every option, malformed input, unusable databases.
#include <ballistics/storage/database.h>
#include <ballistics/storage/records.h>
#include <ballistics/storage/repository.h>
#include <ballistics/units.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "cli_app.h"

namespace balcli {
namespace {

namespace bs = ballistics::storage;
namespace fs = std::filesystem;

int RunCli(const std::vector<std::string>& args, std::string& out, std::string& err) {
    std::ostringstream o, e;
    const int code = Run(args, o, e);
    out = o.str();
    err = e.str();
    return code;
}

bool Has(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

std::vector<std::string> Lines(const std::string& text) {
    std::istringstream in(text);
    std::vector<std::string> lines;
    for (std::string l; std::getline(in, l);) {
        lines.push_back(l);
    }
    return lines;
}

// A database file in the temp directory, gone before and after the test.
class TempDb {
   public:
    explicit TempDb(const std::string& name)
        : path_((fs::temp_directory_path() / ("balcalc_cli_edge_" + name + ".db")).string()) {
        fs::remove(path_);
    }
    ~TempDb() { fs::remove(path_); }
    TempDb(const TempDb&) = delete;
    TempDb& operator=(const TempDb&) = delete;

    const std::string& path() const { return path_; }

    // Runs SQL on the (created and migrated) database: to damage it.
    void Execute(const std::string& sql) const {
        bs::Database db;
        ASSERT_TRUE(db.Open(path_));
        ASSERT_TRUE(db.connection().Execute(sql)) << sql;
    }

   private:
    std::string path_;
};

// Where no database can be created.
std::string UnopenablePath() {
    return (fs::temp_directory_path() / "balcalc_cli_edge_no_such_dir" / "x.db").string();
}

// The quick command for a 175 gr .308 at 800 m/s, plus `extra`.
std::vector<std::string> Quick(std::vector<std::string> extra) {
    std::vector<std::string> args = {"quick", "--bc", "0.243", "--drag", "G7", "--v0", "800"};
    args.insert(args.end(), extra.begin(), extra.end());
    return args;
}

// The cells of the CSV row for `range` ("800,...").
std::vector<std::string> CsvRow(const std::string& out, const std::string& range) {
    for (const std::string& line : Lines(out)) {
        if (line.rfind(range + ",", 0) == 0) {
            std::vector<std::string> cells;
            std::istringstream in(line);
            for (std::string c; std::getline(in, c, ',');) {
                cells.push_back(c);
            }
            return cells;
        }
    }
    return {};
}

// --- Arguments --------------------------------------------------------------

TEST(CliEdgeArgs, HelpGoesToStdoutAndSucceeds) {
    std::string out, err;
    for (const char* flag : {"-h", "--help"}) {
        EXPECT_EQ(RunCli({flag}, out, err), 0) << flag;
        EXPECT_TRUE(Has(out, "Usage: bal-cli")) << flag;
        EXPECT_TRUE(err.empty()) << flag;
    }
    // Help wins over a command.
    EXPECT_EQ(RunCli({"quick", "--help"}, out, err), 0);
    EXPECT_TRUE(Has(out, "Usage: bal-cli"));
}

TEST(CliEdgeArgs, FlagWithoutValue) {
    std::string out, err;
    EXPECT_EQ(RunCli({"version", "--db"}, out, err), 2);
    EXPECT_TRUE(Has(err, "missing value for --db\n"));
    EXPECT_TRUE(Has(err, "Usage: bal-cli"));
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(RunCli({"quick", "--bc", "0.3", "--v0", "800", "--wind"}, out, err), 2);
    EXPECT_TRUE(Has(err, "missing value for --wind\n"));
}

TEST(CliEdgeArgs, UnknownCommandAndStrayArguments) {
    std::string out, err;
    EXPECT_EQ(RunCli({"bogus"}, out, err), 2);
    EXPECT_TRUE(Has(err, "unknown command: bogus\n"));
    EXPECT_TRUE(Has(err, "Usage: bal-cli"));
    EXPECT_EQ(RunCli({"rifles", "x.db"}, out, err), 2);
    EXPECT_TRUE(Has(err, "unexpected argument: x.db\n"));
}

TEST(CliEdgeWind, ClockAndMalformedSpecs) {
    WindSpec w;
    ASSERT_TRUE(ParseWind("4@9h", w));
    EXPECT_DOUBLE_EQ(w.from_deg, 270.0);  // from the left
    ASSERT_TRUE(ParseWind("4@6h", w));
    EXPECT_DOUBLE_EQ(w.from_deg, 180.0);  // tailwind
    ASSERT_TRUE(ParseWind("4@1h30", w));
    EXPECT_DOUBLE_EQ(w.from_deg, 45.0);
    ASSERT_TRUE(ParseWind("0@0", w));  // calm is fine
    EXPECT_DOUBLE_EQ(w.speed_mps, 0.0);

    EXPECT_FALSE(ParseWind("4@xh", w));    // hours
    EXPECT_FALSE(ParseWind("4@3hx", w));   // minutes
    EXPECT_FALSE(ParseWind("4@east", w));  // degrees
    EXPECT_FALSE(ParseWind("-1@90", w));   // negative speed
    EXPECT_FALSE(ParseWind("4@90:0", w));  // empty zone
    EXPECT_FALSE(ParseWind("4@90:far", w));
    EXPECT_FALSE(ParseWind("4@", w));
}

// --- Conditions and table options --------------------------------------------

TEST(CliEdgeQuick, MalformedNumbersAreNamed) {
    std::string out, err;
    for (const char* flag : {"--temp",        "--pressure", "--qnh",      "--alt",     "--humidity",
                             "--powder-temp", "--look",     "--cant",     "--lat",     "--azimuth",
                             "--from",        "--to",       "--step",     "--mass-gr", "--diam-in",
                             "--len-in",      "--twist-in", "--sight-cm", "--zero-m",  "--v0"}) {
        EXPECT_EQ(RunCli(Quick({flag, "12abc"}), out, err), 2) << flag;
        EXPECT_EQ(err, std::string(flag) + ": not a number: 12abc\n") << flag;
        EXPECT_TRUE(out.empty()) << flag;
    }
}

TEST(CliEdgeQuick, BadTableSpec) {
    std::string out, err;
    const std::string msg = "need 0 < --step and --from <= --to <= 5000\n";
    EXPECT_EQ(RunCli(Quick({"--step", "0"}), out, err), 2);
    EXPECT_EQ(err, msg);
    EXPECT_EQ(RunCli(Quick({"--step", "-50"}), out, err), 2);
    EXPECT_EQ(RunCli(Quick({"--from", "500", "--to", "100"}), out, err), 2);
    EXPECT_EQ(err, msg);
    EXPECT_EQ(RunCli(Quick({"--to", "5001"}), out, err), 2);
    EXPECT_EQ(err, msg);
    EXPECT_EQ(RunCli(Quick({"--units", "deg"}), out, err), 2);
    EXPECT_EQ(err, "--units: mrad or moa\n");
    // The limits themselves are fine.
    EXPECT_EQ(RunCli(Quick({"--from", "300", "--to", "300", "--csv"}), out, err), 0) << err;
    EXPECT_EQ(Lines(out).size(), 4u);  // summary, blank, header, the one row
}

TEST(CliEdgeQuick, AirFromFlags) {
    std::string out, err;
    // Station pressure as given.
    ASSERT_EQ(RunCli(Quick({"--temp", "30", "--pressure", "900", "--humidity", "50"}), out, err), 0)
        << err;
    EXPECT_TRUE(Has(out, "air 30.0 C 900.0 hPa 50 %")) << out;
    // QNH brought down to the altitude: 1013.25 hPa at sea level is
    // 898.7 hPa at 1000 m in the standard atmosphere.
    ASSERT_EQ(RunCli(Quick({"--qnh", "1013.25", "--alt", "1000"}), out, err), 0) << err;
    EXPECT_TRUE(Has(out, "898.7 hPa")) << out;
    // Without either, the standard atmosphere at the altitude.
    ASSERT_EQ(RunCli(Quick({"--alt", "1000"}), out, err), 0) << err;
    EXPECT_TRUE(Has(out, "air 8.5 C 898.7 hPa 0 %")) << out;
    // Station pressure wins over QNH.
    ASSERT_EQ(RunCli(Quick({"--pressure", "950", "--qnh", "1013.25", "--alt", "1000"}), out, err),
              0)
        << err;
    EXPECT_TRUE(Has(out, "950.0 hPa")) << out;
    // Quick mode zeroes and shoots with the same powder: no velocity change.
    ASSERT_EQ(RunCli(Quick({"--powder-temp", "40"}), out, err), 0) << err;
    EXPECT_TRUE(Has(out, "V0 800.0 m/s")) << out;
}

TEST(CliEdgeQuick, GeometryAndEarthMoveTheImpact) {
    std::string out, err;
    const std::vector<std::string> card = {"--to", "800", "--step", "800", "--csv"};
    auto row = [&](std::vector<std::string> extra) {
        extra.insert(extra.end(), card.begin(), card.end());
        EXPECT_EQ(RunCli(Quick(extra), out, err), 0) << err;
        return CsvRow(out, "800");
    };
    // Columns: range, elev, wind, drop, windage, ...
    const auto level = row({});
    ASSERT_EQ(level.size(), 9u);
    const double level_elev = std::stod(level[1]);
    EXPECT_GT(level_elev, 0.0);

    // Shooting 30 deg uphill needs less elevation (the rifleman's rule).
    const auto up = row({"--look", "30"});
    ASSERT_EQ(up.size(), 9u);
    EXPECT_LT(std::stod(up[1]), level_elev);
    EXPECT_NEAR(std::stod(up[1]), level_elev * std::cos(ballistics::units::DegToRad(30.0)),
                0.1 * level_elev);

    // Canted right: the bullet goes right, so hold left.
    const auto canted = row({"--cant", "5"});
    ASSERT_EQ(canted.size(), 9u);
    EXPECT_LT(std::stod(canted[2]), std::stod(level[2]) - 0.1);

    // Earth rotation: northern latitude, shooting east - drift right and
    // Eotvos lift, a few centimetres at 800 m; it needs the latitude.
    const auto earth = row({"--lat", "50", "--azimuth", "90"});
    ASSERT_EQ(earth.size(), 9u);
    EXPECT_GT(std::stod(earth[4]), std::stod(level[4]));
    EXPECT_LT(std::stod(earth[4]) - std::stod(level[4]), 10.0);
    EXPECT_GT(std::stod(earth[3]), std::stod(level[3]));
    const auto no_latitude = row({"--azimuth", "90"});
    EXPECT_EQ(no_latitude, level);
}

TEST(CliEdgeQuick, StabilityIsLabelled) {
    std::string out, err;
    auto run = [&](const char* twist) {
        EXPECT_EQ(RunCli(Quick({"--mass-gr", "175", "--diam-in", "0.308", "--len-in", "1.24",
                                "--twist-in", twist, "--to", "100"}),
                         out, err),
                  0)
            << err;
        return Lines(out).front();
    };
    // Miller: Sg ~2.4 at 1:10, ~1.2 at 1:14, ~0.6 at 1:20.
    const std::string fast = run("10");
    EXPECT_TRUE(Has(fast, ", Sg 2.")) << fast;
    EXPECT_FALSE(Has(fast, "(")) << fast;
    const std::string left = run("-10");
    EXPECT_TRUE(Has(left, ", Sg 2.")) << left;  // left twist spins as fast
    EXPECT_TRUE(Has(run("14"), " (marginal)"));
    EXPECT_TRUE(Has(run("20"), " (UNSTABLE)"));
    // No bullet dimensions, no stability.
    ASSERT_EQ(RunCli(Quick({"--to", "100"}), out, err), 0);
    EXPECT_FALSE(Has(out, "Sg"));
}

TEST(CliEdgeQuick, TableStopsWhereTheFlightEnds) {
    // 60 m/s: the flight ends (too far fallen / too long) well before 5 km.
    std::string out, err;
    ASSERT_EQ(
        RunCli({"quick", "--bc", "0.3", "--v0", "60", "--to", "5000", "--step", "1000", "--csv"},
               out, err),
        0)
        << err;
    const auto lines = Lines(out);
    ASSERT_GE(lines.size(), 4u);  // summary, blank, header, the muzzle
    EXPECT_LT(lines.size(), 9u);  // not all of 0..5000
    EXPECT_TRUE(CsvRow(out, "5000").empty());
    EXPECT_EQ(lines[3].rfind("0,-,-,", 0), 0u);
}

TEST(CliEdgeQuick, TextTableIsAligned) {
    std::string out, err;
    ASSERT_EQ(RunCli(Quick({"--to", "1000", "--step", "500", "--units", "moa"}), out, err), 0)
        << err;
    const auto lines = Lines(out);
    ASSERT_EQ(lines.size(), 6u);
    EXPECT_TRUE(Has(lines[2], "Elev MOA"));
    EXPECT_FALSE(Has(lines[2], "clk"));  // no scope in quick mode
    // Right-aligned columns: every line as long as the header.
    for (std::size_t i = 3; i < lines.size(); ++i) {
        EXPECT_EQ(lines[i].size(), lines[2].size()) << lines[i];
    }
    EXPECT_EQ(lines[3].find_first_not_of(' '), lines[2].find("Range m") + 6);  // "0" under "m"
}

// --- Commands on a database -------------------------------------------------

TEST(CliEdgeDb, CommandsNeedADatabase) {
    std::string out, err;
    EXPECT_EQ(RunCli({"rifles"}, out, err), 2);
    EXPECT_EQ(err, "rifles needs --db FILE\n");
    EXPECT_EQ(RunCli({"cartridges"}, out, err), 2);
    EXPECT_EQ(err, "cartridges needs --db FILE\n");
    EXPECT_EQ(RunCli({"profiles"}, out, err), 2);
    EXPECT_EQ(err, "profiles needs --db FILE\n");
    EXPECT_EQ(RunCli({"demo"}, out, err), 2);
    EXPECT_EQ(err, "demo needs --db FILE\n");
    const std::string table_msg =
        "table needs --db FILE and --rifle ID --cartridge ID (or --profile ID)\n";
    EXPECT_EQ(RunCli({"table", "--profile", "1"}, out, err), 2);
    EXPECT_EQ(err, table_msg);
    EXPECT_EQ(RunCli({"--db", "x.db", "table"}, out, err), 2);
    EXPECT_EQ(err, table_msg);
    EXPECT_EQ(RunCli({"--db", "x.db", "table", "--rifle", "1"}, out, err), 2);
    EXPECT_EQ(err, table_msg);
    EXPECT_EQ(RunCli({"--db", "x.db", "table", "--profile", "one"}, out, err), 2);
    EXPECT_EQ(err, "--profile: not a number: one\n");
    EXPECT_EQ(RunCli({"import", "a.ammo"}, out, err), 2);
    EXPECT_EQ(err, "import needs --db FILE\n");
    EXPECT_EQ(RunCli({"seed", "dir"}, out, err), 2);
    EXPECT_EQ(err, "seed needs --db FILE\n");
    EXPECT_FALSE(fs::exists("x.db"));
}

TEST(CliEdgeDb, UnopenableDatabase) {
    const std::string bad = UnopenablePath();
    std::string out, err;
    for (const std::vector<std::string>& args :
         std::vector<std::vector<std::string>>{{"--db", bad, "rifles"},
                                               {"--db", bad, "demo"},
                                               {"--db", bad, "table", "--profile", "1"},
                                               {"--db", bad, "import", "a.ammo"},
                                               {"--db", bad, "seed", "dir"}}) {
        EXPECT_EQ(RunCli(args, out, err), 1) << args[2];
        EXPECT_EQ(err.rfind("cannot open " + bad + ": ", 0), 0u) << err;
        EXPECT_TRUE(out.empty()) << args[2];
    }
}

TEST(CliEdgeDb, DatabaseFromANewerApp) {
    TempDb db("newer");
    db.Execute("PRAGMA user_version = 1000000");
    std::string out, err;
    EXPECT_EQ(RunCli({"--db", db.path(), "profiles"}, out, err), 1);
    EXPECT_TRUE(Has(err, "is newer than this app supports")) << err;
}

TEST(CliEdgeDb, TableErrors) {
    TempDb db("table");
    std::string out, err;
    ASSERT_EQ(RunCli({"--db", db.path(), "demo"}, out, err), 0) << err;
    // Bad conditions and output options are refused before the database.
    EXPECT_EQ(RunCli({"--db", db.path(), "table", "--profile", "1", "--wind", "fast"}, out, err),
              2);
    EXPECT_EQ(err, "--wind: expected SPEED@DIR[:UNTIL], got fast\n");
    EXPECT_EQ(RunCli({"--db", db.path(), "table", "--profile", "1", "--units", "deg"}, out, err),
              2);
    // No such rifle / cartridge.
    EXPECT_EQ(RunCli({"--db", db.path(), "table", "--rifle", "99", "--cartridge", "1"}, out, err),
              1);
    EXPECT_FALSE(err.empty());
    EXPECT_TRUE(out.empty());

    // A bullet against a drag table this build does not know (a newer
    // app's): the profile loads but cannot be solved.
    {
        bs::Database d;
        ASSERT_TRUE(d.Open(db.path()));
        bs::BulletRecord b;
        b.name = "Future";
        b.caliber = ".308";
        b.diameter_m = ballistics::units::InchToM(0.308);
        b.mass_kg = ballistics::units::GrainToKg(175.0);
        b.drag_kind = bs::kDragKindBc;
        b.drag_table = "G42";
        b.bc = 0.25;
        ASSERT_TRUE(bs::Repository<bs::BulletRecord>(d).Save(b));
        bs::CartridgeRecord c;
        c.name = "Future load";
        c.caliber = ".308";
        c.bullet_id = b.id;
        c.muzzle_velocity_mps = 800.0;
        ASSERT_TRUE(bs::Repository<bs::CartridgeRecord>(d).Save(c));
        ASSERT_EQ(c.id, 2);
    }
    EXPECT_EQ(RunCli({"--db", db.path(), "table", "--rifle", "1", "--cartridge", "2"}, out, err),
              1);
    EXPECT_EQ(err, "bullet 'Future': unknown drag table G42\n");
    EXPECT_TRUE(out.empty());
}

TEST(CliEdgeDb, TableWithPowderTemperatureAndCsvClicks) {
    TempDb db("powder");
    std::string out, err;
    ASSERT_EQ(RunCli({"--db", db.path(), "demo"}, out, err), 0) << err;
    EXPECT_EQ(out, "demo rifle 1, cartridge 1 created (profile 1)\n");
    // The demo load gains 0.08 %/K: 790 m/s at 15 C, 802.6 m/s at 35 C.
    ASSERT_EQ(RunCli({"--db", db.path(), "table", "--profile", "1", "--powder-temp", "35", "--to",
                      "300", "--step", "300", "--csv"},
                     out, err),
              0)
        << err;
    EXPECT_TRUE(Has(out, "V0 802.6 m/s")) << out;
    EXPECT_TRUE(Has(out, "Range m,Elev MRAD,Wind MRAD,Elev clk,Wind clk,Drop cm")) << out;
    const auto row = CsvRow(out, "300");
    ASSERT_EQ(row.size(), 11u);
    // 0.1 MRAD clicks: the click count is the hold x 10.
    EXPECT_NEAR(std::stod(row[3]), std::stod(row[1]) * 10.0, 0.5);
    EXPECT_EQ(CsvRow(out, "0")[3], "-");
}

TEST(CliEdgeDb, DamagedDatabaseLists) {
    std::string out, err;
    for (const auto& [command, table] : std::vector<std::pair<std::string, std::string>>{
             {"rifles", "rifle"}, {"cartridges", "cartridge"}, {"profiles", "profile"}}) {
        TempDb db("list_" + command);
        db.Execute("DROP TABLE " + table);
        EXPECT_EQ(RunCli({"--db", db.path(), command}, out, err), 1) << command;
        EXPECT_TRUE(Has(err, table)) << command << ": " << err;
        EXPECT_TRUE(out.empty()) << command;
    }
}

TEST(CliEdgeDb, DamagedDatabaseDemo) {
    std::string out, err;
    for (const char* table : {"bullet", "cartridge", "scope", "rifle", "profile"}) {
        TempDb db(std::string("demo_") + table);
        db.Execute(std::string("DROP TABLE ") + table);
        EXPECT_EQ(RunCli({"--db", db.path(), "demo"}, out, err), 1) << table;
        EXPECT_FALSE(err.empty()) << table;
        EXPECT_TRUE(out.empty()) << table;
    }
}

TEST(CliEdgeDb, EmptyListsPrintNothing) {
    TempDb db("empty");
    std::string out, err;
    for (const char* command : {"rifles", "cartridges", "profiles"}) {
        EXPECT_EQ(RunCli({"--db", db.path(), command}, out, err), 0) << command;
        EXPECT_TRUE(out.empty()) << command;
        EXPECT_TRUE(err.empty()) << command;
    }
}

// --- Import and seed --------------------------------------------------------

TEST(CliEdgeImport, MissingAndUnreadableFiles) {
    TempDb db("import");
    std::string out, err;
    EXPECT_EQ(RunCli({"--db", db.path(), "import"}, out, err), 2);
    EXPECT_EQ(err, "import needs at least one FILE\n");
    // One good, one missing: the good one is imported, the run fails.
    const std::string seed = BALLISTICS_SEED_DIR;
    const std::string missing =
        (fs::temp_directory_path() / "balcalc_cli_edge_missing.ammo").string();
    EXPECT_EQ(
        RunCli({"--db", db.path(), "import", missing, seed + "/reticle/mildot.reticle"}, out, err),
        1);
    EXPECT_EQ(err, missing + ": cannot read\n");
    EXPECT_TRUE(Has(out, "mildot.reticle: imported (id ")) << out;
}

TEST(CliEdgeSeed, ArgumentsAndEmptyDirectories) {
    TempDb db("seed");
    std::string out, err;
    EXPECT_EQ(RunCli({"--db", db.path(), "seed"}, out, err), 2);
    EXPECT_EQ(err, "seed needs the data/seed directory\n");
    EXPECT_EQ(RunCli({"--db", db.path(), "seed", "a", "b"}, out, err), 2);
    EXPECT_EQ(err, "seed needs the data/seed directory\n");

    // A directory without data files (only other files), and none at all.
    const fs::path dir = fs::temp_directory_path() / "balcalc_cli_edge_seed_dir";
    fs::remove_all(dir);
    fs::create_directories(dir / "sub");
    std::ofstream(dir / "notes.txt") << "not a data file\n";
    std::ofstream(dir / "sub" / "x.csv") << "1,2\n";
    EXPECT_EQ(RunCli({"--db", db.path(), "seed", dir.string()}, out, err), 1);
    EXPECT_EQ(err, dir.string() + ": no data files found\n");
    fs::remove_all(dir);
    EXPECT_EQ(RunCli({"--db", db.path(), "seed", dir.string()}, out, err), 1);
    EXPECT_EQ(err, dir.string() + ": no data files found\n");
}

TEST(CliEdgeSeed, SecondRunSkipsWhatIsThere) {
    TempDb db("reseed");
    const std::string seed = BALLISTICS_SEED_DIR;
    std::string out, err;
    ASSERT_EQ(RunCli({"--db", db.path(), "seed", seed}, out, err), 0) << err;
    const std::string first = out;
    ASSERT_EQ(RunCli({"--db", db.path(), "seed", seed}, out, err), 0) << err;
    // Everything the first run brought in is skipped now.
    EXPECT_EQ(out.rfind("0 imported, ", 0), 0u) << first << out;
}

TEST(CliEdgeSeed, DamagedDatabase) {
    TempDb db("seed_damaged");
    db.Execute("DROP TABLE app_setting");
    std::string out, err;
    EXPECT_EQ(RunCli({"--db", db.path(), "seed", BALLISTICS_SEED_DIR}, out, err), 1);
    EXPECT_TRUE(Has(err, "app_setting")) << err;
    EXPECT_TRUE(out.empty());
}

}  // namespace
}  // namespace balcli
