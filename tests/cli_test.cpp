#include "cli_app.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace balcli {
namespace {

TEST(CliWind, ParsesDegreesClockAndZones) {
    WindSpec w;
    ASSERT_TRUE(ParseWind("4@90", w));
    EXPECT_DOUBLE_EQ(w.speed_mps, 4.0);
    EXPECT_DOUBLE_EQ(w.from_deg, 90.0);
    EXPECT_DOUBLE_EQ(w.until_m, 1e5);

    ASSERT_TRUE(ParseWind("3.5@3h:400", w));
    EXPECT_DOUBLE_EQ(w.from_deg, 90.0);
    EXPECT_DOUBLE_EQ(w.until_m, 400.0);

    ASSERT_TRUE(ParseWind("2@10h30", w));
    EXPECT_DOUBLE_EQ(w.from_deg, 315.0);
    ASSERT_TRUE(ParseWind("2@12h", w));
    EXPECT_DOUBLE_EQ(w.from_deg, 0.0);

    EXPECT_FALSE(ParseWind("4", w));
    EXPECT_FALSE(ParseWind("x@90", w));
    EXPECT_FALSE(ParseWind("4@90:-5", w));
}

int RunCli(const std::vector<std::string>& args, std::string& out, std::string& err) {
    std::ostringstream o, e;
    const int code = Run(args, o, e);
    out = o.str();
    err = e.str();
    return code;
}

TEST(CliRun, VersionAndUsage) {
    std::string out, err;
    EXPECT_EQ(RunCli({"version"}, out, err), 0);
    EXPECT_NE(out.find("SQLite"), std::string::npos);
    EXPECT_EQ(RunCli({}, out, err), 2);
    EXPECT_NE(out.find("Usage"), std::string::npos);
    EXPECT_EQ(RunCli({"bogus"}, out, err), 2);
}

TEST(CliRun, QuickTableCsv) {
    std::string out, err;
    ASSERT_EQ(RunCli({"quick", "--bc", "0.243", "--drag", "G7", "--v0", "800", "--to", "1000",
                      "--step", "500", "--wind", "4@3h", "--csv"},
                     out, err),
              0)
        << err;
    // Summary, blank line, header + 3 rows (0, 500, 1000).
    std::istringstream lines(out);
    std::vector<std::string> rows;
    for (std::string l; std::getline(lines, l);) {
        rows.push_back(l);
    }
    ASSERT_EQ(rows.size(), 6U);
    EXPECT_EQ(rows[2].rfind("Range m,Elev MRAD,Wind MRAD", 0), 0U);
    EXPECT_EQ(rows[5].rfind("1000,", 0), 0U);
}

TEST(CliRun, QuickRejectsBadInput) {
    std::string out, err;
    EXPECT_EQ(RunCli({"quick", "--v0", "800"}, out, err), 2);
    EXPECT_EQ(RunCli({"quick", "--bc", "abc", "--v0", "800"}, out, err), 2);
    EXPECT_EQ(RunCli({"quick", "--bc", "0.3", "--v0", "800", "--wind", "4"}, out, err), 2);
    EXPECT_EQ(RunCli({"quick", "--bc", "0.3", "--v0", "800", "--drag", "G42"}, out, err), 1);
}

TEST(CliRun, DemoProfileTable) {
    const auto db =
        (std::filesystem::temp_directory_path() / "balcalc_cli_test.db").string();
    std::filesystem::remove(db);
    std::string out, err;
    ASSERT_EQ(RunCli({"--db", db, "demo"}, out, err), 0) << err;
    ASSERT_EQ(RunCli({"--db", db, "profiles"}, out, err), 0) << err;
    EXPECT_NE(out.find("M24 / M118LR (demo)"), std::string::npos);
    ASSERT_EQ(RunCli({"--db", db, "table", "--profile", "1", "--to", "800", "--temp", "-5",
                      "--alt", "400", "--humidity", "60", "--units", "moa"},
                     out, err),
              0)
        << err;
    EXPECT_NE(out.find("Elev clk"), std::string::npos); // the profile has a scope
    EXPECT_NE(out.find("Sg "), std::string::npos);
    EXPECT_EQ(RunCli({"--db", db, "table", "--profile", "42"}, out, err), 1);
    std::filesystem::remove(db);
}

} // namespace
} // namespace balcli
