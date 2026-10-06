// The JSON facade the Kotlin app talks to: every method, through Call()
// only, as the app sees it.
#include <ballistics/bridge/api.h>

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

namespace ballistics::bridge {
namespace {

using nlohmann::json;

class Bridge : public ::testing::Test {
protected:
    void SetUp() override { Ok("open", {{"path", ":memory:"}}); }

    // The result of a call that must succeed.
    json Ok(const std::string& method, const json& args = json::object()) {
        const json r = json::parse(api_.Call(method, args.dump()));
        EXPECT_TRUE(r.at("ok").get<bool>()) << method << ": " << r.dump();
        return r.value("result", json());
    }
    // The error of a call that must fail.
    std::string Fails(const std::string& method, const json& args = json::object()) {
        const json r = json::parse(api_.Call(method, args.dump()));
        EXPECT_FALSE(r.at("ok").get<bool>()) << method << ": " << r.dump();
        return r.value("error", "");
    }

    json Sample() {
        return Ok("addSample", {{"rifleName", "Rifle"}, {"cartridgeName", "Load"}});
    }

    Api api_;
};

TEST_F(Bridge, EmptyDatabaseNeedsARifleAndCartridge) {
    const json st = Ok("state");
    EXPECT_TRUE(st.at("rifles").empty());
    EXPECT_EQ(st.at("currentProfileId"), 0);
    EXPECT_TRUE(st.at("currentPair").empty());
    EXPECT_EQ(st.at("angleUnit"), "mrad");
    const json sol = Ok("solution");
    EXPECT_FALSE(sol.at("ok").get<bool>());
    EXPECT_EQ(sol.at("error"), "Choose a rifle and a cartridge.");
    EXPECT_EQ(Fails("logShot", {{"rangeM", 300}, {"elevation", 1}}),
              "Choose a rifle and a cartridge.");
}

TEST_F(Bridge, SampleGivesTheKnownSolution) {
    const json st = Sample();
    ASSERT_EQ(st.at("rifles").size(), 1U);
    ASSERT_EQ(st.at("cartridges").size(), 1U);
    EXPECT_GT(st.at("currentProfileId").get<int>(), 0);
    EXPECT_EQ(st.at("currentPair").at("rifleName"), "Rifle");
    EXPECT_EQ(st.at("conditions").at("targetRangeM"), 300.0);
    const json sol = Ok("solution");
    ASSERT_TRUE(sol.at("ok").get<bool>()) << sol.dump();
    // Own G7 bullet (no starter library here); the library SMK gives 1.61.
    EXPECT_NEAR(sol.at("elevation").get<double>(), 1.63, 0.01);
    EXPECT_EQ(sol.at("elevationClicks"), 16.0);
    EXPECT_TRUE(sol.at("hasScope").get<bool>());
    EXPECT_FALSE(sol.at("hasReticle").get<bool>());
    EXPECT_EQ(sol.at("holdMode"), "dial_elevation");
}

TEST_F(Bridge, ConditionsAndSettingsChangeTheSolution) {
    Sample();
    const double base = Ok("solution").at("elevation").get<double>();
    const json c = Ok("setConditions", {{"targetRangeM", 800}, {"temperatureC", -10}});
    EXPECT_EQ(c.at("targetRangeM"), 800.0);
    EXPECT_EQ(c.at("pressureHpa"), 1013.25); // untouched keys kept
    const double far = Ok("solution").at("elevation").get<double>();
    EXPECT_GT(far, base * 3);

    Ok("setSettings", {{"angleUnit", "moa"}});
    EXPECT_NEAR(Ok("solution").at("elevation").get<double>(), far * 3.4377, 0.01);
    Ok("setSettings", {{"angleUnit", "furlongs"}}); // ignored
    EXPECT_EQ(Ok("state").at("angleUnit"), "moa");

    Ok("setSettings", {{"holdMode", "hold"}});
    const json held = Ok("solution");
    EXPECT_EQ(held.at("dialElevationClicks"), 0.0);
    EXPECT_NEAR(held.at("targetY").get<double>(), -held.at("elevation").get<double>() / 3.4377,
                0.01);

    Ok("setConditions", {{"windSpeed", 5}, {"windFromDeg", 90}});
    EXPECT_GT(Ok("solution").at("windage").get<double>(), 0.5);
}

TEST_F(Bridge, DensityAltitudeAndWarnings) {
    Sample();
    json sol = Ok("solution");
    EXPECT_TRUE(sol.at("warnings").is_array());
    EXPECT_GT(sol.at("pointBlankFarM").get<double>(), 100.0);
    EXPECT_GT(sol.at("apexRangeM").get<double>(), 0.0);
    EXPECT_NEAR(sol.at("pressureHpa").get<double>(), 1013.25, 1e-9);

    const json c = Ok("setConditions", {{"useDensityAltitude", true}, {"densityAltitudeM", 2000}});
    EXPECT_EQ(c.at("useDensityAltitude"), true);
    EXPECT_EQ(c.at("densityAltitudeM"), 2000.0);
    sol = Ok("solution");
    EXPECT_NEAR(sol.at("densityAltitudeM").get<double>(), 2000.0, 0.1);
    EXPECT_LT(sol.at("pressureHpa").get<double>(), 850.0);

    // Far from the sample's zero air (15 C): a warning with the difference.
    Ok("setConditions", {{"useDensityAltitude", false}, {"temperatureC", 35}});
    bool found = false;
    const json warnings = Ok("solution").at("warnings");
    for (const json& w : warnings) {
        if (w.at("code") == "zeroTemperature") {
            found = true;
            EXPECT_NEAR(w.at("value").get<double>(), 20.0, 1e-6);
        }
    }
    EXPECT_TRUE(found) << warnings.dump();
}

TEST_F(Bridge, RangeTableAndCurve) {
    Sample();
    Ok("setSettings", {{"tableFromM", 0}, {"tableToM", 1000}, {"tableStepM", 100}});
    const json t = Ok("rangeTable");
    ASSERT_TRUE(t.at("ok").get<bool>());
    ASSERT_EQ(t.at("rows").size(), 11U);
    EXPECT_NEAR(t.at("rows")[3].at("elevation").get<double>(),
                Ok("solution").at("elevation").get<double>(), 1e-9); // 300 m
    EXPECT_EQ(Ok("trajectoryCurve", {{"maxRangeM", 1000}, {"points", 250}}).at("rows").size(), 251U);
}

TEST_F(Bridge, RifleAndCartridgeFormsAndSelection) {
    json r = Ok("rifleForm", {{"id", 0}});
    EXPECT_EQ(r.at("zeroRangeM"), 100.0);
    r["name"] = "Tikka";
    r["caliber"] = ".308 Win";
    const auto rifle = Ok("saveRifle", {{"form", r}}).at("id").get<int>();
    EXPECT_EQ(Ok("state").at("currentRifleId"), rifle);
    EXPECT_EQ(Ok("state").at("currentProfileId"), 0); // no cartridge yet

    json c = Ok("cartridgeForm", {{"id", 0}});
    c["name"] = "Load";
    c["caliber"] = ".308 Win";
    c["bulletName"] = "SMK 175";
    c["bc"] = 0.243;
    c["massGr"] = 175;
    c["diameterIn"] = 0.308;
    c["muzzleVelocity"] = 790;
    const auto cartridge = Ok("saveCartridge", {{"form", c}}).at("id").get<int>();
    json st = Ok("state");
    EXPECT_EQ(st.at("currentCartridgeId"), cartridge);
    EXPECT_GT(st.at("currentProfileId").get<int>(), 0);
    EXPECT_TRUE(st.at("cartridges")[0].at("matches").get<bool>());

    EXPECT_EQ(Ok("rifleForm", {{"id", rifle}}).at("name"), "Tikka");
    EXPECT_EQ(Ok("cartridgeForm", {{"id", cartridge}}).at("bulletName"), "SMK 175");
    c["name"] = "";
    EXPECT_EQ(Fails("saveCartridge", {{"form", c}}), "Enter a cartridge name.");

    // A second rifle with the same cartridge: its own pair.
    const int pair = st.at("currentProfileId");
    r["name"] = "Second";
    r["rifleId"] = 0;
    Ok("saveRifle", {{"form", r}});
    st = Ok("state");
    EXPECT_NE(st.at("currentProfileId"), pair);
    EXPECT_EQ(st.at("currentCartridgeId"), cartridge);
    EXPECT_EQ(Ok("select", {{"rifleId", rifle}}).at("currentProfileId"), pair);

    st = Ok("deleteCartridge", {{"id", cartridge}});
    EXPECT_TRUE(st.at("cartridges").empty());
    EXPECT_EQ(st.at("currentProfileId"), 0);
}

TEST_F(Bridge, ZeroOffsetAndShotLogTruing) {
    Sample();
    Ok("setConditions", {{"targetRangeM", 100}});
    const double before = Ok("solution").at("elevation").get<double>();
    EXPECT_EQ(Ok("setZeroOffset", {{"upCm", 3}, {"rightCm", 0}}).at("offsetUpCm"), 3.0);
    EXPECT_NEAR(Ok("solution").at("elevation").get<double>(), before - 0.3, 0.02);
    Ok("setZeroOffset", {{"upCm", 0}, {"rightCm", 0}});

    for (double range : {500.0, 900.0}) {
        Ok("setConditions", {{"targetRangeM", range}});
        const double e = Ok("solution").at("elevation").get<double>();
        Ok("logShot", {{"rangeM", range}, {"elevation", e * 1.04}, {"notes", "test"}});
    }
    const json shots = Ok("shots");
    ASSERT_EQ(shots.size(), 2U);
    EXPECT_EQ(shots[0].at("notes"), "test");
    const json t = Ok("computeTruing");
    ASSERT_TRUE(t.at("ok").get<bool>()) << t.dump();
    EXPECT_LT(t.at("rmsAfter").get<double>(), t.at("rmsBefore").get<double>() / 3);
    Ok("applyTruing");
    EXPECT_LT(Ok("solution").at("velocityScale").get<double>(), 1.0);
    Ok("resetTruing");
    EXPECT_EQ(Ok("solution").at("velocityScale"), 1.0);
    Ok("setShotUsed", {{"id", shots[0].at("id")}, {"used", false}});
    EXPECT_FALSE(Ok("shots")[0].at("used").get<bool>());
    Ok("deleteShot", {{"id", shots[0].at("id")}});
    EXPECT_EQ(Ok("shots").size(), 1U);
}

TEST_F(Bridge, ShareRifleAndCartridge) {
    const json st = Sample();
    const int rifle = st.at("currentRifleId");
    const json e = Ok("exportJson", {{"kind", "rifle"}, {"id", rifle}});
    EXPECT_EQ(e.at("fileName"), "Rifle.balcalc.json");
    EXPECT_NE(e.at("json").get<std::string>().find("balcalc-rifle"), std::string::npos);
    const json after = Ok("importShared", {{"text", e.at("json")}});
    EXPECT_EQ(after.at("rifles").size(), 2U);
    EXPECT_NE(after.at("currentRifleId"), rifle); // the imported one is current
    const json c = Ok("exportJson", {{"kind", "cartridge"}, {"id", st.at("currentCartridgeId")}});
    EXPECT_EQ(Ok("importShared", {{"text", c.at("json")}}).at("cartridges").size(), 2U);
    EXPECT_FALSE(Fails("importShared", {{"text", "nonsense"}}).empty());
    Fails("exportJson", {{"kind", "scope"}, {"id", 1}});
}

TEST_F(Bridge, BulletLibraryAndImports) {
    json b = Ok("bulletForm", {{"id", 0}});
    b["name"] = "Test 175";
    b["massGr"] = 175;
    b["diameterIn"] = 0.308;
    b["bands"] = json::array({{{"velocity", 869}, {"bc", 0.505}}, {{"velocity", 701}, {"bc", 0.496}}});
    const auto id = Ok("saveBullet", {{"form", b}}).at("id").get<int>();
    const json list = Ok("libraryBullets", {{"filter", "test"}});
    ASSERT_EQ(list.size(), 1U);
    EXPECT_EQ(list[0].at("bcBands"), 2);
    EXPECT_EQ(Ok("bulletForm", {{"id", id}}).at("bands").size(), 2U);

    const json c = Ok("cartridgeFormWithBullet", {{"form", Ok("cartridgeForm", {{"id", 0}})},
                                                  {"bulletId", id}});
    EXPECT_EQ(c.at("libraryBulletId"), id);
    EXPECT_NEAR(c.at("massGr").get<double>(), 175.0, 1e-9);
    Ok("deleteBullet", {{"id", id}});
    EXPECT_TRUE(Ok("libraryBullets", {{"filter", "test"}}).empty());

    const json r = Ok("importFiles", {{"files", {{{"name", "notes.txt"}, {"content", "x"}}}}});
    EXPECT_EQ(r.at("imported"), 0);
    ASSERT_EQ(r.at("problems").size(), 1U);
    EXPECT_EQ(r.at("problems")[0].at("file"), "notes.txt");
    EXPECT_TRUE(Ok("reticles").empty());
}

TEST_F(Bridge, StarterLibraryIsSeededOnce) {
    json files = json::array();
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(std::string(BALLISTICS_SEED_DIR))) {
        const std::string name = entry.path().filename().string();
        if (!entry.is_regular_file() || name == "README.md" || name.find(".py") != std::string::npos ||
            name.rfind("LICENSE", 0) == 0) {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        files.push_back({{"name", name}, {"content", text.str()}});
    }
    const json r = Ok("seed", {{"version", 1}, {"files", files}});
    EXPECT_EQ(r.at("imported"), 166) << r.dump(); // as the Qt app reports
    EXPECT_EQ(Ok("libraryBullets").size(), 162U);
    EXPECT_EQ(Ok("libraryCartridges").size(), 69U);
    EXPECT_EQ(Ok("reticles").size(), 4U);
    EXPECT_TRUE(Ok("state").at("cartridges").empty()); // factory loads stay in the library
    EXPECT_EQ(Ok("seed", {{"version", 1}, {"files", files}}).at("imported"), 0);

    const json lib = Ok("libraryCartridges", {{"filter", "GP11"}});
    ASSERT_FALSE(lib.empty());
    json copy = Ok("cartridgeFormFromLibrary", {{"id", lib[0].at("id")}});
    EXPECT_EQ(copy.at("cartridgeId"), 0);
    EXPECT_GT(copy.at("muzzleVelocity").get<double>(), 0.0);
    Ok("saveCartridge", {{"form", copy}});
    EXPECT_EQ(Ok("state").at("cartridges").size(), 1U);
}

TEST(BridgeFile, SessionSelectionAndSettingsPersist) {
    const auto path = (std::filesystem::temp_directory_path() / "balcalc_bridge.db").string();
    std::filesystem::remove(path);
    int rifle = 0;
    {
        Api api;
        ASSERT_NE(api.Call("open", json{{"path", path}}.dump()).find("\"ok\":true"), std::string::npos);
        const json st = json::parse(api.Call("addSample", "{}")).at("result");
        rifle = st.at("currentRifleId");
        api.Call("setConditions", R"({"targetRangeM": 650, "powderFollowsAir": false, "powderC": 5})");
        api.Call("setSettings", R"({"angleUnit": "moa", "holdMode": "dial", "language": "uk"})");
    }
    {
        Api api;
        api.Call("open", json{{"path", path}}.dump());
        const json st = json::parse(api.Call("state", "")).at("result");
        EXPECT_EQ(st.at("currentRifleId"), rifle);
        EXPECT_GT(st.at("currentProfileId").get<int>(), 0);
        EXPECT_EQ(st.at("angleUnit"), "moa");
        EXPECT_EQ(st.at("holdMode"), "dial");
        EXPECT_EQ(st.at("language"), "uk");
        EXPECT_EQ(st.at("conditions").at("targetRangeM"), 650.0);
        EXPECT_FALSE(st.at("conditions").at("powderFollowsAir").get<bool>());
        EXPECT_EQ(st.at("conditions").at("powderC"), 5.0);
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST(BridgeErrors, CallsFailCleanly) {
    Api api;
    const auto error = [&api](const std::string& method, const std::string& args) {
        const json r = json::parse(api.Call(method, args));
        EXPECT_FALSE(r.at("ok").get<bool>()) << method;
        return r.at("error").get<std::string>();
    };
    EXPECT_EQ(error("state", ""), "The database is not open.");
    EXPECT_NE(json::parse(api.Call("info", "")).at("result").at("engineVersion"), "");
    EXPECT_EQ(error("nope", ""), "Unknown method: nope");
    EXPECT_NE(error("open", "{not json").find("Bad arguments"), std::string::npos);
    EXPECT_EQ(error("open", "[1]"), "Arguments must be a JSON object.");
    ASSERT_TRUE(json::parse(api.Call("open", R"({"path": ":memory:"})")).at("ok").get<bool>());
    EXPECT_EQ(error("open", R"({"path": ":memory:"})"), "The database is already open.");
    EXPECT_NEAR(json::parse(api.Call("stationPressure", R"({"qnhHpa": 1013.25, "altitudeM": 500})"))
                    .at("result")
                    .get<double>(),
                954.6, 0.5);
}

} // namespace
} // namespace ballistics::bridge
