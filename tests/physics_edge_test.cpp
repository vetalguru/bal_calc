// Edge cases of the physics core: degenerate inputs, flights that end
// early, and the paths the reference-driven tests never take.
#include <ballistics/analysis.h>
#include <ballistics/bc.h>
#include <ballistics/drag.h>
#include <ballistics/effects.h>
#include <ballistics/solver.h>
#include <ballistics/units.h>
#include <ballistics/wez.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace ballistics {
namespace {

// Not one of the enumerators: what a corrupt or newer file could carry.
constexpr auto kBogusTable = static_cast<DragTableId>(99);

Shot Sample308() {
    Shot shot;
    shot.drag = DragModel::FromBc(DragTableId::kG7, 0.243);
    shot.muzzle_velocity_mps = 800.0;
    shot.mass_kg = units::GrainToKg(175.0);
    shot.bullet_diameter_m = units::InchToM(0.308);
    shot.bullet_length_m = units::InchToM(1.24);
    shot.sight_height_m = 0.05;
    shot.atmosphere = StandardAtmosphere(0.0);
    return shot;
}

// A shot in empty, weightless space: it flies a straight line along the
// bore at constant speed, so drop at range R is R * tan(elevation).
Shot StraightLine(double speed_mps) {
    Shot shot;
    shot.drag = DragModel::FromBc(DragTableId::kG7, 0.5);
    shot.muzzle_velocity_mps = speed_mps;
    shot.atmosphere = StandardAtmosphere(0.0);
    shot.atmosphere.pressure_pa = 1e-9;  // no air
    shot.gravity_mps2 = 0.0;
    shot.spin_drift = false;
    shot.aerodynamic_jump = false;
    return shot;
}

// --- Units ------------------------------------------------------------------

TEST(PhysicsEdgeUnits, PressureConversionsRoundTrip) {
    double hpa = 1013.25;  // not constexpr: converted at run time
    EXPECT_DOUBLE_EQ(units::HpaToPa(hpa), 101325.0);
    EXPECT_NEAR(units::PaToMmHg(units::MmHgToPa(760.0)), 760.0, 1e-12);
    EXPECT_NEAR(units::InHgToPa(29.92), units::HpaToPa(hpa), 15.0);  // 29.92 inHg ~ 1013 hPa
}

// --- Drag tables and curves -------------------------------------------------

TEST(PhysicsEdgeDrag, UnknownTableHasNoNameAndNoPoints) {
    EXPECT_STREQ(DragTableName(kBogusTable), "?");
    EXPECT_TRUE(StandardDragTable(kBogusTable).empty());
    // A BC against no reference curve is no drag model.
    EXPECT_THROW(DragModel::FromBc(kBogusTable, 0.3), std::invalid_argument);
    EXPECT_THROW(DragModel::FromMultiBc(kBogusTable, {{0.3, 800.0}}), std::invalid_argument);
}

TEST(PhysicsEdgeDrag, TwoPointCurveIsAStraightLine) {
    const DragCurve c({{0.5, 0.2}, {1.5, 0.4}});
    EXPECT_DOUBLE_EQ(c.Cd(0.5), 0.2);
    EXPECT_NEAR(c.Cd(1.0), 0.3, 1e-15);
    EXPECT_NEAR(c.Cd(0.75), 0.25, 1e-15);
    EXPECT_NEAR(c.Cd(1.25), 0.35, 1e-15);
    // Clamped outside.
    EXPECT_DOUBLE_EQ(c.Cd(0.1), 0.2);
    EXPECT_DOUBLE_EQ(c.Cd(3.0), 0.4);
}

TEST(PhysicsEdgeDrag, EndSlopeIsLimitedToKeepTheEndSegmentMonotone) {
    // A long rise then a sharp, short fall: the three-point end slope
    // would be ~2.2 (11 x the secant 0.2), the limiter caps it at 3 x 0.2.
    const DragCurve c({{0.5, 0.2}, {1.5, 0.4}, {1.6, 0.2}});
    const double eps = 1e-7;
    EXPECT_NEAR((c.Cd(0.5 + eps) - c.Cd(0.5)) / eps, 0.6, 1e-5);
    // No overshoot: rising and inside [0.2, 0.4] across the first segment.
    double prev = c.Cd(0.5);
    for (int i = 51; i <= 150; ++i) {
        const double m = i / 100.0;
        const double cd = c.Cd(m);
        EXPECT_GE(cd, prev - 1e-15) << m;
        EXPECT_GE(cd, 0.2) << m;
        EXPECT_LE(cd, 0.4 + 1e-15) << m;
        prev = cd;
    }
}

TEST(PhysicsEdgeDrag, RejectsNonPhysicalModels) {
    EXPECT_THROW(DragModel::FromMultiBc(DragTableId::kG1, {{0.5, 800.0}, {0.0, 600.0}}),
                 std::invalid_argument);
    EXPECT_THROW(DragModel::FromMultiBc(DragTableId::kG1, {{0.5, -1.0}}), std::invalid_argument);
    const auto g7 = StandardDragTable(DragTableId::kG7);
    const double m = units::GrainToKg(175.0);
    const double d = units::InchToM(0.308);
    EXPECT_THROW(DragModel::FromCurve(g7, 0.0, d, 1.0), std::invalid_argument);
    EXPECT_THROW(DragModel::FromCurve(g7, m, -d, 1.0), std::invalid_argument);
    EXPECT_THROW(DragModel::FromCurve(g7, m, d, 0.0), std::invalid_argument);
    EXPECT_THROW(DragModel::FromCurve({{1.0, 0.3}}, m, d, 1.0), std::invalid_argument);
    const DragModel model = DragModel::FromBc(DragTableId::kG7, 0.243);
    EXPECT_THROW(static_cast<void>(model.Scaled(0.0)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(model.Scaled(-1.0)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(model.WithMachScale({{-0.1, 1.0}})), std::invalid_argument);
}

// --- Spin and powder --------------------------------------------------------

TEST(PhysicsEdgeEffects, NoSpinNoDriftNoJump) {
    const double twist = units::InchToM(10.0);
    EXPECT_DOUBLE_EQ(LitzSpinDrift(0.0, 1.0, twist), 0.0);   // not stable
    EXPECT_DOUBLE_EQ(LitzSpinDrift(-1.0, 1.0, twist), 0.0);  // ditto
    EXPECT_DOUBLE_EQ(LitzSpinDrift(2.0, 1.0, 0.0), 0.0);     // smooth bore
    EXPECT_DOUBLE_EQ(LitzSpinDrift(2.0, 0.0, twist), 0.0);   // at the muzzle
    // Right twist drifts right, left twist as far to the left.
    const double right = LitzSpinDrift(2.0, 1.0, twist);
    EXPECT_GT(right, 0.0);
    EXPECT_DOUBLE_EQ(LitzSpinDrift(2.0, 1.0, -twist), -right);
    // Litz: 1.25 * (Sg + 1.2) * t^1.83 inches.
    EXPECT_NEAR(right, units::InchToM(1.25 * 3.2), 1e-12);

    const double l = units::InchToM(1.24);
    const double d = units::InchToM(0.308);
    EXPECT_DOUBLE_EQ(AerodynamicJump(0.0, l, d, twist, 5.0), 0.0);
    EXPECT_DOUBLE_EQ(AerodynamicJump(2.0, l, d, 0.0, 5.0), 0.0);
    EXPECT_DOUBLE_EQ(AerodynamicJump(2.0, l, 0.0, twist, 5.0), 0.0);
    // Left twist mirrors right twist; no crosswind, no jump.
    EXPECT_DOUBLE_EQ(AerodynamicJump(2.0, l, d, -twist, 5.0),
                     -AerodynamicJump(2.0, l, d, twist, 5.0));
    EXPECT_DOUBLE_EQ(AerodynamicJump(2.0, l, d, twist, 0.0), 0.0);
}

TEST(PhysicsEdgeEffects, MillerStabilityNeedsAFullDescription) {
    const double m = units::GrainToKg(175.0);
    const double d = units::InchToM(0.308);
    const double l = units::InchToM(1.24);
    const double tw = units::InchToM(10.0);
    const double t = units::CToK(15.0);
    const double p = 101325.0;
    EXPECT_GT(MillerStability(m, d, l, tw, 800.0, t, p), 1.0);
    EXPECT_DOUBLE_EQ(MillerStability(0.0, d, l, tw, 800.0, t, p), 0.0);
    EXPECT_DOUBLE_EQ(MillerStability(m, d, l, 0.0, 800.0, t, p), 0.0);
    EXPECT_DOUBLE_EQ(MillerStability(m, d, l, tw, 0.0, t, p), 0.0);
    EXPECT_DOUBLE_EQ(MillerStability(m, d, l, tw, 800.0, t, 0.0), 0.0);
    // Left twist is as stable as right twist.
    EXPECT_DOUBLE_EQ(MillerStability(m, d, l, -tw, 800.0, t, p),
                     MillerStability(m, d, l, tw, 800.0, t, p));
}

TEST(PhysicsEdgeEffects, PowderTableWithTwoReadingsAtOneTemperature) {
    PowderSensitivity p;
    p.reference_velocity_mps = 790.0;
    p.fraction_per_kelvin = 0.001;
    const double t = units::CToK(20.0);
    p.table = {{t, 805.0}, {t, 800.0}};
    // No slope to interpolate on: the (lower, after sorting) reading, no
    // division by zero, whatever the temperature.
    EXPECT_DOUBLE_EQ(MuzzleVelocityAt(p, t), 800.0);
    EXPECT_DOUBLE_EQ(MuzzleVelocityAt(p, units::CToK(-10.0)), 800.0);
    EXPECT_DOUBLE_EQ(MuzzleVelocityAt(p, units::CToK(40.0)), 800.0);
    // A single reading is not a table: the coefficient applies.
    p.table = {{t, 805.0}};
    EXPECT_NEAR(MuzzleVelocityAt(p, p.reference_temperature_k + 10.0), 790.0 * 1.01, 1e-9);
}

// --- Solver -----------------------------------------------------------------

TEST(PhysicsEdgeSolver, FlightThatNeverStarts) {
    // Asked for a range behind the muzzle: the flight is just its start.
    const Trajectory t = Fly(Sample308(), -1.0);
    EXPECT_EQ(t.stop_reason(), StopReason::kRangeReached);
    EXPECT_EQ(t.step_count(), 0u);
    EXPECT_DOUBLE_EQ(t.max_slant_range_m(), 0.0);
    EXPECT_FALSE(t.AtSlantRange(0.0));
    EXPECT_FALSE(t.AtTime(0.0));
    EXPECT_TRUE(t.Table(100.0, 1000.0).empty());
    // Nothing to analyse either.
    EXPECT_FALSE(PointBlankRange(t, 0.1, 500.0));
    EXPECT_FALSE(MovingTargetLead(t, 100.0, 3.0));

    // A default trajectory has no nodes at all.
    const Trajectory none;
    EXPECT_EQ(none.step_count(), 0u);
    EXPECT_DOUBLE_EQ(none.max_slant_range_m(), 0.0);
    EXPECT_FALSE(none.AtSlantRange(0.0));
}

TEST(PhysicsEdgeSolver, SamplingOutsideTheFlight) {
    Shot shot = Sample308();
    shot.elevation_rad = FindZero(shot, 100.0).elevation_rad;
    const Trajectory t = Fly(shot, 550.0);
    // Before the muzzle and after the end.
    EXPECT_FALSE(t.AtSlantRange(-1.0));
    EXPECT_FALSE(t.AtSlantRange(t.max_slant_range_m() + 1.0));
    EXPECT_FALSE(t.AtTime(-0.1));
    EXPECT_FALSE(t.AtTime(10.0));

    // Time and distance sampling agree.
    const auto by_range = t.AtSlantRange(500.0);
    ASSERT_TRUE(by_range);
    const auto by_time = t.AtTime(by_range->time_s);
    ASSERT_TRUE(by_time);
    EXPECT_NEAR(by_time->slant_range_m, 500.0, 1e-6);
    EXPECT_NEAR(by_time->drop_m, by_range->drop_m, 1e-6);
    EXPECT_NEAR(by_time->speed_mps, by_range->speed_mps, 1e-6);
    // The muzzle itself.
    const auto start = t.AtTime(0.0);
    ASSERT_TRUE(start);
    EXPECT_DOUBLE_EQ(start->speed_mps, 800.0);
    EXPECT_NEAR(start->drop_m, -0.05, 1e-12);

    // A table stops where the flight ends, and needs a positive step.
    const auto table = t.Table(100.0, 1000.0);
    ASSERT_EQ(table.size(), 6u);  // 0..500
    EXPECT_NEAR(table.back().slant_range_m, 500.0, 1e-6);
    const auto no_zero = t.Table(100.0, 1000.0, false);
    ASSERT_EQ(no_zero.size(), 5u);
    EXPECT_NEAR(no_zero.front().slant_range_m, 100.0, 1e-6);
    EXPECT_TRUE(t.Table(0.0, 1000.0).empty());
    EXPECT_TRUE(t.Table(-10.0, 1000.0).empty());
}

TEST(PhysicsEdgeSolver, StopsAtMaximumDropAndTime) {
    Shot shot = Sample308();
    SolverOptions o;
    o.max_drop_m = 2.0;
    const Trajectory dropped = Fly(shot, 2000.0, o);
    EXPECT_EQ(dropped.stop_reason(), StopReason::kMaxDrop);
    // Flat fired, 800 m/s: 2 m of fall takes ~0.64 s, about 450 m.
    EXPECT_GT(dropped.max_slant_range_m(), 300.0);
    EXPECT_LT(dropped.max_slant_range_m(), 700.0);
    EXPECT_FALSE(dropped.AtSlantRange(1000.0));
    // It ends with the first step past 2 m below the muzzle.
    const auto last = dropped.AtSlantRange(dropped.max_slant_range_m());
    ASSERT_TRUE(last);
    EXPECT_LT(last->position.y, -2.0);
    EXPECT_GT(last->position.y, -3.0);

    o = {};
    o.max_time_s = 0.5;
    const Trajectory timed = Fly(shot, 2000.0, o);
    EXPECT_EQ(timed.stop_reason(), StopReason::kMaxTime);
    EXPECT_FALSE(timed.AtTime(0.6));
    EXPECT_TRUE(timed.AtTime(0.5));
}

TEST(PhysicsEdgeSolver, JumpUsesTheNearestWindZoneWhateverTheOrder) {
    Shot shot = Sample308();
    shot.twist_m = units::InchToM(10.0);
    const WindZone near{300.0, 5.0, units::DegToRad(90.0), 0.0};
    const WindZone calm_far{1e5, 0.0, 0.0, 0.0};
    shot.winds = {calm_far, near};  // listed out of order
    const double jump = Fly(shot, 100.0).aerodynamic_jump_rad();
    shot.winds = {near, calm_far};
    EXPECT_DOUBLE_EQ(Fly(shot, 100.0).aerodynamic_jump_rad(), jump);
    // Wind from the right, right twist: the near zone's 5 m/s throws it low.
    EXPECT_LT(jump, 0.0);
    shot.winds = {near};
    EXPECT_DOUBLE_EQ(Fly(shot, 100.0).aerodynamic_jump_rad(), jump);
}

TEST(PhysicsEdgeSolver, SlantRangeSolveAtTheLimitOfDoublePrecision) {
    // 100 000 km in a straight line: one ulp of the distance (~1.5e-8 m)
    // is far above the 1e-11 m the Newton search aims for, so it runs out
    // of iterations and returns its best estimate.
    const Shot shot = StraightLine(1e6);
    SolverOptions o;
    o.max_time_s = 200.0;
    const double far = 1e8;
    const Trajectory t = Fly(shot, far + 1e3, o);
    ASSERT_EQ(t.stop_reason(), StopReason::kRangeReached);
    for (double r : {far / 3, far / 2, far * 0.7, far}) {
        const auto p = t.AtSlantRange(r);
        ASSERT_TRUE(p) << r;
        EXPECT_NEAR(p->slant_range_m, r, 1e-6) << r;
        EXPECT_NEAR(p->time_s, r / 1e6, 1e-9 * p->time_s) << r;  // a trace of air
        EXPECT_NEAR(p->drop_m, 0.0, 1e-6) << r;
    }
}

TEST(PhysicsEdgeZero, FirstStepPastTheVerticalGivesUp) {
    // 300 m above the aim at 100 m wants 71.6 deg of elevation; the first
    // small-angle secant step is 3 rad, i.e. backwards, and that flight
    // never gets to 100 m.
    const Shot shot = StraightLine(800.0);
    const ZeroResult z = FindZero(shot, 100.0, 300.0);
    EXPECT_FALSE(z.converged);
    EXPECT_EQ(z.iterations, 1);
}

TEST(PhysicsEdgeZero, ZeroBeyondTheFlightGivesUp) {
    Shot shot = Sample308();
    SolverOptions o;
    o.max_time_s = 0.1;  // ~80 m of flight
    const ZeroResult z = FindZero(shot, 100.0, 0.0, o);
    EXPECT_FALSE(z.converged);
    EXPECT_EQ(z.iterations, 0);
}

// Drop at 100 000 km cannot be resolved to the 1e-9 m the secant wants:
// one ulp of 2e7 m is ~3.7e-9 m.
ZeroResult ZeroBeyondDoublePrecision() {
    const Shot shot = StraightLine(1e6);
    SolverOptions o;
    o.max_time_s = 200.0;
    return FindZero(shot, 1e8, 2e7, o);
}

TEST(PhysicsEdgeZero, UnreachablePrecisionStopsAfterThirtyIterations) {
    const ZeroResult z = ZeroBeyondDoublePrecision();
    EXPECT_FALSE(z.converged);
    EXPECT_EQ(z.iterations, 30);
}

// Once the secant stalls - two equal elevations, a miss still above
// 1e-9 m - its slope is 0/0; the search keeps the last estimate (here
// within an ulp of the exact atan(2e7 / 1e8)) instead of turning to NaN.
TEST(PhysicsEdgeZero, UnconvergedZeroKeepsTheLastEstimate) {
    const ZeroResult z = ZeroBeyondDoublePrecision();
    EXPECT_FALSE(z.converged);
    EXPECT_TRUE(std::isfinite(z.elevation_rad));
    EXPECT_NEAR(z.elevation_rad, std::atan(0.2), 1e-12);
}

TEST(PhysicsEdgeZero, ConvergesInStraightLineSpace) {
    // With no gravity and no air, zeroing a point 10 m up at 100 m is the
    // angle of that point, and nothing horizontal to absorb.
    const Shot shot = StraightLine(800.0);
    const ZeroResult z = FindZero(shot, 100.0, 10.0);
    ASSERT_TRUE(z.converged);
    EXPECT_NEAR(z.elevation_rad, std::atan(0.1), 1e-12);
    EXPECT_DOUBLE_EQ(z.windage_rad, 0.0);
}

// --- Analysis and BC --------------------------------------------------------

TEST(PhysicsEdgeBc, TooDraggyBulletsDoNotReachTheFarChronograph) {
    // From 300 m/s a BC of 0.02 stops well short of 1500 m: those trial
    // BCs give no reading and the bisection treats them as too low.
    const Atmosphere air = StandardAtmosphere(0.0);
    const auto bc = BcFromVelocities(DragTableId::kG1, 300.0, 200.0, 1500.0, air);
    ASSERT_TRUE(bc);
    EXPECT_GT(*bc, kMinFittedBc);
    EXPECT_LT(*bc, kMaxFittedBc);
    // The fitted BC reproduces the far reading.
    Shot shot;
    shot.drag = DragModel::FromBc(DragTableId::kG1, *bc);
    shot.muzzle_velocity_mps = 300.0;
    shot.atmosphere = air;
    const auto far = Fly(shot, 1501.0).AtSlantRange(1500.0);
    ASSERT_TRUE(far);
    EXPECT_NEAR(far->speed_mps, 200.0, 0.01);
}

// --- WEZ --------------------------------------------------------------------

Shot Aimed(double range_m) {
    Shot shot = Sample308();
    shot.latitude_rad = units::DegToRad(50.0);
    shot.azimuth_rad = units::DegToRad(90.0);  // shooting east
    const ZeroResult z = FindZero(shot, range_m);
    shot.elevation_rad = z.elevation_rad;
    shot.windage_rad = z.windage_rad;
    return shot;
}

TEST(PhysicsEdgeWez, EarthRotationErrorsAreSmallButCounted) {
    const Shot shot = Aimed(800.0);
    ErrorSources e;
    e.azimuth_rad = units::DegToRad(10.0);
    e.latitude_rad = units::DegToRad(5.0);
    const WezModel model(shot, e, 900.0);
    bool ok = false;
    const Spread s = model.At(800.0, &ok);
    ASSERT_TRUE(ok);
    ASSERT_EQ(s.parts.size(), 2u);
    for (const Spread::Part& p : s.parts) {
        EXPECT_TRUE(p.source == "azimuth" || p.source == "latitude") << p.source;
        // Coriolis/Eotvos at 800 m are centimetres, their errors less.
        const double size = std::hypot(p.up_m, p.right_m);
        EXPECT_GT(size, 1e-6) << p.source;
        EXPECT_LT(size, 0.02) << p.source;
    }
    // Without latitude and azimuth on the shot, these errors do nothing.
    Shot flat = shot;
    flat.latitude_rad.reset();
    flat.azimuth_rad.reset();
    EXPECT_TRUE(WezModel(flat, e, 900.0).At(800.0).parts.empty());
}

TEST(PhysicsEdgeWez, BeyondTheFlightThereIsNoSpread) {
    const Shot shot = Aimed(500.0);
    ErrorSources e;
    e.muzzle_velocity_mps = 5.0;
    e.dispersion_rad = units::MoaToRad(1.0);
    const WezModel model(shot, e, 600.0);
    bool ok = true;
    const Spread s = model.At(800.0, &ok);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(s.parts.empty());
    EXPECT_DOUBLE_EQ(s.sigma_up_m, 0.0);
    EXPECT_DOUBLE_EQ(s.sigma_right_m, 0.0);
    // Without the flag, too.
    EXPECT_TRUE(model.At(800.0).parts.empty());
}

TEST(PhysicsEdgeWez, TargetsWithNoAreaAreNeverHit) {
    Spread s;
    s.sigma_up_m = 0.1;
    s.sigma_right_m = 0.1;
    EXPECT_DOUBLE_EQ(HitProbability(s, {Target::Kind::kRectangle, 0.5, 0.0}), 0.0);
    EXPECT_DOUBLE_EQ(HitProbability(s, {Target::Kind::kEllipse, 0.5, 0.0}), 0.0);
    EXPECT_NEAR(HitProbability(s, {Target::Kind::kRectangle, 0.0, 0.5}), 0.0, 1e-12);
    // A shape this build does not know (a newer one's) covers nothing.
    const auto unknown =
        static_cast<Target::Kind>(7);  // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    EXPECT_DOUBLE_EQ(HitProbability(s, {unknown, 0.5, 0.5}), 0.0);
}

}  // namespace
}  // namespace ballistics
