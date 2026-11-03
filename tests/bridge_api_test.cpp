// The JSON facade beyond the happy paths of bridge_test.cpp: the options of
// each method, bad and missing arguments, unknown ids, settings kept across
// a restart and a database changed or broken behind the bridge's back.
#include <ballistics/bridge/api.h>
#include <ballistics/storage/database.h>
#include <ballistics/storage/repository.h>
#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>

namespace ballistics::bridge {
namespace {

using nlohmann::json;
namespace bs = ballistics::storage;

// A file of the starter library, as the app sends it.
json SeedFile(const std::string& relative) {
    const std::filesystem::path path = std::filesystem::path(BALLISTICS_SEED_DIR) / relative;
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return {{"name", path.filename().string()}, {"content", text.str()}};
}

// Calls through the bridge, as in bridge_test.cpp.
class BridgeApi : public ::testing::Test {
   protected:
    void SetUp() override { Ok("open", {{"path", ":memory:"}}); }

    // The result of a call that must succeed.
    json Ok(const std::string& method, const json& args = json::object()) {
        const json r = json::parse(api_->Call(method, args.dump()));
        EXPECT_TRUE(r.at("ok").get<bool>()) << method << ": " << r.dump();
        return r.value("result", json());
    }
    // The error of a call that must fail.
    std::string Fails(const std::string& method, const json& args = json::object()) {
        const json r = json::parse(api_->Call(method, args.dump()));
        EXPECT_FALSE(r.at("ok").get<bool>()) << method << ": " << r.dump();
        return r.value("error", "");
    }

    json Sample() { return Ok("addSample", {{"rifleName", "Rifle"}, {"cartridgeName", "Load"}}); }

    // A rifle saved from the editor's empty form with these fields changed.
    int SaveRifle(const json& changes) {
        json form = Ok("rifleForm", {{"id", 0}});
        form.update(changes);
        return Ok("saveRifle", {{"form", form}}).at("id").get<int>();
    }
    // The same for a cartridge (a .308 175 gr load unless changed).
    int SaveCartridge(const json& changes) {
        json form = Ok("cartridgeForm", {{"id", 0}});
        form.update({{"name", "Load"},
                     {"caliber", ".308 Win"},
                     {"bulletName", "SMK 175"},
                     {"bc", 0.243},
                     {"massGr", 175},
                     {"diameterIn", 0.308},
                     {"muzzleVelocity", 790}});
        form.update(changes);
        return Ok("saveCartridge", {{"form", form}}).at("id").get<int>();
    }

    std::optional<Api> api_{std::in_place};
};

// The same on a database file, which a second connection can change (or
// break) between calls, and which a new Api opens again as the next start.
class BridgeApiFile : public BridgeApi {
   protected:
    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        path_ = (std::filesystem::temp_directory_path() /
                 (std::string("balcalc_bridge_api_") + info->name() + ".db"))
                    .string();
        std::filesystem::remove(path_);
        Ok("open", {{"path", path_}});
    }
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
        std::filesystem::remove(path_ + "-wal", ec);
        std::filesystem::remove(path_ + "-shm", ec);
    }

    // SQL run on the file by another connection, foreign keys and checks
    // off (as a hand-edited or damaged file would be).
    void Sql(const std::string& sql) {
        bs::Database db;
        ASSERT_TRUE(db.Open(path_).ok());
        ASSERT_TRUE(db.connection().Execute("PRAGMA foreign_keys = OFF").ok());
        ASSERT_TRUE(db.connection().Execute("PRAGMA ignore_check_constraints = ON").ok());
        ASSERT_TRUE(db.connection().Execute(sql).ok()) << sql;
    }
    void Setting(const std::string& key, const std::string& value) {
        bs::Database db;
        ASSERT_TRUE(db.Open(path_).ok());
        ASSERT_TRUE(bs::SetSetting(db, key, value).ok());
    }
    // A new start of the app on the same file.
    json Reopen() {
        api_.emplace();  // the old one closes first
        return Ok("open", {{"path", path_}});
    }

    std::string path_;
};

// ---- Session: open, info, settings ----------------------------------------

TEST_F(BridgeApi, InfoNamesTheDatabase) {
    const json info = Ok("info");
    EXPECT_EQ(info.at("databasePath"), ":memory:");
    EXPECT_FALSE(info.at("engineVersion").get<std::string>().empty());
    EXPECT_FALSE(info.at("sqliteVersion").get<std::string>().empty());
    // No arguments at all is an empty object; anything else must be one.
    EXPECT_TRUE(json::parse(api_->Call("state", "")).at("ok").get<bool>());
    EXPECT_EQ(Fails("state", "text"), "Arguments must be a JSON object.");
}

TEST(BridgeApiErrors, OpenNeedsAUsablePath) {
    Api api;
    const json r = json::parse(api.Call("open", R"({"path": "/no/such/dir/x.db"})"));
    EXPECT_FALSE(r.at("ok").get<bool>());
    EXPECT_FALSE(r.at("error").get<std::string>().empty());
    // Still closed: everything but info and open is refused.
    EXPECT_EQ(json::parse(api.Call("solution", "{}")).at("error"), "The database is not open.");
    EXPECT_EQ(json::parse(api.Call("info", "{}")).at("result").at("databasePath"), "");
    EXPECT_EQ(json::parse(api.Call("open", "\"x\"")).at("error"),
              "Arguments must be a JSON object.");
}

TEST_F(BridgeApi, SettingsIgnoreWhatTheyDoNotKnow) {
    Sample();
    json st = Ok("setSettings", {{"holdMode", "sideways"}});  // unknown: the default
    EXPECT_EQ(st.at("holdMode"), "dial_elevation");
    st = Ok("setSettings", {{"holdMode", "hold"}, {"language", "de"}});
    EXPECT_EQ(st.at("holdMode"), "hold");
    EXPECT_EQ(st.at("language"), "de");
    st = Ok("setSettings", {{"tableFromM", 200}, {"tableToM", 600}, {"tableStepM", 25}});
    EXPECT_EQ(st.at("tableFromM"), 200.0);
    EXPECT_EQ(st.at("tableToM"), 600.0);
    EXPECT_EQ(st.at("tableStepM"), 25.0);
    // A value that is not a number keeps what was there.
    EXPECT_EQ(Ok("setSettings", {{"tableStepM", "wide"}}).at("tableStepM"), 25.0);
    const json t = Ok("rangeTable");
    ASSERT_TRUE(t.at("ok").get<bool>());
    ASSERT_EQ(t.at("rows").size(), 17U);  // 200, 225 ... 600
    EXPECT_EQ(t.at("rows").front().at("rangeM"), 200.0);
    EXPECT_EQ(t.at("rows").back().at("rangeM"), 600.0);
    // Prefs that are not an object change nothing.
    EXPECT_EQ(Ok("setSettings", {{"prefs", "dark"}}).at("prefs"), json::object());
    EXPECT_EQ(Ok("setSettings", {{"angleUnit", 7}}).at("angleUnit"), "mrad");
    EXPECT_NEAR(Ok("stationPressure", {{"qnhHpa", 1013.25}}).get<double>(), 1013.25, 1e-6);
}

// Every condition the app sets survives a restart, but the wind: a start
// is a new day, calm from 6 o'clock, without gusts or zones.
TEST_F(BridgeApiFile, ConditionsAndSettingsSurviveARestart) {
    Sample();
    const json zones = json::array(
        {{{"speedMps", 3}, {"fromDeg", 45}, {"untilM", 600}}, {{"speedMps", 7}, {"untilM", 900}}});
    const json set = Ok("setConditions", {{"temperatureC", -5},
                                          {"pressureHpa", 950},
                                          {"altitudeM", 400},
                                          {"humidityPct", 80},
                                          {"windSpeed", 2},
                                          {"windFromDeg", 270},
                                          {"windUntilM", 300},
                                          {"windZones", zones},
                                          {"windGustMps", 4},
                                          {"targetSpeedMps", 3},
                                          {"targetHeadingDeg", 270},
                                          {"targetSpeedUnit", "mph"},
                                          {"lookAngleDeg", -10},
                                          {"cantDeg", 2},
                                          {"coriolis", true},
                                          {"latitudeDeg", 60},
                                          {"useAzimuth", true},
                                          {"azimuthDeg", 45},
                                          {"targetRangeM", 750},
                                          {"magnification", 12},
                                          {"useDensityAltitude", true},
                                          {"densityAltitudeM", 1500},
                                          {"targetHeightCm", 40}});
    // The zones as the core took them: a missing direction is from the
    // right, the last zone goes to the end.
    ASSERT_EQ(set.at("windZones").size(), 2U);
    EXPECT_EQ(set.at("windZones")[0].at("fromDeg"), 45.0);
    EXPECT_EQ(set.at("windZones")[0].at("untilM"), 600.0);
    EXPECT_EQ(set.at("windZones")[1].at("fromDeg"), 90.0);
    EXPECT_EQ(Ok("solution").at("hasGust"), true);
    Ok("setSettings", {{"tableFromM", 50}, {"tableToM", 1500}, {"tableStepM", 10}});

    Reopen();
    const json st = Ok("state");
    const json& c = st.at("conditions");
    EXPECT_EQ(c.at("temperatureC"), -5.0);
    EXPECT_EQ(c.at("pressureHpa"), 950.0);
    EXPECT_EQ(c.at("altitudeM"), 400.0);
    EXPECT_EQ(c.at("humidityPct"), 80.0);
    EXPECT_TRUE(c.at("powderFollowsAir").get<bool>());
    EXPECT_EQ(c.at("powderC"), -5.0);  // follows the air
    EXPECT_EQ(c.at("windSpeed"), 0.0);
    EXPECT_EQ(c.at("windFromDeg"), 180.0);
    EXPECT_EQ(c.at("windUntilM"), 0.0);
    EXPECT_TRUE(c.at("windZones").empty());
    EXPECT_EQ(c.at("windGustMps"), 0.0);
    EXPECT_EQ(c.at("targetSpeedMps"), 3.0);
    EXPECT_EQ(c.at("targetHeadingDeg"), 270.0);
    EXPECT_EQ(c.at("targetSpeedUnit"), "mph");
    EXPECT_EQ(c.at("lookAngleDeg"), -10.0);
    EXPECT_EQ(c.at("cantDeg"), 2.0);
    EXPECT_TRUE(c.at("coriolis").get<bool>());
    EXPECT_EQ(c.at("latitudeDeg"), 60.0);
    EXPECT_TRUE(c.at("useAzimuth").get<bool>());
    EXPECT_EQ(c.at("azimuthDeg"), 45.0);
    EXPECT_EQ(c.at("targetRangeM"), 750.0);
    EXPECT_EQ(c.at("magnification"), 12.0);
    EXPECT_TRUE(c.at("useDensityAltitude").get<bool>());
    EXPECT_EQ(c.at("densityAltitudeM"), 1500.0);
    EXPECT_EQ(c.at("targetHeightCm"), 40.0);
    EXPECT_EQ(st.at("tableFromM"), 50.0);
    EXPECT_EQ(st.at("tableToM"), 1500.0);
    EXPECT_EQ(st.at("tableStepM"), 10.0);

    // Coriolis without the azimuth: the latitude stays, the azimuth goes.
    Ok("setConditions", {{"useAzimuth", false}, {"coriolis", true}});
    Reopen();
    const json again = Ok("state").at("conditions");
    EXPECT_TRUE(again.at("coriolis").get<bool>());
    EXPECT_FALSE(again.at("useAzimuth").get<bool>());
    EXPECT_EQ(again.at("latitudeDeg"), 60.0);
}

// Damaged settings (a hand-edited file): the defaults, never a failure.
TEST_F(BridgeApiFile, DamagedSettingsFallBackToTheDefaults) {
    Sample();
    Setting("ui.table.from_m", "far");
    Setting("ui.table.to_m", "1200");
    Setting("ui.prefs", "[1, 2]");
    Setting("ui.current_rifle", "rifle");
    Setting("ui.current_cartridge", "x");
    Setting("ui.targets", R"({"not": "a list"})");
    Setting("ui.situations", "not json");
    Reopen();
    const json st = Ok("state");
    EXPECT_EQ(st.at("tableFromM"), 100.0);
    EXPECT_EQ(st.at("tableToM"), 1200.0);
    EXPECT_EQ(st.at("prefs"), json::object());
    // The selection is lost; the first rifle and cartridge are taken.
    EXPECT_EQ(st.at("currentRifleId"), st.at("rifles")[0].at("id"));
    EXPECT_GT(st.at("currentProfileId").get<int>(), 0);
    EXPECT_TRUE(Ok("targets").empty());
    EXPECT_TRUE(Ok("situations").empty());
}

// A database from the version with profiles keeps the profile chosen there.
TEST_F(BridgeApiFile, OldProfileSelectionIsKept) {
    const json first = Sample();
    const json second = Ok("addSample", {{"rifleName", "Second"}, {"cartridgeName", "Other"}});
    ASSERT_NE(first.at("currentProfileId"), second.at("currentProfileId"));
    Ok("select",
       {{"rifleId", first.at("currentRifleId")}, {"cartridgeId", first.at("currentCartridgeId")}});
    Sql("DELETE FROM app_setting WHERE key IN ('ui.current_rifle', 'ui.current_cartridge')");
    Setting("ui.current_profile", second.at("currentProfileId").dump());
    Reopen();
    const json st = Ok("state");
    EXPECT_EQ(st.at("currentRifleId"), second.at("currentRifleId"));
    EXPECT_EQ(st.at("currentCartridgeId"), second.at("currentCartridgeId"));
    EXPECT_EQ(st.at("currentProfileId"), second.at("currentProfileId"));
}

// ---- Conditions --------------------------------------------------------------

TEST_F(BridgeApi, ConditionsIgnoreBadValues) {
    const json before = Ok("state").at("conditions");
    const json c = Ok("setConditions", {{"temperatureC", "warm"},
                                        {"coriolis", "yes"},
                                        {"windZones", "none"},
                                        {"targetSpeedUnit", 5}});
    EXPECT_EQ(c, before);
    // At most two zones after the first.
    const json zones = json::array({{{"speedMps", 1}}, {{"speedMps", 2}}, {{"speedMps", 3}}});
    EXPECT_EQ(Ok("setConditions", {{"windZones", zones}}).at("windZones").size(), 2U);
}

TEST_F(BridgeApi, CoriolisMovesTheImpact) {
    Sample();
    Ok("setConditions", {{"targetRangeM", 1000}, {"windSpeed", 0}});
    const json plain = Ok("solution");
    ASSERT_TRUE(plain.at("ok").get<bool>());
    EXPECT_EQ(Ok("rangeTable").at("rows").back().at("coriolisDriftCm"), 0.0);

    // Northern hemisphere: the bullet drifts to the right; firing east it
    // also rises (Eötvös), so it needs less elevation than firing west.
    Ok("setConditions",
       {{"coriolis", true}, {"latitudeDeg", 60}, {"useAzimuth", true}, {"azimuthDeg", 90}});
    const json east = Ok("solution");
    Ok("setConditions", {{"azimuthDeg", 270}});
    const json west = Ok("solution");
    EXPECT_LT(east.at("elevation").get<double>(), west.at("elevation").get<double>());
    EXPECT_LT(east.at("windage").get<double>(), plain.at("windage").get<double>());
    const json row = Ok("rangeTable").at("rows").back();
    EXPECT_GT(row.at("coriolisDriftCm").get<double>(), 0.0);
    EXPECT_LT(row.at("coriolisLiftCm").get<double>(), 0.0);  // firing west: down

    // Without the azimuth the drift is still there.
    Ok("setConditions", {{"useAzimuth", false}});
    EXPECT_GT(Ok("rangeTable").at("rows").back().at("coriolisDriftCm").get<double>(), 0.0);
}

// ---- Solution ----------------------------------------------------------------

TEST_F(BridgeApi, SolutionWithoutARangeHasNoHold) {
    Sample();
    Ok("setConditions", {{"targetRangeM", 0}});
    const json sol = Ok("solution");
    EXPECT_FALSE(sol.at("ok").get<bool>());
    EXPECT_EQ(sol.at("error"), "Enter a target range.");
    EXPECT_FALSE(sol.at("hasReticle").get<bool>());
    EXPECT_FALSE(sol.contains("holdMode"));
    EXPECT_GE(sol.at("computeMs").get<double>(), 0.0);
}

TEST_F(BridgeApi, SolutionInMoaWithItsClicks) {
    Sample();
    Ok("setSettings", {{"angleUnit", "moa"}});
    const json sol = Ok("solution");
    ASSERT_TRUE(sol.at("ok").get<bool>());
    // The sample's 0.1 mrad clicks are 0.344 MOA.
    EXPECT_NEAR(sol.at("clickElevation").get<double>(), 0.34377, 1e-4);
    EXPECT_NEAR(sol.at("elevationClicks").get<double>(),
                std::round(sol.at("elevation").get<double>() / 0.34377), 1.0);
    EXPECT_GT(sol.at("muzzleVelocity").get<double>(), 700.0);
    EXPECT_GT(sol.at("energy").get<double>(), 0.0);
    EXPECT_GT(sol.at("time").get<double>(), 0.0);
    EXPECT_EQ(sol.at("velocityScale"), 1.0);
    EXPECT_EQ(sol.at("dragScale"), 1.0);
    EXPECT_FALSE(sol.at("subsonic").get<bool>());
}

// A second focal plane scope with a reticle: the drawing and its scale.
TEST_F(BridgeApi, SolutionCarriesTheReticle) {
    const json imported =
        Ok("importFiles",
           {{"files", json::array({SeedFile("reticle/generic-mrad-grid-0.5.reticle")})}});
    ASSERT_EQ(imported.at("imported"), 1) << imported.dump();
    EXPECT_TRUE(imported.at("problems").empty());
    const json reticles = Ok("reticles");
    ASSERT_EQ(reticles.size(), 1U);
    const int reticle = reticles[0].at("id");
    SaveRifle({{"name", "SFP rifle"},
               {"caliber", ".308 Win"},
               {"reticleId", reticle},
               {"focalPlane", "sfp"},
               {"minMagnification", 5},
               {"maxMagnification", 25},
               {"sfpReferenceMagnification", 25}});
    SaveCartridge(json::object());

    json sol = Ok("solution");
    ASSERT_TRUE(sol.at("ok").get<bool>()) << sol.dump();
    ASSERT_TRUE(sol.at("hasReticle").get<bool>());
    EXPECT_EQ(sol.at("reticleName"), reticles[0].at("name"));
    EXPECT_EQ(sol.at("reticleUnits"), "mrad");
    EXPECT_FALSE(sol.at("reticleDefinition").get<std::string>().empty());
    EXPECT_EQ(sol.at("focalPlane"), "sfp");
    EXPECT_EQ(sol.at("minMagnification"), 5.0);
    EXPECT_EQ(sol.at("maxMagnification"), 25.0);
    // No magnification set: the scope's highest, where the marks are true.
    EXPECT_EQ(sol.at("magnification"), 25.0);
    EXPECT_NEAR(sol.at("subtensionScale").get<double>(), 1.0, 1e-9);
    // At half the zoom the marks cover twice the angle.
    Ok("setConditions", {{"magnification", 12.5}});
    sol = Ok("solution");
    EXPECT_EQ(sol.at("magnification"), 12.5);
    EXPECT_NEAR(sol.at("subtensionScale").get<double>(), 2.0, 1e-9);
}

TEST_F(BridgeApi, GustAndMovingTargetTogether) {
    Sample();
    Ok("setConditions", {{"targetRangeM", 500},
                         {"windSpeed", 4},
                         {"windFromDeg", 90},
                         {"windGustMps", 8},
                         {"targetSpeedMps", 2},
                         {"targetHeadingDeg", 90}});
    const json sol = Ok("solution");
    ASSERT_TRUE(sol.at("ok").get<bool>());
    ASSERT_TRUE(sol.at("hasGust").get<bool>());
    // The gust adds to the wind from the same side: more windage.
    EXPECT_GT(sol.at("gustWindage").get<double>(), sol.at("windage").get<double>());
    EXPECT_GT(std::abs(sol.at("gustWindageCm").get<double>()),
              std::abs(sol.at("windageCm").get<double>()));
    ASSERT_TRUE(sol.at("hasLead").get<bool>());
    EXPECT_GT(sol.at("lead").get<double>(), 0.0);  // walking to the right
    EXPECT_NEAR(sol.at("leadTotalWindage").get<double>(),
                sol.at("lead").get<double>() + sol.at("windage").get<double>(), 0.02);
    EXPECT_GT(sol.at("leadRangeM").get<double>(), 0.0);
}

// ---- Tables, curves and the hit chance ----------------------------------------

TEST_F(BridgeApi, TablesNeedAPair) {
    for (const char* method : {"rangeTable", "trajectoryCurve"}) {
        const json t = Ok(method);
        EXPECT_FALSE(t.at("ok").get<bool>()) << method;
        EXPECT_EQ(t.at("error"), "Choose a rifle and a cartridge.");
        EXPECT_TRUE(t.at("rows").empty());
    }
    EXPECT_TRUE(Ok("pairOptions").empty());
    EXPECT_TRUE(Ok("compareCurves").empty());
}

TEST_F(BridgeApi, WindColumnsAreCleanedUp) {
    Sample();
    Ok("setSettings", {{"tableFromM", 100}, {"tableToM", 300}, {"tableStepM", 100}});
    Ok("setConditions", {{"windFromDeg", 90}});
    const json t = Ok("rangeTable", {{"windSpeeds", {"x", -3, 99, 1, 2, 3, 4, 5}}});
    // Numbers only, clamped to 0..40, at most six.
    EXPECT_EQ(t.at("windSpeeds"), json::parse("[0.0, 40.0, 1.0, 2.0, 3.0, 4.0]"));
    ASSERT_EQ(t.at("rows").size(), 3U);
    for (const json& row : t.at("rows")) {
        ASSERT_EQ(row.at("windages").size(), 6U);
        ASSERT_EQ(row.at("windageClicksAt").size(), 6U);
        EXPECT_LT(row.at("windages")[0].get<double>(), row.at("windages")[1].get<double>());
    }
    // Not a list: no columns.
    EXPECT_TRUE(Ok("rangeTable", {{"windSpeeds", 4}}).at("windSpeeds").empty());
    // A table the core cannot build is reported, without columns.
    Ok("setSettings", {{"tableStepM", 0}});
    const json bad = Ok("rangeTable", {{"windSpeeds", {2}}});
    EXPECT_FALSE(bad.at("ok").get<bool>());
    EXPECT_EQ(bad.at("error"), "Check the table range and step.");
}

TEST_F(BridgeApi, CompareCurvesKeepsToFourAndItsLimits) {
    const json st = Sample();
    const int rifle = st.at("currentRifleId");
    const int cartridge = st.at("currentCartridgeId");
    const json pair = {{"rifleId", rifle}, {"cartridgeId", cartridge}};
    const json curves =
        Ok("compareCurves",
           {{"maxRangeM", 9000}, {"points", 2}, {"pairs", {pair, pair, pair, pair, pair, pair}}});
    ASSERT_EQ(curves.size(), 4U);
    for (const json& c : curves) {
        ASSERT_TRUE(c.at("ok").get<bool>());
        EXPECT_EQ(c.at("rows").size(), 11U);                  // at least 10 points
        EXPECT_EQ(c.at("rows").back().at("rangeM"), 3000.0);  // at most 3 km
        EXPECT_EQ(c.at("rifleId"), rifle);
        EXPECT_EQ(c.at("cartridgeId"), cartridge);
    }
    const json missing = Ok("compareCurves", {{"pairs", {{{"rifleId", rifle}}}}});
    ASSERT_EQ(missing.size(), 1U);
    EXPECT_FALSE(missing[0].at("ok").get<bool>());
    EXPECT_EQ(missing[0].at("error"), "cartridge not found");
    EXPECT_EQ(missing[0].at("label"), "");
}

TEST_F(BridgeApi, PairOptionsMatchTheCalibre) {
    const json st = Sample();
    SaveCartridge({{"name", "Varmint"},
                   {"caliber", ".223 Rem"},
                   {"diameterIn", 0.224},
                   {"massGr", 55},
                   {"bc", 0.12},
                   {"muzzleVelocity", 990}});
    const json options = Ok("pairOptions");
    ASSERT_EQ(options.size(), 1U);
    EXPECT_EQ(options[0].at("rifleName"), "Rifle");
    ASSERT_EQ(options[0].at("cartridges").size(), 1U);  // the .223 does not fit
    EXPECT_EQ(options[0].at("cartridges")[0].at("id"), st.at("currentCartridgeId"));
    // The state lists it, but not as a match, after those that are.
    const json cartridges = Ok("state").at("cartridges");
    ASSERT_EQ(cartridges.size(), 2U);
    EXPECT_TRUE(cartridges[0].at("matches").get<bool>());
    EXPECT_FALSE(cartridges[1].at("matches").get<bool>());
    EXPECT_EQ(cartridges[1].at("name"), "Varmint");
}

TEST_F(BridgeApi, TrajectoryCurveLimitsItsPoints) {
    Sample();
    EXPECT_EQ(Ok("trajectoryCurve", {{"maxRangeM", 500}, {"points", 3}}).at("rows").size(), 11U);
    EXPECT_EQ(Ok("trajectoryCurve", {{"maxRangeM", 500}, {"points", 5000}}).at("rows").size(),
              1001U);
    // The defaults: 1000 m in 250 steps.
    const json t = Ok("trajectoryCurve");
    ASSERT_EQ(t.at("rows").size(), 251U);
    EXPECT_EQ(t.at("rows").back().at("rangeM"), 1000.0);
    EXPECT_TRUE(t.at("hasScope").get<bool>());
}

TEST_F(BridgeApi, HitProbabilitySettings) {
    Sample();
    const json all = {{"rangeM", 400},          {"windSpeedMps", 1.5}, {"windDirectionDeg", 45},
                      {"muzzleVelocityMps", 8}, {"bcPercent", 3},      {"temperatureC", 4},
                      {"pressureHpa", 3},       {"humidityPct", 10},   {"lookAngleDeg", 1},
                      {"cantDeg", 1},           {"azimuthDeg", 2},     {"latitudeDeg", 1},
                      {"groupMoa", 0.75},       {"targetWidthCm", 30}, {"targetHeightCm", 45},
                      {"targetKind", "figure"}};
    json r = Ok("wez", {{"settings", all}, {"toM", 600}, {"stepM", 200}});
    ASSERT_TRUE(r.at("ok").get<bool>()) << r.dump();
    EXPECT_EQ(r.at("settings"), all);
    EXPECT_EQ(r.at("rows").size(), 3U);
    for (const json& row : r.at("rows")) {
        EXPECT_GE(row.at("probability").get<double>(), 0.0);
        EXPECT_LE(row.at("probability").get<double>(), 1.0);
        EXPECT_GT(row.at("sigmaUpCm").get<double>(), 0.0);
        EXPECT_GT(row.at("sigmaRightCm").get<double>(), 0.0);
    }
    for (const json& part : r.at("parts")) {
        EXPECT_FALSE(part.at("source").get<std::string>().empty());
        EXPECT_TRUE(std::isfinite(part.at("upCm").get<double>()));
        EXPECT_TRUE(std::isfinite(part.at("rightCm").get<double>()));
    }
    EXPECT_LE(r.at("shots50").get<int>(), r.at("shots80").get<int>());
    EXPECT_LE(r.at("shots80").get<int>(), r.at("shots95").get<int>());
    // An unknown kind of target and a bad number are ignored.
    r = Ok("wez", {{"settings", {{"targetKind", "barrel"}, {"groupMoa", "tight"}}}});
    EXPECT_EQ(r.at("settings").at("targetKind"), "figure");
    EXPECT_EQ(r.at("settings").at("groupMoa"), 0.75);
}

// ---- Rifles, cartridges and the selection --------------------------------------

TEST_F(BridgeApi, UnknownRecordsAreReported) {
    EXPECT_EQ(Fails("rifleForm", {{"id", 999}}), "rifle not found");
    EXPECT_EQ(Fails("cartridgeForm", {{"id", 999}}), "cartridge not found");
    EXPECT_EQ(Fails("bulletForm", {{"id", 999}}), "bullet not found");
    EXPECT_EQ(Fails("cartridgeFormFromLibrary", {{"id", 999}}), "cartridge not found");
    EXPECT_EQ(Fails("cartridgeFormWithBullet", {{"bulletId", 999}}), "bullet not found");
    EXPECT_EQ(Fails("exportJson", {{"kind", "rifle"}, {"id", 999}}), "rifle not found");
    EXPECT_EQ(Fails("exportJson", {{"kind", "scope"}, {"id", 1}}), "Unknown export kind: scope");
    EXPECT_FALSE(Fails("deleteRifle", {{"id", 999}}).empty());
    EXPECT_FALSE(Fails("setShotUsed", {{"id", 999}}).empty());
    // Saving a form that is not filled in.
    EXPECT_EQ(Fails("saveRifle", {{"form", 5}}), "Enter a rifle name.");
    EXPECT_EQ(Fails("saveRifle"), "Enter a rifle name.");
    EXPECT_EQ(Fails("saveCartridge"), "Enter a cartridge name.");
    EXPECT_FALSE(Fails("saveBullet", {{"form", {{"name", ""}}}}).empty());
    EXPECT_TRUE(Ok("state").at("rifles").empty());
}

TEST_F(BridgeApi, DeletingTheCurrentRifleSelectsAnother) {
    const json first = Sample();
    const int other = SaveRifle({{"name", "Other"}, {"caliber", ".308 Win"}});
    EXPECT_EQ(Ok("state").at("currentRifleId"), other);
    const json st = Ok("deleteRifle", {{"id", other}});
    ASSERT_EQ(st.at("rifles").size(), 1U);
    EXPECT_EQ(st.at("currentRifleId"), first.at("currentRifleId"));
    EXPECT_EQ(st.at("currentProfileId"), first.at("currentProfileId"));
    EXPECT_EQ(st.at("currentPair").at("rifleName"), "Rifle");
    // The last one: no pair left.
    const json none = Ok("deleteRifle", {{"id", first.at("currentRifleId")}});
    EXPECT_TRUE(none.at("rifles").empty());
    EXPECT_EQ(none.at("currentRifleId"), 0);
    EXPECT_EQ(none.at("currentProfileId"), 0);
    EXPECT_TRUE(none.at("currentPair").empty());
    EXPECT_EQ(Fails("setZeroOffset", {{"upCm", 1}}), "Choose a rifle and a cartridge.");
}

TEST_F(BridgeApi, SelectKeepsWhatIsNotGiven) {
    const json first = Sample();
    const int load = SaveCartridge({{"name", "Second load"}});
    json st = Ok("select", {{"cartridgeId", first.at("currentCartridgeId")}});
    EXPECT_EQ(st.at("currentRifleId"), first.at("currentRifleId"));
    EXPECT_EQ(st.at("currentCartridgeId"), first.at("currentCartridgeId"));
    st = Ok("select", {{"cartridgeId", load}});
    EXPECT_EQ(st.at("currentCartridgeId"), load);
    EXPECT_EQ(st.at("currentPair").at("cartridgeName"), "Second load");
    // An id that is not there: the first one instead.
    st = Ok("select", {{"rifleId", 999}, {"cartridgeId", "x"}});
    EXPECT_EQ(st.at("currentRifleId"), first.at("currentRifleId"));
    EXPECT_EQ(st.at("currentCartridgeId"), st.at("cartridges")[0].at("id"));
    EXPECT_EQ(Ok("select").at("currentProfileId"), st.at("currentProfileId"));
}

TEST_F(BridgeApi, SampleNamesAndZeroOffset) {
    const json st = Ok("addSample");
    EXPECT_EQ(st.at("currentPair").at("rifleName"), "Sample .308 Win");
    EXPECT_EQ(st.at("currentPair").at("cartridgeName"), "Sample SMK 175 gr");
    EXPECT_EQ(st.at("currentPair").at("zeroRangeM"), 100.0);
    const json pair = Ok("setZeroOffset", {{"upCm", -2}, {"rightCm", 1.5}});
    EXPECT_NEAR(pair.at("offsetUpCm").get<double>(), -2.0, 1e-9);
    EXPECT_NEAR(pair.at("offsetRightCm").get<double>(), 1.5, 1e-9);
    EXPECT_NEAR(Ok("state").at("currentPair").at("offsetRightCm").get<double>(), 1.5, 1e-9);
}

TEST_F(BridgeApi, FormsRoundTrip) {
    const int rifle = SaveRifle({{"name", "Left"},
                                 {"caliber", "6.5 Creedmoor"},
                                 {"sightHeightCm", 6.5},
                                 {"twistIn", 8},
                                 {"twistLeft", true},
                                 {"clickUnits", "moa"},
                                 {"clickValue", 0.25},
                                 {"zeroRangeM", 200},
                                 {"zeroTemperatureC", 5},
                                 {"zeroPressureHpa", 980},
                                 {"zeroAltitudeM", 300},
                                 {"zeroHumidityPct", 20},
                                 {"zeroPowderC", 8}});
    const json r = Ok("rifleForm", {{"id", rifle}});
    EXPECT_EQ(r.at("rifleId"), rifle);
    EXPECT_EQ(r.at("caliber"), "6.5 Creedmoor");
    EXPECT_NEAR(r.at("sightHeightCm").get<double>(), 6.5, 1e-9);
    EXPECT_NEAR(r.at("twistIn").get<double>(), 8.0, 1e-9);
    EXPECT_TRUE(r.at("twistLeft").get<bool>());
    EXPECT_EQ(r.at("clickUnits"), "moa");
    EXPECT_NEAR(r.at("clickValue").get<double>(), 0.25, 1e-9);
    EXPECT_EQ(r.at("focalPlane"), "ffp");
    EXPECT_NEAR(r.at("zeroRangeM").get<double>(), 200.0, 1e-9);
    EXPECT_NEAR(r.at("zeroTemperatureC").get<double>(), 5.0, 1e-9);
    EXPECT_NEAR(r.at("zeroPressureHpa").get<double>(), 980.0, 1e-6);
    EXPECT_NEAR(r.at("zeroAltitudeM").get<double>(), 300.0, 1e-9);
    EXPECT_NEAR(r.at("zeroHumidityPct").get<double>(), 20.0, 1e-9);
    EXPECT_NEAR(r.at("zeroPowderC").get<double>(), 8.0, 1e-9);

    const int cartridge = SaveCartridge({{"caliber", "6.5 Creedmoor"},
                                         {"diameterIn", 0.264},
                                         {"massGr", 140},
                                         {"lengthIn", 1.4},
                                         {"dragTable", "G1"},
                                         {"bc", 0.61},
                                         {"powderReferenceC", 21},
                                         {"powderSensitivity", 0.1}});
    const json c = Ok("cartridgeForm", {{"id", cartridge}});
    EXPECT_EQ(c.at("cartridgeId"), cartridge);
    EXPECT_EQ(c.at("dragTable"), "G1");
    EXPECT_NEAR(c.at("bc").get<double>(), 0.61, 1e-9);
    EXPECT_NEAR(c.at("lengthIn").get<double>(), 1.4, 1e-9);
    EXPECT_NEAR(c.at("powderReferenceC").get<double>(), 21.0, 1e-9);
    EXPECT_NEAR(c.at("powderSensitivity").get<double>(), 0.1, 1e-9);
    EXPECT_EQ(Ok("state").at("cartridges")[0].at("bulletName"), "SMK 175");

    // The bullet editor: every field and its bands come back.
    const json b = {{"id", 0},
                    {"name", "Hybrid 140"},
                    {"manufacturer", "Maker"},
                    {"caliber", "6.5 Creedmoor"},
                    {"massGr", 140},
                    {"diameterIn", 0.264},
                    {"lengthIn", 1.39},
                    {"dragTable", "G7"},
                    {"bc", 0.31},
                    {"bands", "none"},  // not a list: no bands
                    {"notes", "test"},
                    {"hasCustomCurve", false}};
    const int bullet = Ok("saveBullet", {{"form", b}}).at("id");
    const json back = Ok("bulletForm", {{"id", bullet}});
    EXPECT_EQ(back.at("manufacturer"), "Maker");
    EXPECT_EQ(back.at("notes"), "test");
    EXPECT_TRUE(back.at("bands").empty());
    EXPECT_NEAR(back.at("lengthIn").get<double>(), 1.39, 1e-9);
    EXPECT_FALSE(back.at("hasCustomCurve").get<bool>());
    const json list = Ok("libraryBullets", {{"filter", "hybrid"}});
    ASSERT_EQ(list.size(), 1U);
    EXPECT_EQ(list[0].at("manufacturer"), "Maker");
    EXPECT_EQ(list[0].at("bcBands"), 0);
}

// ---- Targets and situations ------------------------------------------------------

TEST_F(BridgeApi, TargetsWithoutAPairOrWithBadInput) {
    // Saved without a rifle: listed, with nothing computed.
    const json t = Ok("saveTargets", {{"targets",
                                       {{{"name", "Far"},
                                         {"rangeM", 800},
                                         {"lookAngleDeg", 90},
                                         {"windSpeed", 99},
                                         {"windFromDeg", 270}}}}});
    ASSERT_EQ(t.size(), 1U);
    EXPECT_FALSE(t[0].at("ok").get<bool>());
    EXPECT_FALSE(t[0].contains("elevation"));
    EXPECT_EQ(t[0].at("lookAngleDeg"), 60.0);  // clamped
    EXPECT_EQ(t[0].at("windSpeed"), 40.0);
    EXPECT_EQ(t[0].at("windFromDeg"), 270.0);

    EXPECT_EQ(Fails("saveTargets", {{"targets", "Far"}}), "Targets must be a list.");
    EXPECT_EQ(Fails("selectTarget", {{"index", -1}}), "No such target.");
    EXPECT_EQ(Fails("selectTarget"), "No such target.");
    EXPECT_EQ(Fails("saveTargets", {{"targets", {{{"name", "x"}, {"rangeM", 3001}}}}}),
              "A target range must be between 10 and 3000 m.");
    EXPECT_EQ(Ok("targets").size(), 1U);  // kept
    // An empty list clears them.
    EXPECT_TRUE(Ok("saveTargets").empty());
    EXPECT_TRUE(Ok("targets").empty());
}

// Selecting a target brings back one wind: its zones are dropped.
TEST_F(BridgeApi, SelectingATargetDropsTheWindZones) {
    Sample();
    Ok("setConditions", {{"windZones", {{{"speedMps", 5}}}}, {"windUntilM", 200}});
    Ok("saveTargets", {{"targets", {{{"name", "A"}, {"rangeM", 450}}}}});
    const json st = Ok("selectTarget", {{"index", 0}});
    EXPECT_EQ(st.at("conditions").at("targetRangeM"), 450.0);
    EXPECT_TRUE(st.at("conditions").at("windZones").empty());
    EXPECT_EQ(st.at("conditions").at("windSpeed"), 0.0);
    EXPECT_EQ(st.at("conditions").at("lookAngleDeg"), 0.0);
}

TEST_F(BridgeApi, SituationsNeedAPairAndKeepTheOthers) {
    EXPECT_EQ(Fails("saveSituation", {{"name", "Range day"}}), "Choose a rifle and a cartridge.");
    Sample();
    Ok("setConditions", {{"targetRangeM", 400}});
    Ok("saveSituation", {{"name", "First"}});
    Ok("setConditions", {{"targetRangeM", 500}});
    json list = Ok("saveSituation", {{"name", "Second"}});
    ASSERT_EQ(list.size(), 2U);
    EXPECT_EQ(list[0].at("name"), "First");
    EXPECT_EQ(list[0].at("rangeM"), 400.0);
    EXPECT_EQ(list[1].at("name"), "Second");
    // Replacing one keeps the other.
    Ok("setConditions", {{"targetRangeM", 550}});
    list = Ok("saveSituation", {{"name", "First"}});
    ASSERT_EQ(list.size(), 2U);
    EXPECT_EQ(list[0].at("name"), "Second");
    EXPECT_EQ(list[1].at("rangeM"), 550.0);
    // Deleting one keeps the other; an unknown name changes nothing.
    list = Ok("deleteSituation", {{"name", "First"}});
    ASSERT_EQ(list.size(), 1U);
    EXPECT_EQ(list[0].at("name"), "Second");
    EXPECT_EQ(Ok("deleteSituation", {{"name", "Nothing"}}).size(), 1U);
    EXPECT_EQ(Fails("applySituation"), "No situation of this name.");
}

// ---- Shot log, truing, DSF and BC ---------------------------------------------

TEST_F(BridgeApi, ShotLogWithWindageAndUnits) {
    EXPECT_TRUE(Ok("shots").empty());  // no pair, no log
    Sample();
    EXPECT_EQ(Fails("logShot", {{"rangeM", 0}, {"elevation", 1}}), "Enter a target range.");
    Ok("logShot", {{"rangeM", 400}, {"elevation", 2.5}, {"hasWindage", true}, {"windage", -0.4}});
    Ok("logShot", {{"rangeM", 600}, {"elevation", 4.5}});
    json shots = Ok("shots");
    ASSERT_EQ(shots.size(), 2U);
    const json* windy = shots[0].at("rangeM") == 400.0 ? &shots[0] : &shots[1];
    const json* plain = windy == &shots[0] ? &shots[1] : &shots[0];
    EXPECT_TRUE(windy->at("hasWindage").get<bool>());
    EXPECT_NEAR(windy->at("observedWindage").get<double>(), -0.4, 1e-9);
    EXPECT_NEAR(windy->at("observed").get<double>(), 2.5, 1e-9);
    EXPECT_TRUE(windy->at("predicted").is_number());
    EXPECT_NEAR(windy->at("temperatureC").get<double>(), 15.0, 1e-6);
    EXPECT_TRUE(windy->at("used").get<bool>());
    EXPECT_FALSE(plain->at("hasWindage").get<bool>());
    EXPECT_EQ(plain->at("observedWindage"), 0.0);
    // In MOA the same shot reads 3.44 times as much.
    Ok("setSettings", {{"angleUnit", "moa"}});
    shots = Ok("shots");
    windy = shots[0].at("rangeM") == 400.0 ? &shots[0] : &shots[1];
    EXPECT_NEAR(windy->at("observed").get<double>(), 2.5 * 3.4377, 1e-3);
    // setShotUsed without "used" puts it back to use.
    Ok("setShotUsed", {{"id", windy->at("id")}, {"used", false}});
    Ok("setShotUsed", {{"id", windy->at("id")}});
    for (const json& s : Ok("shots")) {
        EXPECT_TRUE(s.at("used").get<bool>());
    }
}

TEST_F(BridgeApi, TruingAndDsfNeedSomethingToWorkOn) {
    // Without a pair.
    EXPECT_FALSE(Ok("computeTruing").at("ok").get<bool>());
    EXPECT_FALSE(Ok("computeDsf").at("ok").get<bool>());
    EXPECT_FALSE(Fails("resetTruing").empty());
    EXPECT_FALSE(Fails("resetDsf").empty());
    EXPECT_FALSE(Fails("setDsf").empty());
    Sample();
    const json t = Ok("computeTruing");  // no shots
    EXPECT_FALSE(t.at("ok").get<bool>());
    EXPECT_FALSE(t.at("error").get<std::string>().empty());
    EXPECT_TRUE(t.at("points").empty());
    EXPECT_EQ(Fails("applyTruing"), "Nothing to apply.");
    // A point without a factor is neutral (1); without a Mach it is refused.
    Ok("setDsf", {{"points", {{{"mach", 1.2}}}}});
    const json dsf = Ok("solution").at("dsf");
    ASSERT_EQ(dsf.size(), 1U);
    EXPECT_EQ(dsf[0].at("factor"), 1.0);
    EXPECT_FALSE(Fails("setDsf", {{"points", {{{"factor", 1.1}}}}}).empty());
    Ok("setDsf");  // no points: none
    EXPECT_TRUE(Ok("solution").at("dsf").empty());
    // A truing computed in mrad is dropped when the unit changes.
    for (double range : {500.0, 900.0}) {
        Ok("setConditions", {{"targetRangeM", range}});
        Ok("logShot",
           {{"rangeM", range}, {"elevation", Ok("solution").at("elevation").get<double>() * 1.05}});
    }
    const json fit = Ok("computeTruing");
    ASSERT_TRUE(fit.at("ok").get<bool>()) << fit.dump();
    EXPECT_EQ(fit.at("points").size(), 2U);
    EXPECT_GT(fit.at("velocityBefore").get<double>(), fit.at("velocityAfter").get<double>());
    Ok("setSettings", {{"angleUnit", "moa"}});
    EXPECT_EQ(Fails("applyTruing"), "Nothing to apply.");
}

TEST_F(BridgeApi, BcCalculatorDefaultsToG7) {
    const json r = Ok("bcCalculator", {{"vNearMps", 800}, {"vFarMps", 770}, {"distanceM", 100}});
    EXPECT_TRUE(r.at("ok").get<bool>()) << r.dump();
    EXPECT_EQ(r.at("table"), "G7");
    const json g1 = Ok("bcCalculator",
                       {{"table", "G1"}, {"vNearMps", 800}, {"vFarMps", 770}, {"distanceM", 100}});
    EXPECT_EQ(g1.at("table"), "G1");
    EXPECT_GT(g1.at("bc").get<double>(), r.at("bc").get<double>());  // G1 numbers are larger
}

// ---- Photos, sharing and the library ---------------------------------------------

TEST_F(BridgeApi, PhotoLimitsAndKinds) {
    const json st = Sample();
    const int cartridge = st.at("currentCartridgeId");
    EXPECT_EQ(Fails("photos", {{"kind", "scope"}}), "Unknown kind of record.");
    EXPECT_EQ(Fails("photo", {{"id", cartridge}}), "Unknown kind of record.");
    EXPECT_EQ(Fails("setPhoto", {{"kind", "cartridge"}, {"id", 999}, {"image", "AAEC"}}),
              "Save the record first.");
    // Just over 2 MB: refused.
    const std::string big((2u << 20) / 3 * 4 + 8, 'A');
    EXPECT_EQ(Fails("setPhoto", {{"kind", "cartridge"}, {"id", cartridge}, {"image", big}}),
              "The picture is too large.");
    EXPECT_TRUE(Ok("photos", {{"kind", "cartridge"}}).empty());
    // URL-safe base64 and line breaks are read too; the answer is standard.
    Ok("setPhoto", {{"kind", "cartridge"}, {"id", cartridge}, {"image", "-_8=\n"}});
    EXPECT_EQ(Ok("photo", {{"kind", "cartridge"}, {"id", cartridge}}), "+/8=");
    EXPECT_EQ(Ok("photo", {{"kind", "rifle"}, {"id", 999}}), "");
}

TEST_F(BridgeApi, ExportFileNamesAreSafe) {
    const int rifle = SaveRifle({{"name", R"(A/B: "long" <*>?|\)"}, {"caliber", ".308 Win"}});
    EXPECT_EQ(Ok("exportJson", {{"kind", "rifle"}, {"id", rifle}}).at("fileName"),
              "A_B_ _long_ ______.holdmark.json");
    // A factory load of the library is not in the list: a generic name.
    const json r =
        Ok("importFiles", {{"files", json::array({SeedFile("ammo/338 Lapua 250gr.ammo")})}});
    ASSERT_EQ(r.at("imported"), 1) << r.dump();
    EXPECT_TRUE(Ok("state").at("cartridges").empty());
    const json lib = Ok("libraryCartridges", {{"filter", "338"}});
    ASSERT_EQ(lib.size(), 1U);
    EXPECT_FALSE(lib[0].at("matches").get<bool>());
    const json e = Ok("exportJson", {{"kind", "cartridge"}, {"id", lib[0].at("id")}});
    EXPECT_EQ(e.at("fileName"), "cartridge.holdmark.json");
    // Imported, it is the user's own and the current one.
    const json st = Ok("importShared", {{"text", e.at("json")}});
    ASSERT_EQ(st.at("cartridges").size(), 1U);
    EXPECT_EQ(st.at("currentCartridgeId"), st.at("cartridges")[0].at("id"));
    EXPECT_EQ(st.at("currentRifleId"), rifle);  // the rifle stays
}

TEST_F(BridgeApi, SeedWithoutFilesOrWithBrokenCatalogs) {
    // No files: only the version is recorded.
    const json r = Ok("seed", {{"version", 3}});
    EXPECT_EQ(r.at("imported"), 0);
    EXPECT_EQ(r.at("skipped"), 0);
    EXPECT_EQ(Ok("seedVersion").at("version"), 3);
    // A catalog that does not parse is taken (not imported) and leaves the list empty.
    const json bad = {{"name", "published_scopes.json"},
                      {"content", R"({"format": "balcalc-scopes", "scopes": [oops)"}};
    const json other = {{"name", "published_rifles.json"},
                        {"content", R"({"format": "balcalc-rifles", "rifles": {}})"}};
    EXPECT_EQ(Ok("seed", {{"version", 4}, {"files", {bad, other}}}).at("imported"), 0);
    EXPECT_TRUE(Ok("libraryScopes").empty());
    EXPECT_TRUE(Ok("libraryRifles").empty());
    // A file of the library that is damaged is reported.
    const json broken =
        Ok("seed", {{"version", 5}, {"files", {{{"name", "x.ammo"}, {"content", "?"}}}}});
    EXPECT_EQ(broken.at("imported"), 0);
    EXPECT_EQ(broken.at("skipped"), 1);
    ASSERT_EQ(broken.at("problems").size(), 1U);
    EXPECT_EQ(broken.at("problems")[0].get<std::string>().rfind("x.ammo: ", 0), 0U);
    EXPECT_TRUE(Ok("importFiles").at("problems").empty());
}

// ---- A database changed behind the bridge's back -----------------------------------

// A rifle whose scope is gone: no clicks, no reticle, holds in plain mrad.
TEST_F(BridgeApiFile, RifleWithoutAScope) {
    Sample();
    Sql("UPDATE rifle SET scope_id = NULL");
    Ok("setConditions", {{"targetRangeM", 600}});
    const json sol = Ok("solution");
    ASSERT_TRUE(sol.at("ok").get<bool>()) << sol.dump();
    EXPECT_FALSE(sol.at("hasScope").get<bool>());
    EXPECT_EQ(sol.at("clickElevation"), 0.0);
    EXPECT_EQ(sol.at("clickWindage"), 0.0);
    EXPECT_FALSE(sol.at("hasReticle").get<bool>());
    EXPECT_FALSE(sol.contains("targetX"));
    EXPECT_FALSE(Ok("rangeTable").at("hasScope").get<bool>());
    // Targets are held by their whole correction (nothing is dialled).
    const json t = Ok("saveTargets", {{"targets", {{{"name", "A"}, {"rangeM", 600}}}}});
    ASSERT_TRUE(t[0].at("ok").get<bool>());
    EXPECT_NEAR(t[0].at("holdY").get<double>(), -t[0].at("elevation").get<double>(), 1e-9);
}

// The pair's bullet is gone: every computation says so instead of failing.
TEST_F(BridgeApiFile, PairThatNoLongerLoads) {
    const json st = Sample();
    const int rifle = st.at("currentRifleId");
    const int cartridge = st.at("currentCartridgeId");
    Ok("saveTargets", {{"targets", {{{"name", "A"}, {"rangeM", 300}}}}});
    Sql("DELETE FROM bullet");
    const json sol = Ok("solution");
    EXPECT_FALSE(sol.at("ok").get<bool>());
    EXPECT_NE(sol.at("error").get<std::string>().find("bullet"), std::string::npos);
    const json t = Ok("rangeTable");
    EXPECT_FALSE(t.at("ok").get<bool>());
    EXPECT_EQ(t.at("error"), sol.at("error"));
    const json curves =
        Ok("compareCurves", {{"pairs", {{{"rifleId", rifle}, {"cartridgeId", cartridge}}}}});
    ASSERT_EQ(curves.size(), 1U);
    EXPECT_FALSE(curves[0].at("ok").get<bool>());
    EXPECT_EQ(curves[0].at("error"), sol.at("error"));
    EXPECT_TRUE(Ok("state").at("currentPair").empty());
    EXPECT_FALSE(Ok("targets")[0].at("ok").get<bool>());
    EXPECT_EQ(Fails("wez"), sol.at("error"));
    EXPECT_EQ(Fails("bcCalculator", {{"mode", "hit"}, {"rangeM", 500}, {"elevation", 3}}),
              sol.at("error"));
    EXPECT_EQ(Fails("logShot", {{"rangeM", 300}, {"elevation", 1}}), sol.at("error"));
}

// A pair whose truing on disk is impossible (a drag scale of 0): a
// solution that is not ok and says it is the truing, like other bad data.
TEST_F(BridgeApiFile, ImpossibleTruingOnDisk) {
    Sample();
    Sql("UPDATE profile SET drag_scale = 0");
    const std::string why = "profile truing: drag scale must be positive";
    const json sol = Ok("solution");
    EXPECT_FALSE(sol.at("ok").get<bool>());
    EXPECT_EQ(sol.at("error"), why);
    const json table = Ok("rangeTable");
    EXPECT_FALSE(table.at("ok").get<bool>());
    EXPECT_EQ(table.at("error"), why);
    Sql("UPDATE profile SET drag_scale = 1");
    EXPECT_TRUE(Ok("solution").at("ok").get<bool>());
}

// A rifle added by another connection is not in the bridge's list yet:
// exported under a generic name.
TEST_F(BridgeApiFile, ExportOfARifleNotListedYet) {
    const json st = Sample();
    Sql("INSERT INTO rifle (name, caliber) VALUES ('Hidden', '.308 Win')");
    const json rifles = Ok("state").at("rifles");
    ASSERT_EQ(rifles.size(), 1U);
    const int hidden = rifles[0].at("id").get<int>() + 1;
    const json e = Ok("exportJson", {{"kind", "rifle"}, {"id", hidden}});
    EXPECT_EQ(e.at("fileName"), "rifle.holdmark.json");
    EXPECT_NE(e.at("json").get<std::string>().find("Hidden"), std::string::npos);
    EXPECT_EQ(Ok("select", {{"rifleId", hidden}}).at("rifles").size(), 2U);
}

// A damaged target list in the settings: a field of the wrong type takes
// its default, the list still loads.
TEST_F(BridgeApiFile, DamagedTargetEntry) {
    Sample();
    Setting("ui.targets", R"([{"name": 5, "rangeM": "far"}, 7])");
    json t = Ok("targets");
    ASSERT_EQ(t.size(), 2U);
    EXPECT_EQ(t[0].at("name"), "");
    EXPECT_EQ(t[0].at("rangeM"), 0.0);
    EXPECT_EQ(t[1].at("rangeM"), 0.0);
    EXPECT_EQ(Ok("selectTarget", {{"index", 0}}).at("conditions").at("targetRangeM"), 300.0);
    for (const char* key : {"lookAngleDeg", "windSpeed", "windFromDeg"}) {
        Setting("ui.targets", json::array({{{"name", "A"}, {"rangeM", 300}, {key, "x"}}}).dump());
        t = Ok("targets");
        ASSERT_EQ(t.size(), 1U) << key;
        EXPECT_EQ(t[0].at(key), std::string(key) == "windFromDeg" ? 180.0 : 0.0) << key;
        EXPECT_TRUE(t[0].at("ok").get<bool>()) << key;
    }
    Setting("ui.targets", R"([{"name": "A"}])");  // no range: the session's
    t = Ok("targets");
    ASSERT_EQ(t.size(), 1U);
    EXPECT_EQ(t[0].at("rangeM"), 0.0);
    EXPECT_TRUE(t[0].at("ok").get<bool>());
    EXPECT_EQ(Ok("selectTarget", {{"index", 0}}).at("conditions").at("targetRangeM"), 300.0);
}

// A damaged situation: fields of the wrong type take their defaults.
TEST_F(BridgeApiFile, DamagedSituationEntry) {
    const json st = Sample();
    Setting("ui.situations",
            json::array({{{"name", "Range"},
                          {"rifleId", st.at("currentRifleId")},
                          {"cartridgeId", st.at("currentCartridgeId")},
                          {"holdMode", 3},
                          {"conditions", "windy"}},
                         {{"name", 1}, {"rifleId", "x"}, {"conditions", {{"targetRangeM", 450}}}},
                         5})
                .dump());
    const json list = Ok("situations");
    ASSERT_EQ(list.size(), 3U);
    EXPECT_EQ(list[0].at("rangeM"), 0.0);
    EXPECT_TRUE(list[0].at("available").get<bool>());
    EXPECT_EQ(list[1].at("name"), "");
    EXPECT_EQ(list[1].at("rangeM"), 450.0);
    EXPECT_FALSE(list[1].at("available").get<bool>());
    const json applied = Ok("applySituation", {{"name", "Range"}});
    EXPECT_EQ(applied.at("holdMode"), st.at("holdMode"));
    EXPECT_EQ(applied.at("conditions"), st.at("conditions"));
}

// Tables gone from the file: each call reports the storage error.
TEST_F(BridgeApiFile, StorageErrorsAreReported) {
    const json st = Sample();
    const int rifle = st.at("currentRifleId");
    const int cartridge = st.at("currentCartridgeId");

    Sql("DROP TABLE photo");
    EXPECT_FALSE(Fails("photos", {{"kind", "rifle"}}).empty());
    EXPECT_FALSE(Fails("photo", {{"kind", "rifle"}, {"id", rifle}}).empty());
    EXPECT_FALSE(Fails("setPhoto", {{"kind", "rifle"}, {"id", rifle}, {"image", "AAEC"}}).empty());

    Sql("DROP TABLE dope_log");
    EXPECT_FALSE(Fails("shots").empty());
    EXPECT_FALSE(Fails("logShot", {{"rangeM", 300}, {"elevation", 1}}).empty());

    Sql("DROP TABLE reticle");
    EXPECT_FALSE(Fails("reticles").empty());

    Sql("DROP TABLE bullet_bc_band");
    EXPECT_FALSE(Fails("libraryBullets").empty());

    Sql("DROP TABLE app_setting");
    EXPECT_FALSE(Fails("seedVersion").empty());
    EXPECT_FALSE(Fails("seed", {{"version", 9}}).empty());
    EXPECT_FALSE(Fails("wez").empty());

    Sql("DROP TABLE cartridge");
    EXPECT_FALSE(
        Fails("setPhoto", {{"kind", "cartridge"}, {"id", cartridge}, {"image", "AAEC"}}).empty());
    EXPECT_FALSE(Fails("libraryCartridges").empty());
    EXPECT_FALSE(Fails("pairOptions").empty());

    Sql("DROP TABLE rifle");
    EXPECT_FALSE(Fails("setPhoto", {{"kind", "rifle"}, {"id", rifle}, {"image", "AAEC"}}).empty());
    EXPECT_FALSE(Fails("select", {{"rifleId", rifle}}).empty());
    EXPECT_FALSE(Fails("importFiles").empty());
    EXPECT_FALSE(Fails("addSample").empty());
}

}  // namespace
}  // namespace ballistics::bridge
