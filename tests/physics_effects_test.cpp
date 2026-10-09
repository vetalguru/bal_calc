// Wind zones, Coriolis/Eotvos and spin drift vs. the near-exact reference
// (see reference/generate_reference.py, "effects" set), plus behavioural
// checks for effects the reference does not model (cant, jump, powder).
#include <ballistics/effects.h>
#include <ballistics/solver.h>
#include <ballistics/units.h>
#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "reference/reference_setup.h"

namespace ballistics {
namespace {

using units::DegToRad;

struct Row {
    double slant_m, drop_m, windage_m, speed_mps, time_s;
};

struct Case {
    const char* name;
    double zero_elevation_rad;
    std::vector<Row> rows;
};

const std::vector<Case>& Cases() {
    static const std::vector<Case> cases = {
#include "reference/reference_effects.inc"
    };
    return cases;
}

struct Setup {
    const char* name;
    const char* cartridge;
    const char* atmosphere;
    double twist_in;
    std::vector<WindZone> winds;  // until m, speed m/s, from rad
    std::optional<double> latitude_deg;
    std::optional<double> azimuth_deg;
};

WindZone Wind(double until_m, double speed, double from_deg) {
    return {until_m, speed, DegToRad(from_deg), 0.0};
}

// Must match EFFECTS in generate_reference.py.
const std::vector<Setup>& Setups() {
    static const std::vector<Setup> setups = {
        {"wind_right_4", "308_175smk_g7", "icao", 0.0, {Wind(1e5, 4.0, 90.0)}, {}, {}},
        {"wind_left_front_6", "308_175smk_g7", "icao", 0.0, {Wind(1e5, 6.0, 300.0)}, {}, {}},
        {"wind_zones",
         "308_175smk_g7",
         "mountain_cold_humid",
         0.0,
         {Wind(500.0, 3.0, 270.0), Wind(1500.0, 5.0, 45.0), Wind(1e5, 2.0, 135.0)},
         {},
         {}},
        {"coriolis_n50_east", "338lm_300hybrid_g7", "icao", 0.0, {}, 50.0, 90.0},
        {"coriolis_n50_north", "338lm_300hybrid_g7", "icao", 0.0, {}, 50.0, 0.0},
        {"coriolis_s30_west", "338lm_300hybrid_g7", "icao", 0.0, {}, -30.0, 270.0},
        {"coriolis_n60_sw", "50bmg_750amax_g1", "icao", 0.0, {}, 60.0, 225.0},
        {"spin_right_10", "308_175smk_g7", "icao", 10.0, {}, {}, {}},
        {"spin_left_8", "65cm_140eldm_g7", "hot_humid", -8.0, {}, {}, {}},
        {"combined_338",
         "338lm_300hybrid_g7",
         "mountain_cold_humid",
         9.4,
         {Wind(800.0, 4.0, 80.0), Wind(1e5, 6.0, 110.0)},
         48.0,
         135.0},
        {"combined_50",
         "50bmg_750amax_g1",
         "hot_humid",
         15.0,
         {Wind(1000.0, 5.0, 240.0), Wind(1e5, 3.0, 20.0)},
         35.0,
         10.0},
    };
    return setups;
}

const Setup& SetupNamed(std::string_view name) {
    for (const auto& s : Setups()) {
        if (name == s.name) {
            return s;
        }
    }
    ADD_FAILURE() << "unknown case " << name;
    return Setups().front();
}

Shot MakeShot(const Setup& s) {
    Shot shot = reference::MakeShot(s.cartridge, s.atmosphere);
    shot.twist_m = units::InchToM(s.twist_in);
    shot.winds = s.winds;
    if (s.latitude_deg) {
        shot.latitude_rad = DegToRad(*s.latitude_deg);
    }
    if (s.azimuth_deg) {
        shot.azimuth_rad = DegToRad(*s.azimuth_deg);
    }
    return shot;
}

constexpr double kMaxAngularErrorRad = 0.01e-3;
constexpr double kMaxRelativeError = 1e-3;

TEST(PhysicsEffectsReference, HasAllCases) { EXPECT_EQ(Cases().size(), Setups().size()); }

TEST(PhysicsEffectsReference, MatchesReference) {
    double worst_drop = 0.0, worst_wind = 0.0;
    std::string worst_drop_at, worst_wind_at;
    for (const Case& c : Cases()) {
        Shot shot = MakeShot(SetupNamed(c.name));
        shot.elevation_rad = c.zero_elevation_rad;
        const Trajectory traj = Fly(shot, c.rows.back().slant_m + 10.0);
        for (const Row& r : c.rows) {
            const auto pt = traj.AtSlantRange(r.slant_m);
            ASSERT_TRUE(pt.has_value()) << c.name << " @" << r.slant_m;
            const std::string at = std::string(c.name) + " @" + std::to_string(r.slant_m);
            const double drop_err = std::fabs(pt->drop_m - r.drop_m) / r.slant_m;
            const double wind_err = std::fabs(pt->windage_m - r.windage_m) / r.slant_m;
            EXPECT_LE(drop_err, kMaxAngularErrorRad)
                << at << ": drop " << pt->drop_m << " vs " << r.drop_m;
            EXPECT_LE(wind_err, kMaxAngularErrorRad)
                << at << ": windage " << pt->windage_m << " vs " << r.windage_m;
            EXPECT_NEAR(pt->speed_mps, r.speed_mps, kMaxRelativeError * r.speed_mps) << at;
            EXPECT_NEAR(pt->time_s, r.time_s, kMaxRelativeError * r.time_s) << at;
            if (drop_err > worst_drop) {
                worst_drop = drop_err;
                worst_drop_at = at;
            }
            if (wind_err > worst_wind) {
                worst_wind = wind_err;
                worst_wind_at = at;
            }
        }
    }
    std::printf("worst drop %.6f MRAD (%s), windage %.6f MRAD (%s)\n", worst_drop * 1e3,
                worst_drop_at.c_str(), worst_wind * 1e3, worst_wind_at.c_str());
}

Shot Level308() {
    Shot shot = reference::MakeShot("308_175smk_g7", "icao");
    shot.sound_speed = SoundSpeedModel::kHumidAir;
    shot.elevation_rad = FindZero(shot, 100.0).elevation_rad;
    return shot;
}

TEST(PhysicsEffects, CantMovesImpactTowardsCantAndLow) {
    Shot level = Level308();
    level.elevation_rad += units::MradToRad(10.0);  // dialled for long range
    Shot canted = level;
    canted.cant_rad = DegToRad(5.0);  // top to the right
    const auto a = Fly(level, 1010.0).AtSlantRange(1000.0);
    const auto b = Fly(canted, 1010.0).AtSlantRange(1000.0);
    ASSERT_TRUE(a && b);
    EXPECT_GT(b->windage_m, a->windage_m + 0.5);
    EXPECT_LT(b->drop_m, a->drop_m);
    // Dialled elevation e turns into e*sin(cant) of windage (plus the sight
    // offset), so ~ (10 mrad + zero) * sin 5 deg * 1000 m.
    const double expected = (level.elevation_rad) * std::sin(canted.cant_rad) * 1000.0;
    EXPECT_NEAR(b->windage_m - a->windage_m, expected, 0.15 * expected);
}

TEST(PhysicsEffects, HeadwindShortensTailwindLengthens) {
    Shot calm = Level308();
    Shot head = calm;
    head.winds = {{1e5, 5.0, 0.0, 0.0}};
    Shot tail = calm;
    tail.winds = {{1e5, 5.0, units::kPi, 0.0}};
    const double d_calm = Fly(calm, 1010.0).AtSlantRange(1000.0).value().drop_m;
    EXPECT_LT(Fly(head, 1010.0).AtSlantRange(1000.0).value().drop_m, d_calm);
    EXPECT_GT(Fly(tail, 1010.0).AtSlantRange(1000.0).value().drop_m, d_calm);
}

TEST(PhysicsEffects, AerodynamicJumpRightTwistWindFromRightIsLow) {
    Shot shot = Level308();
    shot.twist_m = units::InchToM(10.0);
    shot.winds = {{1e5, 5.0, DegToRad(90.0), 0.0}};
    shot.aerodynamic_jump = true;
    const Trajectory with_jump = Fly(shot, 1010.0);
    shot.aerodynamic_jump = false;
    const Trajectory without = Fly(shot, 1010.0);
    EXPECT_LT(with_jump.aerodynamic_jump_rad(), 0.0);
    // Litz: (0.01*Sg - 0.0024*L + 0.032) MOA/mph; Sg ~2.3, L ~4 cal -> ~0.046 MOA/mph.
    EXPECT_NEAR(units::RadToMoa(-with_jump.aerodynamic_jump_rad()) / (5.0 / 0.44704), 0.046, 0.003);
    EXPECT_LT(with_jump.AtSlantRange(1000.0).value().drop_m,
              without.AtSlantRange(1000.0).value().drop_m);
}

TEST(PhysicsEffects, MillerStabilityKnownValue) {
    // .308 175gr SMK, 1.24", 1:10, 2600 fps, standard air. By hand:
    // 30*175 / (32.47^2 * 0.308^3 * 4.026 * (1 + 4.026^2)) = 2.459,
    // times (2600/2800)^(1/3) = 0.9756 -> 2.399.
    const double sg = MillerStability(
        units::GrainToKg(175.0), units::InchToM(0.308), units::InchToM(1.24), units::InchToM(10.0),
        units::FpsToMps(2600.0), units::FToK(59.0), units::InHgToPa(29.92));
    EXPECT_NEAR(sg, 2.399, 0.002);
}

TEST(PhysicsEffects, LocalGravity) {
    EXPECT_NEAR(LocalGravity(0.0, 0.0), 9.7803, 1e-4);
    EXPECT_NEAR(LocalGravity(DegToRad(90.0), 0.0), 9.8322, 1e-4);
    EXPECT_NEAR(LocalGravity(DegToRad(45.0), 1000.0), 9.8062 - 0.0031, 2e-4);
}

TEST(PhysicsEffects, PowderSensitivity) {
    PowderSensitivity p;
    p.reference_velocity_mps = 800.0;
    p.reference_temperature_k = units::CToK(15.0);
    p.fraction_per_kelvin = 0.001;
    EXPECT_NEAR(MuzzleVelocityAt(p, units::CToK(35.0)), 816.0, 1e-9);
    EXPECT_NEAR(MuzzleVelocityAt(p, units::CToK(-5.0)), 784.0, 1e-9);

    p.table = {{units::CToK(-20.0), 770.0}, {units::CToK(0.0), 790.0}, {units::CToK(30.0), 812.0}};
    EXPECT_NEAR(MuzzleVelocityAt(p, units::CToK(15.0)), 801.0, 1e-9);
    EXPECT_NEAR(MuzzleVelocityAt(p, units::CToK(-30.0)), 760.0, 1e-9);  // extrapolated
    EXPECT_NEAR(MuzzleVelocityAt(p, units::CToK(40.0)), 819.3333333333, 1e-6);
}

}  // namespace
}  // namespace ballistics
