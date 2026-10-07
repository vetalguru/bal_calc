// The JSON facade the Kotlin app talks to: every method, through Call()
// only, as the app sees it.
#include <ballistics/bridge/api.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace ballistics::bridge {
namespace {

using nlohmann::json;

// Standard base64, as the app sends pictures.
std::string TestBase64(const std::vector<std::uint8_t>& d) {
    static const char* a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < d.size(); i += 3) {
        const unsigned n = (d[i] << 16) | (i + 1 < d.size() ? d[i + 1] << 8 : 0) | (i + 2 < d.size() ? d[i + 2] : 0);
        out += a[(n >> 18) & 63];
        out += a[(n >> 12) & 63];
        out += i + 1 < d.size() ? a[(n >> 6) & 63] : '=';
        out += i + 2 < d.size() ? a[n & 63] : '=';
    }
    return out;
}

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

TEST_F(Bridge, WindZonesAndGust) {
    Sample();
    Ok("setConditions", {{"targetRangeM", 800}, {"windSpeed", 0}, {"windFromDeg", 270}});
    const double still = Ok("solution").at("windage").get<double>(); // spin drift only
    EXPECT_FALSE(Ok("solution").at("hasGust").get<bool>());

    // Calm here, wind from the right further out, more of it at the end.
    const json zones = json::array({{{"speedMps", 4}, {"fromDeg", 90}, {"untilM", 600}},
                                    {{"speedMps", 6}, {"fromDeg", 90}, {"untilM", 0}},
                                    {{"speedMps", 9}, {"fromDeg", 90}, {"untilM", 0}}});
    const json c = Ok("setConditions", {{"windUntilM", 300}, {"windZones", zones}});
    ASSERT_EQ(c.at("windZones").size(), 2u); // three zones in all
    EXPECT_EQ(c.at("windUntilM"), 300.0);
    EXPECT_EQ(c.at("windFromDeg"), 270.0); // a calm zone keeps its direction
    const double zoned = Ok("solution").at("windage").get<double>();
    EXPECT_GT(zoned, still + 0.1);

    // A gust in the first zone (from the left): the bracket goes the other way.
    Ok("setConditions", {{"windGustMps", 8}});
    const json sol = Ok("solution");
    EXPECT_TRUE(sol.at("hasGust").get<bool>());
    EXPECT_LT(sol.at("gustWindage").get<double>(), zoned);

    // Back to one wind.
    EXPECT_TRUE(Ok("setConditions", {{"windZones", json::array()}}).at("windZones").empty());
}

TEST_F(Bridge, MovingTarget) {
    Sample();
    EXPECT_EQ(Ok("state").at("conditions").at("targetSpeedUnit"), "kmh");
    EXPECT_FALSE(Ok("solution").at("hasLead").get<bool>());
    Ok("setConditions", {{"targetSpeedMps", 5}, {"targetHeadingDeg", 270}, {"targetSpeedUnit", "mps"}});
    const json sol = Ok("solution");
    ASSERT_TRUE(sol.at("hasLead").get<bool>());
    EXPECT_LT(sol.at("lead").get<double>(), -1.0); // to the left, ~5 mrad at 300 m
    EXPECT_NEAR(sol.at("leadCm").get<double>(), -500.0 * sol.at("time").get<double>(), 1e-6);
    const json table = Ok("rangeTable");
    EXPECT_LT(table.at("rows").back().at("lead").get<double>(), sol.at("lead").get<double>());
    EXPECT_EQ(Ok("state").at("conditions").at("targetSpeedUnit"), "mps");
}

TEST_F(Bridge, CompareCurves) {
    Sample();
    const json st = Ok("state");
    const auto rifle = st.at("currentRifleId").get<int>();
    const auto first = st.at("currentCartridgeId").get<int>();
    json form = Ok("cartridgeForm", {{"id", first}});
    form["cartridgeId"] = 0;
    form["name"] = "Hot load";
    form["muzzleVelocity"] = 850.0;
    const auto hot = Ok("saveCartridge", {{"form", form}}).at("id").get<int>();
    const auto chosen = Ok("state").at("currentCartridgeId").get<int>(); // saving selects it

    const json options = Ok("pairOptions");
    ASSERT_EQ(options.size(), 1u);
    EXPECT_EQ(options[0].at("cartridges").size(), 2u);

    const json curves = Ok("compareCurves", {{"maxRangeM", 1000}, {"points", 10},
        {"pairs", json::array({{{"rifleId", rifle}, {"cartridgeId", first}},
                               {{"rifleId", rifle}, {"cartridgeId", hot}},
                               {{"rifleId", 999}, {"cartridgeId", hot}}})}});
    ASSERT_EQ(curves.size(), 3u);
    ASSERT_TRUE(curves[0].at("ok").get<bool>());
    ASSERT_TRUE(curves[1].at("ok").get<bool>());
    EXPECT_EQ(curves[1].at("label"), "Rifle · Hot load");
    EXPECT_EQ(curves[0].at("rows").size(), 11u);
    const json& slow = curves[0].at("rows").back();
    const json& fast = curves[1].at("rows").back();
    EXPECT_GT(fast.at("velocity").get<double>(), slow.at("velocity").get<double>() + 10.0);
    EXPECT_GT(fast.at("dropCm").get<double>(), slow.at("dropCm").get<double>()); // less drop
    EXPECT_FALSE(curves[2].at("ok").get<bool>());
    EXPECT_FALSE(curves[2].at("error").get<std::string>().empty());
    // The current choice is untouched.
    EXPECT_EQ(Ok("state").at("currentCartridgeId"), chosen);
}

TEST_F(Bridge, DsfTable) {
    Sample();
    EXPECT_TRUE(Ok("solution").at("dsf").empty());
    EXPECT_FALSE(Ok("computeDsf").at("ok").get<bool>()); // no shots yet
    EXPECT_EQ(Fails("applyDsf"), "Nothing to apply.");

    Ok("setConditions", {{"targetRangeM", 1300}});
    const double before = Ok("solution").at("elevation").get<double>();
    Ok("setDsf", {{"points", json::array({{{"mach", 1.4}, {"factor", 1.0}}, {{"mach", 0.9}, {"factor", 1.15}}})}});
    const json sol = Ok("solution");
    ASSERT_EQ(sol.at("dsf").size(), 2u);
    EXPECT_DOUBLE_EQ(sol.at("dsf")[0].at("mach").get<double>(), 0.9);
    EXPECT_GT(sol.at("elevation").get<double>(), before + 0.05);
    EXPECT_EQ(Fails("setDsf", {{"points", json::array({{{"mach", 1.0}, {"factor", 3.0}}})}}),
              "Each DSF point needs a Mach between 0 and 5 and a factor between 0.5 and 2.");

    // Shots in the transonic part give a table to apply.
    Ok("resetDsf");
    EXPECT_TRUE(Ok("solution").at("dsf").empty());
    for (int range : {900, 1100, 1300}) {
        Ok("setConditions", {{"targetRangeM", range}});
        const double predicted = Ok("solution").at("elevation").get<double>();
        const double more = range == 900 ? 1.0 : 1.03; // transonic: 3 % more drop
        Ok("logShot", {{"rangeM", range}, {"elevation", predicted * more}});
    }
    const json fit = Ok("computeDsf");
    ASSERT_TRUE(fit.at("ok").get<bool>()) << fit.dump();
    EXPECT_EQ(fit.at("shots").size(), 3u);
    Ok("applyDsf");
    EXPECT_EQ(Ok("solution").at("dsf").size(), fit.at("points").size());
}

TEST_F(Bridge, BcCalculator) {
    EXPECT_EQ(Fails("bcCalculator", {{"mode", "hit"}, {"rangeM", 500}, {"elevation", 3}}),
              "Choose a rifle and a cartridge.");
    Sample();
    json r = Ok("bcCalculator", {{"mode", "chronograph"}, {"table", "G7"}, {"vNearMps", 790},
                                 {"vFarMps", 760}, {"distanceM", 100}});
    ASSERT_TRUE(r.at("ok").get<bool>()) << r.dump();
    EXPECT_GT(r.at("bc").get<double>(), 0.4); // 30 m/s over 100 m: a long, sleek bullet
    EXPECT_LT(r.at("bc").get<double>(), 0.6);

    // The sample's own correction at 800 m gives back its BC (G7 0.243).
    Ok("setConditions", {{"targetRangeM", 800}});
    const double hit = Ok("solution").at("elevation").get<double>();
    r = Ok("bcCalculator", {{"mode", "hit"}, {"table", "G7"}, {"rangeM", 800}, {"elevation", hit}});
    ASSERT_TRUE(r.at("ok").get<bool>()) << r.dump();
    EXPECT_NEAR(r.at("bc").get<double>(), 0.243, 0.002); // the shown correction is rounded

    r = Ok("bcCalculator", {{"table", "G7"}, {"vNearMps", 700}, {"vFarMps", 760}, {"distanceM", 100}});
    EXPECT_FALSE(r.at("ok").get<bool>());
    EXPECT_FALSE(r.at("error").get<std::string>().empty());
}

TEST_F(Bridge, HitProbability) {
    json r = Ok("wez");
    EXPECT_FALSE(r.at("ok").get<bool>()); // no rifle yet, settings still there
    EXPECT_EQ(r.at("settings").at("targetKind"), "rectangle");
    Sample();
    r = Ok("wez", {{"toM", 1000}, {"stepM", 100}, {"settings", {{"targetKind", "ellipse"}, {"groupMoa", 0.5}}}});
    ASSERT_TRUE(r.at("ok").get<bool>()) << r.dump();
    EXPECT_EQ(r.at("rows").size(), 10u);
    EXPECT_EQ(r.at("settings").at("targetKind"), "ellipse");
    EXPECT_EQ(r.at("settings").at("groupMoa"), 0.5);
    EXPECT_GT(r.at("atTarget").at("probability").get<double>(), 0.5); // 300 m, a 50 cm ellipse
    EXPECT_GE(r.at("shots95").get<int>(), r.at("shots50").get<int>());
    EXPECT_FALSE(r.at("parts").empty());
    // Kept.
    EXPECT_EQ(Ok("wez").at("settings").at("groupMoa"), 0.5);
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

TEST_F(Bridge, SituationsRestoreTheRifleCartridgeAndConditions) {
    const json st = Sample();
    const int cartridge = st.at("currentCartridgeId");
    EXPECT_TRUE(Ok("situations").empty());
    EXPECT_EQ(Fails("saveSituation", {{"name", "  "}}), "Enter a name for the situation.");

    Ok("setConditions", {{"targetRangeM", 650}, {"windSpeed", 4}, {"temperatureC", -5}});
    Ok("setSettings", {{"holdMode", "dial"}});
    json list = Ok("saveSituation", {{"name", " Winter match "}});
    ASSERT_EQ(list.size(), 1U);
    EXPECT_EQ(list[0].at("name"), "Winter match");
    EXPECT_EQ(list[0].at("rifleName"), "Rifle");
    EXPECT_EQ(list[0].at("cartridgeName"), "Load");
    EXPECT_EQ(list[0].at("rangeM"), 650.0);
    EXPECT_TRUE(list[0].at("available").get<bool>());

    // The same name again replaces it.
    Ok("setConditions", {{"targetRangeM", 700}});
    EXPECT_EQ(Ok("saveSituation", {{"name", "Winter match"}}).size(), 1U);
    EXPECT_EQ(Ok("situations")[0].at("rangeM"), 700.0);

    Ok("setConditions", {{"targetRangeM", 300}, {"windSpeed", 0}, {"temperatureC", 25}});
    Ok("setSettings", {{"holdMode", "hold"}});
    const json applied = Ok("applySituation", {{"name", "Winter match"}});
    EXPECT_EQ(applied.at("conditions").at("targetRangeM"), 700.0);
    EXPECT_EQ(applied.at("conditions").at("windSpeed"), 4.0);
    EXPECT_EQ(applied.at("conditions").at("temperatureC"), -5.0);
    EXPECT_EQ(applied.at("holdMode"), "dial");
    EXPECT_EQ(applied.at("currentCartridgeId"), cartridge);
    EXPECT_EQ(Fails("applySituation", {{"name", "Summer"}}), "No situation of this name.");

    Ok("deleteCartridge", {{"id", cartridge}});
    EXPECT_FALSE(Ok("situations")[0].at("available").get<bool>());
    EXPECT_EQ(Fails("applySituation", {{"name", "Winter match"}}),
              "The rifle or cartridge of this situation was deleted.");
    EXPECT_TRUE(Ok("deleteSituation", {{"name", "Winter match"}}).empty());
}

TEST_F(Bridge, PhotosGoWithTheirRecords) {
    const json st = Sample();
    const int rifle = st.at("currentRifleId");
    const int cartridge = st.at("currentCartridgeId");
    // Every byte value, and lengths that need padding.
    std::string bytes;
    for (int i = 0; i < 256; ++i) {
        bytes += static_cast<char>(i);
    }
    for (const std::string& data : {bytes, std::string("a"), std::string("ab")}) {
        std::vector<std::uint8_t> v(data.begin(), data.end());
        Ok("setPhoto", {{"kind", "rifle"}, {"id", rifle}, {"image", TestBase64(v)}});
        EXPECT_EQ(Ok("photo", {{"kind", "rifle"}, {"id", rifle}}), TestBase64(v));
    }
    EXPECT_EQ(Ok("photo", {{"kind", "cartridge"}, {"id", cartridge}}), "");
    Ok("setPhoto", {{"kind", "cartridge"}, {"id", cartridge}, {"image", "AAEC"}});
    const json all = Ok("photos", {{"kind", "cartridge"}});
    ASSERT_EQ(all.size(), 1U);
    EXPECT_EQ(all.at(std::to_string(cartridge)), "AAEC");

    EXPECT_EQ(Fails("setPhoto", {{"kind", "rifle"}, {"id", 999}, {"image", "AAEC"}}), "Save the record first.");
    EXPECT_EQ(Fails("setPhoto", {{"kind", "bullet"}, {"id", 1}, {"image", "AAEC"}}), "Unknown kind of record.");
    EXPECT_EQ(Fails("setPhoto", {{"kind", "rifle"}, {"id", rifle}, {"image", "*"}}), "The picture is damaged.");

    Ok("setPhoto", {{"kind", "rifle"}, {"id", rifle}, {"image", ""}}); // removes
    EXPECT_TRUE(Ok("photos", {{"kind", "rifle"}}).empty());
    Ok("deleteCartridge", {{"id", cartridge}});
    EXPECT_TRUE(Ok("photos", {{"kind", "cartridge"}}).empty());
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

// An app updated with a bigger starter library (a higher seed version) gets
// the new records once, keeping what it has.
TEST_F(Bridge, NewerSeedAddsOnlyWhatIsNew) {
    json old_files = json::array();
    json files = json::array();
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(std::string(BALLISTICS_SEED_DIR))) {
        const std::string name = entry.path().filename().string();
        if (!entry.is_regular_file() || entry.path().parent_path().filename() == "sources" ||
            (entry.path().extension() != ".ammo" && entry.path().extension() != ".drg" &&
             entry.path().extension() != ".reticle" && entry.path().extension() != ".json")) {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        const json f = {{"name", name}, {"content", text.str()}};
        files.push_back(f);
        // What version 1 shipped: no published bullets, no generic reticles.
        if (name != "published_bullets.json" && name.rfind("generic-", 0) != 0) {
            old_files.push_back(f);
        }
    }
    EXPECT_EQ(Ok("seed", {{"version", 1}, {"files", old_files}}).at("imported"), 69 + 1 + 4);
    EXPECT_EQ(Ok("seed", {{"version", 2}, {"files", files}}).at("imported"), 10 + 253);
    EXPECT_EQ(Ok("seed", {{"version", 2}, {"files", files}}).at("imported"), 0);
    EXPECT_EQ(Ok("reticles").size(), 14U);
}

TEST_F(Bridge, StarterLibraryIsSeededOnce) {
    json files = json::array();
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(std::string(BALLISTICS_SEED_DIR))) {
        const std::string name = entry.path().filename().string();
        if (entry.path().parent_path().filename() == "sources") {
            continue; // collection inputs, not shipped
        }
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
    EXPECT_EQ(r.at("imported"), 337) << r.dump(); // 69 ammo + 1 drg + 14 reticles + 253 bullets
    EXPECT_EQ(Ok("libraryBullets").size(), 323U);
    EXPECT_EQ(Ok("libraryCartridges").size(), 69U);
    EXPECT_EQ(Ok("reticles").size(), 14U);
    EXPECT_EQ(Ok("libraryScopes").size(), 50U);
    EXPECT_EQ(Ok("libraryRifles").size(), 126U);
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

// The bundled scope and rifle catalogs: what the makers publish, in range.
TEST_F(Bridge, PublishedScopesAndRiflesAreSane) {
    json files = json::array();
    for (const char* name : {"published_scopes.json", "published_rifles.json"}) {
        std::ifstream in(std::filesystem::path(BALLISTICS_SEED_DIR) / name, std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        files.push_back({{"name", name}, {"content", text.str()}});
    }
    EXPECT_EQ(Ok("seed", {{"version", 1}, {"files", files}}).at("imported"), 0); // not records

    const json scopes = Ok("libraryScopes");
    ASSERT_GE(scopes.size(), 50U);
    std::set<std::string> names;
    for (const json& s : scopes) {
        const std::string name = s.at("maker").get<std::string>() + " " + s.at("model").get<std::string>();
        SCOPED_TRACE(name);
        EXPECT_TRUE(names.insert(name).second) << "duplicate";
        EXPECT_EQ(s.at("source").get<std::string>().rfind("https://", 0), 0U);
        const double lo = s.at("minMagnification"), hi = s.at("maxMagnification");
        EXPECT_GE(lo, 1.0);
        EXPECT_GT(hi, lo - 1e-9);
        EXPECT_LE(hi, 80.0);
        const std::string plane = s.at("focalPlane");
        EXPECT_TRUE(plane == "ffp" || plane == "sfp");
        if (s.contains("sfpReferenceMagnification")) {
            EXPECT_EQ(plane, "sfp");
            EXPECT_GE(s.at("sfpReferenceMagnification").get<double>(), lo);
            EXPECT_LE(s.at("sfpReferenceMagnification").get<double>(), hi);
        }
        ASSERT_FALSE(s.at("clicks").empty());
        for (const json& c : s.at("clicks")) {
            const double v = c.at("value");
            if (c.at("units") == "mrad") {
                EXPECT_TRUE(v >= 0.025 && v <= 0.25) << v;
            } else {
                EXPECT_EQ(c.at("units"), "moa");
                EXPECT_TRUE(v >= 0.1 && v <= 1.0) << v;
            }
        }
    }

    const json rifles = Ok("libraryRifles");
    ASSERT_GE(rifles.size(), 100U);
    std::set<std::string> keys;
    for (const json& r : rifles) {
        const std::string key = r.at("maker").get<std::string>() + " " + r.at("model").get<std::string>() +
                                " " + r.at("caliber").get<std::string>() + " " + r.at("twistIn").dump();
        SCOPED_TRACE(key);
        EXPECT_TRUE(keys.insert(key).second) << "duplicate";
        EXPECT_EQ(r.at("source").get<std::string>().rfind("https://", 0), 0U);
        EXPECT_FALSE(r.at("caliber").get<std::string>().empty());
        EXPECT_GE(r.at("twistIn").get<double>(), 6.5);
        EXPECT_LE(r.at("twistIn").get<double>(), 24.0);
        ASSERT_FALSE(r.at("barrelsIn").empty());
        for (const json& b : r.at("barrelsIn")) {
            EXPECT_GE(b.get<double>(), 16.0);
            EXPECT_LE(b.get<double>(), 30.0);
        }
    }

    // Every word of the filter, in any order and case.
    const json atacr = Ok("libraryScopes", {{"filter", "f1 ATACR"}});
    EXPECT_EQ(atacr.size(), 5U);
    for (const json& s : atacr) {
        EXPECT_EQ(s.at("focalPlane"), "ffp");
    }
    const json tikka = Ok("libraryRifles", {{"filter", "tikka ctr 6.5 creedmoor"}});
    ASSERT_EQ(tikka.size(), 1U);
    EXPECT_EQ(tikka[0].at("twistIn"), 8.0);
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
