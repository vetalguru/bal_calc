#include <ballistics/applogic/armory.h>
#include <ballistics/applogic/library.h>
#include <ballistics/applogic/reticle.h>
#include <ballistics/storage/repository.h>
#include <ballistics/units.h>
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>

// Edge cases and failure paths of the armory and bullet library: forms
// rejected field by field, ids that do not exist, rows that went missing
// behind a reference, and storage failures (forced with triggers, renamed
// tables, an open transaction or a closed database) that must surface as
// errors and leave nothing half-saved.
namespace ballistics::applogic {
namespace {

using sqlite_manager::ErrorCode;
using storage::BulletRecord;
using storage::CartridgeRecord;
using storage::ProfileRecord;
using storage::Repository;
using storage::RifleRecord;
using storage::ScopeRecord;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

RifleForm Rifle() {
    RifleForm f;
    f.name = "Tikka T3x";
    f.caliber = ".308 Win";
    f.sight_height_cm = 4.5;
    f.twist_in = 11.0;
    f.click_units = kClickMrad;
    f.click_value = 0.1;
    f.zero_range_m = 100.0;
    return f;
}

CartridgeForm Cartridge() {
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
    return f;
}

BulletForm Bullet() {
    BulletForm b;
    b.name = "ELD-M 178";
    b.manufacturer = "Hornady";
    b.caliber = ".308";
    b.mass_gr = 178.0;
    b.diameter_in = 0.308;
    b.length_in = 1.35;
    b.drag_table = "G7";
    b.bc = 0.275;
    return b;
}

// Whether `problem` mentions `words`.
bool Mentions(const std::string& problem, const std::string& words) {
    return problem.find(words) != std::string::npos;
}

class AppArmory : public ::testing::Test {
   protected:
    void SetUp() override { ASSERT_TRUE(db_.Open(":memory:").ok()); }

    void Exec(const std::string& sql) {
        const Status s = db_.connection().Execute(sql);
        ASSERT_TRUE(s.ok()) << sql << ": " << s.error().message;
    }

    // Deletes rows behind the database's back, ignoring foreign keys.
    void ExecUnchecked(const std::string& sql) {
        Exec("PRAGMA foreign_keys = OFF");
        Exec(sql);
        Exec("PRAGMA foreign_keys = ON");
    }

    // Makes every `when` (e.g. "INSERT", "DELETE") on `table` fail.
    void Refuse(const std::string& when, const std::string& table) {
        Exec("CREATE TRIGGER refuse_" + table + "_" + when + " BEFORE " + when + " ON " + table +
             " BEGIN SELECT RAISE(ABORT, 'refused'); END");
    }

    template <typename T>
    std::size_t Count() {
        return Repository<T>(db_).List().value().size();
    }

    Id Pair() {
        const Id rifle = SaveRifleForm(db_, Rifle()).value();
        const Id cartridge = SaveCartridgeForm(db_, Cartridge()).value();
        return EnsureProfile(db_, rifle, cartridge).value();
    }

    ProfileRecord Profile(Id id) { return *Repository<ProfileRecord>(db_).Get(id).value(); }

    Id LibraryBullet(BulletForm b = Bullet()) { return SaveBulletForm(db_, b).value(); }

    storage::Database db_;
};

// ---- Clicks and calibres --------------------------------------------------

TEST(AppArmoryClicks, NonPositiveOrUnknownClicksAreZero) {
    EXPECT_EQ(ClickToRad(kClickMrad, 0.0), 0.0);
    EXPECT_EQ(ClickToRad(kClickMrad, -0.1), 0.0);
    EXPECT_EQ(ClickToRad(kClickMoa, kNaN), 0.0);
    EXPECT_EQ(ClickToRad("", 1.0), 0.0);
    // Without a known click there is nothing to count in.
    EXPECT_EQ(RadToClick("furlong", 0.01), 0.0);
    EXPECT_NEAR(RadToClick(kClickCm100m, 0.001), 10.0, 1e-12);  // 10 cm at 100 m
    EXPECT_NEAR(RadToClick(kClickSmoa, ClickToRad(kClickSmoa, 0.5)), 0.5, 1e-12);
}

TEST(AppArmoryCaliber, CommasPrefixesAndTrailingDots) {
    EXPECT_TRUE(SameCaliber("6,5x55", "6.5x55 SE"));   // European decimal comma
    EXPECT_TRUE(SameCaliber("Cal. 308", ".308 Win"));  // a dot before the number
    EXPECT_TRUE(SameCaliber("308. Win", "308 Win"));   // a trailing dot is dropped
    EXPECT_TRUE(SameCaliber("7.62x39", "7.62 NATO"));  // the number ends at 'x'
    EXPECT_FALSE(SameCaliber("Magnum", "Magnum"));     // no number at all
    EXPECT_FALSE(SameCaliber("7.62x39", "7.65 Browning"));
}

// ---- Rifles ---------------------------------------------------------------

TEST(AppArmoryRifleForm, EachProblemIsNamed) {
    RifleForm r = Rifle();
    r.twist_in = -1.0;
    EXPECT_TRUE(Mentions(Validate(r), "twist"));
    r = Rifle();
    r.twist_in = 0.0;  // unknown twist is allowed
    EXPECT_TRUE(Validate(r).empty());

    r = Rifle();
    r.sight_height_cm = 30.0;
    EXPECT_TRUE(Mentions(Validate(r), "Sight height"));
    r.sight_height_cm = -0.1;
    EXPECT_TRUE(Mentions(Validate(r), "Sight height"));
    r.sight_height_cm = 0.0;
    EXPECT_TRUE(Validate(r).empty());

    r = Rifle();
    r.zero_range_m = 1000.0;
    EXPECT_TRUE(Validate(r).empty());
    r.zero_range_m = 1000.5;
    EXPECT_TRUE(Mentions(Validate(r), "Zero range"));
    r.zero_range_m = 10.0;
    EXPECT_TRUE(Validate(r).empty());

    r = Rifle();
    r.click_units = "furlong";
    EXPECT_TRUE(Mentions(Validate(r), "click"));

    r = Rifle();
    r.min_magnification = -1.0;
    EXPECT_TRUE(Mentions(Validate(r), "magnification"));
    r = Rifle();
    r.max_magnification = -1.0;
    EXPECT_TRUE(Mentions(Validate(r), "magnification"));
    r = Rifle();
    r.min_magnification = 25.0;
    r.max_magnification = 5.0;
    EXPECT_TRUE(Mentions(Validate(r), "magnification"));
    r.max_magnification = 0.0;  // unknown top end: any minimum goes
    EXPECT_TRUE(Validate(r).empty());

    r = Rifle();
    r.zero_pressure_hpa = 300.0;
    EXPECT_TRUE(Mentions(Validate(r), "pressure"));
    r.zero_pressure_hpa = 1200.0;
    EXPECT_TRUE(Mentions(Validate(r), "pressure"));
    r.zero_pressure_hpa = kNaN;
    EXPECT_TRUE(Mentions(Validate(r), "pressure"));

    r = Rifle();
    r.zero_humidity_pct = -1.0;
    EXPECT_TRUE(Mentions(Validate(r), "Humidity"));
    r.zero_humidity_pct = 100.5;
    EXPECT_TRUE(Mentions(Validate(r), "Humidity"));
    r.zero_humidity_pct = 100.0;
    EXPECT_TRUE(Validate(r).empty());
}

TEST_F(AppArmory, InvalidRifleIsRefusedAsAConstraint) {
    RifleForm r = Rifle();
    r.name.clear();
    auto id = SaveRifleForm(db_, r);
    ASSERT_FALSE(id.ok());
    EXPECT_EQ(id.error().code, ErrorCode::kConstraint);
    EXPECT_EQ(id.error().message, "Enter a rifle name.");
}

TEST_F(AppArmory, ScopeDetailsRoundTrip) {
    RifleForm r = Rifle();
    r.click_units = kClickCm100m;
    r.click_value = 1.0;
    r.focal_plane = "sfp";
    r.sfp_reference_magnification = 12.0;
    r.min_magnification = 3.0;
    r.max_magnification = 18.0;
    r.zero_altitude_m = 450.0;
    r.zero_humidity_pct = 70.0;
    r.zero_powder_c = 25.0;
    const Id id = SaveRifleForm(db_, r).value();
    const RifleForm g = LoadRifleForm(db_, id).value();
    EXPECT_EQ(g.click_units, kClickCm100m);
    EXPECT_NEAR(g.click_value, 1.0, 1e-12);
    EXPECT_EQ(g.focal_plane, "sfp");
    EXPECT_DOUBLE_EQ(g.sfp_reference_magnification, 12.0);
    EXPECT_DOUBLE_EQ(g.min_magnification, 3.0);
    EXPECT_DOUBLE_EQ(g.max_magnification, 18.0);
    EXPECT_EQ(g.reticle_id, 0);
    EXPECT_DOUBLE_EQ(g.zero_altitude_m, 450.0);
    EXPECT_NEAR(g.zero_humidity_pct, 70.0, 1e-9);
    EXPECT_NEAR(g.zero_powder_c, 25.0, 1e-9);
    EXPECT_NEAR(g.sight_height_cm, 4.5, 1e-12);

    // Anything but "sfp" is stored as a first focal plane scope.
    RifleForm odd = g;
    odd.focal_plane = "middle";
    ASSERT_TRUE(SaveRifleForm(db_, odd).ok());
    EXPECT_EQ(LoadRifleForm(db_, id).value().focal_plane, "ffp");
}

TEST_F(AppArmory, ListRiflesGivesNamesAndCalibres) {
    EXPECT_TRUE(ListRifles(db_).value().empty());
    const Id id = SaveRifleForm(db_, Rifle()).value();
    const auto rifles = ListRifles(db_).value();
    ASSERT_EQ(rifles.size(), 1U);
    EXPECT_EQ(rifles[0].id, id);
    EXPECT_EQ(rifles[0].name, "Tikka T3x");
    EXPECT_EQ(rifles[0].caliber, ".308 Win");
}

TEST_F(AppArmory, UnknownRifleIsNotFound) {
    auto load = LoadRifleForm(db_, 42);
    ASSERT_FALSE(load.ok());
    EXPECT_EQ(load.error().code, ErrorCode::kNotFound);
    EXPECT_EQ(load.error().message, "rifle not found");

    RifleForm r = Rifle();
    r.rifle_id = 42;
    auto save = SaveRifleForm(db_, r);
    ASSERT_FALSE(save.ok());
    EXPECT_EQ(save.error().code, ErrorCode::kNotFound);
    EXPECT_EQ(Count<ScopeRecord>(), 0U);  // nothing created instead

    EXPECT_EQ(DeleteRifle(db_, 42).error().code, ErrorCode::kNotFound);
}

TEST_F(AppArmory, RifleWithAMissingScopeIsReported) {
    const Id id = SaveRifleForm(db_, Rifle()).value();
    ExecUnchecked("DELETE FROM scope");

    auto load = LoadRifleForm(db_, id);
    ASSERT_FALSE(load.ok());
    EXPECT_EQ(load.error().code, ErrorCode::kNotFound);
    EXPECT_EQ(load.error().message, "scope not found");

    RifleForm r = Rifle();
    r.rifle_id = id;
    r.name = "renamed";
    auto save = SaveRifleForm(db_, r);
    ASSERT_FALSE(save.ok());
    EXPECT_EQ(save.error().message, "scope not found");
    EXPECT_EQ(Repository<RifleRecord>(db_).Get(id).value()->name, "Tikka T3x");  // untouched
}

TEST_F(AppArmory, RifleWithAnUnknownReticleIsNotSaved) {
    RifleForm r = Rifle();
    r.reticle_id = 999;  // no such reticle: the scope's reference fails
    auto id = SaveRifleForm(db_, r);
    ASSERT_FALSE(id.ok());
    EXPECT_EQ(id.error().code, ErrorCode::kConstraint);
    EXPECT_EQ(Count<ScopeRecord>(), 0U);
    EXPECT_EQ(Count<RifleRecord>(), 0U);
}

TEST_F(AppArmory, RefusedRifleRowRollsBackItsScope) {
    Refuse("INSERT", "rifle");
    auto id = SaveRifleForm(db_, Rifle());
    ASSERT_FALSE(id.ok());
    EXPECT_EQ(id.error().code, ErrorCode::kConstraint);
    EXPECT_EQ(Count<ScopeRecord>(), 0U);  // the scope saved first is rolled back
}

TEST_F(AppArmory, FailedRifleCommitLeavesNothing) {
    // A deferred foreign key only fails at COMMIT.
    Exec("PRAGMA defer_foreign_keys = ON");
    RifleForm r = Rifle();
    r.reticle_id = 999;
    auto id = SaveRifleForm(db_, r);
    ASSERT_FALSE(id.ok());
    EXPECT_EQ(id.error().code, ErrorCode::kConstraint);
    EXPECT_FALSE(db_.connection().InTransaction());
    EXPECT_EQ(Count<RifleRecord>(), 0U);
    EXPECT_EQ(Count<ScopeRecord>(), 0U);
}

TEST_F(AppArmory, RifleChangesNeedTheirOwnTransaction) {
    const Id pair = Pair();
    const Id rifle = Profile(pair).rifle_id;
    Exec("BEGIN");
    EXPECT_FALSE(SaveRifleForm(db_, Rifle()).ok());
    EXPECT_FALSE(DeleteRifle(db_, rifle).ok());
    Exec("ROLLBACK");
    EXPECT_EQ(Count<RifleRecord>(), 1U);
    EXPECT_EQ(Count<ProfileRecord>(), 1U);
}

TEST_F(AppArmory, FailedRifleDeletionKeepsEverything) {
    const Id pair = Pair();
    const Id rifle = Profile(pair).rifle_id;

    // The pair cannot go: the rifle stays.
    Refuse("DELETE", "profile");
    EXPECT_EQ(DeleteRifle(db_, rifle).error().code, ErrorCode::kConstraint);
    EXPECT_EQ(Count<RifleRecord>(), 1U);
    EXPECT_EQ(Count<ProfileRecord>(), 1U);
    Exec("DROP TRIGGER refuse_profile_DELETE");

    // The rifle cannot go: its pair comes back too.
    Refuse("DELETE", "rifle");
    EXPECT_EQ(DeleteRifle(db_, rifle).error().code, ErrorCode::kConstraint);
    EXPECT_EQ(Count<RifleRecord>(), 1U);
    EXPECT_EQ(Count<ProfileRecord>(), 1U);
    EXPECT_EQ(Count<ScopeRecord>(), 1U);
}

TEST_F(AppArmory, RifleWithoutPairsIsDeleted) {
    const Id rifle = SaveRifleForm(db_, Rifle()).value();
    ASSERT_TRUE(DeleteRifle(db_, rifle).ok());
    EXPECT_EQ(Count<RifleRecord>(), 0U);
    EXPECT_EQ(Count<ScopeRecord>(), 0U);
}

// ---- Cartridges -----------------------------------------------------------

TEST(AppArmoryCartridgeForm, EachProblemIsNamed) {
    CartridgeForm c = Cartridge();
    c.muzzle_velocity_mps = 50.0;
    EXPECT_TRUE(Mentions(Validate(c), "Muzzle velocity"));
    c.muzzle_velocity_mps = 2000.0;
    EXPECT_TRUE(Mentions(Validate(c), "Muzzle velocity"));
    c.muzzle_velocity_mps = kNaN;
    EXPECT_TRUE(Mentions(Validate(c), "Muzzle velocity"));

    c = Cartridge();
    c.bc = 2.0;
    EXPECT_TRUE(Mentions(Validate(c), "coefficient"));
    c = Cartridge();
    c.mass_gr = 0.0;
    EXPECT_EQ(Validate(c), "Enter the bullet weight.");
    c = Cartridge();
    c.diameter_in = 1.0;
    EXPECT_TRUE(Mentions(Validate(c), "diameter"));
    c.diameter_in = 0.0;
    EXPECT_TRUE(Mentions(Validate(c), "diameter"));
    c = Cartridge();
    c.length_in = -0.1;
    EXPECT_TRUE(Mentions(Validate(c), "length"));
    c.length_in = 0.0;  // unknown length is allowed
    EXPECT_TRUE(Validate(c).empty());

    // A library bullet brings its own BC, weight and diameter.
    c = Cartridge();
    c.library_bullet_id = 7;
    c.bc = 0.0;
    c.mass_gr = 0.0;
    c.diameter_in = 0.0;
    EXPECT_TRUE(Validate(c).empty());
    c.length_in = -1.0;  // but not a negative length
    EXPECT_TRUE(Mentions(Validate(c), "length"));
}

TEST_F(AppArmory, InvalidCartridgeIsRefusedAsAConstraint) {
    CartridgeForm c = Cartridge();
    c.mass_gr = -5.0;
    auto id = SaveCartridgeForm(db_, c);
    ASSERT_FALSE(id.ok());
    EXPECT_EQ(id.error().code, ErrorCode::kConstraint);
    EXPECT_EQ(id.error().message, "Enter the bullet weight.");
}

TEST_F(AppArmory, OwnBulletTakesTheCartridgeNameWhenUnnamed) {
    CartridgeForm c = Cartridge();
    c.bullet_name.clear();
    const Id id = SaveCartridgeForm(db_, c).value();
    EXPECT_EQ(LoadCartridgeForm(db_, id).value().bullet_name, "Handload SMK 175");
    EXPECT_EQ(ListCartridges(db_).value().at(0).bullet_name, "Handload SMK 175");
}

TEST_F(AppArmory, UnknownCartridgeOrBulletIsNotFound) {
    auto load = LoadCartridgeForm(db_, 42);
    ASSERT_FALSE(load.ok());
    EXPECT_EQ(load.error().code, ErrorCode::kNotFound);
    EXPECT_EQ(load.error().message, "cartridge not found");

    EXPECT_EQ(CartridgeFromLibrary(db_, 42).error().message, "cartridge not found");

    auto with = WithLibraryBullet(db_, Cartridge(), 42);
    ASSERT_FALSE(with.ok());
    EXPECT_EQ(with.error().code, ErrorCode::kNotFound);
    EXPECT_EQ(with.error().message, "bullet not found");

    CartridgeForm edit = Cartridge();
    edit.cartridge_id = 42;
    EXPECT_EQ(SaveCartridgeForm(db_, edit).error().message, "cartridge not found");
    CartridgeForm copy = Cartridge();
    copy.copy_of = 42;
    EXPECT_EQ(SaveCartridgeForm(db_, copy).error().message, "cartridge not found");
    CartridgeForm lib = Cartridge();
    lib.library_bullet_id = 42;
    EXPECT_EQ(SaveCartridgeForm(db_, lib).error().message, "bullet not found");
    EXPECT_EQ(Count<CartridgeRecord>(), 0U);
    EXPECT_EQ(Count<BulletRecord>(), 0U);

    EXPECT_EQ(DeleteCartridge(db_, 42).error().code, ErrorCode::kNotFound);
}

TEST_F(AppArmory, CartridgeWithAMissingBulletIsReported) {
    const Id id = SaveCartridgeForm(db_, Cartridge()).value();
    ExecUnchecked("DELETE FROM bullet");

    auto load = LoadCartridgeForm(db_, id);
    ASSERT_FALSE(load.ok());
    EXPECT_EQ(load.error().code, ErrorCode::kNotFound);
    EXPECT_EQ(load.error().message, "bullet not found");

    // The list still shows the cartridge, without a bullet name.
    const auto list = ListCartridges(db_).value();
    ASSERT_EQ(list.size(), 1U);
    EXPECT_EQ(list[0].name, "Handload SMK 175");
    EXPECT_TRUE(list[0].bullet_name.empty());

    CartridgeForm c = Cartridge();
    c.cartridge_id = id;
    EXPECT_EQ(SaveCartridgeForm(db_, c).error().message, "bullet not found");
}

TEST_F(AppArmory, CalibreComesFromTheBulletWhenNotGiven) {
    BulletForm b = Bullet();
    b.caliber = ".308 Win";
    const Id lib = LibraryBullet(b);

    CartridgeForm c = Cartridge();
    c.caliber.clear();
    const CartridgeForm with = WithLibraryBullet(db_, c, lib).value();
    EXPECT_EQ(with.caliber, ".308 Win");
    EXPECT_EQ(with.bullet_name, "ELD-M 178");
    EXPECT_NEAR(with.diameter_in, 0.308, 1e-12);
    EXPECT_NEAR(with.length_in, 1.35, 1e-12);

    // A calibre already typed is kept.
    c.caliber = "7.62x51";
    EXPECT_EQ(WithLibraryBullet(db_, c, lib).value().caliber, "7.62x51");

    // A stored cartridge without a calibre shows its bullet's.
    c.caliber.clear();
    const Id id = SaveCartridgeForm(db_, WithLibraryBullet(db_, c, lib).value()).value();
    ExecUnchecked("UPDATE cartridge SET caliber = '' WHERE id = " + std::to_string(id));
    EXPECT_EQ(LoadCartridgeForm(db_, id).value().caliber, ".308 Win");
}

TEST_F(AppArmory, BandedLibraryBulletShowsItsFastestBand) {
    BulletForm b = Bullet();
    b.bands = {{600.0, 0.48}, {850.0, 0.51}};
    const Id lib = LibraryBullet(b);
    const CartridgeForm f = WithLibraryBullet(db_, Cartridge(), lib).value();
    EXPECT_DOUBLE_EQ(f.bc, 0.51);
    const Id id = SaveCartridgeForm(db_, f).value();
    const CartridgeForm g = LoadCartridgeForm(db_, id).value();
    EXPECT_EQ(g.library_bullet_id, lib);
    EXPECT_DOUBLE_EQ(g.bc, 0.51);
}

TEST_F(AppArmory, OwnBandedBulletKeepsItsBandsWhenEdited) {
    // A cartridge's own bullet with velocity bands, as an import leaves it.
    BulletRecord b;
    b.name = "own banded";
    b.diameter_m = units::InchToM(0.308);
    b.mass_kg = units::GrainToKg(175.0);
    b.drag_kind = storage::kDragKindMultiBc;
    b.drag_table = "G1";
    b.bc_bands = {{850.0, 0.505}, {600.0, 0.496}};
    b.source = kSourceUser;
    ASSERT_TRUE(Repository<BulletRecord>(db_).Save(b).ok());
    CartridgeRecord c;
    c.name = "own load";
    c.caliber = ".308";
    c.bullet_id = b.id;
    c.muzzle_velocity_mps = 800.0;
    c.source = kSourceUser;
    ASSERT_TRUE(Repository<CartridgeRecord>(db_).Save(c).ok());

    CartridgeForm f = LoadCartridgeForm(db_, c.id).value();
    EXPECT_EQ(f.library_bullet_id, 0);
    EXPECT_DOUBLE_EQ(f.bc, 0.505);
    f.bc = 0.3;  // the form's single BC does not replace the bands
    f.drag_table = "G7";
    f.mass_gr = 180.0;
    ASSERT_TRUE(SaveCartridgeForm(db_, f).ok());
    const BulletRecord after = *Repository<BulletRecord>(db_).Get(b.id).value();
    EXPECT_EQ(after.drag_kind, storage::kDragKindMultiBc);
    EXPECT_EQ(after.drag_table, "G1");
    ASSERT_EQ(after.bc_bands.size(), 2U);
    EXPECT_NEAR(units::KgToGrain(after.mass_kg), 180.0, 1e-9);  // the rest is edited
}

TEST_F(AppArmory, CopyOfAnOwnCartridgeGetsItsOwnBullet) {
    const Id original = SaveCartridgeForm(db_, Cartridge()).value();
    CartridgeForm f = LoadCartridgeForm(db_, original).value();
    f.cartridge_id = 0;
    f.copy_of = original;
    f.name = "Copy";
    f.mass_gr = 168.0;
    const Id copy = SaveCartridgeForm(db_, f).value();
    EXPECT_NE(copy, original);
    EXPECT_EQ(Count<BulletRecord>(), 2U);
    const auto a = *Repository<CartridgeRecord>(db_).Get(original).value();
    const auto b = *Repository<CartridgeRecord>(db_).Get(copy).value();
    EXPECT_NE(a.bullet_id, b.bullet_id);
    EXPECT_NEAR(LoadCartridgeForm(db_, original).value().mass_gr, 175.0, 1e-9);
    EXPECT_NEAR(LoadCartridgeForm(db_, copy).value().mass_gr, 168.0, 1e-9);

    // Deleting the copy takes only its own bullet.
    ASSERT_TRUE(DeleteCartridge(db_, copy).ok());
    EXPECT_EQ(Count<BulletRecord>(), 1U);
    EXPECT_TRUE(LoadCartridgeForm(db_, original).ok());
}

TEST_F(AppArmory, SwitchingBetweenLibraryBulletsKeepsBoth) {
    const Id first = LibraryBullet();
    BulletForm other = Bullet();
    other.name = "Berger Hybrid 185";
    const Id second = LibraryBullet(other);
    const Id id =
        SaveCartridgeForm(db_, WithLibraryBullet(db_, Cartridge(), first).value()).value();
    CartridgeForm f = LoadCartridgeForm(db_, id).value();
    f = WithLibraryBullet(db_, f, second).value();
    ASSERT_TRUE(SaveCartridgeForm(db_, f).ok());
    EXPECT_EQ(LoadCartridgeForm(db_, id).value().library_bullet_id, second);
    EXPECT_EQ(Count<BulletRecord>(), 2U);  // the library bullet left behind stays
    EXPECT_EQ(ListLibraryBullets(db_).value().size(), 2U);
}

TEST_F(AppArmory, RefusedCartridgeWritesLeaveNothing) {
    Refuse("INSERT", "bullet");
    auto own = SaveCartridgeForm(db_, Cartridge());
    ASSERT_FALSE(own.ok());
    EXPECT_EQ(own.error().code, ErrorCode::kConstraint);
    EXPECT_EQ(Count<CartridgeRecord>(), 0U);
    Exec("DROP TRIGGER refuse_bullet_INSERT");

    Refuse("INSERT", "cartridge");
    auto row = SaveCartridgeForm(db_, Cartridge());
    ASSERT_FALSE(row.ok());
    EXPECT_EQ(row.error().code, ErrorCode::kConstraint);
    EXPECT_EQ(Count<BulletRecord>(), 0U);  // the bullet saved first is rolled back
}

TEST_F(AppArmory, FailedCartridgeCommitLeavesNothing) {
    // Points the new cartridge at a missing bullet; deferred, this only
    // fails at COMMIT.
    Exec(
        "CREATE TRIGGER dangle AFTER INSERT ON cartridge BEGIN "
        "UPDATE cartridge SET bullet_id = 99999 WHERE id = NEW.id; END");
    Exec("PRAGMA defer_foreign_keys = ON");
    auto id = SaveCartridgeForm(db_, Cartridge());
    ASSERT_FALSE(id.ok());
    EXPECT_EQ(id.error().code, ErrorCode::kConstraint);
    EXPECT_FALSE(db_.connection().InTransaction());
    EXPECT_EQ(Count<CartridgeRecord>(), 0U);
    EXPECT_EQ(Count<BulletRecord>(), 0U);
}

TEST_F(AppArmory, CartridgeChangesNeedTheirOwnTransaction) {
    const Id id = SaveCartridgeForm(db_, Cartridge()).value();
    Exec("BEGIN");
    EXPECT_FALSE(SaveCartridgeForm(db_, Cartridge()).ok());
    EXPECT_FALSE(DeleteCartridge(db_, id).ok());
    Exec("ROLLBACK");
    EXPECT_EQ(Count<CartridgeRecord>(), 1U);
}

TEST_F(AppArmory, FailedCartridgeDeletionKeepsEverything) {
    const Id pair = Pair();
    const Id cartridge = Profile(pair).cartridge_id;

    Refuse("DELETE", "profile");
    EXPECT_EQ(DeleteCartridge(db_, cartridge).error().code, ErrorCode::kConstraint);
    EXPECT_EQ(Count<CartridgeRecord>(), 1U);
    EXPECT_EQ(Count<ProfileRecord>(), 1U);
    Exec("DROP TRIGGER refuse_profile_DELETE");

    Refuse("DELETE", "cartridge");
    EXPECT_EQ(DeleteCartridge(db_, cartridge).error().code, ErrorCode::kConstraint);
    EXPECT_EQ(Count<CartridgeRecord>(), 1U);
    EXPECT_EQ(Count<ProfileRecord>(), 1U);
    EXPECT_EQ(Count<BulletRecord>(), 1U);
}

TEST_F(AppArmory, LibraryCartridgeSearchMatchesTheCalibreToo) {
    const Id bullet = LibraryBullet();
    CartridgeRecord lib;
    lib.name = "Factory Match";
    lib.caliber = "6.5 Creedmoor";
    lib.bullet_id = bullet;
    lib.muzzle_velocity_mps = 820.0;
    lib.source = "import:ammo";
    ASSERT_TRUE(Repository<CartridgeRecord>(db_).Save(lib).ok());

    const auto hits = ListLibraryCartridges(db_, "CREEDMOOR").value();
    ASSERT_EQ(hits.size(), 1U);
    EXPECT_EQ(hits[0].bullet_name, "ELD-M 178");
    EXPECT_DOUBLE_EQ(hits[0].muzzle_velocity_mps, 820.0);
    EXPECT_TRUE(ListLibraryCartridges(db_, "308").value().empty());

    // The copy is the user's: listed with the user's cartridges.
    CartridgeForm f = CartridgeFromLibrary(db_, lib.id).value();
    EXPECT_EQ(f.copy_of, lib.id);
    EXPECT_EQ(f.caliber, "6.5 Creedmoor");
    ASSERT_TRUE(SaveCartridgeForm(db_, f).ok());
    EXPECT_EQ(ListCartridges(db_, "6.5x47").value().size(), 1U);
}

// ---- Pairs ----------------------------------------------------------------

TEST_F(AppArmory, PairOfUnknownRifleOrCartridgeIsNotFound) {
    const Id rifle = SaveRifleForm(db_, Rifle()).value();
    const Id cartridge = SaveCartridgeForm(db_, Cartridge()).value();
    EXPECT_EQ(EnsureProfile(db_, 42, cartridge).error().message, "rifle not found");
    EXPECT_EQ(EnsureProfile(db_, rifle, 42).error().message, "cartridge not found");
    EXPECT_EQ(Count<ProfileRecord>(), 0U);

    const Id pair = EnsureProfile(db_, rifle, cartridge).value();
    EXPECT_EQ(Profile(pair).name, "Tikka T3x / Handload SMK 175");
}

TEST_F(AppArmory, RefusedPairIsAnError) {
    const Id rifle = SaveRifleForm(db_, Rifle()).value();
    const Id cartridge = SaveCartridgeForm(db_, Cartridge()).value();
    Refuse("INSERT", "profile");
    auto pair = EnsureProfile(db_, rifle, cartridge);
    ASSERT_FALSE(pair.ok());
    EXPECT_EQ(pair.error().code, ErrorCode::kConstraint);
    EXPECT_EQ(Count<ProfileRecord>(), 0U);
}

TEST_F(AppArmory, ZeroOffsetLimitsAndFailures) {
    EXPECT_EQ(SetZeroOffset(db_, 42, 1.0, 1.0).error().code, ErrorCode::kNotFound);

    const Id pair = Pair();
    ASSERT_TRUE(SetZeroOffset(db_, pair, -100.0, 100.0).ok());  // the limits themselves
    EXPECT_DOUBLE_EQ(Profile(pair).zero_offset_up_m, -1.0);
    EXPECT_DOUBLE_EQ(Profile(pair).zero_offset_right_m, 1.0);

    const Status wide = SetZeroOffset(db_, pair, 0.0, -100.5);
    ASSERT_FALSE(wide.ok());
    EXPECT_EQ(wide.error().code, ErrorCode::kConstraint);
    EXPECT_TRUE(Mentions(wide.error().message, "100 cm"));
    EXPECT_FALSE(SetZeroOffset(db_, pair, kNaN, 0.0).ok());
    EXPECT_DOUBLE_EQ(Profile(pair).zero_offset_up_m, -1.0);  // unchanged

    Refuse("UPDATE", "profile");
    EXPECT_EQ(SetZeroOffset(db_, pair, 1.0, 1.0).error().code, ErrorCode::kConstraint);
    EXPECT_DOUBLE_EQ(Profile(pair).zero_offset_up_m, -1.0);
}

TEST_F(AppArmory, SampleProfileNeedsNames) {
    auto no_rifle = CreateSampleProfile(db_, "", "Load");
    ASSERT_FALSE(no_rifle.ok());
    EXPECT_EQ(no_rifle.error().message, "Enter a rifle name.");
    EXPECT_EQ(Count<RifleRecord>(), 0U);

    auto no_cartridge = CreateSampleProfile(db_, "Rifle", "");
    ASSERT_FALSE(no_cartridge.ok());
    EXPECT_EQ(no_cartridge.error().message, "Enter a cartridge name.");
    EXPECT_EQ(Count<CartridgeRecord>(), 0U);
    EXPECT_EQ(Count<ProfileRecord>(), 0U);
}

// ---- Storage failures -----------------------------------------------------

TEST_F(AppArmory, MissingPairTableFailsPairOperations) {
    const Id rifle = SaveRifleForm(db_, Rifle()).value();
    const Id cartridge = SaveCartridgeForm(db_, Cartridge()).value();
    Exec("ALTER TABLE profile RENAME TO profile_gone");
    EXPECT_FALSE(EnsureProfile(db_, rifle, cartridge).ok());
    EXPECT_FALSE(DeleteRifle(db_, rifle).ok());
    EXPECT_FALSE(DeleteCartridge(db_, cartridge).ok());
    EXPECT_EQ(Count<RifleRecord>(), 1U);
    EXPECT_EQ(Count<CartridgeRecord>(), 1U);
}

TEST_F(AppArmory, UnreadableScopeOrBulletFailsTheForms) {
    const Id rifle = SaveRifleForm(db_, Rifle()).value();
    const Id cartridge = SaveCartridgeForm(db_, Cartridge()).value();
    Exec("ALTER TABLE scope RENAME TO scope_gone");
    auto r = LoadRifleForm(db_, rifle);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().code, ErrorCode::kNotFound);

    Exec("ALTER TABLE bullet RENAME TO bullet_gone");
    auto c = LoadCartridgeForm(db_, cartridge);
    ASSERT_FALSE(c.ok());
    EXPECT_NE(c.error().code, ErrorCode::kNotFound);
    // The list does without the bullet's name.
    const auto list = ListCartridges(db_).value();
    ASSERT_EQ(list.size(), 1U);
    EXPECT_TRUE(list[0].bullet_name.empty());
}

TEST_F(AppArmory, ClosedDatabaseFailsEverything) {
    storage::Database closed;
    EXPECT_FALSE(ListRifles(closed).ok());
    EXPECT_FALSE(ListCartridges(closed).ok());
    EXPECT_FALSE(ListLibraryCartridges(closed).ok());
    EXPECT_FALSE(ListLibraryBullets(closed).ok());

    // A read error is not "not found".
    auto rifle = LoadRifleForm(closed, 1);
    ASSERT_FALSE(rifle.ok());
    EXPECT_NE(rifle.error().code, ErrorCode::kNotFound);
    auto bullet = LoadBulletForm(closed, 1);
    ASSERT_FALSE(bullet.ok());
    EXPECT_NE(bullet.error().code, ErrorCode::kNotFound);
    auto cartridge = LoadCartridgeForm(closed, 1);
    ASSERT_FALSE(cartridge.ok());
    EXPECT_NE(cartridge.error().code, ErrorCode::kNotFound);
    auto with = WithLibraryBullet(closed, Cartridge(), 1);
    ASSERT_FALSE(with.ok());
    EXPECT_NE(with.error().code, ErrorCode::kNotFound);
    const Status offset = SetZeroOffset(closed, 1, 0.0, 0.0);
    ASSERT_FALSE(offset.ok());
    EXPECT_NE(offset.error().code, ErrorCode::kNotFound);

    EXPECT_FALSE(SaveRifleForm(closed, Rifle()).ok());
    EXPECT_FALSE(SaveCartridgeForm(closed, Cartridge()).ok());
    EXPECT_FALSE(SaveBulletForm(closed, Bullet()).ok());
    BulletForm edit = Bullet();
    edit.id = 1;
    auto saved = SaveBulletForm(closed, edit);
    ASSERT_FALSE(saved.ok());
    EXPECT_NE(saved.error().code, ErrorCode::kNotFound);

    // Not a bullet in use: the error passes through unchanged.
    const Status del = DeleteBullet(closed, 1);
    ASSERT_FALSE(del.ok());
    EXPECT_NE(del.error().code, ErrorCode::kConstraint);
    EXPECT_FALSE(Mentions(del.error().message, "used by a cartridge"));
}

// ---- Bullet library -------------------------------------------------------

TEST(AppArmoryBulletForm, EachProblemIsNamed) {
    BulletForm b = Bullet();
    EXPECT_TRUE(Validate(b).empty());
    b.mass_gr = 0.0;
    EXPECT_EQ(Validate(b), "Enter the bullet weight.");
    b = Bullet();
    b.diameter_in = 1.0;
    EXPECT_TRUE(Mentions(Validate(b), "diameter"));
    b = Bullet();
    b.length_in = -1.0;
    EXPECT_TRUE(Mentions(Validate(b), "length"));
    b = Bullet();
    b.bc = 0.0;
    EXPECT_TRUE(Mentions(Validate(b), "coefficient"));
    b.bc = 2.0;
    EXPECT_TRUE(Mentions(Validate(b), "coefficient"));
    b.has_custom_curve = true;  // a Cd curve needs no BC
    EXPECT_TRUE(Validate(b).empty());

    b = Bullet();
    b.bc = 0.0;  // the bands carry the BC
    b.bands = {{800.0, 0.5}, {600.0, 0.48}};
    EXPECT_TRUE(Validate(b).empty());
    b.bands[1].velocity_mps = 0.0;
    EXPECT_TRUE(Mentions(Validate(b), "band"));
}

TEST_F(AppArmory, UnknownBulletIsNotFound) {
    auto load = LoadBulletForm(db_, 42);
    ASSERT_FALSE(load.ok());
    EXPECT_EQ(load.error().code, ErrorCode::kNotFound);
    EXPECT_EQ(load.error().message, "bullet not found");

    BulletForm b = Bullet();
    b.id = 42;
    auto save = SaveBulletForm(db_, b);
    ASSERT_FALSE(save.ok());
    EXPECT_EQ(save.error().code, ErrorCode::kNotFound);
    EXPECT_EQ(Count<BulletRecord>(), 0U);

    BulletForm bad = Bullet();
    bad.name.clear();
    auto invalid = SaveBulletForm(db_, bad);
    ASSERT_FALSE(invalid.ok());
    EXPECT_EQ(invalid.error().code, ErrorCode::kConstraint);
    EXPECT_EQ(invalid.error().message, "Enter the bullet name.");
}

TEST_F(AppArmory, BulletRoundTripKeepsTheDetails) {
    BulletForm b = Bullet();
    b.notes = "tested 2026";
    b.source.clear();  // defaults to the library
    const Id id = LibraryBullet(b);
    const BulletForm g = LoadBulletForm(db_, id).value();
    EXPECT_EQ(g.manufacturer, "Hornady");
    EXPECT_EQ(g.notes, "tested 2026");
    EXPECT_EQ(g.source, kSourceLibrary);
    EXPECT_NEAR(g.length_in, 1.35, 1e-12);
    EXPECT_DOUBLE_EQ(g.bc, 0.275);
    EXPECT_FALSE(g.has_custom_curve);

    const auto list = ListLibraryBullets(db_, "hornady").value();
    ASSERT_EQ(list.size(), 1U);
    EXPECT_EQ(list[0].drag_kind, storage::kDragKindBc);
    EXPECT_EQ(list[0].bc_bands, 0);
    EXPECT_NEAR(list[0].mass_gr, 178.0, 1e-9);
    EXPECT_EQ(list[0].source, kSourceLibrary);
    EXPECT_TRUE(ListLibraryBullets(db_, "sierra").value().empty());
}

TEST_F(AppArmory, BulletWithACustomCurveKeepsItsDrag) {
    storage::DragCurveRecord curve;
    curve.name = "Doppler";
    curve.points = {{0.5, 0.2}, {1.0, 0.35}, {2.0, 0.3}};
    ASSERT_TRUE(Repository<storage::DragCurveRecord>(db_).Save(curve).ok());
    BulletRecord rec;
    rec.name = "Radar bullet";
    rec.diameter_m = units::InchToM(0.308);
    rec.mass_kg = units::GrainToKg(175.0);
    rec.drag_kind = storage::kDragKindCurve;
    rec.curve_id = curve.id;
    rec.source = kSourceLibrary;
    ASSERT_TRUE(Repository<BulletRecord>(db_).Save(rec).ok());

    BulletForm f = LoadBulletForm(db_, rec.id).value();
    EXPECT_TRUE(f.has_custom_curve);
    EXPECT_TRUE(Validate(f).empty());
    f.mass_gr = 180.0;
    f.bc = 0.5;  // read-only for a curve: ignored
    f.drag_table = "G1";
    ASSERT_TRUE(SaveBulletForm(db_, f).ok());
    const BulletRecord after = *Repository<BulletRecord>(db_).Get(rec.id).value();
    EXPECT_EQ(after.drag_kind, storage::kDragKindCurve);
    EXPECT_EQ(after.curve_id, curve.id);
    EXPECT_FALSE(after.bc.has_value());
    EXPECT_NEAR(units::KgToGrain(after.mass_kg), 180.0, 1e-9);
}

TEST_F(AppArmory, BulletInUseCannotBeDeleted) {
    const Id lib = LibraryBullet();
    const Id cartridge =
        SaveCartridgeForm(db_, WithLibraryBullet(db_, Cartridge(), lib).value()).value();
    const Status s = DeleteBullet(db_, lib);
    ASSERT_FALSE(s.ok());
    EXPECT_EQ(s.error().code, ErrorCode::kConstraint);
    EXPECT_EQ(s.error().message, "This bullet is used by a cartridge and cannot be deleted.");
    ASSERT_TRUE(DeleteCartridge(db_, cartridge).ok());
    EXPECT_TRUE(DeleteBullet(db_, lib).ok());
    EXPECT_TRUE(DeleteBullet(db_, lib).ok());  // already gone: nothing to do
}

// ---- Reticle --------------------------------------------------------------

TEST(AppArmoryReticle, ScopeWithoutClicksDialsExactly) {
    storage::ScopeRecord scope;  // no turret data
    scope.click_vertical_rad = scope.click_horizontal_rad = 0.0;
    const ReticleHold h = ComputeReticleHold(units::MradToRad(4.37), units::MradToRad(-1.2), scope,
                                             10.0, HoldMode::kDialAll);
    EXPECT_DOUBLE_EQ(h.dial_elevation_clicks, 0.0);
    EXPECT_DOUBLE_EQ(h.dial_windage_clicks, 0.0);
    EXPECT_DOUBLE_EQ(h.dial_elevation_rad, units::MradToRad(4.37));
    EXPECT_DOUBLE_EQ(h.dial_windage_rad, units::MradToRad(-1.2));
    EXPECT_NEAR(h.target_x, 0.0, 1e-12);  // nothing left to hold
    EXPECT_NEAR(h.target_y, 0.0, 1e-12);
}

}  // namespace
}  // namespace ballistics::applogic
