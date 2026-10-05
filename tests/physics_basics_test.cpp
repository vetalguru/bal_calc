#include <ballistics/atmosphere.h>
#include <ballistics/drag.h>
#include <ballistics/solver.h>
#include <ballistics/units.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace ballistics {
namespace {

TEST(PhysicsAtmosphere, IcaoSeaLevel) {
    const Atmosphere a = StandardAtmosphere(0.0);
    EXPECT_DOUBLE_EQ(a.pressure_pa, 101325.0);
    EXPECT_DOUBLE_EQ(a.temperature_k, 288.15);
    // ICAO: 1.2250 kg/m^3 (ideal gas), 340.29 m/s. CIPM-2007 includes the
    // real-gas compressibility (Z ~ 0.9996), so it reads ~0.04 % higher.
    EXPECT_NEAR(AirDensity(a.temperature_k, a.pressure_pa, 0.0), 1.2250, 0.001);
    EXPECT_NEAR(SpeedOfSound(a.temperature_k, a.pressure_pa, 0.0), 340.29, 0.05);
}

TEST(PhysicsAtmosphere, IcaoAt2000m) {
    const Atmosphere a = StandardAtmosphere(2000.0);
    // ICAO tables: 275.15 K, 79495 Pa, 1.0066 kg/m^3, 332.53 m/s.
    EXPECT_NEAR(a.temperature_k, 275.15, 1e-9);
    EXPECT_NEAR(a.pressure_pa, 79495.0, 5.0);
    EXPECT_NEAR(AirDensity(a.temperature_k, a.pressure_pa, 0.0), 1.0066, 0.001);
    EXPECT_NEAR(SpeedOfSound(a.temperature_k, a.pressure_pa, 0.0), 332.53, 0.05);
}

TEST(PhysicsAtmosphere, HumidityLowersDensityRaisesSoundSpeed) {
    const double t = units::CToK(30.0);
    const double dry = AirDensity(t, 101325.0, 0.0);
    const double wet = AirDensity(t, 101325.0, 1.0);
    // Saturated air at 30 C is ~1.6 % lighter (CIPM-2007).
    EXPECT_NEAR((dry - wet) / dry, 0.0160, 0.001);
    EXPECT_GT(SpeedOfSound(t, 101325.0, 1.0), SpeedOfSound(t, 101325.0, 0.0) + 1.0);
}

TEST(PhysicsAtmosphere, ModelMatchesStandardAtAltitude) {
    const AtmosphereModel model(StandardAtmosphere(0.0));
    const Atmosphere at = StandardAtmosphere(1000.0);
    const AirState s = model.At(1000.0);
    EXPECT_NEAR(s.density_kg_m3, AirDensity(at.temperature_k, at.pressure_pa, 0.0), 1e-9);
}

TEST(PhysicsDrag, CurvePassesThroughTablePoints) {
    const auto table = StandardDragTable(DragTableId::kG7);
    const DragCurve curve(table);
    for (const auto& p : table) {
        EXPECT_NEAR(curve.Cd(p.mach), p.cd, 1e-12);
    }
}

TEST(PhysicsDrag, CurveIsMonotoneBetweenPoints) {
    const auto table = StandardDragTable(DragTableId::kG1);
    const DragCurve curve(table);
    for (std::size_t i = 0; i + 1 < table.size(); ++i) {
        const double lo = std::min(table[i].cd, table[i + 1].cd);
        const double hi = std::max(table[i].cd, table[i + 1].cd);
        for (int k = 1; k < 10; ++k) {
            const double m = table[i].mach + (table[i + 1].mach - table[i].mach) * k / 10.0;
            EXPECT_GE(curve.Cd(m), lo - 1e-12);
            EXPECT_LE(curve.Cd(m), hi + 1e-12);
        }
    }
}

TEST(PhysicsDrag, StandardTablesAreAvailable) {
    for (auto id : {DragTableId::kG1, DragTableId::kG2, DragTableId::kG5, DragTableId::kG6,
                    DragTableId::kG7, DragTableId::kG8, DragTableId::kGI, DragTableId::kGS,
                    DragTableId::kRA4}) {
        EXPECT_GT(StandardDragTable(id).size(), 20U) << DragTableName(id);
    }
}

Shot Sample308() {
    Shot shot;
    shot.drag = DragModel::FromBc(DragTableId::kG7, 0.243);
    shot.muzzle_velocity_mps = 800.0;
    shot.mass_kg = units::GrainToKg(175.0);
    shot.sight_height_m = 0.05;
    shot.atmosphere = StandardAtmosphere(0.0);
    return shot;
}

TEST(PhysicsSolver, VacuumMatchesClosedForm) {
    Shot shot = Sample308();
    shot.atmosphere.pressure_pa = 1e-9; // no air
    shot.sight_height_m = 0.0;
    shot.elevation_rad = units::DegToRad(2.0);
    const Trajectory traj = Fly(shot, 1000.0);
    const double vx = 800.0 * std::cos(shot.elevation_rad);
    const double vy = 800.0 * std::sin(shot.elevation_rad);
    const auto pt = traj.AtSlantRange(1000.0);
    ASSERT_TRUE(pt);
    const double t = 1000.0 / vx;
    EXPECT_NEAR(pt->time_s, t, 1e-9);
    EXPECT_NEAR(pt->drop_m, vy * t - 0.5 * kStandardGravity * t * t, 1e-6);
}

TEST(PhysicsSolver, ZeroPutsImpactOnLineOfSight) {
    Shot shot = Sample308();
    const ZeroResult zero = FindZero(shot, 300.0);
    ASSERT_TRUE(zero.converged);
    shot.elevation_rad = zero.elevation_rad;
    const auto pt = Fly(shot, 400.0).AtSlantRange(300.0);
    ASSERT_TRUE(pt);
    EXPECT_NEAR(pt->drop_m, 0.0, 1e-8);
}

TEST(PhysicsSolver, ZeroOffsetRaisesImpact) {
    Shot shot = Sample308();
    const ZeroResult zero = FindZero(shot, 100.0, 0.03);
    ASSERT_TRUE(zero.converged);
    shot.elevation_rad = zero.elevation_rad;
    EXPECT_NEAR(Fly(shot, 150.0).AtSlantRange(100.0)->drop_m, 0.03, 1e-8);
}

TEST(PhysicsSolver, StepRefinementConverges) {
    // Plan criterion: <= 1 mm at 2500 m between default and much tighter tolerance.
    Shot shot = Sample308();
    shot.elevation_rad = FindZero(shot, 100.0).elevation_rad;
    SolverOptions fine;
    fine.relative_tolerance = 1e-13;
    fine.position_tolerance_m = 1e-10;
    fine.velocity_tolerance_mps = 1e-10;
    const auto a = Fly(shot, 2510.0).AtSlantRange(2500.0);
    const auto b = Fly(shot, 2510.0, fine).AtSlantRange(2500.0);
    ASSERT_TRUE(a && b);
    EXPECT_NEAR(a->drop_m, b->drop_m, 1e-3);
}

TEST(PhysicsSolver, StopsBelowMinimumSpeed) {
    Shot shot = Sample308();
    shot.muzzle_velocity_mps = 300.0;
    SolverOptions o;
    o.min_speed_mps = 250.0;
    const Trajectory traj = Fly(shot, 5000.0, o);
    EXPECT_EQ(traj.stop_reason(), StopReason::kMinSpeed);
    EXPECT_FALSE(traj.AtSlantRange(4000.0).has_value());
}

TEST(PhysicsSolver, TableEvery100mTo2500m) {
    Shot shot = Sample308();
    shot.elevation_rad = FindZero(shot, 100.0).elevation_rad;
    const auto table = Fly(shot, 2510.0).Table(100.0, 2500.0);
    ASSERT_EQ(table.size(), 26U);
    EXPECT_NEAR(table[1].drop_m, 0.0, 1e-8);
    for (std::size_t i = 2; i < table.size(); ++i) {
        EXPECT_LT(table[i].drop_m, table[i - 1].drop_m);
        EXPECT_LT(table[i].speed_mps, table[i - 1].speed_mps);
        EXPECT_GT(table[i].hold_elevation_rad, 0.0);
    }
}

TEST(PhysicsSolver, FastEnoughForPhones) {
    Shot shot = Sample308();
    shot.elevation_rad = FindZero(shot, 100.0).elevation_rad;
    const auto start = std::chrono::steady_clock::now();
    const auto table = Fly(shot, 2510.0).Table(10.0, 2500.0);
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
            .count();
    EXPECT_EQ(table.size(), 251U);
    // Desktop budget; phones are ~5x slower and the plan allows 50 ms there.
    EXPECT_LT(ms, 10.0);
}

} // namespace
} // namespace ballistics
