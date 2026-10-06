#include <ballistics/applogic/armory.h>
#include <ballistics/applogic/session.h>
#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>
#include <ballistics/units.h>

#include <gtest/gtest.h>

#include <cmath>

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
    EXPECT_EQ(Count<storage::ScopeRecord>(db_), 1U); // the same scope updated
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
    EXPECT_EQ(f.library_bullet_id, 0); // its own bullet
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
    EXPECT_EQ(EnsureProfile(db_, tikka, cartridge).value(), a); // found, not duplicated
    EXPECT_EQ(Count<storage::ProfileRecord>(db_), 2U);
    EXPECT_FALSE(EnsureProfile(db_, tikka, 999).ok());

    // Each rifle zeroes at its own range.
    SessionConditions s;
    s.temperature_c = 10.0;
    s.pressure_hpa = 990.0;
    s.target_range_m = 300.0;
    s.powder_c = 15.0; // the rifles' zero_powder_c
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
    EXPECT_EQ(Count<storage::CartridgeRecord>(db_), 1U); // the cartridge stays
    EXPECT_FALSE(DeleteRifle(db_, p.rifle_id).ok());

    const Id rifle = SaveRifleForm(db_, SampleRifle()).value();
    ASSERT_TRUE(EnsureProfile(db_, rifle, p.cartridge_id).ok());
    ASSERT_TRUE(DeleteCartridge(db_, p.cartridge_id).ok());
    EXPECT_EQ(Count<storage::ProfileRecord>(db_), 0U);
    EXPECT_EQ(Count<storage::CartridgeRecord>(db_), 0U);
    EXPECT_EQ(Count<storage::BulletRecord>(db_), 0U); // its own bullet went too
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
    EXPECT_EQ(all[0].name, "A .300 WM load"); // by name
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

    EXPECT_TRUE(ListCartridges(db_).value().empty()); // not the user's
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
    EXPECT_EQ(copy.bullet_id, b.id);          // the library bullet, shared
    EXPECT_DOUBLE_EQ(copy.barrel_length_m, 0.61); // kept from the original
    EXPECT_EQ(ListCartridges(db_).value().size(), 1U);
    EXPECT_EQ(ListLibraryCartridges(db_).value().size(), 1U); // original untouched
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
    const Id id = SamplePair(db_);
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
    const Id id = SamplePair(db_);
    const storage::LoadedProfile p = storage::LoadProfile(db_, id).value();
    EXPECT_FALSE(BuildRangeTable(p, {}, AngleUnit::kMrad, 0.0, 1000.0, 0.0).ok);
    EXPECT_FALSE(BuildRangeTable(p, {}, AngleUnit::kMrad, 500.0, 100.0, 50.0).ok);
    EXPECT_FALSE(BuildRangeTable(p, {}, AngleUnit::kMrad, 0.0, 3000.0, 1.0).ok); // > 2000 rows
}

} // namespace
} // namespace ballistics::applogic
