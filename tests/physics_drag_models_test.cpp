// Multi-BC and custom-curve drag models.
#include <ballistics/drag.h>
#include <ballistics/solver.h>
#include <ballistics/units.h>

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace ballistics {
namespace {

struct Row {
    double slant_m, drop_m, speed_mps, time_s;
};

struct Case {
    const char* name;
    double zero_elevation_rad;
    std::vector<Row> rows;
};

const std::vector<Case>& Cases() {
    static const std::vector<Case> cases = {
#include "reference/reference_multibc.inc"
    };
    return cases;
}

// Must match MULTIBC in generate_reference.py.
Shot MultiBcShot(const Case& c) {
    Shot shot;
    if (std::string_view(c.name) == "smk175_sierra_g1") {
        shot.drag = DragModel::FromMultiBc(DragTableId::kG1,
                                           {{869.0, 0.505}, {701.0, 0.496}, {457.0, 0.485}});
        shot.muzzle_velocity_mps = 800.0;
        shot.sight_height_m = 0.05;
    } else {
        shot.drag = DragModel::FromMultiBc(DragTableId::kG7, {{800.0, 0.330}, {400.0, 0.300}});
        shot.muzzle_velocity_mps = 823.0;
        shot.sight_height_m = 0.045;
    }
    shot.atmosphere = StandardAtmosphere(0.0);
    shot.sound_speed = SoundSpeedModel::kDryAir;
    return shot;
}

TEST(PhysicsDragModels, MultiBcMatchesReference) {
    ASSERT_EQ(Cases().size(), 2U);
    for (const Case& c : Cases()) {
        Shot shot = MultiBcShot(c);
        shot.elevation_rad = c.zero_elevation_rad;
        const Trajectory traj = Fly(shot, c.rows.back().slant_m + 10.0);
        for (const Row& r : c.rows) {
            const auto pt = traj.AtSlantRange(r.slant_m);
            ASSERT_TRUE(pt) << c.name << " @" << r.slant_m;
            EXPECT_LE(std::fabs(pt->drop_m - r.drop_m) / r.slant_m, 0.01e-3)
                << c.name << " @" << r.slant_m << ": " << pt->drop_m << " vs " << r.drop_m;
            EXPECT_NEAR(pt->speed_mps, r.speed_mps, 1e-3 * r.speed_mps) << c.name;
            EXPECT_NEAR(pt->time_s, r.time_s, 1e-3 * r.time_s) << c.name;
        }
    }
}

double DropAt(const DragModel& drag, double range_m) {
    Shot shot;
    shot.drag = drag;
    shot.muzzle_velocity_mps = 800.0;
    shot.sight_height_m = 0.05;
    shot.atmosphere = StandardAtmosphere(0.0);
    shot.elevation_rad = FindZero(shot, 100.0).elevation_rad;
    return Fly(shot, range_m + 10.0).AtSlantRange(range_m)->drop_m;
}

TEST(PhysicsDragModels, SingleMultiBcPointEqualsPlainBc) {
    EXPECT_NEAR(DropAt(DragModel::FromMultiBc(DragTableId::kG7, {{700.0, 0.243}}), 1500.0),
                DropAt(DragModel::FromBc(DragTableId::kG7, 0.243), 1500.0), 1e-6);
}

TEST(PhysicsDragModels, LowerBcAtLowSpeedDropsMore) {
    const double flat = DropAt(DragModel::FromBc(DragTableId::kG7, 0.243), 1500.0);
    const double falling =
        DropAt(DragModel::FromMultiBc(DragTableId::kG7, {{800.0, 0.243}, {400.0, 0.220}}), 1500.0);
    EXPECT_LT(falling, flat - 0.1);
}

TEST(PhysicsDragModels, CustomCurveWithMatchingSectionalDensityEqualsBc) {
    // A G7 "measured" curve for a bullet whose m/d^2 equals BC 0.243
    // behaves exactly like the published G7 BC.
    const double d = units::InchToM(0.308);
    const double mass = units::BcToSi(0.243) * d * d;
    const DragModel custom = DragModel::FromCurve(StandardDragTable(DragTableId::kG7), mass, d);
    EXPECT_NEAR(DropAt(custom, 2000.0), DropAt(DragModel::FromBc(DragTableId::kG7, 0.243), 2000.0),
                1e-6);
}

TEST(PhysicsDragModels, FormFactorScalesDrag) {
    const double d = units::InchToM(0.308);
    const double m = units::GrainToKg(175.0);
    const auto table = StandardDragTable(DragTableId::kG7);
    const DragModel base = DragModel::FromCurve(table, m, d, 1.0);
    const DragModel draggy = DragModel::FromCurve(table, m, d, 1.1);
    EXPECT_NEAR(draggy.Coefficient(2.0) / base.Coefficient(2.0), 1.1, 1e-12);
}

TEST(PhysicsDragModels, RejectsInvalidInput) {
    EXPECT_THROW(DragModel::FromBc(DragTableId::kG1, 0.0), std::invalid_argument);
    EXPECT_THROW(DragModel::FromMultiBc(DragTableId::kG1, {}), std::invalid_argument);
    EXPECT_THROW(DragCurve({{1.0, 0.3}}), std::invalid_argument);
    EXPECT_THROW(DragCurve({{1.0, 0.3}, {1.0, 0.4}}), std::invalid_argument);
}
TEST(PhysicsDsf, FactorInterpolatesAndHoldsTheEnds) {
    EXPECT_DOUBLE_EQ(DsfFactor({}, 1.0), 1.0);
    const std::vector<DsfPoint> t = {{0.9, 1.10}, {1.1, 1.04}, {1.4, 1.00}};
    EXPECT_DOUBLE_EQ(DsfFactor(t, 0.5), 1.10);
    EXPECT_DOUBLE_EQ(DsfFactor(t, 0.9), 1.10);
    EXPECT_NEAR(DsfFactor(t, 1.0), 1.07, 1e-12);
    EXPECT_NEAR(DsfFactor(t, 1.25), 1.02, 1e-12);
    EXPECT_DOUBLE_EQ(DsfFactor(t, 3.0), 1.00);
}

TEST(PhysicsDsf, ScalesTheDragAtEachMach) {
    const DragModel m = DragModel::FromBc(DragTableId::kG7, 0.243);
    const DragModel scaled = m.WithMachScale({{1.4, 1.0}, {0.9, 1.2}}); // unsorted on purpose
    ASSERT_EQ(scaled.mach_scale().size(), 2u);
    EXPECT_DOUBLE_EQ(scaled.mach_scale().front().mach, 0.9);
    EXPECT_DOUBLE_EQ(scaled.Coefficient(2.0), m.Coefficient(2.0));
    EXPECT_NEAR(scaled.Coefficient(0.8), 1.2 * m.Coefficient(0.8), 1e-15);
    EXPECT_NEAR(scaled.Coefficient(1.15), 1.1 * m.Coefficient(1.15), 1e-15);
    // Together with the overall scale.
    EXPECT_NEAR(m.Scaled(1.05).WithMachScale({{1.0, 1.2}}).Coefficient(0.5),
                1.05 * 1.2 * m.Coefficient(0.5), 1e-15);
    EXPECT_THROW(m.WithMachScale({{1.0, 0.0}}), std::invalid_argument);
}

} // namespace
} // namespace ballistics
