#include <ballistics/applogic/armory.h>
#include <ballistics/applogic/session.h>
#include <ballistics/applogic/wez.h>
#include <ballistics/atmosphere.h>
#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>
#include <ballistics/units.h>
#include <gtest/gtest.h>

#include <cmath>
#include <utility>
#include <vector>

namespace ballistics::applogic {
namespace {

RifleForm SampleRifle() {
    RifleForm f;
    f.name = "Tikka T3x";
    f.caliber = ".308 Win";
    f.sight_height_cm = 4.5;
    f.twist_in = 11.0;
    f.click_units = kClickMoa;
    f.click_value = 0.25;
    f.zero_range_m = 100.0;
    f.zero_temperature_c = 10.0;
    f.zero_pressure_hpa = 990.0;
    return f;
}

CartridgeForm SampleCartridge() {
    CartridgeForm f;
    f.name = "Handload SMK 175";
    f.caliber = ".308 Win";
    f.bullet_name = "SMK 175";
    f.drag_table = "G7";
    f.bc = 0.243;
    f.mass_gr = 175.0;
    f.diameter_in = 0.308;
    f.length_in = 1.24;
    f.muzzle_velocity_mps = 790.0;
    f.powder_sensitivity_pct_per_c = 0.1;
    return f;
}

// Profile id of the sample rifle + cartridge.
Id SamplePair(storage::Database& db) {
    const Id rifle = SaveRifleForm(db, SampleRifle()).value();
    const Id cartridge = SaveCartridgeForm(db, SampleCartridge()).value();
    return EnsureProfile(db, rifle, cartridge).value();
}

template <typename T>
std::size_t Count(storage::Database& db) {
    return storage::Repository<T>(db).List().value().size();
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

TEST(AppLogicCaliber, LeadingNumberMatches) {
    EXPECT_TRUE(SameCaliber(".308 Win", "308 Winchester"));
    EXPECT_TRUE(SameCaliber("6.5 Creedmoor", "6.5x47 Lapua"));
    EXPECT_TRUE(SameCaliber(".338 LM", ".338"));
    EXPECT_FALSE(SameCaliber(".308 Win", ".300 WM"));
    EXPECT_FALSE(SameCaliber("6.5 Creedmoor", "6mm Creedmoor"));
    EXPECT_FALSE(SameCaliber("", ""));
}

TEST(AppLogicForm, ValidationNamesTheProblem) {
    EXPECT_TRUE(Validate(SampleRifle()).empty());
    EXPECT_TRUE(Validate(SampleCartridge()).empty());
    RifleForm r = SampleRifle();
    r.name.clear();
    EXPECT_NE(Validate(r).find("rifle name"), std::string::npos);
    r = SampleRifle();
    r.click_value = 0.0;
    EXPECT_NE(Validate(r).find("click"), std::string::npos);
    CartridgeForm c = SampleCartridge();
    c.name.clear();
    EXPECT_NE(Validate(c).find("cartridge name"), std::string::npos);
    c = SampleCartridge();
    c.bc = 0.0;
    EXPECT_NE(Validate(c).find("coefficient"), std::string::npos);
}

TEST_F(AppLogic, RifleRoundTrip) {
    auto id = SaveRifleForm(db_, SampleRifle());
    ASSERT_TRUE(id.ok()) << id.error().message;
    const RifleForm f = LoadRifleForm(db_, id.value()).value();
    EXPECT_EQ(f.rifle_id, id.value());
    EXPECT_EQ(f.name, "Tikka T3x");
    EXPECT_NEAR(f.twist_in, 11.0, 1e-12);
    EXPECT_FALSE(f.twist_left);
    EXPECT_EQ(f.click_units, kClickMoa);
    EXPECT_NEAR(f.click_value, 0.25, 1e-12);
    EXPECT_NEAR(f.zero_temperature_c, 10.0, 1e-9);
    EXPECT_NEAR(f.zero_pressure_hpa, 990.0, 1e-9);

    RifleForm g = f;
    g.twist_left = true;
    g.zero_range_m = 200.0;
    ASSERT_TRUE(SaveRifleForm(db_, g).ok());
    EXPECT_EQ(Count<storage::RifleRecord>(db_), 1U);
    EXPECT_EQ(Count<storage::ScopeRecord>(db_), 1U);  // the same scope updated
    const RifleForm h = LoadRifleForm(db_, id.value()).value();
    EXPECT_TRUE(h.twist_left);
    EXPECT_DOUBLE_EQ(h.zero_range_m, 200.0);
}

TEST_F(AppLogic, CartridgeRoundTrip) {
    auto id = SaveCartridgeForm(db_, SampleCartridge());
    ASSERT_TRUE(id.ok()) << id.error().message;
    CartridgeForm f = LoadCartridgeForm(db_, id.value()).value();
    EXPECT_EQ(f.name, "Handload SMK 175");
    EXPECT_EQ(f.caliber, ".308 Win");
    EXPECT_EQ(f.bullet_name, "SMK 175");
    EXPECT_EQ(f.library_bullet_id, 0);  // its own bullet
    EXPECT_NEAR(f.mass_gr, 175.0, 1e-9);
    EXPECT_NEAR(f.powder_sensitivity_pct_per_c, 0.1, 1e-12);

    f.muzzle_velocity_mps = 805.0;
    ASSERT_TRUE(SaveCartridgeForm(db_, f).ok());
    EXPECT_EQ(Count<storage::CartridgeRecord>(db_), 1U);
    EXPECT_EQ(Count<storage::BulletRecord>(db_), 1U);
    EXPECT_DOUBLE_EQ(LoadCartridgeForm(db_, id.value()).value().muzzle_velocity_mps, 805.0);
}

TEST_F(AppLogic, InvalidFormsAreNotSaved) {
    CartridgeForm c = SampleCartridge();
    c.muzzle_velocity_mps = 0.0;
    EXPECT_FALSE(SaveCartridgeForm(db_, c).ok());
    EXPECT_EQ(Count<storage::BulletRecord>(db_), 0U);
    RifleForm r = SampleRifle();
    r.zero_range_m = 5.0;
    EXPECT_FALSE(SaveRifleForm(db_, r).ok());
    EXPECT_EQ(Count<storage::ScopeRecord>(db_), 0U);
}

TEST_F(AppLogic, OneCartridgeServesSeveralRifles) {
    const Id tikka = SaveRifleForm(db_, SampleRifle()).value();
    RifleForm other = SampleRifle();
    other.name = "Remington 700";
    other.zero_range_m = 300.0;
    const Id remington = SaveRifleForm(db_, other).value();
    const Id cartridge = SaveCartridgeForm(db_, SampleCartridge()).value();

    const Id a = EnsureProfile(db_, tikka, cartridge).value();
    const Id b = EnsureProfile(db_, remington, cartridge).value();
    EXPECT_NE(a, b);
    EXPECT_EQ(EnsureProfile(db_, tikka, cartridge).value(), a);  // found, not duplicated
    EXPECT_EQ(Count<storage::ProfileRecord>(db_), 2U);
    EXPECT_FALSE(EnsureProfile(db_, tikka, 999).ok());

    // Each rifle zeroes at its own range.
    SessionConditions s;
    s.temperature_c = 10.0;
    s.pressure_hpa = 990.0;
    s.target_range_m = 300.0;
    s.powder_c = 15.0;  // the rifles' zero_powder_c
    const auto at_a = Summarize(storage::LoadProfile(db_, a).value(), s, AngleUnit::kMrad);
    const auto at_b = Summarize(storage::LoadProfile(db_, b).value(), s, AngleUnit::kMrad);
    EXPECT_GT(at_a.elevation, 1.0);
    EXPECT_NEAR(at_b.elevation, 0.0, 0.01);
}

TEST_F(AppLogic, ZeroOffsetShiftsThePointOfImpact) {
    const Id id = SamplePair(db_);
    SessionConditions s;
    s.temperature_c = 10.0;
    s.pressure_hpa = 990.0;
    s.target_range_m = 100.0;
    const auto before = Summarize(storage::LoadProfile(db_, id).value(), s, AngleUnit::kMrad);
    // This cartridge hits 2 cm high at the 100 m zero: hold 0.2 mrad lower.
    ASSERT_TRUE(SetZeroOffset(db_, id, 2.0, 0.0).ok());
    const auto after = Summarize(storage::LoadProfile(db_, id).value(), s, AngleUnit::kMrad);
    EXPECT_NEAR(after.elevation - before.elevation, -0.2, 0.005);
    EXPECT_FALSE(SetZeroOffset(db_, id, 500.0, 0.0).ok());
}

TEST_F(AppLogic, DeletingARifleOrCartridgeRemovesItsPairs) {
    const Id pair = SamplePair(db_);
    const auto p = storage::Repository<storage::ProfileRecord>(db_).Get(pair).value().value();
    ASSERT_TRUE(DeleteRifle(db_, p.rifle_id).ok());
    EXPECT_EQ(Count<storage::ProfileRecord>(db_), 0U);
    EXPECT_EQ(Count<storage::RifleRecord>(db_), 0U);
    EXPECT_EQ(Count<storage::ScopeRecord>(db_), 0U);
    EXPECT_EQ(Count<storage::CartridgeRecord>(db_), 1U);  // the cartridge stays
    EXPECT_FALSE(DeleteRifle(db_, p.rifle_id).ok());

    const Id rifle = SaveRifleForm(db_, SampleRifle()).value();
    ASSERT_TRUE(EnsureProfile(db_, rifle, p.cartridge_id).ok());
    ASSERT_TRUE(DeleteCartridge(db_, p.cartridge_id).ok());
    EXPECT_EQ(Count<storage::ProfileRecord>(db_), 0U);
    EXPECT_EQ(Count<storage::CartridgeRecord>(db_), 0U);
    EXPECT_EQ(Count<storage::BulletRecord>(db_), 0U);  // its own bullet went too
    EXPECT_EQ(Count<storage::RifleRecord>(db_), 1U);
}

TEST_F(AppLogic, CartridgesOfTheRiflesCaliberComeFirst) {
    CartridgeForm magnum = SampleCartridge();
    magnum.name = "A .300 WM load";
    magnum.caliber = ".300 WM";
    ASSERT_TRUE(SaveCartridgeForm(db_, magnum).ok());
    ASSERT_TRUE(SaveCartridgeForm(db_, SampleCartridge()).ok());
    const auto all = ListCartridges(db_).value();
    ASSERT_EQ(all.size(), 2U);
    EXPECT_EQ(all[0].name, "A .300 WM load");  // by name
    const auto for_308 = ListCartridges(db_, "308 Winchester").value();
    EXPECT_EQ(for_308[0].name, "Handload SMK 175");
    EXPECT_EQ(for_308[0].bullet_name, "SMK 175");
}

TEST_F(AppLogic, LibraryCartridgesAreCopied) {
    // A factory load as the .ammo importer stores it.
    storage::BulletRecord b;
    b.name = "Factory 168";
    b.diameter_m = units::InchToM(0.308);
    b.mass_kg = units::GrainToKg(168.0);
    b.drag_kind = storage::kDragKindBc;
    b.drag_table = "G1";
    b.bc = 0.462;
    b.source = "import:ammo";
    ASSERT_TRUE(storage::Repository<storage::BulletRecord>(db_).Save(b).ok());
    storage::CartridgeRecord lib;
    lib.name = "Factory 168";
    lib.caliber = ".308 Win";
    lib.bullet_id = b.id;
    lib.muzzle_velocity_mps = 810.0;
    lib.barrel_length_m = 0.61;
    lib.source = "import:ammo";
    ASSERT_TRUE(storage::Repository<storage::CartridgeRecord>(db_).Save(lib).ok());

    EXPECT_TRUE(ListCartridges(db_).value().empty());  // not the user's
    const auto library = ListLibraryCartridges(db_, "factory").value();
    ASSERT_EQ(library.size(), 1U);
    EXPECT_TRUE(ListLibraryCartridges(db_, "nosler").value().empty());

    CartridgeForm f = CartridgeFromLibrary(db_, library[0].id).value();
    EXPECT_EQ(f.cartridge_id, 0);
    EXPECT_EQ(f.library_bullet_id, b.id);
    f.name = "My factory 168";
    const Id mine = SaveCartridgeForm(db_, f).value();
    EXPECT_NE(mine, lib.id);
    const auto copy = storage::Repository<storage::CartridgeRecord>(db_).Get(mine).value().value();
    EXPECT_EQ(copy.bullet_id, b.id);               // the library bullet, shared
    EXPECT_DOUBLE_EQ(copy.barrel_length_m, 0.61);  // kept from the original
    EXPECT_EQ(ListCartridges(db_).value().size(), 1U);
    EXPECT_EQ(ListLibraryCartridges(db_).value().size(), 1U);  // original untouched
}

TEST_F(AppLogic, SampleProfile) {
    const Id id = CreateSampleProfile(db_, "Sample rifle", "Sample cartridge").value();
    const auto p = storage::LoadProfile(db_, id).value();
    EXPECT_EQ(p.rifle.name, "Sample rifle");
    EXPECT_EQ(p.cartridge.name, "Sample cartridge");
    ASSERT_TRUE(p.scope.has_value());
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
    const Id id = SamplePair(db_);
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
    EXPECT_GT(mrad.windage, 0.5);  // wind from the right: hold right
    EXPECT_GT(mrad.stability, 1.0);
    // 1/4 MOA clicks.
    EXPECT_NEAR(mrad.elevation_clicks, std::round(units::RadToMoa(mrad.elevation * 1e-3) * 4.0),
                1.0);
    const SolutionSummary moa = Summarize(p, s, AngleUnit::kMoa);
    EXPECT_NEAR(moa.elevation, units::RadToMoa(mrad.elevation * 1e-3), 1e-9);

    s.target_range_m = 1200.0;
    const SolutionSummary far = Summarize(p, s, AngleUnit::kMrad);
    ASSERT_TRUE(far.ok);
    EXPECT_GT(far.transonic_range_m, 600.0);  // Mach 1.2 reached on the way
    EXPECT_LT(far.transonic_range_m, 1200.0);

    s.target_range_m = 0.0;
    EXPECT_FALSE(Summarize(p, s, AngleUnit::kMrad).ok);
}

TEST_F(AppLogic, RangeTableMatchesSummary) {
    const Id id = SamplePair(db_);
    const storage::LoadedProfile p = storage::LoadProfile(db_, id).value();
    SessionConditions s;
    s.temperature_c = 10.0;  // the profile's zero conditions
    s.pressure_hpa = 990.0;
    s.powder_c = 15.0;  // zero_powder_c of the form
    s.winds = {{3.0, 90.0, 0.0}};
    const RangeTable t = BuildRangeTable(p, s, AngleUnit::kMrad, 0.0, 1000.0, 100.0);
    ASSERT_TRUE(t.ok) << t.error;
    ASSERT_EQ(t.rows.size(), 11U);
    EXPECT_DOUBLE_EQ(t.rows[0].range_m, 0.0);
    EXPECT_DOUBLE_EQ(t.rows[0].elevation, 0.0);  // no hold at the muzzle
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
    EXPECT_LT(t.rows[1].drop_cm, -0.5);  // wind from the right: jump low

    // Same numbers as the single-target summary.
    s.target_range_m = 700.0;
    const SolutionSummary one = Summarize(p, s, AngleUnit::kMrad);
    EXPECT_NEAR(t.rows[7].elevation, one.elevation, 1e-9);
    EXPECT_NEAR(t.rows[7].windage, one.windage, 1e-9);
    EXPECT_DOUBLE_EQ(t.rows[7].elevation_clicks, one.elevation_clicks);
}

TEST_F(AppLogic, RangeTableRejectsBadSpec) {
    const Id id = SamplePair(db_);
    const storage::LoadedProfile p = storage::LoadProfile(db_, id).value();
    EXPECT_FALSE(BuildRangeTable(p, {}, AngleUnit::kMrad, 0.0, 1000.0, 0.0).ok);
    EXPECT_FALSE(BuildRangeTable(p, {}, AngleUnit::kMrad, 500.0, 100.0, 50.0).ok);
    EXPECT_FALSE(BuildRangeTable(p, {}, AngleUnit::kMrad, 0.0, 3000.0, 1.0).ok);  // > 2000 rows
}

bool Has(const SolutionSummary& s, const char* code, double* value = nullptr) {
    for (const Warning& w : s.warnings) {
        if (w.code == code) {
            if (value) {
                *value = w.value;
            }
            return true;
        }
    }
    return false;
}

// At the zero conditions of the sample rifle (10 C, 990 hPa).
SessionConditions AtZero(double range_m) {
    SessionConditions s;
    s.temperature_c = 10.0;
    s.pressure_hpa = 990.0;
    s.target_range_m = range_m;
    return s;
}

TEST_F(AppLogic, SummaryApexPointBlankAndDensityAltitude) {
    const storage::LoadedProfile p = storage::LoadProfile(db_, SamplePair(db_)).value();
    const SolutionSummary r = Summarize(p, AtZero(300.0), AngleUnit::kMrad);
    ASSERT_TRUE(r.ok) << r.error;
    // Zeroed at 100 m: the bullet tops out just past ~50 m, a few mm high.
    EXPECT_GT(r.apex_cm, 0.0);
    EXPECT_LT(r.apex_cm, 2.0);
    EXPECT_GT(r.apex_range_m, 30.0);
    EXPECT_LT(r.apex_range_m, 100.0);
    // 20 cm target, aim at the centre: from the muzzle to ~230 m.
    EXPECT_DOUBLE_EQ(r.point_blank_near_m, 0.0);
    EXPECT_GT(r.point_blank_far_m, 180.0);
    EXPECT_LT(r.point_blank_far_m, 300.0);
    // 10 C, 990 hPa, 50 %: the session air.
    EXPECT_NEAR(r.density_altitude_m, DensityAltitude({0.0, 99000.0, units::CToK(10.0), 0.5}),
                1e-6);
    EXPECT_NEAR(r.pressure_hpa, 990.0, 1e-9);
    EXPECT_TRUE(r.warnings.empty());

    SessionConditions big = AtZero(300.0);
    big.target_height_cm = 50.0;
    EXPECT_GT(Summarize(p, big, AngleUnit::kMrad).point_blank_far_m, r.point_blank_far_m + 50.0);
}

TEST_F(AppLogic, DensityAltitudeSetsThePressure) {
    const storage::LoadedProfile p = storage::LoadProfile(db_, SamplePair(db_)).value();
    SessionConditions s = AtZero(600.0);
    s.density_altitude_m = 1500.0;
    const SolutionSummary r = Summarize(p, s, AngleUnit::kMrad);
    ASSERT_TRUE(r.ok);
    EXPECT_NEAR(r.density_altitude_m, 1500.0, 0.1);
    EXPECT_LT(r.pressure_hpa, 900.0);
    // Thinner air: less elevation than at the typed 990 hPa.
    EXPECT_LT(r.elevation, Summarize(p, AtZero(600.0), AngleUnit::kMrad).elevation);
}

TEST_F(AppLogic, WarningsFollowTheirThresholds) {
    storage::LoadedProfile p = storage::LoadProfile(db_, SamplePair(db_)).value();
    double v = 0.0;

    SessionConditions hot = AtZero(300.0);
    hot.temperature_c = 30.0;
    EXPECT_TRUE(Has(Summarize(p, hot, AngleUnit::kMrad), kWarnZeroTemperature, &v));
    EXPECT_NEAR(v, 20.0, 1e-9);
    hot.temperature_c = 24.0;  // 14 C off: still fine
    EXPECT_FALSE(Has(Summarize(p, hot, AngleUnit::kMrad), kWarnZeroTemperature));

    SessionConditions high = AtZero(300.0);
    high.pressure_hpa = 900.0;
    EXPECT_TRUE(Has(Summarize(p, high, AngleUnit::kMrad), kWarnZeroPressure, &v));
    EXPECT_NEAR(v, -90.0, 1e-9);

    SessionConditions old = AtZero(300.0);
    old.weather_at_unix = 1.0e9;
    EXPECT_FALSE(Has(Summarize(p, old, AngleUnit::kMrad, 1.0e9 + 3600.0), kWarnStaleWeather));
    EXPECT_TRUE(
        Has(Summarize(p, old, AngleUnit::kMrad, 1.0e9 + 25 * 3600.0), kWarnStaleWeather, &v));
    EXPECT_NEAR(v, 25.0, 1e-9);
    EXPECT_FALSE(Has(Summarize(p, old, AngleUnit::kMrad), kWarnStaleWeather));  // no clock

    const SolutionSummary far = Summarize(p, AtZero(1300.0), AngleUnit::kMrad);
    ASSERT_TRUE(far.ok);
    EXPECT_EQ(Has(far, kWarnSubsonic), far.mach < 1.0);
    EXPECT_EQ(Has(far, kWarnTransonic), far.mach >= 1.0 && far.mach < 1.2);
    EXPECT_TRUE(far.mach < 1.2);

    // A slow twist for a long bullet.
    p.rifle.twist_m = units::InchToM(16.0);
    const SolutionSummary slow = Summarize(p, AtZero(300.0), AngleUnit::kMrad);
    ASSERT_GT(slow.stability, 0.0);
    ASSERT_LT(slow.stability, kMarginalStability);
    EXPECT_TRUE(Has(slow, slow.stability < 1.0 ? kWarnUnstable : kWarnLowStability, &v));
    EXPECT_DOUBLE_EQ(v, slow.stability);
}

TEST_F(AppLogic, SessionKeepsTheNewFields) {
    SessionConditions s;
    s.density_altitude_m = 1234.0;
    s.target_height_cm = 35.0;
    s.weather_at_unix = 1.7e9;
    ASSERT_TRUE(SaveSession(db_, s).ok());
    const SessionConditions g = LoadSession(db_).value();
    EXPECT_EQ(g.density_altitude_m, 1234.0);
    EXPECT_DOUBLE_EQ(g.target_height_cm, 35.0);
    EXPECT_DOUBLE_EQ(g.weather_at_unix, 1.7e9);
    s.density_altitude_m.reset();
    ASSERT_TRUE(SaveSession(db_, s).ok());
    EXPECT_FALSE(LoadSession(db_).value().density_altitude_m.has_value());
}

TEST_F(AppLogic, ThreeEqualZonesAreOneWind) {
    const storage::LoadedProfile p = storage::LoadProfile(db_, SamplePair(db_)).value();
    SessionConditions one = AtZero(900.0);
    one.winds = {{5.0, 60.0, 0.0}};
    SessionConditions three = one;
    three.winds = {{5.0, 60.0, 300.0}, {5.0, 60.0, 600.0}, {5.0, 60.0, 0.0}};
    const SolutionSummary a = Summarize(p, one, AngleUnit::kMrad);
    const SolutionSummary b = Summarize(p, three, AngleUnit::kMrad);
    ASSERT_TRUE(a.ok && b.ok);
    EXPECT_NEAR(a.windage, b.windage, 1e-9);
    EXPECT_NEAR(a.elevation, b.elevation, 1e-9);
}

TEST_F(AppLogic, ZonesWeighByWhereTheWindBlows) {
    const storage::LoadedProfile p = storage::LoadProfile(db_, SamplePair(db_)).value();
    auto windage = [&](std::vector<WindInput> winds) {
        SessionConditions s = AtZero(900.0);
        s.winds = std::move(winds);
        return Summarize(p, s, AngleUnit::kMrad).windage;
    };
    const double everywhere = windage({{5.0, 90.0, 0.0}});
    const double near = windage({{5.0, 90.0, 450.0}, {0.0, 90.0, 0.0}});
    const double far = windage({{0.0, 90.0, 450.0}, {5.0, 90.0, 0.0}});
    // Both halves push right; the near wind has longer to act on the bullet.
    EXPECT_GT(near, 0.0);
    EXPECT_GT(far, 0.0);
    EXPECT_GT(near, far);
    // Nearly linear in the wind; spin drift is in each of them once.
    const double calm = windage({});
    EXPECT_NEAR(near + far - calm, everywhere, 0.02 * (everywhere - calm));
    // Opposite winds in the two halves nearly cancel.
    EXPECT_LT(std::abs(windage({{5.0, 90.0, 450.0}, {5.0, 270.0, 0.0}}) - calm),
              0.5 * (everywhere - calm));
}

TEST_F(AppLogic, WindBracketIsTheSecondSpeed) {
    const storage::LoadedProfile p = storage::LoadProfile(db_, SamplePair(db_)).value();
    SessionConditions s = AtZero(700.0);
    s.winds = {{3.0, 90.0, 300.0}, {2.0, 45.0, 0.0}};
    s.wind_gust_mps = 6.0;
    const SolutionSummary r = Summarize(p, s, AngleUnit::kMrad);
    ASSERT_TRUE(r.ok && r.has_gust);
    SessionConditions strong = s;
    strong.winds.front().speed_mps = 6.0;
    strong.wind_gust_mps = 0.0;
    const SolutionSummary g = Summarize(p, strong, AngleUnit::kMrad);
    EXPECT_FALSE(g.has_gust);
    EXPECT_NEAR(r.gust_windage, g.windage, 1e-9);
    EXPECT_NEAR(r.gust_windage_clicks, g.windage_clicks, 1e-9);
    EXPECT_NEAR(r.gust_windage_cm, g.windage_cm, 1e-9);
    EXPECT_GT(r.gust_windage, r.windage);

    // No wind at all: the gust blows from the right.
    SessionConditions calm = AtZero(700.0);
    calm.wind_gust_mps = 4.0;
    EXPECT_GT(Summarize(p, calm, AngleUnit::kMrad).gust_windage, 0.1);
    EXPECT_TRUE(LoadSession(db_).value().wind_gust_mps == 0.0);
    ASSERT_TRUE(SaveSession(db_, s).ok());
    EXPECT_DOUBLE_EQ(LoadSession(db_).value().wind_gust_mps, 6.0);
}

TEST_F(AppLogic, MovingTargetLead) {
    const storage::LoadedProfile p = storage::LoadProfile(db_, SamplePair(db_)).value();
    SessionConditions s = AtZero(500.0);
    s.winds = {{3.0, 90.0, 0.0}};
    const SolutionSummary still = Summarize(p, s, AngleUnit::kMrad);
    EXPECT_FALSE(still.has_lead);

    s.target_speed_mps = 4.0;  // a walking... running man, to the right
    s.target_heading_deg = 90.0;
    const SolutionSummary r = Summarize(p, s, AngleUnit::kMrad);
    ASSERT_TRUE(r.ok && r.has_lead);
    EXPECT_NEAR(r.lead, units::RadToMrad(std::atan2(4.0 * r.time_s, 500.0)), 1e-9);
    EXPECT_NEAR(r.lead_cm, 400.0 * r.time_s, 1e-9);
    EXPECT_NEAR(r.lead_total_windage, r.windage + r.lead, 1e-9);
    EXPECT_NEAR(r.lead_range_m, 500.0, 1e-9);
    EXPECT_NEAR(r.lead_elevation, r.elevation, 1e-9);
    EXPECT_NEAR(r.lead_clicks, std::round(units::RadToMoa(r.lead * 1e-3) * 4.0), 1e-9);  // 1/4 MOA

    s.target_heading_deg = 270.0;  // to the left
    EXPECT_NEAR(Summarize(p, s, AngleUnit::kMrad).lead, -r.lead, 1e-9);

    s.target_heading_deg = 0.0;  // straight away: no lead, a longer shot
    const SolutionSummary away = Summarize(p, s, AngleUnit::kMrad);
    EXPECT_NEAR(away.lead, 0.0, 1e-9);
    EXPECT_GT(away.lead_range_m, 502.0);
    EXPECT_GT(away.lead_elevation, away.elevation);

    // The range card has the same lead at the same range.
    s.target_heading_deg = 90.0;
    const RangeTable t = BuildRangeTable(p, s, AngleUnit::kMrad, 0.0, 1000.0, 100.0);
    ASSERT_TRUE(t.ok);
    EXPECT_DOUBLE_EQ(t.rows.front().lead, 0.0);
    EXPECT_NEAR(t.rows[5].lead, r.lead, 1e-9);
    EXPECT_GT(t.rows[10].lead, t.rows[5].lead);  // flight time grows faster than range

    ASSERT_TRUE(SaveSession(db_, s).ok());
    EXPECT_DOUBLE_EQ(LoadSession(db_).value().target_speed_mps, 4.0);
}

TEST_F(AppLogic, RangeRowsCarryTheParts) {
    const storage::LoadedProfile p = storage::LoadProfile(db_, SamplePair(db_)).value();
    SessionConditions s = AtZero(800.0);
    const RangeTable plain = BuildRangeTable(p, s, AngleUnit::kMrad, 0.0, 1000.0, 200.0);
    ASSERT_TRUE(plain.ok);
    for (const RangeRow& r : plain.rows) {
        EXPECT_DOUBLE_EQ(r.coriolis_drift_cm, 0.0);
        EXPECT_DOUBLE_EQ(r.coriolis_lift_cm, 0.0);
        EXPECT_DOUBLE_EQ(r.lead_cm, 0.0);
    }
    EXPECT_NEAR(plain.rows[4].spin_drift_cm, Summarize(p, s, AngleUnit::kMrad).spin_drift_cm, 1e-9);

    // Northern hemisphere, firing east: drift to the right, Eotvos lift.
    s.latitude_deg = 50.0;
    s.azimuth_deg = 90.0;
    s.target_speed_mps = 3.0;
    const RangeTable t = BuildRangeTable(p, s, AngleUnit::kMrad, 0.0, 1000.0, 200.0);
    ASSERT_TRUE(t.ok);
    const RangeRow& far = t.rows.back();
    EXPECT_GT(far.coriolis_drift_cm, 2.0);
    EXPECT_GT(far.coriolis_lift_cm, 2.0);
    EXPECT_NEAR(far.windage_cm - plain.rows.back().windage_cm, far.coriolis_drift_cm, 1e-6);
    EXPECT_NEAR(far.lead_cm, 300.0 * far.time_s, 1e-6);
    s.azimuth_deg = 270.0;  // west: it sinks
    EXPECT_LT(
        BuildRangeTable(p, s, AngleUnit::kMrad, 0.0, 1000.0, 200.0).rows.back().coriolis_lift_cm,
        -2.0);
}

TEST_F(AppLogic, HitProbabilityFallsWithRange) {
    const storage::LoadedProfile p = storage::LoadProfile(db_, SamplePair(db_)).value();
    EXPECT_DOUBLE_EQ(LoadWezSettings(db_).value().group_moa, 1.0);  // defaults
    WezSettings w;
    w.target_kind = "figure";
    w.target_width_cm = 45;
    w.target_height_cm = 100;
    ASSERT_TRUE(SaveWezSettings(db_, w).ok());
    EXPECT_EQ(LoadWezSettings(db_).value().target_kind, "figure");

    SessionConditions s = AtZero(600.0);
    s.winds = {{3.0, 90.0, 0.0}};
    const WezResult r = ComputeWez(p, s, w, 1200.0, 100.0);
    ASSERT_TRUE(r.ok) << r.error;
    ASSERT_EQ(r.rows.size(), 12u);
    EXPECT_GT(r.rows.front().probability, 0.99);  // 100 m: a sure hit
    for (std::size_t i = 1; i < r.rows.size(); ++i) {
        EXPECT_LE(r.rows[i].probability, r.rows[i - 1].probability + 1e-9) << i;
        EXPECT_GT(r.rows[i].sigma_right_cm, r.rows[i - 1].sigma_right_cm);
    }
    EXPECT_LT(r.rows.back().probability, 0.6);
    EXPECT_NEAR(r.at_target.probability, r.rows[5].probability, 1e-12);  // 600 m
    ASSERT_FALSE(r.parts.empty());
    EXPECT_LE(r.shots_50, r.shots_80);
    EXPECT_LE(r.shots_80, r.shots_95);
    EXPECT_GE(r.shots_50, 1);

    // Better known wind: better odds.
    WezSettings calm = w;
    calm.wind_speed_mps = 0.2;
    EXPECT_GT(ComputeWez(p, s, calm, 1200.0, 100.0).rows.back().probability,
              r.rows.back().probability + 0.05);
    EXPECT_FALSE(ComputeWez(p, s, w, 1200.0, 0.0).ok);
}

}  // namespace
}  // namespace ballistics::applogic
