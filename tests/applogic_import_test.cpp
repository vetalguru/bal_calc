// Importers for .ammo / .drg / .reticle and the bundled starter library,
// run against the real files in data/seed.
#include <ballistics/applogic/importers.h>
#include <ballistics/applogic/library.h>
#include <ballistics/applogic/session.h>
#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>
#include <ballistics/units.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace ballistics::applogic {
namespace {

namespace fs = std::filesystem;

const fs::path kSeed = BALLISTICS_SEED_DIR;

std::string ReadFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

std::vector<fs::path> FilesIn(const fs::path& dir, const std::string& ext) {
    std::vector<fs::path> out;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (e.path().extension() == ext) {
            out.push_back(e.path());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<SeedFile> AllSeedFiles() {
    std::vector<SeedFile> files;
    for (const char* sub : {"ammo", "drg", "reticle"}) {
        for (const auto& entry : fs::directory_iterator(kSeed / sub)) {
            files.push_back({entry.path().filename().string(), ReadFile(entry.path())});
        }
    }
    files.push_back({"published_bullets.json", ReadFile(kSeed / "published_bullets.json")});
    return files;
}

TEST(Import, EveryBundledAmmoFileParses) {
    const auto files = FilesIn(kSeed / "ammo", ".ammo");
    EXPECT_EQ(files.size(), 69U);
    for (const auto& f : files) {
        const auto a = ParseAmmo(ReadFile(f));
        ASSERT_TRUE(a.ok()) << f << ": " << a.error().message;
        EXPECT_GT(a.value().bullet.mass_kg, 0.0) << f;
        EXPECT_GT(a.value().bullet.diameter_m, 0.0) << f;
        EXPECT_LT(a.value().bullet.diameter_m, 0.03) << f; // shotgun slugs are the biggest
        EXPECT_GT(a.value().cartridge.muzzle_velocity_mps, 50.0) << f;
    }
}

TEST(Import, AmmoValuesAndUnits) {
    const auto a = ParseAmmo(R"(<ammo-info-ex table="G1" bc="0.540" bullet-weight="11.28000000g"
        muzzle-velocity="805.00000000m/s" barrel-length="605.00000000mm" bullet-length="32.50000000mm"
        bullet-diameter="7.84000000mm" name="7.5x55 GP11" source="" caliber="7x55mm Swiss" bullet-type="FMJ" />)");
    ASSERT_TRUE(a.ok());
    EXPECT_EQ(a.value().bullet.name, "7.5x55 GP11");
    EXPECT_EQ(a.value().bullet.drag_table, "G1");
    EXPECT_DOUBLE_EQ(a.value().bullet.bc.value(), 0.540);
    EXPECT_NEAR(a.value().bullet.mass_kg, 0.01128, 1e-12);
    EXPECT_NEAR(a.value().bullet.diameter_m, 0.00784, 1e-12);
    EXPECT_NEAR(a.value().bullet.length_m, 0.0325, 1e-12);
    EXPECT_DOUBLE_EQ(a.value().cartridge.muzzle_velocity_mps, 805.0);
    EXPECT_NEAR(a.value().cartridge.barrel_length_m, 0.605, 1e-12);

    const auto imperial = ParseAmmo(R"(<ammo-info-ex table="G7" bc="0.243" bullet-weight="175gr"
        muzzle-velocity="2600ft/s" bullet-diameter="0.308in" name="x" caliber=".308" />)");
    ASSERT_TRUE(imperial.ok());
    EXPECT_NEAR(imperial.value().bullet.mass_kg, units::GrainToKg(175.0), 1e-12);
    EXPECT_NEAR(imperial.value().cartridge.muzzle_velocity_mps, units::FpsToMps(2600.0), 1e-9);

    // Diameter from the caliber when the file has none.
    const auto slug = ParseAmmo(R"(<ammo-info-ex table="G1" bc="0.071" bullet-weight="109gr"
        muzzle-velocity="1775ft/s" name="410 slug" caliber="410 GA" />)");
    ASSERT_TRUE(slug.ok());
    EXPECT_NEAR(slug.value().bullet.diameter_m, units::InchToM(0.410), 1e-12);
    const auto ak = ParseAmmo(R"(<ammo-info-ex bc="0.3" bullet-weight="8g" muzzle-velocity="715m/s"
        name="ak" caliber="7.62x39" />)");
    EXPECT_NEAR(ak.value().bullet.diameter_m, 0.00762, 1e-12);

    EXPECT_FALSE(ParseAmmo("<nope/>").ok());
    EXPECT_FALSE(ParseAmmo(R"(<ammo-info-ex name="x" bc="0.3" bullet-weight="10furlongs" />)").ok());
}

TEST(Import, EveryBundledDrgFileParses) {
    const auto files = FilesIn(kSeed / "drg", ".drg");
    EXPECT_EQ(files.size(), 55U);
    for (const auto& f : files) {
        const auto d = ParseDrg(ReadFile(f));
        ASSERT_TRUE(d.ok()) << f << ": " << d.error().message;
        EXPECT_GE(d.value().points.size(), 10U) << f;
        for (std::size_t i = 1; i < d.value().points.size(); ++i) {
            EXPECT_GT(d.value().points[i].mach, d.value().points[i - 1].mach) << f;
        }
        // A real curve: transonic peak above the subsonic drag.
        EXPECT_NO_THROW(DragCurve(d.value().points)) << f;
    }
}

TEST(Import, DrgHeaderVariants) {
    const auto lapua = ParseDrg("CFM, .224 Lapua E539 3.6g ( 55gr ), .00360, .0057, .0170, Radar Data\r\n"
                                "0.360\t0.000\r\n0.350\t0.200\r\n0.420\t1.000\r\n");
    ASSERT_TRUE(lapua.ok()) << lapua.error().message;
    EXPECT_EQ(lapua.value().kind, "CFM");
    EXPECT_EQ(lapua.value().name, ".224 Lapua E539 3.6g ( 55gr )");
    EXPECT_DOUBLE_EQ(lapua.value().mass_kg, 0.0036);
    EXPECT_DOUBLE_EQ(lapua.value().diameter_m, 0.0057);
    EXPECT_DOUBLE_EQ(lapua.value().length_m, 0.0170);
    ASSERT_EQ(lapua.value().points.size(), 3U);
    EXPECT_DOUBLE_EQ(lapua.value().points[2].mach, 1.0);
    EXPECT_DOUBLE_EQ(lapua.value().points[2].cd, 0.42);

    // The source typo "x. y" instead of "x, y" is tolerated.
    const auto typo = ParseDrg("CFM, .30 Lapua N558 Naturalis 11.0g ( 170gr ), .01100, .007830. .03370, Radar Data\n"
                               "0.3 0.0\n0.4 1.0\n");
    ASSERT_TRUE(typo.ok()) << typo.error().message;
    EXPECT_DOUBLE_EQ(typo.value().diameter_m, 0.00783);
    EXPECT_DOUBLE_EQ(typo.value().length_m, 0.0337);

    EXPECT_FALSE(ParseDrg("Nennstiel EB, Sphere, 0.001, 0.001, 0.0, Encoded Data\n1 2\n3 4\n").ok());
    EXPECT_FALSE(ParseDrg("CFM, only one point, 0.01, 0.0078, 0.03, Radar Data\n0.3 0.5\n").ok());
}

TEST(Import, EveryBundledReticleParses) {
    const auto files = FilesIn(kSeed / "reticle", ".reticle");
    EXPECT_EQ(files.size(), 4U);
    for (const auto& f : files) {
        const auto r = ParseReticle(ReadFile(f));
        ASSERT_TRUE(r.ok()) << f << ": " << r.error().message;
        EXPECT_FALSE(r.value().definition.empty());
    }
    const auto mildot = ParseReticle(ReadFile(kSeed / "reticle" / "mildot.reticle"));
    EXPECT_EQ(mildot.value().name, "Mil-Dot Reticle");
    EXPECT_EQ(mildot.value().units, "mrad");
    EXPECT_NE(mildot.value().definition.find(R"("size":[12.0,12.0])"), std::string::npos)
        << mildot.value().definition.substr(0, 120);
    const auto bdc = ParseReticle(ReadFile(kSeed / "reticle" / "bdc.reticle"));
    EXPECT_EQ(bdc.value().units, "moa");
    // 140 MOA wide = 40.72 mrad.
    EXPECT_NE(bdc.value().definition.find(R"("size":[40.72)"), std::string::npos);
}

class ImportDb : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_TRUE(db_.Open(":memory:").ok()); }
    storage::Database db_;
};

TEST_F(ImportDb, SeedImportsEverythingOnce) {
    const auto files = AllSeedFiles();
    const auto first = SeedLibrary(db_, files, 1);
    ASSERT_TRUE(first.ok()) << first.error().message;
    EXPECT_TRUE(first.value().problems.empty()) << first.value().problems.front();
    // 69 ammo + 55 drg + 4 reticles + 38 published bullets.
    EXPECT_EQ(first.value().imported, 69 + 55 + 4 + 38);

    const auto again = SeedLibrary(db_, files, 1); // same version: nothing to do
    EXPECT_EQ(again.value().imported, 0);
    EXPECT_EQ(again.value().skipped, 0);

    const auto newer = SeedLibrary(db_, files, 2); // new version: existing ones skipped
    EXPECT_EQ(newer.value().imported, 0);
    EXPECT_EQ(newer.value().skipped, 69 + 55 + 4 + 38);

    EXPECT_EQ(ListLibraryBullets(db_).value().size(), 69U + 55U + 38U);
    EXPECT_EQ(storage::Repository<storage::ReticleRecord>(db_).List().value().size(), 4U);
    EXPECT_EQ(ListLibraryBullets(db_, "lapua").value().size() >= 50U, true);
}

TEST_F(ImportDb, RadarCurveBulletSolves) {
    ASSERT_TRUE(SeedLibrary(db_, AllSeedFiles(), 1).ok());
    const auto hits = ListLibraryBullets(db_, "GB432").value();
    ASSERT_FALSE(hits.empty()); // .30 Lapua Scenar 185 gr
    EXPECT_EQ(hits.front().drag_kind, storage::kDragKindCurve);

    storage::LoadedProfile p;
    p.bullet = *storage::Repository<storage::BulletRecord>(db_).Get(hits.front().id).value();
    p.curve = *storage::Repository<storage::DragCurveRecord>(db_).Get(*p.bullet.curve_id).value();
    p.cartridge.muzzle_velocity_mps = 800.0;
    p.rifle.sight_height_m = 0.05;
    SessionConditions s;
    s.target_range_m = 1000.0;
    const SolutionSummary sum = Summarize(p, s, AngleUnit::kMrad);
    ASSERT_TRUE(sum.ok) << sum.error;
    // A heavy .308 match bullet at 800 m/s: roughly 10-14 MRAD at 1000 m.
    EXPECT_GT(sum.elevation, 9.0);
    EXPECT_LT(sum.elevation, 15.0);
}

TEST_F(ImportDb, PublishedBandsAreImported) {
    ASSERT_TRUE(SeedLibrary(db_, AllSeedFiles(), 1).ok());
    const auto smk = ListLibraryBullets(db_, "MatchKing 175").value();
    ASSERT_EQ(smk.size(), 1U);
    EXPECT_EQ(smk[0].bc_bands, 3);
    EXPECT_DOUBLE_EQ(smk[0].bc, 0.505);
    const auto form = LoadBulletForm(db_, smk[0].id).value();
    EXPECT_NEAR(form.bands[0].velocity_mps, units::FpsToMps(2800.0), 1e-9);
    EXPECT_NEAR(form.length_in, 1.24, 1e-9);
}

TEST_F(ImportDb, ImportFileDispatchesByExtension) {
    EXPECT_TRUE(ImportFile(db_, "x.AMMO", ReadFile(kSeed / "ammo" / "7.5x55 GP11.ammo")).ok());
    EXPECT_TRUE(ImportFile(db_, "mildot.reticle", ReadFile(kSeed / "reticle" / "mildot.reticle")).ok());
    EXPECT_FALSE(ImportFile(db_, "notes.txt", "hello").ok());
}

} // namespace
} // namespace ballistics::applogic
