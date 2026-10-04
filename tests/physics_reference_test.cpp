// Core point-mass solution vs. an independent near-exact reference
// (py-ballisticcalc SciPy DOP853, rtol 1e-12; see reference/generate_reference.py).
#include <ballistics/solver.h>
#include <ballistics/units.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace ballistics {
namespace {

struct Row {
    double slant_m, drop_m, speed_mps, time_s;
};

struct Case {
    const char* cartridge;
    const char* atmosphere;
    double look_deg;
    double zero_elevation_rad;
    std::vector<Row> rows;
};

const std::vector<Case>& Cases() {
    static const std::vector<Case> cases = {
#include "reference/reference_trajectories.inc"
    };
    return cases;
}

struct Cartridge {
    const char* name;
    DragTableId table;
    double bc, grains, v0_mps, sight_cm, zero_m;
};

// Must match CARTRIDGES in generate_reference.py.
constexpr Cartridge kCartridges[] = {
    {"308_175smk_g7", DragTableId::kG7, 0.243, 175.0, 800.0, 5.0, 100.0},
    {"65cm_140eldm_g7", DragTableId::kG7, 0.326, 140.0, 823.0, 4.5, 100.0},
    {"338lm_300hybrid_g7", DragTableId::kG7, 0.419, 300.0, 838.0, 5.5, 100.0},
    {"50bmg_750amax_g1", DragTableId::kG1, 1.050, 750.0, 860.0, 7.0, 100.0},
    {"556_m855_g1", DragTableId::kG1, 0.304, 62.0, 930.0, 6.5, 100.0},
};

// Must match ATMOSPHERES in generate_reference.py.
Atmosphere AtmosphereNamed(const std::string& name) {
    if (name == "mountain_cold_humid") {
        return {1500.0, units::HpaToPa(850.0), units::CToK(-10.0), 0.5};
    }
    if (name == "hot_humid") {
        return {200.0, units::HpaToPa(990.0), units::CToK(35.0), 0.9};
    }
    return {0.0, units::HpaToPa(1013.25), units::CToK(15.0), 0.0};
}

const Cartridge& CartridgeNamed(std::string_view name) {
    for (const auto& c : kCartridges) {
        if (name == c.name) {
            return c;
        }
    }
    ADD_FAILURE() << "unknown cartridge " << name;
    return kCartridges[0];
}

Shot MakeShot(const Case& c) {
    const Cartridge& cart = CartridgeNamed(c.cartridge);
    Shot shot;
    shot.drag = DragModel::FromBc(cart.table, cart.bc);
    shot.muzzle_velocity_mps = cart.v0_mps;
    shot.mass_kg = units::GrainToKg(cart.grains);
    shot.sight_height_m = cart.sight_cm / 100.0;
    shot.atmosphere = AtmosphereNamed(c.atmosphere);
    shot.sound_speed = SoundSpeedModel::kDryAir; // as in the reference
    return shot;
}

std::string CaseName(const Case& c) {
    return std::string(c.cartridge) + "/" + c.atmosphere + "/look" +
           std::to_string(static_cast<int>(c.look_deg));
}

// Plan criterion: drop within 0.01 MRAD at every range, speed and time
// of flight within 0.1 %.
constexpr double kMaxAngularErrorRad = 0.01e-3;
constexpr double kMaxRelativeError = 1e-3;

TEST(PhysicsReference, HasAllCases) { EXPECT_EQ(Cases().size(), 30U); }

TEST(PhysicsReference, TrajectoryMatchesReference) {
    double worst_rad = 0.0;
    double worst_speed = 0.0;
    double worst_time = 0.0;
    std::string worst_where;
    for (const Case& c : Cases()) {
        Shot shot = MakeShot(c);
        shot.look_angle_rad = units::DegToRad(c.look_deg);
        shot.elevation_rad = c.zero_elevation_rad;
        if (c.look_deg != 0.0) {
            // The reference keeps the sight vertically above the bore when
            // the rifle is tilted; bal_calc keeps it perpendicular to the
            // LOS (as mounted). The reference uses no sight offset here.
            shot.sight_height_m = 0.0;
        }
        const Trajectory traj = Fly(shot, c.rows.back().slant_m + 10.0);
        for (const Row& r : c.rows) {
            const auto pt = traj.AtSlantRange(r.slant_m);
            ASSERT_TRUE(pt.has_value()) << CaseName(c) << " @" << r.slant_m;
            const double err_rad = std::fabs(pt->drop_m - r.drop_m) / r.slant_m;
            EXPECT_LE(err_rad, kMaxAngularErrorRad)
                << CaseName(c) << " @" << r.slant_m << " m: drop " << pt->drop_m << " vs "
                << r.drop_m;
            EXPECT_NEAR(pt->speed_mps, r.speed_mps, kMaxRelativeError * r.speed_mps)
                << CaseName(c) << " @" << r.slant_m;
            EXPECT_NEAR(pt->time_s, r.time_s, kMaxRelativeError * r.time_s)
                << CaseName(c) << " @" << r.slant_m;
            worst_speed = std::max(worst_speed, std::fabs(pt->speed_mps / r.speed_mps - 1.0));
            worst_time = std::max(worst_time, std::fabs(pt->time_s / r.time_s - 1.0));
            if (err_rad > worst_rad) {
                worst_rad = err_rad;
                worst_where = CaseName(c) + " @" + std::to_string(r.slant_m);
            }
        }
    }
    std::printf("worst drop deviation %.6f MRAD (%s); speed %.2e, time %.2e relative\n",
                worst_rad * 1e3, worst_where.c_str(), worst_speed, worst_time);
}

TEST(PhysicsReference, ZeroMatchesReference) {
    for (const Case& c : Cases()) {
        if (c.look_deg != 0.0) {
            continue;
        }
        const Shot shot = MakeShot(c);
        const ZeroResult zero = FindZero(shot, CartridgeNamed(c.cartridge).zero_m);
        ASSERT_TRUE(zero.converged) << CaseName(c);
        // 1e-7 rad = 0.01 mm at 100 m.
        EXPECT_NEAR(zero.elevation_rad, c.zero_elevation_rad, 1e-7) << CaseName(c);
    }
}

} // namespace
} // namespace ballistics
