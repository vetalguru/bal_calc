// A handloaded cartridge's recipe and chronograph spread (schema v6):
// powder and charge, primer, brass, COAL and CBTO, SD and ES. For the owner:
// they do not change the shot.
#include <ballistics/applogic/armory.h>
#include <ballistics/applogic/profile_io.h>
#include <ballistics/storage/database.h>
#include <ballistics/storage/repository.h>
#include <ballistics/units.h>
#include <gtest/gtest.h>
#include <sqlite_manager/connection.h>

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

#include "../storage/src/schema.h"

namespace ballistics::applogic {
namespace {

using storage::CartridgeRecord;

CartridgeForm Handload() {
    CartridgeForm f;
    f.name = "308 SMK 175 handload";
    f.caliber = ".308 Win";
    f.bullet_name = "SMK 175";
    f.drag_table = "G7";
    f.bc = 0.243;
    f.mass_gr = 175.0;
    f.diameter_in = 0.308;
    f.length_in = 1.24;
    f.muzzle_velocity_mps = 790.0;
    f.powder = "Vihtavuori N150";
    f.charge_gr = 44.5;
    f.primer = "CCI BR2";
    f.brass = "Lapua";
    f.coal_in = 2.810;
    f.cbto_in = 2.235;
    f.velocity_sd_mps = 3.2;
    f.velocity_es_mps = 11.0;
    return f;
}

bool Mentions(const std::string& problem, const std::string& words) {
    return problem.find(words) != std::string::npos;
}

class Handloads : public ::testing::Test {
   protected:
    void SetUp() override { ASSERT_TRUE(db_.Open(":memory:").ok()); }
    storage::Database db_;
};

TEST_F(Handloads, RecipeRoundTrips) {
    const Id id = SaveCartridgeForm(db_, Handload()).value();
    const CartridgeForm g = LoadCartridgeForm(db_, id).value();
    EXPECT_EQ(g.powder, "Vihtavuori N150");
    EXPECT_DOUBLE_EQ(g.charge_gr, 44.5);  // not 44.4999999 after the kilograms
    EXPECT_EQ(g.primer, "CCI BR2");
    EXPECT_EQ(g.brass, "Lapua");
    EXPECT_DOUBLE_EQ(g.coal_in, 2.81);
    EXPECT_DOUBLE_EQ(g.cbto_in, 2.235);
    EXPECT_DOUBLE_EQ(g.velocity_sd_mps, 3.2);
    EXPECT_DOUBLE_EQ(g.velocity_es_mps, 11.0);
    // Stored in SI.
    const CartridgeRecord c = *storage::Repository<CartridgeRecord>(db_).Get(id).value();
    EXPECT_NEAR(c.charge_kg, units::GrainToKg(44.5), 1e-12);
    EXPECT_NEAR(c.coal_m, units::InchToM(2.81), 1e-12);
}

TEST(HandloadForm, EachProblemIsNamed) {
    EXPECT_TRUE(Validate(Handload()).empty());
    for (auto spoil :
         {+[](CartridgeForm& f) { f.charge_gr = -1.0; },
          +[](CartridgeForm& f) { f.charge_gr = 301.0; },
          +[](CartridgeForm& f) { f.coal_in = 7.5; }, +[](CartridgeForm& f) { f.cbto_in = -0.1; },
          +[](CartridgeForm& f) { f.velocity_sd_mps = 201.0; },
          +[](CartridgeForm& f) { f.velocity_es_mps = -1.0; }}) {
        CartridgeForm f = Handload();
        spoil(f);
        EXPECT_TRUE(Mentions(Validate(f), "handload")) << Validate(f);
    }
}

TEST_F(Handloads, ShareFileCarriesTheRecipeAndOldFilesStillImport) {
    const Id id = SaveCartridgeForm(db_, Handload()).value();
    const std::string json = ExportCartridgeJson(db_, id).value();
    const CartridgeForm g =
        LoadCartridgeForm(db_, ImportShareJson(db_, json).value().cartridge_id).value();
    EXPECT_EQ(g.powder, "Vihtavuori N150");
    EXPECT_DOUBLE_EQ(g.charge_gr, 44.5);
    EXPECT_DOUBLE_EQ(g.cbto_in, 2.235);
    EXPECT_DOUBLE_EQ(g.velocity_es_mps, 11.0);

    // A file from before v6: not stated; a damaged one: negatives as not stated.
    nlohmann::json doc = nlohmann::json::parse(json);
    for (const char* k : {"powder", "charge_kg", "primer", "brass", "coal_m", "cbto_m",
                          "velocity_sd_mps", "velocity_es_mps"}) {
        doc["cartridge"].erase(k);
    }
    const CartridgeForm old =
        LoadCartridgeForm(db_, ImportShareJson(db_, doc.dump()).value().cartridge_id).value();
    EXPECT_TRUE(old.powder.empty());
    EXPECT_DOUBLE_EQ(old.charge_gr, 0.0);
    doc["cartridge"]["charge_kg"] = -1.0;
    doc["cartridge"]["velocity_sd_mps"] = -3.0;
    const auto odd = ImportShareJson(db_, doc.dump());
    ASSERT_TRUE(odd.ok()) << odd.error().message;
    const CartridgeForm o = LoadCartridgeForm(db_, odd.value().cartridge_id).value();
    EXPECT_DOUBLE_EQ(o.charge_gr, 0.0);
    EXPECT_DOUBLE_EQ(o.velocity_sd_mps, 0.0);
}

TEST(HandloadMigration, VersionFiveCartridgeGetsAnEmptyRecipe) {
    const auto path = (std::filesystem::temp_directory_path() / "balcalc_v5_cartridge.db").string();
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
INSERT INTO bullet (id, name, caliber, diameter_m, mass_kg, drag_kind, drag_table, bc, source)
    VALUES (1, 'SMK 175', '.308', 0.00782, 0.01134, 'bc', 'G7', 0.243, 'user');
INSERT INTO cartridge (id, name, bullet_id, muzzle_velocity_mps, caliber)
    VALUES (1, 'Load', 1, 790, '.308 Win');
)sql")
                        .ok());
        ASSERT_TRUE(c.Close().ok());
    }
    {
        storage::Database db;
        ASSERT_TRUE(db.Open(path).ok());
        const CartridgeForm f = LoadCartridgeForm(db, 1).value();
        EXPECT_EQ(f.name, "Load");
        EXPECT_TRUE(f.powder.empty());
        EXPECT_DOUBLE_EQ(f.charge_gr, 0.0);
        EXPECT_DOUBLE_EQ(f.coal_in, 0.0);
        EXPECT_DOUBLE_EQ(f.velocity_sd_mps, 0.0);
        CartridgeForm more = f;
        more.powder = "N150";
        ASSERT_TRUE(SaveCartridgeForm(db, more).ok());
        EXPECT_EQ(LoadCartridgeForm(db, 1).value().powder, "N150");
    }
    std::filesystem::remove(path);
}

}  // namespace
}  // namespace ballistics::applogic
