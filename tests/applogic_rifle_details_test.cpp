// A rifle's fuller description (schema v6): maker and model, rifling, muzzle
// device, rail slope, rings, the scope's maker, model, tube, objective and
// turret travel; the separate horizontal click. What it changes: a solution
// beyond the turret's reach warns.
#include <ballistics/applogic/armory.h>
#include <ballistics/applogic/profile_io.h>
#include <ballistics/applogic/session.h>
#include <ballistics/storage/database.h>
#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>
#include <ballistics/units.h>
#include <gtest/gtest.h>
#include <sqlite_manager/connection.h>

#include <cmath>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

#include "../storage/src/schema.h"

namespace ballistics::applogic {
namespace {

using storage::RifleRecord;
using storage::ScopeRecord;

RifleForm Described() {
    RifleForm f;
    f.name = "Long range";
    f.caliber = ".308 Win";
    f.sight_height_cm = 4.5;
    f.twist_in = 10.0;
    f.maker = "Tikka";
    f.model = "T3x TAC A1";
    f.rifling_grooves = 4;
    f.rifling_type = "conventional";
    f.muzzle_device = "brake";
    f.rail_slope = 20.0;
    f.rail_slope_units = kClickMoa;
    f.ring_height_mm = 30.0;
    f.click_units = kClickMrad;
    f.click_value = 0.1;
    f.click_value_horizontal = 0.05;
    f.elevation_travel = 29.0;
    f.windage_travel = 14.5;
    f.scope_maker = "Vortex";
    f.scope_model = "Razor HD Gen III 6-36x56";
    f.tube_diameter_mm = 34.0;
    f.objective_mm = 56.0;
    return f;
}

CartridgeForm Cartridge() {
    CartridgeForm c;
    c.name = "SMK 175";
    c.bullet_name = "SMK 175";
    c.drag_table = "G7";
    c.bc = 0.243;
    c.mass_gr = 175.0;
    c.diameter_in = 0.308;
    c.length_in = 1.24;
    c.muzzle_velocity_mps = 800.0;
    return c;
}

bool Mentions(const std::string& problem, const std::string& words) {
    return problem.find(words) != std::string::npos;
}

class RifleDetails : public ::testing::Test {
   protected:
    void SetUp() override { ASSERT_TRUE(db_.Open(":memory:").ok()); }
    storage::Database db_;
};

TEST_F(RifleDetails, DescriptionRoundTrips) {
    const Id id = SaveRifleForm(db_, Described()).value();
    const RifleForm g = LoadRifleForm(db_, id).value();
    EXPECT_EQ(g.maker, "Tikka");
    EXPECT_EQ(g.model, "T3x TAC A1");
    EXPECT_EQ(g.rifling_grooves, 4);
    EXPECT_EQ(g.rifling_type, "conventional");
    EXPECT_EQ(g.muzzle_device, "brake");
    EXPECT_DOUBLE_EQ(g.rail_slope, 20.0);  // not 19.9999999 after the radians
    EXPECT_EQ(g.rail_slope_units, kClickMoa);
    EXPECT_NEAR(g.ring_height_mm, 30.0, 1e-9);
    EXPECT_NEAR(g.click_value, 0.1, 1e-12);
    EXPECT_DOUBLE_EQ(g.click_value_horizontal, 0.05);
    EXPECT_DOUBLE_EQ(g.elevation_travel, 29.0);
    EXPECT_DOUBLE_EQ(g.windage_travel, 14.5);
    EXPECT_EQ(g.scope_maker, "Vortex");
    EXPECT_EQ(g.scope_model, "Razor HD Gen III 6-36x56");
    EXPECT_NEAR(g.tube_diameter_mm, 34.0, 1e-9);
    EXPECT_NEAR(g.objective_mm, 56.0, 1e-9);

    // Stored in SI: the rail in radians, the clicks per axis.
    const RifleRecord r = *storage::Repository<RifleRecord>(db_).Get(id).value();
    EXPECT_NEAR(r.rail_slope_rad, units::MoaToRad(20.0), 1e-12);
    const ScopeRecord s = *storage::Repository<ScopeRecord>(db_).Get(*r.scope_id).value();
    EXPECT_NEAR(s.click_vertical_rad, 0.0001, 1e-12);
    EXPECT_NEAR(s.click_horizontal_rad, 0.00005, 1e-12);
    EXPECT_NEAR(s.elevation_travel_rad, 0.029, 1e-12);
}

TEST_F(RifleDetails, HorizontalClickFollowsTheVerticalUnlessSet) {
    RifleForm f = Described();
    f.click_value_horizontal = 0.0;
    const Id id = SaveRifleForm(db_, f).value();
    RifleForm g = LoadRifleForm(db_, id).value();
    EXPECT_DOUBLE_EQ(g.click_value_horizontal, 0.0);  // still "as the vertical one"
    const RifleRecord r = *storage::Repository<RifleRecord>(db_).Get(id).value();
    const ScopeRecord s = *storage::Repository<ScopeRecord>(db_).Get(*r.scope_id).value();
    EXPECT_DOUBLE_EQ(s.click_horizontal_rad, s.click_vertical_rad);

    // A rail in MRAD keeps its unit.
    g.rail_slope = 6.0;
    g.rail_slope_units = kClickMrad;
    ASSERT_TRUE(SaveRifleForm(db_, g).ok());
    const RifleForm again = LoadRifleForm(db_, id).value();
    EXPECT_EQ(again.rail_slope_units, kClickMrad);
    EXPECT_DOUBLE_EQ(again.rail_slope, 6.0);
}

TEST(RifleDetailsForm, EachProblemIsNamed) {
    EXPECT_TRUE(Validate(Described()).empty());
    RifleForm r = Described();
    r.click_value_horizontal = -0.1;
    EXPECT_TRUE(Mentions(Validate(r), "horizontal click"));

    r = Described();
    r.rifling_type = "spiral";
    EXPECT_TRUE(Mentions(Validate(r), "rifling"));
    r = Described();
    r.muzzle_device = "bayonet";
    EXPECT_TRUE(Mentions(Validate(r), "muzzle device"));
    r = Described();
    r.rifling_grooves = 17;
    EXPECT_TRUE(Mentions(Validate(r), "rifling"));
    r.rifling_grooves = -1;
    EXPECT_TRUE(Mentions(Validate(r), "rifling"));

    r = Described();
    r.rail_slope = 60.0;  // the steepest made
    EXPECT_TRUE(Validate(r).empty());
    r.rail_slope = 61.0;
    EXPECT_TRUE(Mentions(Validate(r), "Rail slope"));
    r.rail_slope = -1.0;
    EXPECT_TRUE(Mentions(Validate(r), "Rail slope"));
    r = Described();
    r.rail_slope_units = kClickSmoa;
    EXPECT_TRUE(Mentions(Validate(r), "Rail slope"));

    r = Described();
    r.elevation_travel = 101.0;  // MRAD
    EXPECT_TRUE(Mentions(Validate(r), "travel"));
    r.elevation_travel = -1.0;
    EXPECT_TRUE(Mentions(Validate(r), "travel"));

    r = Described();
    r.tube_diameter_mm = 51.0;
    EXPECT_TRUE(Mentions(Validate(r), "tube"));
    r = Described();
    r.ring_height_mm = -1.0;
    EXPECT_TRUE(Mentions(Validate(r), "rings"));
}

TEST_F(RifleDetails, ShareFileCarriesTheDescription) {
    const Id id = SaveRifleForm(db_, Described()).value();
    const std::string json = ExportRifleJson(db_, id).value();
    const Imported got = ImportShareJson(db_, json).value();
    ASSERT_NE(got.rifle_id, 0);
    RifleForm g = LoadRifleForm(db_, got.rifle_id).value();
    RifleForm want = Described();
    EXPECT_EQ(g.name, "Long range (2)");
    EXPECT_EQ(g.maker, want.maker);
    EXPECT_EQ(g.rifling_type, want.rifling_type);
    EXPECT_EQ(g.muzzle_device, want.muzzle_device);
    EXPECT_DOUBLE_EQ(g.rail_slope, 20.0);
    EXPECT_DOUBLE_EQ(g.click_value_horizontal, 0.05);
    EXPECT_DOUBLE_EQ(g.elevation_travel, 29.0);
    EXPECT_EQ(g.scope_model, want.scope_model);
    EXPECT_NEAR(g.objective_mm, 56.0, 1e-9);
}

TEST_F(RifleDetails, OlderAndNewerShareFilesStillImport) {
    // A file from before v6, without the description: all "not stated".
    const Id id = SaveRifleForm(db_, Described()).value();
    nlohmann::json doc = nlohmann::json::parse(ExportRifleJson(db_, id).value());
    for (const char* k : {"maker", "model", "rifling_grooves", "rifling_type", "muzzle_device",
                          "rail_slope_rad", "rail_slope_units", "ring_height_m"}) {
        doc["rifle"].erase(k);
    }
    for (const char* k : {"maker", "model", "tube_diameter_m", "objective_m",
                          "elevation_travel_rad", "windage_travel_rad"}) {
        doc["scope"].erase(k);
    }
    RifleForm old = LoadRifleForm(db_, ImportShareJson(db_, doc.dump()).value().rifle_id).value();
    EXPECT_TRUE(old.maker.empty());
    EXPECT_TRUE(old.rifling_type.empty());
    EXPECT_DOUBLE_EQ(old.rail_slope, 0.0);
    EXPECT_EQ(old.rail_slope_units, kClickMoa);
    EXPECT_DOUBLE_EQ(old.elevation_travel, 0.0);

    // Values a later version may add, or a damaged file: not stated, not refused.
    doc["rifle"]["rifling_type"] = "spiral";
    doc["rifle"]["muzzle_device"] = "suppressor";  // a suppressor is its own part now
    doc["rifle"]["rifling_grooves"] = -3;
    doc["rifle"]["rail_slope_units"] = "degrees";
    doc["rifle"]["ring_height_m"] = -0.01;
    doc["scope"]["elevation_travel_rad"] = -1.0;
    const auto odd = ImportShareJson(db_, doc.dump());
    ASSERT_TRUE(odd.ok()) << odd.error().message;
    const RifleForm g = LoadRifleForm(db_, odd.value().rifle_id).value();
    EXPECT_TRUE(g.rifling_type.empty());
    EXPECT_TRUE(g.muzzle_device.empty());
    EXPECT_EQ(g.rifling_grooves, 0);
    EXPECT_EQ(g.rail_slope_units, kClickMoa);
    EXPECT_DOUBLE_EQ(g.ring_height_mm, 0.0);
    EXPECT_DOUBLE_EQ(g.elevation_travel, 0.0);
}

TEST(RifleDetailsMigration, VersionFiveRifleGetsAnEmptyDescription) {
    const auto path = (std::filesystem::temp_directory_path() / "balcalc_v5.db").string();
    std::filesystem::remove(path);
    {
        sqlite_manager::Connection c;
        ASSERT_TRUE(c.Open(path).ok());
        for (const storage::detail::Migration& m : storage::detail::Migrations()) {
            if (m.version <= 5) {
                ASSERT_TRUE(c.Execute(m.sql).ok()) << "v" << m.version;
            }
        }
        ASSERT_TRUE(c.Execute(R"sql(
PRAGMA user_version = 5;
INSERT INTO scope (id, name, click_units, click_vertical_rad, click_horizontal_rad)
    VALUES (1, 'Tikka', 'moa', 0.0000727, 0.0000727);
INSERT INTO rifle (id, name, caliber, twist_m, sight_height_m, scope_id)
    VALUES (1, 'Tikka', '.308 Win', 0.254, 0.045, 1);
)sql")
                        .ok());
        ASSERT_TRUE(c.Close().ok());
    }
    {
        storage::Database db;
        ASSERT_TRUE(db.Open(path).ok());
        EXPECT_EQ(db.SchemaVersion().value(), storage::Database::LatestSchemaVersion());
        const RifleForm f = LoadRifleForm(db, 1).value();
        EXPECT_EQ(f.name, "Tikka");
        EXPECT_EQ(f.click_units, kClickMoa);
        EXPECT_TRUE(f.maker.empty());
        EXPECT_TRUE(f.muzzle_device.empty());
        EXPECT_EQ(f.rifling_grooves, 0);
        EXPECT_DOUBLE_EQ(f.rail_slope, 0.0);
        EXPECT_EQ(f.rail_slope_units, kClickMoa);
        EXPECT_DOUBLE_EQ(f.click_value_horizontal, 0.0);
        EXPECT_DOUBLE_EQ(f.elevation_travel, 0.0);
        EXPECT_TRUE(f.scope_model.empty());
        // And it saves as a v6 rifle.
        RifleForm more = f;
        more.maker = "Tikka";
        ASSERT_TRUE(SaveRifleForm(db, more).ok());
        EXPECT_EQ(LoadRifleForm(db, 1).value().maker, "Tikka");
    }
    std::filesystem::remove(path);
}

TEST_F(RifleDetails, MuzzleDeviceShiftRoundTripsAndIsChecked) {
    RifleForm f = Described();
    f.muzzle_device = "brake";
    f.muzzle_poi_up = -1.5;  // MOA: down
    f.muzzle_poi_right = 0.25;
    f.muzzle_poi_units = kClickMoa;
    f.muzzle_device_on = false;
    f.zeroed_with_device = true;
    const Id id = SaveRifleForm(db_, f).value();
    const RifleForm g = LoadRifleForm(db_, id).value();
    EXPECT_DOUBLE_EQ(g.muzzle_poi_up, -1.5);
    EXPECT_DOUBLE_EQ(g.muzzle_poi_right, 0.25);
    EXPECT_EQ(g.muzzle_poi_units, kClickMoa);
    EXPECT_FALSE(g.muzzle_device_on);
    EXPECT_TRUE(g.zeroed_with_device);
    const RifleRecord r = *storage::Repository<RifleRecord>(db_).Get(id).value();
    EXPECT_NEAR(r.muzzle_poi_up_rad, -units::MoaToRad(1.5), 1e-12);

    // Through a share file too.
    const RifleForm shared =
        LoadRifleForm(db_, ImportShareJson(db_, ExportRifleJson(db_, id).value()).value().rifle_id)
            .value();
    EXPECT_DOUBLE_EQ(shared.muzzle_poi_up, -1.5);
    EXPECT_FALSE(shared.has_suppressor);
    EXPECT_FALSE(shared.muzzle_device_on);
    EXPECT_TRUE(shared.zeroed_with_device);

    RifleForm bad = f;
    bad.suppressor_poi_up = 35.0;  // MOA, over 10 MRAD
    EXPECT_TRUE(Mentions(Validate(bad), "shift must be within"));
    bad = f;
    bad.muzzle_poi_up = -35.0;
    EXPECT_TRUE(Mentions(Validate(bad), "shift must be within"));
    bad = f;
    bad.muzzle_poi_units = kClickSmoa;
    EXPECT_TRUE(Mentions(Validate(bad), "shift must be within"));
    bad = f;
    bad.muzzle_poi_right = -10.0;
    bad.muzzle_poi_units = kClickMrad;  // the limit itself
    EXPECT_TRUE(Validate(bad).empty());
}

TEST(RifleDetailsShift, OnlyWhenTheDeviceDiffersFromTheZero) {
    RifleRecord r;
    r.muzzle_poi_up_rad = -0.0004;
    r.muzzle_poi_right_rad = 0.0001;
    EXPECT_DOUBLE_EQ(storage::MuzzleDeviceShift(r).up_rad, 0.0);  // no device stated
    r.muzzle_device = "none";
    EXPECT_DOUBLE_EQ(storage::MuzzleDeviceShift(r).up_rad, 0.0);
    r.muzzle_device = "brake";
    r.muzzle_device_on = true;  // on, zeroed without it: shifted
    r.zeroed_with_device = false;
    EXPECT_DOUBLE_EQ(storage::MuzzleDeviceShift(r).up_rad, -0.0004);
    EXPECT_DOUBLE_EQ(storage::MuzzleDeviceShift(r).right_rad, 0.0001);
    r.zeroed_with_device = true;  // on, as at the zero
    EXPECT_DOUBLE_EQ(storage::MuzzleDeviceShift(r).up_rad, 0.0);
    r.muzzle_device_on = false;  // off, zeroed with it: back by the shift
    EXPECT_DOUBLE_EQ(storage::MuzzleDeviceShift(r).up_rad, 0.0004);
    EXPECT_DOUBLE_EQ(storage::MuzzleDeviceShift(r).right_rad, -0.0001);

    // A suppressor over it, on its own: the shifts add up.
    r.muzzle_device_on = true;
    r.zeroed_with_device = false;
    r.suppressor_poi_up_rad = -0.0003;
    r.suppressor_poi_right_rad = 0.0002;
    EXPECT_DOUBLE_EQ(storage::MuzzleDeviceShift(r).up_rad, -0.0004);  // not stated yet
    r.has_suppressor = true;
    EXPECT_NEAR(storage::MuzzleDeviceShift(r).up_rad, -0.0007, 1e-15);
    EXPECT_NEAR(storage::MuzzleDeviceShift(r).right_rad, 0.0003, 1e-15);
    r.zeroed_with_suppressor = true;  // zeroed with the suppressor on: only the brake's
    EXPECT_NEAR(storage::MuzzleDeviceShift(r).up_rad, -0.0004, 1e-15);
    r.suppressor_on = false;  // then taken off: back by its shift
    EXPECT_NEAR(storage::MuzzleDeviceShift(r).up_rad, -0.0001, 1e-15);
}

TEST_F(RifleDetails, SuppressorThatHitsLowIsDialledUp) {
    RifleForm f = Described();
    f.elevation_travel = 0.0;
    f.windage_travel = 0.0;
    f.has_suppressor = true;
    f.suppressor_name = "Ase Utra SL7";
    f.suppressor_on = false;
    const Id rifle = SaveRifleForm(db_, f).value();
    const Id pair = EnsureProfile(db_, rifle, SaveCartridgeForm(db_, Cartridge()).value()).value();
    SessionConditions s;
    s.target_range_m = 600.0;
    auto solve = [&] {
        return Summarize(storage::LoadProfile(db_, pair).value(), s, AngleUnit::kMoa);
    };
    const SolutionSummary off = solve();
    ASSERT_TRUE(off.ok) << off.error;

    // On, after a zero without it: 1.5 MOA low and 0.5 MOA right = dial that much up and left.
    f.rifle_id = rifle;
    f.suppressor_on = true;
    f.suppressor_poi_up = -1.5;
    f.suppressor_poi_right = 0.5;
    ASSERT_TRUE(SaveRifleForm(db_, f).ok());
    const SolutionSummary on = solve();
    EXPECT_NEAR(on.elevation - off.elevation, 1.5, 0.02);
    EXPECT_NEAR(on.windage - off.windage, -0.5, 0.02);
    const RifleForm g = LoadRifleForm(db_, rifle).value();
    EXPECT_TRUE(g.has_suppressor);
    EXPECT_EQ(g.suppressor_name, "Ase Utra SL7");
    EXPECT_DOUBLE_EQ(g.suppressor_poi_up, -1.5);
    EXPECT_TRUE(g.suppressor_on);

    // Zeroed with it on: no change while it stays on.
    f.zeroed_with_suppressor = true;
    ASSERT_TRUE(SaveRifleForm(db_, f).ok());
    EXPECT_NEAR(solve().elevation, off.elevation, 1e-9);
}

TEST(RifleDetailsReach, HalfTheTravelPlusTheRail) {
    RifleRecord rifle;
    ScopeRecord scope;
    EXPECT_DOUBLE_EQ(Reach(rifle, scope).up_rad, 0.0);  // travel not stated
    rifle.rail_slope_rad = units::MoaToRad(20.0);
    EXPECT_DOUBLE_EQ(Reach(rifle, scope).up_rad, 0.0);  // a rail alone says nothing
    scope.elevation_travel_rad = 0.029;
    scope.windage_travel_rad = 0.0145;
    EXPECT_NEAR(Reach(rifle, scope).up_rad, 0.0145 + units::MoaToRad(20.0), 1e-12);
    EXPECT_NEAR(Reach(rifle, scope).side_rad, 0.00725, 1e-12);
}

TEST_F(RifleDetails, BeyondTheTurretTheSolutionWarns) {
    RifleForm f = Described();
    f.elevation_travel = 0.0;
    f.windage_travel = 0.0;
    f.rail_slope = 0.0;
    const Id rifle = SaveRifleForm(db_, f).value();
    const Id pair = EnsureProfile(db_, rifle, SaveCartridgeForm(db_, Cartridge()).value()).value();
    SessionConditions s;
    s.target_range_m = 1000.0;
    s.winds = {{6.0, 90.0, 0.0}};  // from the right, a lot

    auto warned = [&](const SolutionSummary& r, const std::string& code) -> const Warning* {
        for (const Warning& w : r.warnings) {
            if (w.code == code) {
                return &w;
            }
        }
        return nullptr;
    };

    // No travel stated: no warning, whatever the correction.
    const SolutionSummary free =
        Summarize(storage::LoadProfile(db_, pair).value(), s, AngleUnit::kMrad);
    ASSERT_TRUE(free.ok) << free.error;
    ASSERT_GT(free.elevation, 4.0);
    EXPECT_EQ(warned(free, kWarnElevationTravel), nullptr);
    EXPECT_EQ(warned(free, kWarnWindageTravel), nullptr);

    // Turrets that reach half the correction: the other half is held.
    f.rifle_id = rifle;
    f.elevation_travel = free.elevation;  // up: half of it
    f.windage_travel = std::abs(free.windage);
    ASSERT_TRUE(SaveRifleForm(db_, f).ok());
    const SolutionSummary r =
        Summarize(storage::LoadProfile(db_, pair).value(), s, AngleUnit::kMrad);
    const Warning* up = warned(r, kWarnElevationTravel);
    ASSERT_NE(up, nullptr);
    EXPECT_NEAR(up->value, free.elevation / 2.0, 1e-3);
    const Warning* side = warned(r, kWarnWindageTravel);
    ASSERT_NE(side, nullptr);
    EXPECT_NEAR(side->value, std::abs(free.windage) / 2.0, 1e-3);

    // A rail as steep as the missing half: the turret reaches again.
    f.rail_slope_units = kClickMrad;
    f.rail_slope = free.elevation / 2.0 + 0.1;
    ASSERT_TRUE(SaveRifleForm(db_, f).ok());
    const SolutionSummary railed =
        Summarize(storage::LoadProfile(db_, pair).value(), s, AngleUnit::kMrad);
    EXPECT_EQ(warned(railed, kWarnElevationTravel), nullptr);
}

}  // namespace
}  // namespace ballistics::applogic
