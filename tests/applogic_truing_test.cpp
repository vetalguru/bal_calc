// Truing: the fitted scales reproduce the corrections that hit.
#include <ballistics/applogic/armory.h>
#include <ballistics/applogic/truing.h>
#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>
#include <ballistics/units.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>

namespace ballistics::applogic {
namespace {

class Truing : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(db_.Open(":memory:").ok());
        RifleForm r;
        r.name = "Truing test";
        r.sight_height_cm = 5.0;
        r.twist_in = 10.0;
        CartridgeForm c;
        c.name = "Truing test";
        c.bullet_name = "SMK 175";
        c.drag_table = "G7";
        c.bc = 0.243;
        c.mass_gr = 175.0;
        c.diameter_in = 0.308;
        c.length_in = 1.24;
        c.muzzle_velocity_mps = 800.0;
        profile_ = EnsureProfile(db_, SaveRifleForm(db_, r).value(), SaveCartridgeForm(db_, c).value())
                       .value();
    }

    // The correction the rifle "really" needs: same profile, other scales.
    double TruthAt(double range_m, const SessionConditions& s, double v_scale, double d_scale) {
        storage::LoadedProfile p = storage::LoadProfile(db_, profile_).value();
        p.profile.velocity_scale = v_scale;
        p.profile.drag_scale = d_scale;
        auto sol = storage::Solve(p, ToConditions(s), range_m + 1.0);
        return sol.value().trajectory.AtSlantRange(range_m)->hold_elevation_rad;
    }

    SessionConditions Air(double temp_c) {
        SessionConditions s;
        s.temperature_c = temp_c;
        s.pressure_hpa = 995.0;
        s.humidity_pct = 40.0;
        return s;
    }

    storage::Database db_;
    Id profile_ = 0;
};

TEST_F(Truing, NeedsShots) {
    const TruingResult r = ComputeTruing(db_, profile_);
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.error.empty());
}

TEST_F(Truing, LoggedShotStoresConditionsAndPrediction) {
    const SessionConditions s = Air(-5.0);
    const Id id = LogShot(db_, profile_, s, 600.0, units::MradToRad(4.4), units::MradToRad(0.3),
                          "first group").value();
    const auto shots = ListShots(db_, profile_).value();
    ASSERT_EQ(shots.size(), 1U);
    EXPECT_EQ(shots[0].id, id);
    EXPECT_NEAR(units::KToC(shots[0].atmosphere.temperature_k), -5.0, 1e-9);
    ASSERT_TRUE(shots[0].predicted_elevation_rad.has_value());
    EXPECT_GT(*shots[0].predicted_elevation_rad, units::MradToRad(3.0));
    EXPECT_EQ(shots[0].notes, "first group");

    ASSERT_TRUE(SetShotUsedForTruing(db_, id, false).ok());
    EXPECT_FALSE(ComputeTruing(db_, profile_).ok); // the only shot is excluded
    ASSERT_TRUE(DeleteShot(db_, id).ok());
    EXPECT_TRUE(ListShots(db_, profile_).value().empty());
}

TEST_F(Truing, OneShotFitsVelocityOnly) {
    const SessionConditions s = Air(15.0);
    // The rifle actually shoots ~2 % slower than entered.
    LogShot(db_, profile_, s, 700.0, TruthAt(700.0, s, 0.98, 1.0)).value();
    const TruingResult r = ComputeTruing(db_, profile_);
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_FALSE(r.drag_fitted);
    EXPECT_DOUBLE_EQ(r.drag_scale, 1.0);
    EXPECT_NEAR(r.velocity_scale, 0.98, 0.001);
    EXPECT_LT(r.rms_after_rad, units::MradToRad(0.001));
    EXPECT_GT(r.rms_before_rad, units::MradToRad(0.1));
}

TEST_F(Truing, RecoversVelocityWithinOneMeterPerSecondAndDrag) {
    // Plan criterion: V0 recovered within 1 m/s from synthetic data.
    const double true_v = 1.0125; // 810 m/s instead of 800
    const double true_d = 0.96;
    for (const auto& [range, temp] : {std::pair{300.0, 10.0}, std::pair{600.0, 12.0},
                                      std::pair{900.0, 8.0}, std::pair{1200.0, 11.0}}) {
        const SessionConditions s = Air(temp);
        LogShot(db_, profile_, s, range, TruthAt(range, s, true_v, true_d)).value();
    }
    const TruingResult r = ComputeTruing(db_, profile_);
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_TRUE(r.drag_fitted);
    std::printf("fitted V0 %.2f m/s (true 810), drag x%.4f (true 0.96), rms %.4f -> %.6f mrad\n",
                r.muzzle_velocity_after_mps, r.drag_scale, units::RadToMrad(r.rms_before_rad),
                units::RadToMrad(r.rms_after_rad));
    EXPECT_NEAR(r.muzzle_velocity_after_mps, 810.0, 1.0);
    EXPECT_NEAR(r.drag_scale, true_d, 0.01);
    EXPECT_LT(r.rms_after_rad, units::MradToRad(0.002));
    ASSERT_EQ(r.points.size(), 4U);
    EXPECT_NEAR(r.points[3].predicted_after_rad, r.points[3].observed_rad, units::MradToRad(0.005));

    // Applying moves the profile; the logged shots are then predicted well.
    ASSERT_TRUE(ApplyTruing(db_, profile_, r).ok());
    const auto p = storage::LoadProfile(db_, profile_).value();
    EXPECT_DOUBLE_EQ(p.profile.velocity_scale, r.velocity_scale);
    EXPECT_DOUBLE_EQ(p.profile.drag_scale, r.drag_scale);
    const TruingResult again = ComputeTruing(db_, profile_);
    EXPECT_LT(again.rms_before_rad, units::MradToRad(0.002));

    ASSERT_TRUE(ResetTruing(db_, profile_).ok());
    EXPECT_DOUBLE_EQ(storage::LoadProfile(db_, profile_).value().profile.velocity_scale, 1.0);
}

TEST_F(Truing, NoisyObservationsStillImprove) {
    // Real logs are quantised to clicks (0.1 mrad): rounding is the noise.
    const double true_v = 0.985, true_d = 1.04;
    for (double range : {400.0, 650.0, 800.0, 1000.0, 1150.0}) {
        const SessionConditions s = Air(20.0);
        const double exact = TruthAt(range, s, true_v, true_d);
        const double clicked = std::round(units::RadToMrad(exact) * 10.0) / 10.0;
        LogShot(db_, profile_, s, range, units::MradToRad(clicked)).value();
    }
    const TruingResult r = ComputeTruing(db_, profile_);
    ASSERT_TRUE(r.ok) << r.error;
    std::printf("with click rounding: V0 x%.4f (true 0.985), drag x%.4f (true 1.04), rms %.3f -> %.3f mrad\n",
                r.velocity_scale, r.drag_scale, units::RadToMrad(r.rms_before_rad),
                units::RadToMrad(r.rms_after_rad));
    EXPECT_LT(r.rms_after_rad, r.rms_before_rad / 5.0);
    EXPECT_NEAR(r.velocity_scale, true_v, 0.01);
    EXPECT_NEAR(r.drag_scale, true_d, 0.05);
}

} // namespace
} // namespace ballistics::applogic
