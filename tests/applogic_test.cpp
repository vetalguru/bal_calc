#include <ballistics/applogic/profile_form.h>
#include <ballistics/applogic/session.h>
#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>
#include <ballistics/units.h>

#include <gtest/gtest.h>

#include <cmath>

namespace ballistics::applogic {
namespace {

ProfileForm SampleForm() {
    ProfileForm f;
    f.name = "Tikka T3x .308";
    f.caliber = ".308 Win";
    f.sight_height_cm = 4.5;
    f.twist_in = 11.0;
    f.click_units = kClickMoa;
    f.click_value = 0.25;
    f.bullet_name = "SMK 175";
    f.drag_table = "G7";
    f.bc = 0.243;
    f.mass_gr = 175.0;
    f.diameter_in = 0.308;
    f.length_in = 1.24;
    f.muzzle_velocity_mps = 790.0;
    f.powder_sensitivity_pct_per_c = 0.1;
    f.zero_range_m = 100.0;
    f.zero_temperature_c = 10.0;
    f.zero_pressure_hpa = 990.0;
    return f;
}

class AppLogic : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_TRUE(db_.Open(":memory:").ok()); }
    storage::Database db_;
};

TEST(AppLogicClicks, UnitConversions) {
    EXPECT_NEAR(ClickToRad(kClickMrad, 0.1), 1e-4, 1e-15);
    EXPECT_NEAR(ClickToRad(kClickMoa, 0.25), units::MoaToRad(0.25), 1e-15);
    // 1/4" at 100 yd.
    EXPECT_NEAR(ClickToRad(kClickSmoa, 0.25) * units::YardToM(100.0), units::InchToM(0.25), 1e-12);
    EXPECT_NEAR(ClickToRad(kClickCm100m, 1.0) * 100.0, 0.01, 1e-15);
    EXPECT_EQ(ClickToRad("furlong", 1.0), 0.0);
    EXPECT_NEAR(RadToClick(kClickMoa, ClickToRad(kClickMoa, 0.125)), 0.125, 1e-12);
}

TEST(AppLogicForm, ValidationNamesTheProblem) {
    EXPECT_TRUE(Validate(SampleForm()).empty());
    ProfileForm f = SampleForm();
    f.name.clear();
    EXPECT_NE(Validate(f).find("name"), std::string::npos);
    f = SampleForm();
    f.bc = 0.0;
    EXPECT_NE(Validate(f).find("coefficient"), std::string::npos);
    f = SampleForm();
    f.click_value = 0.0;
    EXPECT_NE(Validate(f).find("click"), std::string::npos);
}

TEST_F(AppLogic, SaveLoadRoundTrip) {
    auto id = SaveProfileForm(db_, SampleForm());
    ASSERT_TRUE(id.ok()) << id.error().message;
    auto loaded = LoadProfileForm(db_, id.value());
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    const ProfileForm& f = loaded.value();
    EXPECT_EQ(f.profile_id, id.value());
    EXPECT_EQ(f.name, "Tikka T3x .308");
    EXPECT_EQ(f.bullet_name, "SMK 175");
    EXPECT_NEAR(f.mass_gr, 175.0, 1e-9);
    EXPECT_NEAR(f.diameter_in, 0.308, 1e-12);
    EXPECT_NEAR(f.twist_in, 11.0, 1e-12);
    EXPECT_FALSE(f.twist_left);
    EXPECT_EQ(f.click_units, kClickMoa);
    EXPECT_NEAR(f.click_value, 0.25, 1e-12);
    EXPECT_NEAR(f.powder_sensitivity_pct_per_c, 0.1, 1e-12);
    EXPECT_NEAR(f.zero_temperature_c, 10.0, 1e-9);
    EXPECT_NEAR(f.zero_pressure_hpa, 990.0, 1e-9);
}

TEST_F(AppLogic, EditUpdatesInPlace) {
    const Id id = SaveProfileForm(db_, SampleForm()).value();
    ProfileForm f = LoadProfileForm(db_, id).value();
    f.muzzle_velocity_mps = 805.0;
    f.twist_left = true;
    ASSERT_TRUE(SaveProfileForm(db_, f).ok());
    EXPECT_EQ(storage::Repository<storage::ProfileRecord>(db_).List().value().size(), 1U);
    EXPECT_EQ(storage::Repository<storage::BulletRecord>(db_).List().value().size(), 1U);
    const ProfileForm g = LoadProfileForm(db_, id).value();
    EXPECT_DOUBLE_EQ(g.muzzle_velocity_mps, 805.0);
    EXPECT_TRUE(g.twist_left);
}

TEST_F(AppLogic, InvalidFormIsNotSaved) {
    ProfileForm f = SampleForm();
    f.muzzle_velocity_mps = 0.0;
    EXPECT_FALSE(SaveProfileForm(db_, f).ok());
    EXPECT_TRUE(storage::Repository<storage::BulletRecord>(db_).List().value().empty());
}

TEST_F(AppLogic, DeleteRemovesOwnedRecords) {
    const Id id = SaveProfileForm(db_, SampleForm()).value();
    ASSERT_TRUE(DeleteProfile(db_, id).ok());
    EXPECT_TRUE(storage::Repository<storage::ProfileRecord>(db_).List().value().empty());
    EXPECT_TRUE(storage::Repository<storage::RifleRecord>(db_).List().value().empty());
    EXPECT_TRUE(storage::Repository<storage::ScopeRecord>(db_).List().value().empty());
    EXPECT_TRUE(storage::Repository<storage::CartridgeRecord>(db_).List().value().empty());
    EXPECT_TRUE(storage::Repository<storage::BulletRecord>(db_).List().value().empty());
    EXPECT_FALSE(DeleteProfile(db_, id).ok());
}

TEST_F(AppLogic, SessionRoundTrip) {
    SessionConditions s;
    s.temperature_c = -12.5;
    s.pressure_hpa = 940.0;
    s.altitude_m = 650.0;
    s.humidity_pct = 80.0;
    s.powder_c = 5.0;
    s.winds = {{3.0, 90.0, 400.0}, {5.5, 45.0, 0.0}};
    s.look_angle_deg = -7.0;
    s.latitude_deg = 48.5;
    s.target_range_m = 875.0;
    ASSERT_TRUE(SaveSession(db_, s).ok());
    const SessionConditions g = LoadSession(db_).value();
    EXPECT_DOUBLE_EQ(g.temperature_c, -12.5);
    EXPECT_DOUBLE_EQ(g.pressure_hpa, 940.0);
    ASSERT_TRUE(g.powder_c.has_value());
    EXPECT_DOUBLE_EQ(*g.powder_c, 5.0);
    ASSERT_EQ(g.winds.size(), 2U);
    EXPECT_DOUBLE_EQ(g.winds[1].speed_mps, 5.5);
    EXPECT_DOUBLE_EQ(g.winds[0].until_m, 400.0);
    ASSERT_TRUE(g.latitude_deg.has_value());
    EXPECT_FALSE(g.azimuth_deg.has_value());
    EXPECT_DOUBLE_EQ(g.target_range_m, 875.0);

    // Clearing an optional survives the round trip too.
    s.powder_c.reset();
    ASSERT_TRUE(SaveSession(db_, s).ok());
    EXPECT_FALSE(LoadSession(db_).value().powder_c.has_value());
}

TEST_F(AppLogic, FreshSessionHasDefaults) {
    const SessionConditions g = LoadSession(db_).value();
    EXPECT_DOUBLE_EQ(g.temperature_c, 15.0);
    EXPECT_TRUE(g.winds.empty());
    EXPECT_FALSE(g.latitude_deg.has_value());
}

TEST_F(AppLogic, SummaryAtTarget) {
    const Id id = SaveProfileForm(db_, SampleForm()).value();
    const storage::LoadedProfile p = storage::LoadProfile(db_, id).value();
    SessionConditions s;
    s.temperature_c = 10.0;
    s.pressure_hpa = 990.0;
    s.target_range_m = 800.0;
    s.winds = {{4.0, 90.0, 0.0}};
    const SolutionSummary mrad = Summarize(p, s, AngleUnit::kMrad);
    ASSERT_TRUE(mrad.ok) << mrad.error;
    EXPECT_GT(mrad.elevation, 5.0);
    EXPECT_LT(mrad.elevation, 15.0);
    EXPECT_GT(mrad.windage, 0.5); // wind from the right: hold right
    EXPECT_GT(mrad.stability, 1.0);
    // 1/4 MOA clicks.
    EXPECT_NEAR(mrad.elevation_clicks, std::round(units::RadToMoa(mrad.elevation * 1e-3) * 4.0), 1.0);
    const SolutionSummary moa = Summarize(p, s, AngleUnit::kMoa);
    EXPECT_NEAR(moa.elevation, units::RadToMoa(mrad.elevation * 1e-3), 1e-9);

    s.target_range_m = 1200.0;
    const SolutionSummary far = Summarize(p, s, AngleUnit::kMrad);
    ASSERT_TRUE(far.ok);
    EXPECT_GT(far.transonic_range_m, 600.0); // Mach 1.2 reached on the way
    EXPECT_LT(far.transonic_range_m, 1200.0);

    s.target_range_m = 0.0;
    EXPECT_FALSE(Summarize(p, s, AngleUnit::kMrad).ok);
}

TEST_F(AppLogic, RangeTableMatchesSummary) {
    const Id id = SaveProfileForm(db_, SampleForm()).value();
    const storage::LoadedProfile p = storage::LoadProfile(db_, id).value();
    SessionConditions s;
    s.temperature_c = 10.0; // the profile's zero conditions
    s.pressure_hpa = 990.0;
    s.powder_c = 15.0; // zero_powder_c of the form
    s.winds = {{3.0, 90.0, 0.0}};
    const RangeTable t = BuildRangeTable(p, s, AngleUnit::kMrad, 0.0, 1000.0, 100.0);
    ASSERT_TRUE(t.ok) << t.error;
    ASSERT_EQ(t.rows.size(), 11U);
    EXPECT_DOUBLE_EQ(t.rows[0].range_m, 0.0);
    EXPECT_DOUBLE_EQ(t.rows[0].elevation, 0.0); // no hold at the muzzle
    for (std::size_t i = 2; i < t.rows.size(); ++i) {
        EXPECT_GT(t.rows[i].elevation, t.rows[i - 1].elevation);
        EXPECT_LT(t.rows[i].velocity_mps, t.rows[i - 1].velocity_mps);
        EXPECT_GT(t.rows[i].time_s, t.rows[i - 1].time_s);
    }
    // Zeroed at 100 m in this air. (The crosswind above adds ~0.08 MRAD
    // of aerodynamic jump, so check without it.)
    SessionConditions calm = s;
    calm.winds.clear();
    EXPECT_NEAR(BuildRangeTable(p, calm, AngleUnit::kMrad, 100.0, 100.0, 1.0).rows.at(0).drop_cm,
                0.0, 0.01);
    EXPECT_LT(t.rows[1].drop_cm, -0.5); // wind from the right: jump low

    // Same numbers as the single-target summary.
    s.target_range_m = 700.0;
    const SolutionSummary one = Summarize(p, s, AngleUnit::kMrad);
    EXPECT_NEAR(t.rows[7].elevation, one.elevation, 1e-9);
    EXPECT_NEAR(t.rows[7].windage, one.windage, 1e-9);
    EXPECT_DOUBLE_EQ(t.rows[7].elevation_clicks, one.elevation_clicks);
}

TEST_F(AppLogic, RangeTableRejectsBadSpec) {
    const Id id = SaveProfileForm(db_, SampleForm()).value();
    const storage::LoadedProfile p = storage::LoadProfile(db_, id).value();
    EXPECT_FALSE(BuildRangeTable(p, {}, AngleUnit::kMrad, 0.0, 1000.0, 0.0).ok);
    EXPECT_FALSE(BuildRangeTable(p, {}, AngleUnit::kMrad, 500.0, 100.0, 50.0).ok);
    EXPECT_FALSE(BuildRangeTable(p, {}, AngleUnit::kMrad, 0.0, 3000.0, 1.0).ok); // > 2000 rows
}

} // namespace
} // namespace ballistics::applogic
