// Hit probability: the integration against closed forms, the linearised
// spread against real flights with random errors.
#include <ballistics/units.h>
#include <ballistics/wez.h>

#include <gtest/gtest.h>

#include <cmath>
#include <random>

namespace ballistics {
namespace {

double Phi(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }

Spread Round(double sigma) {
    Spread s;
    s.sigma_up_m = sigma;
    s.sigma_right_m = sigma;
    return s;
}

TEST(PhysicsWez, CircleAgainstRayleigh) {
    for (double r : {0.05, 0.1, 0.25}) {
        for (double sigma : {0.03, 0.1, 0.2}) {
            const double p = HitProbability(Round(sigma), {Target::Kind::kEllipse, 2 * r, 2 * r});
            EXPECT_NEAR(p, 1 - std::exp(-r * r / (2 * sigma * sigma)), 0.002) << r << " " << sigma;
        }
    }
}

TEST(PhysicsWez, RectangleIsAProductWithoutCorrelation) {
    Spread s;
    s.sigma_up_m = 0.12;
    s.sigma_right_m = 0.3;
    const double p = HitProbability(s, {Target::Kind::kRectangle, 0.5, 0.4});
    EXPECT_NEAR(p, (2 * Phi(0.25 / 0.3) - 1) * (2 * Phi(0.2 / 0.12) - 1), 1e-4);
    // Tiny spread, big target: always; tiny target, big spread: almost never.
    EXPECT_NEAR(HitProbability(Round(0.001), {Target::Kind::kRectangle, 0.5, 0.5}), 1.0, 1e-6);
    EXPECT_LT(HitProbability(Round(2.0), {Target::Kind::kRectangle, 0.05, 0.05}), 0.001);
}

TEST(PhysicsWez, CorrelatedSpreadAgainstSampling) {
    Spread s;
    s.sigma_up_m = 0.2;
    s.sigma_right_m = 0.15;
    s.correlation = 0.6;
    for (Target t : {Target{Target::Kind::kRectangle, 0.3, 0.3}, Target{Target::Kind::kFigure, 0.5, 0.8},
                     Target{Target::Kind::kEllipse, 0.4, 0.2}}) {
        std::mt19937 rng(7);
        std::normal_distribution<double> n(0.0, 1.0);
        int hits = 0;
        const int total = 400000;
        for (int i = 0; i < total; ++i) {
            const double a = n(rng), b = n(rng);
            const double up = s.sigma_up_m * a;
            const double right = s.sigma_right_m * (s.correlation * a + std::sqrt(1 - 0.36) * b);
            bool inside = false;
            if (t.kind == Target::Kind::kRectangle) {
                inside = std::abs(up) <= t.height_m / 2 && std::abs(right) <= t.width_m / 2;
            } else if (t.kind == Target::Kind::kEllipse) {
                inside = std::pow(2 * up / t.height_m, 2) + std::pow(2 * right / t.width_m, 2) <= 1;
            } else {
                inside = (up >= -0.35 * t.height_m && up <= 0.35 * t.height_m && std::abs(right) <= t.width_m / 2) ||
                         (up > 0.35 * t.height_m && up <= 0.65 * t.height_m && std::abs(right) <= 0.2 * t.width_m);
            }
            hits += inside;
        }
        EXPECT_NEAR(HitProbability(s, t), static_cast<double>(hits) / total, 0.004)
            << static_cast<int>(t.kind);
    }
}

TEST(PhysicsWez, ShotsToHit) {
    EXPECT_EQ(ShotsToHit(0.5, 0.95), 5); // 1 - 0.5^5 = 0.97
    EXPECT_EQ(ShotsToHit(0.5, 0.5), 1);
    EXPECT_EQ(ShotsToHit(0.1, 0.9), 22);
    EXPECT_EQ(ShotsToHit(1.0, 0.99), 1);
    EXPECT_EQ(ShotsToHit(0.0, 0.5), 0);
}

Shot Aimed308(double range_m, std::vector<WindZone> winds) {
    Shot shot;
    shot.drag = DragModel::FromBc(DragTableId::kG7, 0.243);
    shot.muzzle_velocity_mps = 790.0;
    shot.mass_kg = units::GrainToKg(175.0);
    shot.sight_height_m = 0.05;
    shot.atmosphere = StandardAtmosphere(300.0);
    shot.atmosphere.humidity = 0.5;
    // Aimed at the target: bore angles that hit it in these conditions.
    shot.winds = std::move(winds);
    const ZeroResult z = FindZero(shot, range_m);
    shot.elevation_rad = z.elevation_rad;
    shot.windage_rad = z.windage_rad;
    return shot;
}

TEST(PhysicsWez, LinearisedSpreadMatchesRandomFlights) {
    const double range = 800.0;
    const Shot shot = Aimed308(range, {{1e9, 3.0, units::DegToRad(60.0), 0.0}});
    ErrorSources e;
    e.range_m = 10.0;
    e.wind_speed_mps = 1.0;
    e.wind_direction_rad = units::DegToRad(15.0);
    e.muzzle_velocity_mps = 5.0;
    e.drag_fraction = 0.02;
    e.temperature_k = 3.0;
    e.pressure_pa = 300.0;
    e.dispersion_rad = units::MoaToRad(1.0) / kGroupSpreadPerSigma;
    const WezModel model(shot, e, range + 100.0);
    bool ok = false;
    const Spread s = model.At(range, &ok);
    ASSERT_TRUE(ok);
    ASSERT_FALSE(s.parts.empty());

    // Real flights, every error drawn at once.
    std::mt19937 rng(11);
    std::normal_distribution<double> n(0.0, 1.0);
    const int total = 3000;
    const Target target{Target::Kind::kRectangle, 0.5, 0.5};
    int hits = 0;
    double su = 0, sr = 0;
    for (int i = 0; i < total; ++i) {
        Shot r = shot;
        r.muzzle_velocity_mps += e.muzzle_velocity_mps * n(rng);
        r.drag = r.drag.Scaled(1 + e.drag_fraction * n(rng));
        r.atmosphere.temperature_k += e.temperature_k * n(rng);
        r.atmosphere.pressure_pa += e.pressure_pa * n(rng);
        WindZone& w = r.winds.front();
        w.from_rad += e.wind_direction_rad * n(rng);
        const double cross = e.wind_speed_mps * n(rng);
        const double x = w.speed_mps * std::sin(w.from_rad) + cross;
        const double y = w.speed_mps * std::cos(w.from_rad);
        w.speed_mps = std::hypot(x, y);
        w.from_rad = std::atan2(x, y);
        const double at = range + e.range_m * n(rng);
        const auto p = Fly(r, range + 100.0).AtSlantRange(at);
        ASSERT_TRUE(p);
        const double up = p->drop_m + e.dispersion_rad * range * n(rng);
        const double right = p->windage_m + e.dispersion_rad * range * n(rng);
        su += up * up;
        sr += right * right;
        hits += std::abs(up) <= 0.25 && std::abs(right) <= 0.25;
    }
    EXPECT_NEAR(s.sigma_up_m, std::sqrt(su / total), 0.06 * s.sigma_up_m);
    EXPECT_NEAR(s.sigma_right_m, std::sqrt(sr / total), 0.06 * s.sigma_right_m);
    // Sampling noise of 3000 shots is about 0.009.
    EXPECT_NEAR(HitProbability(s, target), static_cast<double>(hits) / total, 0.03);
}

TEST(PhysicsWez, PartsNameTheirSources) {
    const Shot shot = Aimed308(600.0, {});
    ErrorSources e;
    e.wind_speed_mps = 2.0;
    e.muzzle_velocity_mps = 3.0;
    const Spread s = WezModel(shot, e, 700.0).At(600.0);
    ASSERT_EQ(s.parts.size(), 2u);
    EXPECT_EQ(s.parts[0].source, "windSpeed"); // 2 m/s of wind beats 3 m/s of velocity
    EXPECT_GT(std::abs(s.parts[0].right_m), 5 * std::abs(s.parts[0].up_m));
    EXPECT_EQ(s.parts[1].source, "muzzleVelocity");
    EXPECT_GT(std::abs(s.parts[1].up_m), 5 * std::abs(s.parts[1].right_m));
    // No errors at all: a point.
    const Spread none = WezModel(shot, {}, 700.0).At(600.0);
    EXPECT_DOUBLE_EQ(none.sigma_up_m, 0.0);
    EXPECT_NEAR(HitProbability(none, {}), 1.0, 1e-9);
}

} // namespace
} // namespace ballistics
