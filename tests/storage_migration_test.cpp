// Schema v3: a v2 database (every profile owning its rifle, scope and
// cartridge) becomes independent rifles and cartridges plus their pairs.
#include <ballistics/storage/database.h>
#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include <sqlite_manager/connection.h>
#include <sqlite_manager/statement.h>

#include "../storage/src/schema.h"

namespace ballistics::storage {
namespace {

TEST(StorageMigration, VersionTwoProfilesBecomeRiflesCartridgesAndPairs) {
    const auto path = (std::filesystem::temp_directory_path() / "balcalc_v2.db").string();
    std::filesystem::remove(path);
    {
        sqlite_manager::Connection c;
        ASSERT_TRUE(c.Open(path).ok());
        for (const detail::Migration& m : detail::Migrations()) {
            if (m.version <= 2) {
                ASSERT_TRUE(c.Execute(m.sql).ok()) << "v" << m.version;
            }
        }
        ASSERT_TRUE(c.Execute(R"sql(
PRAGMA user_version = 2;
INSERT INTO bullet (id, name, caliber, diameter_m, mass_kg, drag_kind, drag_table, bc, source)
    VALUES (1, 'SMK 175', '.308', 0.00782, 0.01134, 'bc', 'G7', 0.243, 'user'),
           (2, 'ELD-M 140', '6.5mm', 0.00671, 0.00907, 'bc', 'G7', 0.326, 'user');
INSERT INTO cartridge (id, name, bullet_id, muzzle_velocity_mps)
    VALUES (1, 'Tikka', 1, 790), (2, 'Bergara', 2, 830);
INSERT INTO rifle (id, name, caliber, twist_m, sight_height_m)
    VALUES (1, 'Tikka', '.308 Win', 0.254, 0.045), (2, 'Bergara', '', 0.203, 0.05);
INSERT INTO scope (id, name, click_units, click_vertical_rad, click_horizontal_rad)
    VALUES (1, 'Tikka', 'moa', 0.0000727, 0.0000727), (2, 'Bergara', 'mrad', 0.0001, 0.0001);
INSERT INTO profile (id, name, rifle_id, scope_id, cartridge_id, zero_range_m,
                     zero_offset_up_m, zero_altitude_m, zero_pressure_pa, zero_temperature_k,
                     zero_humidity, zero_powder_temp_k, velocity_scale, drag_scale)
    VALUES (1, 'Tikka', 1, 1, 1, 200, 0.01, 350, 97000, 268.15, 0.4, 270.15, 1.01, 0.98),
           (2, 'Bergara', 2, 2, 2, 100, 0, 0, 101325, 288.15, 0.5, 288.15, 1, 1);
INSERT INTO dope_log (profile_id, range_m, observed_elevation_rad, altitude_m, pressure_pa,
                      temperature_k, humidity)
    VALUES (1, 600, 0.004, 0, 101325, 288.15, 0.5), (1, 800, 0.0065, 0, 101325, 288.15, 0.5);
)sql")
                        .ok());
        ASSERT_TRUE(c.Close().ok());
    }
    {
        Database db;
        ASSERT_TRUE(db.Open(path).ok());
        EXPECT_EQ(db.SchemaVersion().value(), 3);

        const LoadedProfile tikka = LoadProfile(db, 1).value();
        EXPECT_EQ(tikka.rifle.name, "Tikka");
        EXPECT_DOUBLE_EQ(tikka.rifle.zero_range_m, 200.0);
        EXPECT_DOUBLE_EQ(tikka.rifle.zero_atmosphere.altitude_m, 350.0);
        EXPECT_DOUBLE_EQ(tikka.rifle.zero_atmosphere.pressure_pa, 97000.0);
        EXPECT_DOUBLE_EQ(tikka.rifle.zero_atmosphere.temperature_k, 268.15);
        EXPECT_DOUBLE_EQ(tikka.rifle.zero_atmosphere.humidity, 0.4);
        EXPECT_DOUBLE_EQ(tikka.rifle.zero_powder_temp_k, 270.15);
        ASSERT_TRUE(tikka.scope.has_value());
        EXPECT_EQ(tikka.scope->click_units, "moa");
        // What depends on the pair stays with it.
        EXPECT_DOUBLE_EQ(tikka.profile.zero_offset_up_m, 0.01);
        EXPECT_DOUBLE_EQ(tikka.profile.velocity_scale, 1.01);
        EXPECT_DOUBLE_EQ(tikka.profile.drag_scale, 0.98);
        // Calibre: the rifle's, or the bullet's when the rifle has none.
        EXPECT_EQ(tikka.cartridge.caliber, ".308 Win");
        const LoadedProfile bergara = LoadProfile(db, 2).value();
        EXPECT_EQ(bergara.cartridge.caliber, "6.5mm");
        EXPECT_EQ(bergara.scope->click_units, "mrad");

        // The shot log survived (no cascade from the column changes).
        auto count = sqlite_manager::Statement::Prepare(
            db.connection(), "SELECT count(*) FROM dope_log WHERE profile_id = 1");
        ASSERT_TRUE(count.ok());
        ASSERT_TRUE(count.value().Step().ok());
        EXPECT_EQ(count.value().ColumnInt64(0), 2);

        ConditionsRecord air;
        air.atmosphere = tikka.rifle.zero_atmosphere;
        auto sol = Solve(tikka, air, 1000.0);
        ASSERT_TRUE(sol.ok()) << sol.error().message;
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
    EXPECT_FALSE(ec) << ec.message();
}

} // namespace
} // namespace ballistics::storage
