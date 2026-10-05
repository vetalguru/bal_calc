#include <ballistics/storage/database.h>
#include <ballistics/storage/repository.h>
#include <ballistics/units.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace ballistics::storage {
namespace {

namespace fs = std::filesystem;

class TempDbFile {
public:
    TempDbFile()
        : path_(fs::temp_directory_path() /
                ("balcalc_test_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".db")) {
        fs::remove(path_);
    }
    ~TempDbFile() { fs::remove(path_); }
    std::string path() const { return path_.string(); }

private:
    fs::path path_;
};

TEST(StorageDatabase, FreshDatabaseIsMigratedToLatest) {
    Database db;
    ASSERT_TRUE(db.Open(":memory:").ok());
    auto version = db.SchemaVersion();
    ASSERT_TRUE(version.ok());
    EXPECT_EQ(version.value(), Database::LatestSchemaVersion());
    EXPECT_GE(Database::LatestSchemaVersion(), 1);
}

TEST(StorageDatabase, ReopeningKeepsDataAndVersion) {
    TempDbFile file;
    {
        Database db;
        ASSERT_TRUE(db.Open(file.path()).ok());
        ASSERT_TRUE(SetSetting(db, "units", "metric").ok());
    }
    Database db;
    ASSERT_TRUE(db.Open(file.path()).ok());
    EXPECT_EQ(db.SchemaVersion().value(), Database::LatestSchemaVersion());
    EXPECT_EQ(GetSetting(db, "units").value().value_or(""), "metric");
}

TEST(StorageDatabase, RefusesNewerSchema) {
    TempDbFile file;
    {
        Database db;
        ASSERT_TRUE(db.Open(file.path()).ok());
        const std::string sql =
            "PRAGMA user_version = " + std::to_string(Database::LatestSchemaVersion() + 1);
        ASSERT_TRUE(db.connection().Execute(sql).ok());
    }
    Database db;
    const Status s = db.Open(file.path());
    EXPECT_FALSE(s.ok());
    EXPECT_FALSE(db.IsOpen());
}

TEST(StorageDatabase, ReportsBundledSqliteVersion) {
    EXPECT_EQ(std::string(SqliteVersion()).rfind("3.", 0), 0U);
}

class StorageRepositories : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_TRUE(db_.Open(":memory:").ok()); }

    // A bullet + cartridge + rifle + profile chain; returns the profile.
    ProfileRecord MakeProfile() {
        BulletRecord b;
        b.name = "SMK 175";
        b.caliber = ".308";
        b.diameter_m = units::InchToM(0.308);
        b.mass_kg = units::GrainToKg(175.0);
        b.bc = 0.243;
        EXPECT_TRUE(Repository<BulletRecord>(db_).Save(b).ok());

        CartridgeRecord c;
        c.name = "M118LR";
        c.bullet_id = b.id;
        c.muzzle_velocity_mps = 790.0;
        EXPECT_TRUE(Repository<CartridgeRecord>(db_).Save(c).ok());

        RifleRecord r;
        r.name = "M24";
        r.twist_m = units::InchToM(11.25);
        r.sight_height_m = 0.05;
        EXPECT_TRUE(Repository<RifleRecord>(db_).Save(r).ok());

        ProfileRecord p;
        p.name = "M24 / M118LR";
        p.rifle_id = r.id;
        p.cartridge_id = c.id;
        EXPECT_TRUE(Repository<ProfileRecord>(db_).Save(p).ok());
        return p;
    }

    Database db_;
};

TEST_F(StorageRepositories, BulletRoundTripWithBands) {
    Repository<BulletRecord> repo(db_);
    BulletRecord b;
    b.name = "SMK 175 HPBT";
    b.manufacturer = "Sierra";
    b.caliber = ".308";
    b.diameter_m = units::InchToM(0.308);
    b.mass_kg = units::GrainToKg(175.0);
    b.length_m = units::InchToM(1.24);
    b.drag_kind = kDragKindMultiBc;
    b.drag_table = "G1";
    b.bc_bands = {{869.0, 0.505}, {701.0, 0.496}, {457.0, 0.485}};
    auto id = repo.Save(b);
    ASSERT_TRUE(id.ok()) << id.error().message;
    EXPECT_GT(b.id, 0);

    auto got = repo.Get(b.id);
    ASSERT_TRUE(got.ok());
    ASSERT_TRUE(got.value().has_value());
    const BulletRecord& g = *got.value();
    EXPECT_EQ(g.name, "SMK 175 HPBT");
    EXPECT_EQ(g.drag_kind, kDragKindMultiBc);
    EXPECT_FALSE(g.bc.has_value());
    EXPECT_DOUBLE_EQ(g.mass_kg, b.mass_kg);
    ASSERT_EQ(g.bc_bands.size(), 3U);
    EXPECT_DOUBLE_EQ(g.bc_bands[0].velocity_mps, 869.0);
    EXPECT_DOUBLE_EQ(g.bc_bands[2].bc_lb_in2, 0.485);

    // Update replaces children.
    b.bc_bands = {{800.0, 0.5}};
    b.notes = "updated";
    ASSERT_TRUE(repo.Save(b).ok());
    const BulletRecord g2 = *repo.Get(b.id).value();
    EXPECT_EQ(g2.notes, "updated");
    ASSERT_EQ(g2.bc_bands.size(), 1U);
}

TEST_F(StorageRepositories, CheckConstraintsRejectBadRows) {
    Repository<BulletRecord> repo(db_);
    BulletRecord b;
    b.name = "no mass";
    b.diameter_m = 0.00782;
    b.mass_kg = 0.0;
    b.bc = 0.3;
    EXPECT_FALSE(repo.Save(b).ok());
    EXPECT_EQ(b.id, 0);

    BulletRecord no_bc;
    no_bc.name = "bc kind without bc";
    no_bc.diameter_m = 0.00782;
    no_bc.mass_kg = 0.01;
    EXPECT_FALSE(repo.Save(no_bc).ok());
}

TEST_F(StorageRepositories, DragCurveRoundTrip) {
    Repository<DragCurveRecord> repo(db_);
    DragCurveRecord c;
    c.name = "Doppler 6.5 140";
    c.points = {{0.5, 0.15}, {1.0, 0.38}, {2.0, 0.29}};
    ASSERT_TRUE(repo.Save(c).ok());
    const auto got = *repo.Get(c.id).value();
    ASSERT_EQ(got.points.size(), 3U);
    EXPECT_DOUBLE_EQ(got.points[1].cd, 0.38);
}

TEST_F(StorageRepositories, ProfileReferencesAreEnforced) {
    const ProfileRecord p = MakeProfile();
    // The cartridge is used by the profile: it cannot be deleted.
    EXPECT_FALSE(Repository<CartridgeRecord>(db_).Remove(p.cartridge_id).ok());
    // After removing the profile it can.
    ASSERT_TRUE(Repository<ProfileRecord>(db_).Remove(p.id).ok());
    EXPECT_TRUE(Repository<CartridgeRecord>(db_).Remove(p.cartridge_id).ok());

    ProfileRecord dangling;
    dangling.name = "broken";
    dangling.rifle_id = 999;
    dangling.cartridge_id = 999;
    EXPECT_FALSE(Repository<ProfileRecord>(db_).Save(dangling).ok());
}

TEST_F(StorageRepositories, ProfileStoresZeroConditionsAndDefaults) {
    ProfileRecord p = MakeProfile();
    p.zero_atmosphere = {350.0, 97000.0, units::CToK(-5.0), 0.4};
    p.zero_range_m = 300.0;
    ASSERT_TRUE(Repository<ProfileRecord>(db_).Save(p).ok());
    const ProfileRecord got = *Repository<ProfileRecord>(db_).Get(p.id).value();
    EXPECT_DOUBLE_EQ(got.zero_range_m, 300.0);
    EXPECT_DOUBLE_EQ(got.zero_atmosphere.pressure_pa, 97000.0);
    EXPECT_DOUBLE_EQ(got.zero_atmosphere.humidity, 0.4);
    EXPECT_FALSE(got.created_at.empty()); // filled by the database
    EXPECT_FALSE(got.last_used_at.has_value());
}

TEST_F(StorageRepositories, ConditionsWithWindZones) {
    Repository<ConditionsRecord> repo(db_);
    ConditionsRecord c;
    c.name = "Range day";
    c.latitude_rad = units::DegToRad(50.4);
    c.winds = {{300.0, 3.0, units::DegToRad(90.0), 0.0}, {1e5, 5.0, units::DegToRad(45.0), 0.5}};
    ASSERT_TRUE(repo.Save(c).ok());
    const auto got = *repo.Get(c.id).value();
    ASSERT_TRUE(got.latitude_rad.has_value());
    EXPECT_FALSE(got.azimuth_rad.has_value());
    ASSERT_EQ(got.winds.size(), 2U);
    EXPECT_DOUBLE_EQ(got.winds[1].vertical_mps, 0.5);

    ASSERT_TRUE(repo.Remove(c.id).ok()); // wind zones cascade
    EXPECT_FALSE(repo.Get(c.id).value().has_value());
}

TEST_F(StorageRepositories, ListFiltersByName) {
    Repository<RifleRecord> repo(db_);
    for (const char* n : {"Tikka T3x", "Sako TRG 22", "Accuracy AX"}) {
        RifleRecord r;
        r.name = n;
        ASSERT_TRUE(repo.Save(r).ok());
    }
    EXPECT_EQ(repo.List().value().size(), 3U);
    const auto filtered = repo.List("trg").value();
    ASSERT_EQ(filtered.size(), 1U);
    EXPECT_EQ(filtered[0].name, "Sako TRG 22");
    EXPECT_EQ(repo.List().value().front().name, "Accuracy AX"); // ordered by name
}

TEST_F(StorageRepositories, DopeLogAndCartridgeVelocityTable) {
    const ProfileRecord p = MakeProfile();
    Repository<CartridgeRecord> carts(db_);
    CartridgeRecord c = *carts.Get(p.cartridge_id).value();
    c.velocity_points = {{units::CToK(-10.0), 770.0}, {units::CToK(30.0), 805.0}};
    ASSERT_TRUE(carts.Save(c).ok());
    EXPECT_EQ(carts.Get(c.id).value()->velocity_points.size(), 2U);

    Repository<DopeRecord> log(db_);
    DopeRecord d;
    d.profile_id = p.id;
    d.range_m = 800.0;
    d.observed_elevation_rad = units::MradToRad(6.3);
    ASSERT_TRUE(log.Save(d).ok());
    const DopeRecord got = *log.Get(d.id).value();
    EXPECT_FALSE(got.shot_at.empty());
    EXPECT_TRUE(got.use_for_truing);
    EXPECT_FALSE(got.observed_windage_rad.has_value());

    // Profile removal cascades to its log.
    ASSERT_TRUE(Repository<ProfileRecord>(db_).Remove(p.id).ok());
    EXPECT_TRUE(log.List().value().empty());
}

TEST_F(StorageRepositories, UpdatingMissingRowFails) {
    RifleRecord r;
    r.id = 12345;
    r.name = "ghost";
    EXPECT_FALSE(Repository<RifleRecord>(db_).Save(r).ok());
}

} // namespace
} // namespace ballistics::storage
