// Core point-mass solution vs. an independent near-exact reference
// (py-ballisticcalc SciPy DOP853, rtol 1e-12; see reference/generate_reference.py).
#include <ballistics/solver.h>
#include <ballistics/units.h>

#include "reference/reference_setup.h"

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

using reference::CartridgeNamed;

Shot MakeShot(const Case& c) { return reference::MakeShot(c.cartridge, c.atmosphere); }

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
