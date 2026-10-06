// BC calculator: the BC that made the measurements is recovered.
#include <ballistics/bc.h>
#include <ballistics/solver.h>
#include <ballistics/units.h>

#include <gtest/gtest.h>

namespace ballistics {
namespace {

// A real shot: zeroed, from the muzzle, gravity and all; chronographs at
// `near_m` and `far_m` read its speed there.
std::pair<double, double> Chronographs(DragTableId table, double bc, double mv, double near_m,
                                       double far_m, const Atmosphere& air) {
    Shot shot;
    shot.drag = DragModel::FromBc(table, bc);
    shot.muzzle_velocity_mps = mv;
    shot.mass_kg = units::GrainToKg(175.0);
    shot.sight_height_m = 0.05;
    shot.atmosphere = air;
    shot.elevation_rad = FindZero(shot, 100.0).elevation_rad;
    const Trajectory t = Fly(shot, far_m + 1.0);
    return {t.AtSlantRange(near_m)->speed_mps, t.AtSlantRange(far_m)->speed_mps};
}

TEST(PhysicsBc, FromTwoChronographs) {
    struct Case {
        DragTableId table;
        double bc, mv, near_m, far_m;
    };
    Atmosphere warm = StandardAtmosphere(400.0);
    warm.temperature_k = units::CToK(28.0);
    for (const Atmosphere& air : {StandardAtmosphere(0.0), warm}) {
        for (const Case& c : {Case{DragTableId::kG7, 0.243, 790, 3, 103},
                              Case{DragTableId::kG7, 0.305, 860, 5, 300},
                              Case{DragTableId::kG1, 0.462, 820, 3, 200},
                              Case{DragTableId::kG1, 0.125, 1000, 3, 53}}) {
            const auto [near, far] = Chronographs(c.table, c.bc, c.mv, c.near_m, c.far_m, air);
            const auto bc = BcFromVelocities(c.table, near, far, c.far_m - c.near_m, air);
            ASSERT_TRUE(bc) << c.bc;
            EXPECT_NEAR(*bc, c.bc, 0.005 * c.bc) << DragTableName(c.table) << " " << c.bc;
        }
    }
}

TEST(PhysicsBc, ImpossibleReadingsGiveNothing) {
    const Atmosphere air = StandardAtmosphere(0.0);
    EXPECT_FALSE(BcFromVelocities(DragTableId::kG7, 800, 810, 100, air)); // faster further out
    EXPECT_FALSE(BcFromVelocities(DragTableId::kG7, 800, 799.99, 100, air)); // a BC above 2
    EXPECT_FALSE(BcFromVelocities(DragTableId::kG7, 800, 200, 10, air));     // a BC below 0.02
    EXPECT_FALSE(BcFromVelocities(DragTableId::kG7, 800, 700, 0, air));
}

TEST(PhysicsBc, FitIsABisectionOnARisingQuantity) {
    const auto bc = FitBc([](double x) -> std::optional<double> { return 3.0 * x; }, 0.9);
    ASSERT_TRUE(bc);
    EXPECT_NEAR(*bc, 0.3, 1e-6);
    // Low BCs that cannot get there count as below the target.
    const auto far = FitBc(
        [](double x) -> std::optional<double> {
            if (x < 0.1) return std::nullopt;
            return x;
        },
        0.5);
    ASSERT_TRUE(far);
    EXPECT_NEAR(*far, 0.5, 1e-6);
    EXPECT_FALSE(FitBc([](double x) -> std::optional<double> { return x; }, 5.0));
}

} // namespace
} // namespace ballistics
