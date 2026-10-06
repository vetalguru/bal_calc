#include <ballistics/analysis.h>
#include <ballistics/atmosphere.h>
#include <ballistics/drag.h>
#include <ballistics/solver.h>
#include <ballistics/units.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace ballistics {
namespace {

Shot Zeroed308(double zero_range_m) {
    Shot shot;
    shot.drag = DragModel::FromBc(DragTableId::kG7, 0.243);
    shot.muzzle_velocity_mps = 800.0;
    shot.mass_kg = units::GrainToKg(175.0);
    shot.sight_height_m = 0.05;
    shot.atmosphere = StandardAtmosphere(0.0);
    shot.elevation_rad = FindZero(shot, zero_range_m).elevation_rad;
    return shot;
}

TEST(PhysicsDensityAltitude, StandardAirIsItsOwnAltitude) {
    for (double h : {0.0, 500.0, 1000.0, 2000.0, 3000.0, 4500.0}) {
        // ICAO density vs CIPM-2007 real gas: a few metres at most.
        EXPECT_NEAR(DensityAltitude(StandardAtmosphere(h)), h, 10.0) << h;
    }
}

TEST(PhysicsDensityAltitude, HotHumidAirIsHigher) {
    Atmosphere hot = StandardAtmosphere(0.0);
    hot.temperature_k = units::CToK(35.0);
    hot.humidity = 0.8;
    // About 120 ft per degree C above standard: 20 C -> ~750 m, humidity adds.
    EXPECT_GT(DensityAltitude(hot), 750.0);
    EXPECT_LT(DensityAltitude(hot), 1100.0);
}

TEST(PhysicsDensityAltitude, PressureRoundTrip) {
    for (double t_c : {-20.0, 0.0, 15.0, 35.0}) {
        for (double humidity : {0.0, 0.5, 1.0}) {
            const Atmosphere air{300.0, 95000.0, units::CToK(t_c), humidity};
            const double p = StationPressureFromDensityAltitude(DensityAltitude(air),
                                                                air.temperature_k, humidity);
            EXPECT_NEAR(p, air.pressure_pa, 0.5) << t_c << " " << humidity;
        }
    }
}

// Brute force over a dense table: the analysis must find the same points.
TEST(PhysicsAnalysis, MaxOrdinateMatchesDenseScan) {
    for (double zero : {100.0, 200.0, 300.0}) {
        const Trajectory traj = Fly(Zeroed308(zero), 1000.0);
        double best_h = -1e9, best_r = 0.0;
        for (const TrajectoryPoint& p : traj.Table(0.02, 600.0)) {
            if (p.drop_m > best_h) {
                best_h = p.drop_m;
                best_r = p.slant_range_m;
            }
        }
        const Apex apex = MaxOrdinate(traj, 600.0);
        EXPECT_NEAR(apex.height_m, best_h, 1e-5) << zero;
        // The top is flat: its range is known less precisely than its height.
        EXPECT_NEAR(apex.slant_range_m, best_r, 0.5) << zero;
        EXPECT_GT(apex.slant_range_m, 0.4 * zero);
        EXPECT_LT(apex.slant_range_m, zero);
    }
}

TEST(PhysicsAnalysis, MaxOrdinateIsCappedAtTheTarget) {
    const Trajectory traj = Fly(Zeroed308(300.0), 1000.0);
    // Short of the apex the highest point is the target itself.
    const Apex apex = MaxOrdinate(traj, 50.0);
    EXPECT_NEAR(apex.slant_range_m, 50.0, 0.02);
    EXPECT_NEAR(apex.height_m, traj.AtSlantRange(50.0)->drop_m, 1e-6);
}

TEST(PhysicsAnalysis, PointBlankMatchesDenseScan) {
    for (double half : {0.05, 0.10, 0.20}) {
        const Trajectory traj = Fly(Zeroed308(200.0), 1000.0);
        double near = -1.0, far = -1.0;
        for (const TrajectoryPoint& p : traj.Table(0.02, 1000.0)) {
            const bool in = std::abs(p.drop_m) <= half;
            if (near < 0.0 && in) {
                near = p.slant_range_m;
            } else if (near >= 0.0 && far < 0.0 && !in) {
                far = p.slant_range_m;
            }
        }
        const auto pbr = PointBlankRange(traj, half, 1000.0);
        ASSERT_TRUE(pbr) << half;
        EXPECT_NEAR(pbr->near_m, near, 0.05) << half;
        EXPECT_NEAR(pbr->far_m, far, 0.05) << half;
    }
}

TEST(PhysicsAnalysis, PointBlankWithTheSightInsideStartsAtTheMuzzle) {
    const Trajectory traj = Fly(Zeroed308(200.0), 1000.0);
    const auto pbr = PointBlankRange(traj, 0.10, 1000.0); // sight 5 cm < 10 cm
    ASSERT_TRUE(pbr);
    EXPECT_DOUBLE_EQ(pbr->near_m, 0.0);
    EXPECT_GT(pbr->far_m, 200.0);
    EXPECT_LT(pbr->far_m, 400.0);
}

TEST(PhysicsAnalysis, PointBlankBeyondTheEnd) {
    const Trajectory traj = Fly(Zeroed308(100.0), 1000.0);
    const auto pbr = PointBlankRange(traj, 5.0, 300.0); // a 10 m tall "target"
    ASSERT_TRUE(pbr);
    EXPECT_DOUBLE_EQ(pbr->far_m, 300.0);
}

} // namespace
} // namespace ballistics
