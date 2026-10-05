#include <ballistics/applogic/library.h>
#include <ballistics/applogic/profile_form.h>
#include <ballistics/applogic/profile_io.h>
#include <ballistics/applogic/session.h>
#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>
#include <ballistics/units.h>

#include <gtest/gtest.h>

namespace ballistics::applogic {
namespace {

using storage::BulletRecord;
using storage::Repository;

class AppLibrary : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_TRUE(db_.Open(":memory:").ok()); }

    Id AddLibraryBullet(const std::string& name, double bc) {
        BulletForm b;
        b.name = name;
        b.manufacturer = "Sierra";
        b.caliber = ".308";
        b.mass_gr = 175.0;
        b.diameter_in = 0.308;
        b.length_in = 1.24;
        b.drag_table = "G7";
        b.bc = bc;
        auto id = SaveBulletForm(db_, b);
        EXPECT_TRUE(id.ok()) << id.error().message;
        return id.value();
    }

    ProfileForm Form() {
        ProfileForm f;
        f.name = "AX308";
        f.caliber = ".308";
        f.sight_height_cm = 5.0;
        f.twist_in = 10.0;
        f.bullet_name = "own";
        f.bc = 0.25;
        f.mass_gr = 168.0;
        f.diameter_in = 0.308;
        f.length_in = 1.2;
        f.muzzle_velocity_mps = 800.0;
        return f;
    }

    storage::Database db_;
};

TEST_F(AppLibrary, BandedBulletRoundTripAndSearch) {
    BulletForm b;
    b.name = "MatchKing 175 HPBT";
    b.manufacturer = "Sierra";
    b.caliber = ".308";
    b.mass_gr = 175.0;
    b.diameter_in = 0.308;
    b.drag_table = "G1";
    b.bands = {{457.0, 0.485}, {869.0, 0.505}, {701.0, 0.496}};
    const Id id = SaveBulletForm(db_, b).value();
    const BulletForm g = LoadBulletForm(db_, id).value();
    ASSERT_EQ(g.bands.size(), 3U);
    EXPECT_DOUBLE_EQ(g.bands[0].velocity_mps, 869.0); // fastest first
    EXPECT_DOUBLE_EQ(g.bc, 0.0);

    AddLibraryBullet("ELD-M 178", 0.275);
    EXPECT_EQ(ListLibraryBullets(db_).value().size(), 2U);
    const auto hits = ListLibraryBullets(db_, "matchk").value();
    ASSERT_EQ(hits.size(), 1U);
    EXPECT_EQ(hits[0].bc_bands, 3);
    EXPECT_DOUBLE_EQ(hits[0].bc, 0.505);
    EXPECT_EQ(ListLibraryBullets(db_, "SIERRA").value().size(), 2U);

    // Back to a single BC.
    BulletForm single = g;
    single.bands.clear();
    single.bc = 0.243;
    single.drag_table = "G7";
    ASSERT_TRUE(SaveBulletForm(db_, single).ok());
    const auto rec = *Repository<BulletRecord>(db_).Get(id).value();
    EXPECT_EQ(rec.drag_kind, storage::kDragKindBc);
    EXPECT_TRUE(rec.bc_bands.empty());
}

TEST_F(AppLibrary, ValidationMessages) {
    BulletForm b;
    EXPECT_NE(Validate(b).find("name"), std::string::npos);
    b.name = "x";
    b.mass_gr = 100;
    b.diameter_in = 0.308;
    b.bands = {{800.0, 0.0}};
    EXPECT_NE(Validate(b).find("band"), std::string::npos);
}

TEST_F(AppLibrary, ProfilesHideTheirOwnBulletsFromTheLibrary) {
    ASSERT_TRUE(SaveProfileForm(db_, Form()).ok());
    EXPECT_TRUE(ListLibraryBullets(db_).value().empty());
}

TEST_F(AppLibrary, ProfileUsesLibraryBulletWithoutChangingIt) {
    const Id lib = AddLibraryBullet("SMK 175", 0.243);
    ProfileForm f = WithLibraryBullet(db_, Form(), lib).value();
    EXPECT_EQ(f.library_bullet_id, lib);
    EXPECT_DOUBLE_EQ(f.bc, 0.243);
    EXPECT_NEAR(f.mass_gr, 175.0, 1e-9);
    f.bc = 0.9; // ignored: library bullets are not edited from a profile
    const Id pid = SaveProfileForm(db_, f).value();

    const auto loaded = LoadProfileForm(db_, pid).value();
    EXPECT_EQ(loaded.library_bullet_id, lib);
    EXPECT_DOUBLE_EQ(Repository<BulletRecord>(db_).Get(lib).value()->bc.value(), 0.243);
    EXPECT_EQ(Repository<BulletRecord>(db_).List().value().size(), 1U);

    // Library bullets survive profile deletion and refuse deletion in use.
    const Id pid2 = SaveProfileForm(db_, WithLibraryBullet(db_, Form(), lib).value()).value();
    EXPECT_FALSE(DeleteBullet(db_, lib).ok());
    ASSERT_TRUE(DeleteProfile(db_, pid).ok());
    ASSERT_TRUE(DeleteProfile(db_, pid2).ok());
    EXPECT_TRUE(Repository<BulletRecord>(db_).Get(lib).value().has_value());
    EXPECT_TRUE(DeleteBullet(db_, lib).ok());
}

TEST_F(AppLibrary, SwitchingFromOwnToLibraryBulletDropsTheOwnOne) {
    const Id pid = SaveProfileForm(db_, Form()).value();
    EXPECT_EQ(Repository<BulletRecord>(db_).List().value().size(), 1U);
    const Id lib = AddLibraryBullet("SMK 175", 0.243);
    ProfileForm f = LoadProfileForm(db_, pid).value();
    f = WithLibraryBullet(db_, f, lib).value();
    ASSERT_TRUE(SaveProfileForm(db_, f).ok());
    const auto bullets = Repository<BulletRecord>(db_).List().value();
    ASSERT_EQ(bullets.size(), 1U);
    EXPECT_EQ(bullets[0].id, lib);

    // Detaching makes a private copy and leaves the library bullet alone.
    f = LoadProfileForm(db_, pid).value();
    f.library_bullet_id = 0;
    f.bc = 0.26;
    ASSERT_TRUE(SaveProfileForm(db_, f).ok());
    EXPECT_EQ(Repository<BulletRecord>(db_).List().value().size(), 2U);
    EXPECT_DOUBLE_EQ(Repository<BulletRecord>(db_).Get(lib).value()->bc.value(), 0.243);
}

TEST_F(AppLibrary, JsonRoundTripGivesTheSameSolution) {
    ProfileForm f = Form();
    f.zero_range_m = 200.0;
    f.zero_temperature_c = -3.0;
    const Id pid = SaveProfileForm(db_, f).value();
    const std::string json = ExportProfileJson(db_, pid).value();
    EXPECT_NE(json.find("\"format\": \"balcalc-profile\""), std::string::npos);

    const Id copy = ImportProfileJson(db_, json).value();
    EXPECT_NE(copy, pid);
    const auto a = LoadProfileForm(db_, pid).value();
    const auto b = LoadProfileForm(db_, copy).value();
    EXPECT_EQ(b.name, "AX308 (2)");
    EXPECT_DOUBLE_EQ(b.zero_range_m, 200.0);
    EXPECT_NEAR(b.zero_temperature_c, -3.0, 1e-9);

    SessionConditions s;
    s.target_range_m = 900.0;
    const auto sa = Summarize(storage::LoadProfile(db_, pid).value(), s, AngleUnit::kMrad);
    const auto sb = Summarize(storage::LoadProfile(db_, copy).value(), s, AngleUnit::kMrad);
    ASSERT_TRUE(sa.ok && sb.ok);
    EXPECT_DOUBLE_EQ(sa.elevation, sb.elevation);
    EXPECT_DOUBLE_EQ(sa.windage, sb.windage);
    EXPECT_EQ(a.click_units, b.click_units);
}

TEST_F(AppLibrary, ImportReusesIdenticalLibraryBullet) {
    const Id lib = AddLibraryBullet("SMK 175", 0.243);
    const Id pid = SaveProfileForm(db_, WithLibraryBullet(db_, Form(), lib).value()).value();
    const std::string json = ExportProfileJson(db_, pid).value();
    ASSERT_TRUE(ImportProfileJson(db_, json).ok());
    EXPECT_EQ(Repository<BulletRecord>(db_).List().value().size(), 1U);
}

TEST_F(AppLibrary, ImportRejectsGarbageAndRollsBack) {
    EXPECT_FALSE(ImportProfileJson(db_, "not json").ok());
    EXPECT_FALSE(ImportProfileJson(db_, R"({"format":"other"})").ok());
    EXPECT_FALSE(ImportProfileJson(db_, R"({"format":"balcalc-profile","version":99})").ok());
    // Valid header, bullet fine, cartridge missing: nothing is left behind.
    const auto r = ImportProfileJson(db_, R"({"format":"balcalc-profile","version":1,
        "bullet":{"name":"b","diameter_m":0.0078,"mass_kg":0.011,"drag_kind":"bc","bc":0.3}})");
    EXPECT_FALSE(r.ok());
    EXPECT_TRUE(Repository<BulletRecord>(db_).List().value().empty());
}

} // namespace
} // namespace ballistics::applogic
