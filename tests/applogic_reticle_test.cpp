#include <ballistics/applogic/importers.h>
#include <ballistics/applogic/reticle.h>
#include <ballistics/storage/database.h>
#include <ballistics/storage/repository.h>
#include <ballistics/units.h>
#include <gtest/gtest.h>
#include <sqlite_manager/connection.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../storage/src/schema.h"

namespace ballistics::applogic {
namespace {

using units::MoaToRad;
using units::MradToRad;

storage::ScopeRecord MilScope() {
    storage::ScopeRecord s;
    s.click_vertical_rad = s.click_horizontal_rad = MradToRad(0.1);
    return s;
}

TEST(Reticle, HoldAllPutsTheTargetBelowAndAgainstTheWind) {
    // Needs 3.2 mrad up and 0.8 mrad right: the target sits 3.2 below and
    // 0.8 left of the centre.
    const ReticleHold h =
        ComputeReticleHold(MradToRad(3.2), MradToRad(0.8), MilScope(), 10.0, HoldMode::kHoldAll);
    EXPECT_DOUBLE_EQ(h.dial_elevation_clicks, 0.0);
    EXPECT_NEAR(h.target_x, -0.8, 1e-12);
    EXPECT_NEAR(h.target_y, -3.2, 1e-12);
}

TEST(Reticle, DialElevationHoldsTheClickRemainderAndWind) {
    const ReticleHold h = ComputeReticleHold(MradToRad(5.47), MradToRad(-0.6), MilScope(), 10.0,
                                             HoldMode::kDialElevation);
    EXPECT_DOUBLE_EQ(h.dial_elevation_clicks, 55.0);  // 5.5 mrad dialled
    EXPECT_NEAR(h.target_y, 0.03, 1e-9);              // 0.03 over-dialled: hold a hair high
    EXPECT_NEAR(h.target_x, 0.6, 1e-12);              // wind from the left: hold right
    EXPECT_DOUBLE_EQ(h.dial_windage_clicks, 0.0);
}

TEST(Reticle, DialAllUsesBothTurrets) {
    storage::ScopeRecord moa;
    moa.click_vertical_rad = moa.click_horizontal_rad = MoaToRad(0.25);
    const ReticleHold h =
        ComputeReticleHold(MoaToRad(20.1), MoaToRad(2.0), moa, 10.0, HoldMode::kDialAll);
    EXPECT_DOUBLE_EQ(h.dial_elevation_clicks, 80.0);
    EXPECT_DOUBLE_EQ(h.dial_windage_clicks, 8.0);
    EXPECT_NEAR(h.target_y, -units::RadToMrad(MoaToRad(0.1)), 1e-9);
    EXPECT_NEAR(h.target_x, 0.0, 1e-12);
}

TEST(Reticle, SecondFocalPlaneScalesWithMagnification) {
    storage::ScopeRecord sfp = MilScope();
    sfp.focal_plane = "sfp";
    sfp.sfp_reference_magnification = 20.0;
    // At 10x the marks cover twice their value: a 4 mrad hold is the 2 mark.
    EXPECT_DOUBLE_EQ(SubtensionScale(sfp, 10.0), 2.0);
    const ReticleHold h = ComputeReticleHold(MradToRad(4.0), 0.0, sfp, 10.0, HoldMode::kHoldAll);
    EXPECT_NEAR(h.target_y, -2.0, 1e-12);
    // At the reference magnification the marks are true; FFP never scales.
    EXPECT_DOUBLE_EQ(SubtensionScale(sfp, 20.0), 1.0);
    EXPECT_DOUBLE_EQ(SubtensionScale(MilScope(), 5.0), 1.0);
    sfp.sfp_reference_magnification = 0.0;  // unknown: no scaling
    EXPECT_DOUBLE_EQ(SubtensionScale(sfp, 5.0), 1.0);
}

TEST(Reticle, HoldModeNames) {
    for (HoldMode m : {HoldMode::kDialElevation, HoldMode::kHoldAll, HoldMode::kDialAll}) {
        EXPECT_EQ(HoldModeFromString(ToString(m)), m);
    }
    EXPECT_EQ(HoldModeFromString("nonsense"), HoldMode::kDialElevation);
}

TEST(Reticle, ScopeFocalPlaneIsStored) {
    storage::Database db;
    ASSERT_TRUE(db.Open(":memory:").ok());
    storage::ScopeRecord s = MilScope();
    s.name = "SFP scope";
    s.focal_plane = "sfp";
    s.sfp_reference_magnification = 14.0;
    ASSERT_TRUE(storage::Repository<storage::ScopeRecord>(db).Save(s).ok());
    const auto got = *storage::Repository<storage::ScopeRecord>(db).Get(s.id).value();
    EXPECT_EQ(got.focal_plane, "sfp");
    EXPECT_DOUBLE_EQ(got.sfp_reference_magnification, 14.0);
    s.focal_plane = "middle";
    EXPECT_FALSE(storage::Repository<storage::ScopeRecord>(db).Save(s).ok());  // CHECK
}

TEST(Reticle, VersionOneDatabaseIsUpgraded) {
    const auto path = (std::filesystem::temp_directory_path() / "balcalc_v1.db").string();
    std::filesystem::remove(path);
    {
        // A v1 file: the first migration only.
        sqlite_manager::Connection c;
        ASSERT_TRUE(c.Open(path).ok());
        ASSERT_TRUE(c.Execute(storage::detail::Migrations().front().sql).ok());
        ASSERT_TRUE(c.Execute("INSERT INTO scope (name, click_vertical_rad, click_horizontal_rad)"
                              " VALUES ('old', 0.0001, 0.0001);"
                              "PRAGMA user_version = 1;")
                        .ok());
        ASSERT_TRUE(c.Close().ok());
    }
    {
        storage::Database db;
        ASSERT_TRUE(db.Open(path).ok());
        EXPECT_EQ(db.SchemaVersion().value(), storage::Database::LatestSchemaVersion());
        const auto scopes = storage::Repository<storage::ScopeRecord>(db).List().value();
        ASSERT_EQ(scopes.size(), 1U);
        EXPECT_EQ(scopes[0].focal_plane, "ffp");  // default for existing scopes
    }
    // Closed first: Windows cannot delete a file that is still open.
    std::error_code ec;
    std::filesystem::remove(path, ec);
    EXPECT_FALSE(ec) << ec.message();
}

// The generic reticles (data/seed/make_reticles.py): a hold is read against
// their marks, so every mark must sit at a whole number of steps - checked
// on the parsed drawing, in mrad, as the app draws it.
struct Generic {
    const char* file;
    double step;  // in the reticle's unit
    bool moa;
    double right;  // marks on the horizontal axis to here (unit)
    double down;   // marks or rows below the centre to here (unit)
};

TEST(Reticle, GenericReticlesHaveTheirMarksWhereTheirNamesSay) {
    const std::array<Generic, 9> all = {{
        {"generic-mrad-hash-0.5", 0.5, false, 10, 10},
        {"generic-mrad-hash-0.2", 0.2, false, 10, 10},
        {"generic-mrad-tree-0.2", 0.2, false, 6, 10},
        {"generic-mrad-tree-0.5", 0.5, false, 7, 12},
        {"generic-mrad-grid-0.5", 0.5, false, 6, 10},
        {"generic-moa-hash-1", 1, true, 30, 30},
        {"generic-moa-hash-2", 2, true, 40, 40},
        {"generic-moa-tree-2", 2, true, 24, 40},
        {"generic-moa-grid-2", 2, true, 20, 34},
    }};
    for (const Generic& g : all) {
        SCOPED_TRACE(g.file);
        std::ifstream in(std::filesystem::path(BALLISTICS_SEED_DIR) / "reticle" /
                         (std::string(g.file) + ".reticle"));
        std::stringstream text;
        text << in.rdbuf();
        auto parsed = ParseReticle(text.str());
        ASSERT_TRUE(parsed.ok()) << parsed.error().message;
        EXPECT_EQ(parsed.value().units, g.moa ? "moa" : "mrad");
        const double k = g.moa ? units::RadToMrad(MoaToRad(1.0)) : 1.0;  // unit -> mrad
        const auto def = nlohmann::json::parse(parsed.value().definition);

        // Positions of the marks: short vertical lines crossing the horizontal
        // axis, short horizontal lines crossing the vertical axis, dots.
        std::vector<double> on_x;
        std::vector<double> on_y;
        std::vector<std::pair<double, double>> dots;
        for (const auto& e : def.at("elements")) {
            if (e.at("t") == "line") {
                const double x1 = e.at("x1"), x2 = e.at("x2"), y1 = e.at("y1"), y2 = e.at("y2");
                if (std::abs(x1 - x2) < 1e-9 && y1 * y2 <= 0 && std::abs(y1 - y2) < 2 * k) {
                    on_x.push_back(x1);
                }
                if (std::abs(y1 - y2) < 1e-9 && x1 * x2 <= 0 && std::abs(x1 - x2) < 2 * k) {
                    on_y.push_back(y1);
                }
            } else if (e.at("t") == "circle" && e.at("fill").get<bool>()) {
                dots.emplace_back(e.at("x"), e.at("y"));
            }
        }
        const auto has = [](const std::vector<double>& v, double at) {
            return std::any_of(v.begin(), v.end(),
                               [&](double p) { return std::abs(p - at) < 1e-6; });
        };
        // Every step is marked, right and left...
        for (int i = 1; i * g.step <= g.right + 1e-9; ++i) {
            EXPECT_TRUE(has(on_x, i * g.step * k)) << "no mark at " << i * g.step << " right";
            EXPECT_TRUE(has(on_x, -i * g.step * k)) << "no mark at " << i * g.step << " left";
        }
        // ...and down the vertical axis (a tree marks every step there too).
        for (int i = 1; i * g.step <= g.down + 1e-9; ++i) {
            EXPECT_TRUE(has(on_y, -i * g.step * k)) << "no mark at " << i * g.step << " down";
        }
        // Nothing off the steps: every mark and dot is a whole number of them.
        const auto whole = [&](double v) {
            return std::abs(v / (g.step * k) - std::round(v / (g.step * k))) < 1e-6;
        };
        for (double v : on_x) {
            EXPECT_TRUE(whole(v)) << v;
        }
        for (double v : on_y) {
            EXPECT_TRUE(whole(v)) << v;
        }
        for (const auto& [x, y] : dots) {
            EXPECT_TRUE(whole(x) && whole(y)) << x << ", " << y;
        }
    }
}

}  // namespace
}  // namespace ballistics::applogic
