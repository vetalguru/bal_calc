#include <ballistics/storage/database.h>
#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>
#include <ballistics/units.h>

#include <gtest/gtest.h>

#include <cmath>

namespace ballistics::storage {
namespace {

class StorageSolution : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(db_.Open(":memory:").ok());
        BulletRecord b;
        b.name = "SMK 175";
        b.caliber = ".308";
        b.diameter_m = units::InchToM(0.308);
        b.mass_kg = units::GrainToKg(175.0);
        b.length_m = units::InchToM(1.24);
        b.drag_table = "G7";
        b.bc = 0.243;
        ASSERT_TRUE(Repository<BulletRecord>(db_).Save(b).ok());

        CartridgeRecord c;
        c.name = "M118LR";
        c.bullet_id = b.id;
        c.muzzle_velocity_mps = 790.0;
        c.powder_sensitivity_per_k = 0.001;
        ASSERT_TRUE(Repository<CartridgeRecord>(db_).Save(c).ok());

        ScopeRecord s;
        s.name = "0.1 mil";
        s.click_vertical_rad = s.click_horizontal_rad = units::MradToRad(0.1);
        ASSERT_TRUE(Repository<ScopeRecord>(db_).Save(s).ok());

        RifleRecord r;
        r.name = "M24";
        r.twist_m = units::InchToM(11.25);
        r.sight_height_m = 0.05;
        r.scope_id = s.id;
        ASSERT_TRUE(Repository<RifleRecord>(db_).Save(r).ok());

        ProfileRecord p;
        p.name = "M24 / M118LR";
        p.rifle_id = r.id;
        p.cartridge_id = c.id;
        ASSERT_TRUE(Repository<ProfileRecord>(db_).Save(p).ok());
        profile_id_ = p.id;
    }

    LoadedProfile Load() {
        auto p = LoadProfile(db_, profile_id_);
        EXPECT_TRUE(p.ok());
        return p.value();
    }

    Database db_;
    Id profile_id_ = 0;
};

TEST_F(StorageSolution, LoadsTheWholeProfile) {
    const LoadedProfile p = Load();
    EXPECT_EQ(p.bullet.name, "SMK 175");
    EXPECT_EQ(p.cartridge.name, "M118LR");
    ASSERT_TRUE(p.scope.has_value());
    EXPECT_FALSE(p.curve.has_value());
    EXPECT_FALSE(LoadProfile(db_, 9999).ok());
}

TEST_F(StorageSolution, ZeroAbsorbsSpinDriftAtZeroRange) {
    ConditionsRecord cond;
    auto sol = Solve(Load(), cond, 1010.0);
    ASSERT_TRUE(sol.ok()) << sol.error().message;
    const auto at_zero = sol.value().trajectory.AtSlantRange(100.0);
    ASSERT_TRUE(at_zero);
    EXPECT_NEAR(at_zero->drop_m, 0.0, 1e-6);
    EXPECT_NEAR(at_zero->windage_m, 0.0, 1e-6);
    EXPECT_GT(at_zero->spin_drift_m, 0.0); // right twist drifts right...
    EXPECT_LT(sol.value().zero.windage_rad, 0.0); // ...so the zero aims left
    // Far out, spin drift outgrows the zero correction.
    EXPECT_GT(sol.value().trajectory.AtSlantRange(1000.0)->windage_m, 0.05);
}

TEST_F(StorageSolution, ProfileZeroOffsetIsHonoured) {
    ProfileRecord p = *Repository<ProfileRecord>(db_).Get(profile_id_).value();
    p.zero_offset_up_m = 0.03;
    p.zero_offset_right_m = -0.01;
    ASSERT_TRUE(Repository<ProfileRecord>(db_).Save(p).ok());
    auto sol = Solve(Load(), ConditionsRecord{}, 200.0);
    ASSERT_TRUE(sol.ok());
    const auto pt = sol.value().trajectory.AtSlantRange(100.0);
    EXPECT_NEAR(pt->drop_m, 0.03, 1e-6);
    EXPECT_NEAR(pt->windage_m, -0.01, 1e-6);
}

TEST_F(StorageSolution, HotPowderShootsFlatter) {
    ConditionsRecord cold;
    cold.powder_temp_k = units::CToK(-10.0);
    ConditionsRecord hot;
    hot.powder_temp_k = units::CToK(35.0);
    auto a = Solve(Load(), cold, 810.0);
    auto b = Solve(Load(), hot, 810.0);
    ASSERT_TRUE(a.ok() && b.ok());
    EXPECT_NEAR(a.value().shot.muzzle_velocity_mps, 790.0 * (1.0 - 0.025), 1e-9);
    EXPECT_NEAR(b.value().shot.muzzle_velocity_mps, 790.0 * (1.0 + 0.020), 1e-9);
    EXPECT_GT(b.value().trajectory.AtSlantRange(800.0)->drop_m,
              a.value().trajectory.AtSlantRange(800.0)->drop_m);
}

TEST_F(StorageSolution, TruingScalesApply) {
    auto base = Solve(Load(), ConditionsRecord{}, 1010.0);
    ProfileRecord p = *Repository<ProfileRecord>(db_).Get(profile_id_).value();
    p.drag_scale = 1.05;
    ASSERT_TRUE(Repository<ProfileRecord>(db_).Save(p).ok());
    auto draggy = Solve(Load(), ConditionsRecord{}, 1010.0);
    ASSERT_TRUE(base.ok() && draggy.ok());
    EXPECT_LT(draggy.value().trajectory.AtSlantRange(1000.0)->drop_m,
              base.value().trajectory.AtSlantRange(1000.0)->drop_m - 0.1);
}

TEST_F(StorageSolution, BadBulletDataIsReported) {
    BulletRecord b = *Repository<BulletRecord>(db_).Get(Load().bullet.id).value();
    b.drag_table = "G42";
    EXPECT_FALSE(MakeDragModel(b, nullptr).ok());
    b.drag_kind = kDragKindCurve;
    EXPECT_FALSE(MakeDragModel(b, nullptr).ok());
}

TEST(StorageClicks, RoundsToNearestClick) {
    EXPECT_DOUBLE_EQ(ToClicks(units::MradToRad(1.26), units::MradToRad(0.1)), 13.0);
    EXPECT_NEAR(ToClicks(units::MradToRad(1.26), units::MradToRad(0.1), false), 12.6, 1e-9);
    EXPECT_DOUBLE_EQ(ToClicks(units::MoaToRad(-2.0), units::MoaToRad(0.25)), -8.0);
}

} // namespace
} // namespace ballistics::storage
