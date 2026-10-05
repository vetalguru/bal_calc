#include <ballistics/applogic/library.h>
#include <ballistics/applogic/armory.h>
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

    CartridgeForm Cartridge() {
        CartridgeForm f;
        f.name = "AX308 load";
        f.caliber = ".308";
        f.bullet_name = "own";
        f.bc = 0.25;
        f.mass_gr = 168.0;
        f.diameter_in = 0.308;
        f.length_in = 1.2;
        f.muzzle_velocity_mps = 800.0;
        return f;
    }

    Id Rifle(double zero_range_m = 100.0, double zero_temperature_c = 15.0) {
        RifleForm f;
        f.name = "AX308";
        f.caliber = ".308";
        f.sight_height_cm = 5.0;
        f.twist_in = 10.0;
        f.zero_range_m = zero_range_m;
        f.zero_temperature_c = zero_temperature_c;
        return SaveRifleForm(db_, f).value();
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

TEST_F(AppLibrary, CartridgesHideTheirOwnBulletsFromTheLibrary) {
    ASSERT_TRUE(SaveCartridgeForm(db_, Cartridge()).ok());
    EXPECT_TRUE(ListLibraryBullets(db_).value().empty());
}

TEST_F(AppLibrary, CartridgeUsesLibraryBulletWithoutChangingIt) {
    const Id lib = AddLibraryBullet("SMK 175", 0.243);
    CartridgeForm f = WithLibraryBullet(db_, Cartridge(), lib).value();
    EXPECT_EQ(f.library_bullet_id, lib);
    EXPECT_DOUBLE_EQ(f.bc, 0.243);
    EXPECT_NEAR(f.mass_gr, 175.0, 1e-9);
    f.bc = 0.9; // ignored: library bullets are not edited from a cartridge
    const Id cid = SaveCartridgeForm(db_, f).value();

    EXPECT_EQ(LoadCartridgeForm(db_, cid).value().library_bullet_id, lib);
    EXPECT_DOUBLE_EQ(Repository<BulletRecord>(db_).Get(lib).value()->bc.value(), 0.243);
    EXPECT_EQ(Repository<BulletRecord>(db_).List().value().size(), 1U);

    // Library bullets survive cartridge deletion and refuse deletion in use.
    CartridgeForm g = WithLibraryBullet(db_, Cartridge(), lib).value();
    g.name = "second";
    const Id cid2 = SaveCartridgeForm(db_, g).value();
    EXPECT_FALSE(DeleteBullet(db_, lib).ok());
    ASSERT_TRUE(DeleteCartridge(db_, cid).ok());
    ASSERT_TRUE(DeleteCartridge(db_, cid2).ok());
    EXPECT_TRUE(Repository<BulletRecord>(db_).Get(lib).value().has_value());
    EXPECT_TRUE(DeleteBullet(db_, lib).ok());
}

TEST_F(AppLibrary, SwitchingFromOwnToLibraryBulletDropsTheOwnOne) {
    const Id cid = SaveCartridgeForm(db_, Cartridge()).value();
    EXPECT_EQ(Repository<BulletRecord>(db_).List().value().size(), 1U);
    const Id lib = AddLibraryBullet("SMK 175", 0.243);
    CartridgeForm f = LoadCartridgeForm(db_, cid).value();
    f = WithLibraryBullet(db_, f, lib).value();
    ASSERT_TRUE(SaveCartridgeForm(db_, f).ok());
    const auto bullets = Repository<BulletRecord>(db_).List().value();
    ASSERT_EQ(bullets.size(), 1U);
    EXPECT_EQ(bullets[0].id, lib);

    // Detaching makes a private copy and leaves the library bullet alone.
    f = LoadCartridgeForm(db_, cid).value();
    f.library_bullet_id = 0;
    f.bc = 0.26;
    ASSERT_TRUE(SaveCartridgeForm(db_, f).ok());
    EXPECT_EQ(Repository<BulletRecord>(db_).List().value().size(), 2U);
    EXPECT_DOUBLE_EQ(Repository<BulletRecord>(db_).Get(lib).value()->bc.value(), 0.243);
}

TEST_F(AppLibrary, SampleProfileUsesTheLibrarySmkWhenPresent) {
    // Empty library: the sample brings its own bullet.
    const Id own = CreateSampleProfile(db_, "Rifle A", "Load A").value();
    const auto own_p = storage::LoadProfile(db_, own).value();
    EXPECT_EQ(LoadCartridgeForm(db_, own_p.cartridge.id).value().library_bullet_id, 0);

    BulletForm smk;
    smk.name = "MatchKing 175 gr HPBT #2275";
    smk.mass_gr = 175;
    smk.diameter_in = 0.308;
    smk.drag_table = "G1";
    smk.bands = {{869.0, 0.505}, {549.0, 0.496}};
    const Id lib = SaveBulletForm(db_, smk).value();
    const Id with_lib = CreateSampleProfile(db_, "Rifle B", "Load B").value();
    const auto p = storage::LoadProfile(db_, with_lib).value();
    EXPECT_EQ(p.bullet.id, lib);
    SessionConditions s;
    s.target_range_m = 1000.0;
    EXPECT_TRUE(Summarize(p, s, AngleUnit::kMrad).ok);
}

TEST_F(AppLibrary, RifleAndCartridgeJsonRoundTripGivesTheSameSolution) {
    const Id rifle = Rifle(200.0, -3.0);
    const Id cartridge = SaveCartridgeForm(db_, Cartridge()).value();
    const std::string rifle_json = ExportRifleJson(db_, rifle).value();
    const std::string cart_json = ExportCartridgeJson(db_, cartridge).value();
    EXPECT_NE(rifle_json.find("\"format\": \"balcalc-rifle\""), std::string::npos);
    EXPECT_NE(cart_json.find("\"format\": \"balcalc-cartridge\""), std::string::npos);

    const Imported r = ImportShareJson(db_, rifle_json).value();
    const Imported c = ImportShareJson(db_, cart_json).value();
    EXPECT_NE(r.rifle_id, 0);
    EXPECT_EQ(r.cartridge_id, 0);
    EXPECT_EQ(c.rifle_id, 0);
    EXPECT_NE(c.cartridge_id, 0);
    EXPECT_EQ(r.profile_id, 0);
    const RifleForm copy = LoadRifleForm(db_, r.rifle_id).value();
    EXPECT_EQ(copy.name, "AX308 (2)");
    EXPECT_DOUBLE_EQ(copy.zero_range_m, 200.0);
    EXPECT_NEAR(copy.zero_temperature_c, -3.0, 1e-9);
    EXPECT_EQ(LoadCartridgeForm(db_, c.cartridge_id).value().name, "AX308 load (2)");

    SessionConditions s;
    s.target_range_m = 900.0;
    const auto sa = Summarize(
        storage::LoadProfile(db_, EnsureProfile(db_, rifle, cartridge).value()).value(), s,
        AngleUnit::kMrad);
    const auto sb = Summarize(
        storage::LoadProfile(db_, EnsureProfile(db_, r.rifle_id, c.cartridge_id).value()).value(),
        s, AngleUnit::kMrad);
    ASSERT_TRUE(sa.ok && sb.ok);
    EXPECT_DOUBLE_EQ(sa.elevation, sb.elevation);
    EXPECT_DOUBLE_EQ(sa.windage, sb.windage);
}

TEST_F(AppLibrary, LegacyProfileFileImportsAsRifleCartridgeAndPair) {
    // A file exported by version 0.1 (one "profile" per rifle + cartridge).
    const Imported im = ImportShareJson(db_, R"({
      "format": "balcalc-profile", "version": 1,
      "profile": {"name": "Old", "zero_range_m": 300.0, "zero_offset_up_m": 0.01,
                  "zero_offset_right_m": 0.0, "zero_powder_temp_k": 280.0,
                  "zero_atmosphere": {"altitude_m": 100.0, "pressure_pa": 98000.0,
                                      "temperature_k": 280.0, "humidity": 0.5},
                  "velocity_scale": 1.02, "drag_scale": 0.97},
      "rifle": {"name": "Old rifle", "caliber": ".308 Win", "sight_height_m": 0.05,
                "twist_m": 0.254},
      "scope": {"name": "s", "click_units": "mrad", "click_vertical_rad": 0.0001,
                "click_horizontal_rad": 0.0001, "reticle": null},
      "cartridge": {"name": "Old load", "muzzle_velocity_mps": 800.0},
      "bullet": {"name": "b", "caliber": ".308", "diameter_m": 0.0078, "mass_kg": 0.0113,
                 "drag_kind": "bc", "drag_table": "G7", "bc": 0.25}})")
                              .value();
    ASSERT_NE(im.profile_id, 0);
    const auto p = storage::LoadProfile(db_, im.profile_id).value();
    EXPECT_EQ(p.rifle.name, "Old rifle");
    EXPECT_DOUBLE_EQ(p.rifle.zero_range_m, 300.0);
    EXPECT_DOUBLE_EQ(p.rifle.zero_powder_temp_k, 280.0);
    EXPECT_DOUBLE_EQ(p.rifle.zero_atmosphere.pressure_pa, 98000.0);
    ASSERT_TRUE(p.scope.has_value());
    EXPECT_EQ(p.cartridge.caliber, ".308 Win"); // taken from the rifle
    EXPECT_DOUBLE_EQ(p.profile.zero_offset_up_m, 0.01);
    EXPECT_DOUBLE_EQ(p.profile.velocity_scale, 1.02);
    EXPECT_DOUBLE_EQ(p.profile.drag_scale, 0.97);
}

TEST_F(AppLibrary, ImportReusesIdenticalLibraryBullet) {
    const Id lib = AddLibraryBullet("SMK 175", 0.243);
    const Id cid = SaveCartridgeForm(db_, WithLibraryBullet(db_, Cartridge(), lib).value()).value();
    ASSERT_TRUE(ImportShareJson(db_, ExportCartridgeJson(db_, cid).value()).ok());
    EXPECT_EQ(Repository<BulletRecord>(db_).List().value().size(), 1U);
}

TEST_F(AppLibrary, ImportRejectsGarbageAndRollsBack) {
    EXPECT_FALSE(ImportShareJson(db_, "not json").ok());
    EXPECT_FALSE(ImportShareJson(db_, R"({"format":"other"})").ok());
    EXPECT_FALSE(ImportShareJson(db_, R"({"format":"balcalc-rifle","version":99})").ok());
    // Valid header, bullet fine, cartridge missing: nothing is left behind.
    const auto r = ImportShareJson(db_, R"({"format":"balcalc-cartridge","version":1,
        "bullet":{"name":"b","diameter_m":0.0078,"mass_kg":0.011,"drag_kind":"bc","bc":0.3}})");
    EXPECT_FALSE(r.ok());
    EXPECT_TRUE(Repository<BulletRecord>(db_).List().value().empty());
}

} // namespace
} // namespace ballistics::applogic
