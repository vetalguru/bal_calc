// Session, WEZ and truing at their edges: broken storage, broken profiles,
// shots at the very end of the bullet's flight.
#include <ballistics/applogic/armory.h>
#include <ballistics/applogic/session.h>
#include <ballistics/applogic/truing.h>
#include <ballistics/applogic/wez.h>
#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>
#include <ballistics/units.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace ballistics::applogic {
namespace {

// A .308 that flies for the solver's whole 60 s (~5.4 km), then stops.
class ApplogicEdges : public ::testing::Test {
   protected:
    void SetUp() override {
        ASSERT_TRUE(db_.Open(":memory:").ok());
        RifleForm r;
        r.name = "Edge test";
        r.sight_height_cm = 5.0;
        r.twist_in = 10.0;
        CartridgeForm c;
        c.name = "Edge test";
        c.bullet_name = "SMK 175";
        c.drag_table = "G7";
        c.bc = 0.243;
        c.mass_gr = 175.0;
        c.diameter_in = 0.308;
        c.length_in = 1.24;
        c.muzzle_velocity_mps = 800.0;
        profile_ =
            EnsureProfile(db_, SaveRifleForm(db_, r).value(), SaveCartridgeForm(db_, c).value())
                .value();
    }

    void Exec(const std::string& sql) { ASSERT_TRUE(db_.connection().Execute(sql).ok()) << sql; }

    storage::LoadedProfile Loaded() { return storage::LoadProfile(db_, profile_).value(); }

    // The settings table becomes a view that fails to read `key` (SQLite's
    // integer overflow error); every other key reads as before.
    void PoisonSetting(const std::string& key) {
        Exec("ALTER TABLE app_setting RENAME TO app_setting_data");
        Exec("CREATE VIEW app_setting AS SELECT key, CASE WHEN key = '" + key +
             "' THEN abs(-9223372036854775807 - 1) ELSE value END AS value FROM app_setting_data");
        Exec("INSERT INTO app_setting_data (key, value) VALUES ('" + key + "', '1')");
    }

    // Writing `key` to the settings fails.
    void RejectSetting(const std::string& key) {
        Exec("CREATE TRIGGER reject BEFORE INSERT ON app_setting WHEN NEW.key = '" + key +
             "' BEGIN SELECT RAISE(ABORT, 'rejected'); END");
    }

    // Farthest the bullet gets (LOS distance) in the default session air.
    static double Reach(const storage::LoadedProfile& p) {
        return storage::Solve(p, ToConditions(SessionConditions{}), 1e6)
            .value()
            .trajectory.max_slant_range_m();
    }
    double Reach(double v_scale, double d_scale) {
        storage::LoadedProfile p = Loaded();
        p.profile.velocity_scale = v_scale;
        p.profile.drag_scale = d_scale;
        return Reach(p);
    }

    // Elevation the profile predicts at `range_m` in the default air.
    double ElevationAt(const storage::LoadedProfile& p, double range_m) {
        return storage::Solve(p, ToConditions(SessionConditions{}), range_m + 1.0)
            .value()
            .trajectory.AtSlantRange(range_m)
            ->hold_elevation_rad;
    }

    Id Log(double range_m, double elevation_rad) {
        return LogShot(db_, profile_, SessionConditions{}, range_m, elevation_rad).value();
    }

    storage::Database db_;
    Id profile_ = 0;
};

// ---------------------------------------------------------------- session

TEST_F(ApplogicEdges, SessionNeedsTheSettingsTable) {
    Exec("DROP TABLE app_setting");
    EXPECT_FALSE(LoadSession(db_).ok());
    EXPECT_FALSE(SaveSession(db_, SessionConditions{}).ok());
}

TEST_F(ApplogicEdges, SessionReportsAnOptionalFieldItCannotRead) {
    PoisonSetting("session.powder_c");
    const auto s = LoadSession(db_);
    ASSERT_FALSE(s.ok());
    EXPECT_NE(s.error().message.find("overflow"), std::string::npos) << s.error().message;
}

TEST_F(ApplogicEdges, SessionReportsWindsItCannotRead) {
    PoisonSetting("session.winds");
    EXPECT_FALSE(LoadSession(db_).ok());
}

TEST_F(ApplogicEdges, SessionIsSavedWholeOrNotAtAll) {
    RejectSetting("session.winds");  // the last key written
    SessionConditions s;
    s.temperature_c = -20.0;
    s.target_range_m = 850.0;
    s.winds = {{4.0, 270.0, 0.0}};
    EXPECT_FALSE(SaveSession(db_, s).ok());
    // The keys written before the failure are rolled back.
    const SessionConditions g = LoadSession(db_).value();
    EXPECT_DOUBLE_EQ(g.temperature_c, 15.0);
    EXPECT_DOUBLE_EQ(g.target_range_m, 300.0);
    EXPECT_TRUE(g.winds.empty());
}

TEST_F(ApplogicEdges, SessionIsNotSavedInsideAnotherTransaction) {
    Exec("BEGIN");
    EXPECT_FALSE(SaveSession(db_, SessionConditions{}).ok());
    Exec("ROLLBACK");
    EXPECT_TRUE(SaveSession(db_, SessionConditions{}).ok());
}

TEST_F(ApplogicEdges, SessionSkipsWhatItCannotParse) {
    ASSERT_TRUE(storage::SetSetting(db_, "session.temperature_c", "warm").ok());
    ASSERT_TRUE(storage::SetSetting(db_, "session.pressure_hpa", "990hPa").ok());
    ASSERT_TRUE(storage::SetSetting(db_, "session.powder_c", "").ok());
    ASSERT_TRUE(storage::SetSetting(db_, "session.altitude_m", "250").ok());
    // One good zone among a short one, a bad number and an empty one.
    ASSERT_TRUE(storage::SetSetting(db_, "session.winds", "5,90,400;3,270;x,0,0;;2.5,45,0").ok());
    const SessionConditions g = LoadSession(db_).value();
    EXPECT_DOUBLE_EQ(g.temperature_c, 15.0);
    EXPECT_DOUBLE_EQ(g.pressure_hpa, 1013.25);
    EXPECT_FALSE(g.powder_c.has_value());
    EXPECT_DOUBLE_EQ(g.altitude_m, 250.0);
    ASSERT_EQ(g.winds.size(), 2u);
    EXPECT_DOUBLE_EQ(g.winds[0].speed_mps, 5.0);
    EXPECT_DOUBLE_EQ(g.winds[0].until_m, 400.0);
    EXPECT_DOUBLE_EQ(g.winds[1].from_deg, 45.0);
}

TEST_F(ApplogicEdges, ConditionsOfTheSession) {
    SessionConditions s;
    s.winds = {{3.0, 90.0, 0.0}, {5.0, 270.0, 600.0}};
    s.azimuth_deg = 45.0;  // no latitude: no Earth rotation at all
    const storage::ConditionsRecord c = ToConditions(s);
    ASSERT_EQ(c.winds.size(), 2u);
    EXPECT_DOUBLE_EQ(c.winds[0].until_range_m, 1e6);  // 0 = to the end of the range
    EXPECT_DOUBLE_EQ(c.winds[1].until_range_m, 600.0);
    EXPECT_NEAR(c.winds[1].from_rad, units::DegToRad(270.0), 1e-12);
    EXPECT_FALSE(c.latitude_rad.has_value());
    EXPECT_FALSE(c.azimuth_rad.has_value());
    EXPECT_FALSE(c.powder_temp_k.has_value());
}

TEST_F(ApplogicEdges, BrokenProfileGivesTheSolverError) {
    storage::LoadedProfile p = Loaded();
    p.bullet.drag_table = "G42";
    const SolutionSummary r = Summarize(p, SessionConditions{}, AngleUnit::kMrad);
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error.find("unknown drag table G42"), std::string::npos) << r.error;
    const RangeTable t = BuildRangeTable(p, SessionConditions{}, AngleUnit::kMrad, 0, 500, 100);
    EXPECT_FALSE(t.ok);
    EXPECT_EQ(t.error, r.error);
    EXPECT_TRUE(t.rows.empty());
}

TEST_F(ApplogicEdges, BeyondTheFlightThereIsNoSolution) {
    const storage::LoadedProfile p = Loaded();
    const double reach = Reach(p);
    ASSERT_GT(reach, 4000.0);
    ASSERT_LT(reach, 8000.0);
    SessionConditions s;
    s.target_range_m = 8000.0;
    const SolutionSummary r = Summarize(p, s, AngleUnit::kMoa);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.error, "The bullet does not reach this range.");

    // The range card stops at the last row the bullet gets to.
    const RangeTable t = BuildRangeTable(p, s, AngleUnit::kMrad, 0.0, 8000.0, 500.0);
    ASSERT_TRUE(t.ok) << t.error;
    ASSERT_FALSE(t.rows.empty());
    EXPECT_LT(t.rows.size(), 17u);
    EXPECT_LE(t.rows.back().range_m, reach);
    EXPECT_GT(t.rows.back().range_m + 500.0, reach);
    EXPECT_LT(t.rows.back().mach, 1.0);
    for (std::size_t i = 1; i < t.rows.size(); ++i) {
        EXPECT_GT(t.rows[i].time_s, t.rows[i - 1].time_s) << i;
        EXPECT_GT(t.rows[i].elevation, t.rows[i - 1].elevation) << i;
    }
}

TEST_F(ApplogicEdges, MarginalStabilityWarns) {
    storage::LoadedProfile p = Loaded();
    p.rifle.twist_m = units::InchToM(14.0);  // Sg ~1.23 for the 1.24" SMK
    const SolutionSummary r = Summarize(p, SessionConditions{}, AngleUnit::kMrad);
    ASSERT_TRUE(r.ok) << r.error;
    ASSERT_GT(r.stability, 1.0);
    ASSERT_LT(r.stability, kMarginalStability);
    bool low = false, unstable = false;
    for (const Warning& w : r.warnings) {
        low = low || (w.code == kWarnLowStability && w.value == r.stability);
        unstable = unstable || w.code == kWarnUnstable;
    }
    EXPECT_TRUE(low);
    EXPECT_FALSE(unstable);
}

// -------------------------------------------------------------------- WEZ

TEST_F(ApplogicEdges, WezSettingsNeedTheSettingsTable) {
    Exec("DROP TABLE app_setting");
    EXPECT_FALSE(LoadWezSettings(db_).ok());
    EXPECT_FALSE(SaveWezSettings(db_, WezSettings{}).ok());
}

TEST_F(ApplogicEdges, WezSettingsReportATargetKindTheyCannotRead) {
    PoisonSetting("wez.target_kind");
    EXPECT_FALSE(LoadWezSettings(db_).ok());
}

TEST_F(ApplogicEdges, WezSettingsKeepTheDefaultForAWrongNumber) {
    ASSERT_TRUE(storage::SetSetting(db_, "wez.range_m", "far").ok());
    ASSERT_TRUE(storage::SetSetting(db_, "wez.group_moa", "0.75").ok());
    const WezSettings w = LoadWezSettings(db_).value();
    EXPECT_DOUBLE_EQ(w.range_m, 5.0);
    EXPECT_DOUBLE_EQ(w.group_moa, 0.75);
}

TEST_F(ApplogicEdges, WezSettingsAreSavedWholeOrNotAtAll) {
    RejectSetting("wez.target_kind");
    WezSettings w;
    w.group_moa = 0.4;
    w.target_kind = "ellipse";
    EXPECT_FALSE(SaveWezSettings(db_, w).ok());
    const WezSettings g = LoadWezSettings(db_).value();
    EXPECT_DOUBLE_EQ(g.group_moa, 1.0);
    EXPECT_EQ(g.target_kind, "rectangle");

    Exec("DROP TRIGGER reject");
    Exec("BEGIN");
    EXPECT_FALSE(SaveWezSettings(db_, w).ok());
    Exec("ROLLBACK");
    ASSERT_TRUE(SaveWezSettings(db_, w).ok());
    EXPECT_EQ(LoadWezSettings(db_).value().target_kind, "ellipse");
}

TEST_F(ApplogicEdges, WezTargetsAndNegativeErrors) {
    WezSettings w;
    w.target_kind = "ellipse";
    w.target_width_cm = 0.0;  // kept above zero
    EXPECT_EQ(ToTarget(w).kind, Target::Kind::kEllipse);
    EXPECT_DOUBLE_EQ(ToTarget(w).width_m, 0.001);
    w.target_kind = "square";  // unknown: a rectangle
    EXPECT_EQ(ToTarget(w).kind, Target::Kind::kRectangle);
    // A sigma below zero means no error, not a negative one.
    w.wind_speed_mps = -3.0;
    w.bc_percent = -1.0;
    const ErrorSources e = ToErrorSources(w);
    EXPECT_DOUBLE_EQ(e.wind_speed_mps, 0.0);
    EXPECT_DOUBLE_EQ(e.drag_fraction, 0.0);
    EXPECT_DOUBLE_EQ(e.pressure_pa, 200.0);
}

TEST_F(ApplogicEdges, WezOfABrokenProfileIsTheSolverError) {
    storage::LoadedProfile p = Loaded();
    p.bullet.drag_table = "G42";
    const WezResult r = ComputeWez(p, SessionConditions{}, WezSettings{}, 1000.0, 100.0);
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error.find("G42"), std::string::npos) << r.error;
}

TEST_F(ApplogicEdges, WezStopsWhereTheBulletStops) {
    const storage::LoadedProfile p = Loaded();
    const double reach = Reach(p);
    SessionConditions s;
    s.target_range_m = 600.0;
    const WezResult r = ComputeWez(p, s, WezSettings{}, 8000.0, 500.0);
    ASSERT_TRUE(r.ok) << r.error;
    ASSERT_FALSE(r.rows.empty());
    EXPECT_LT(r.rows.size(), 16u);
    EXPECT_LE(r.rows.back().range_m, reach);
    EXPECT_LT(r.rows.back().probability, r.at_target.probability);
    EXPECT_GT(r.at_target.probability, 0.0);

    s.target_range_m = 8000.0;
    const WezResult beyond = ComputeWez(p, s, WezSettings{}, 1000.0, 100.0);
    EXPECT_FALSE(beyond.ok);
    EXPECT_EQ(beyond.error, "The bullet does not reach this range.");
    EXPECT_EQ(beyond.rows.size(), 10u);  // the rows up to 1000 m are still there
}

// ----------------------------------------------------------------- truing

TEST_F(ApplogicEdges, TruingOfAMissingProfileOrShot) {
    const Id none = profile_ + 100;
    EXPECT_FALSE(LogShot(db_, none, SessionConditions{}, 500.0, 0.003).ok());
    EXPECT_FALSE(ComputeTruing(db_, none).error.empty());
    EXPECT_FALSE(ComputeDsf(db_, none).error.empty());
    TruingResult neutral;
    neutral.ok = true;
    EXPECT_EQ(ApplyTruing(db_, none, neutral).error().code, sqlite_manager::ErrorCode::kNotFound);
    EXPECT_EQ(ResetTruing(db_, none).error().message, "profile not found");
    EXPECT_EQ(SetDsf(db_, none, {{1.0, 1.1}}).error().code, sqlite_manager::ErrorCode::kNotFound);
    EXPECT_EQ(SetShotUsedForTruing(db_, 12345, false).error().message, "shot not found");
    // A failed result is never applied.
    EXPECT_EQ(ApplyTruing(db_, profile_, TruingResult{}).error().code,
              sqlite_manager::ErrorCode::kMisuse);
    EXPECT_FALSE(LogShot(db_, profile_, SessionConditions{}, 0.0, 0.003).ok());
    EXPECT_TRUE(ListShots(db_, profile_).value().empty());
}

TEST_F(ApplogicEdges, TruingNeedsTheShotLog) {
    const Id shot = Log(500.0, 0.003);
    Exec("DROP TABLE dope_log");
    EXPECT_FALSE(ListShots(db_, profile_).ok());
    EXPECT_FALSE(ComputeTruing(db_, profile_).error.empty());
    EXPECT_FALSE(ComputeDsf(db_, profile_).error.empty());
    EXPECT_FALSE(SetShotUsedForTruing(db_, shot, false).ok());
}

TEST_F(ApplogicEdges, TruingNeedsTheProfileTable) {
    Exec("PRAGMA foreign_keys = OFF");
    Exec("DROP TABLE profile");
    TruingResult neutral;
    neutral.ok = true;
    EXPECT_FALSE(ApplyTruing(db_, profile_, neutral).ok());
    EXPECT_FALSE(SetDsf(db_, profile_, {}).ok());
}

TEST_F(ApplogicEdges, TruingReportsRejectedWrites) {
    const Id shot = Log(500.0, 0.003);
    Exec(
        "CREATE TRIGGER frozen_profile BEFORE UPDATE ON profile "
        "BEGIN SELECT RAISE(ABORT, 'frozen'); END");
    Exec(
        "CREATE TRIGGER frozen_log BEFORE UPDATE ON dope_log "
        "BEGIN SELECT RAISE(ABORT, 'frozen'); END");
    TruingResult r;
    r.ok = true;
    r.velocity_scale = 1.05;
    EXPECT_FALSE(ApplyTruing(db_, profile_, r).ok());
    EXPECT_FALSE(SetDsf(db_, profile_, {{1.0, 1.1}}).ok());
    EXPECT_FALSE(SetShotUsedForTruing(db_, shot, false).ok());
    // Nothing changed.
    EXPECT_DOUBLE_EQ(Loaded().profile.velocity_scale, 1.0);
    EXPECT_TRUE(Loaded().profile.dsf.empty());
    EXPECT_TRUE(ListShots(db_, profile_).value().front().use_for_truing);
}

TEST_F(ApplogicEdges, TruingOfAProfileTheSolverRejects) {
    Exec("UPDATE bullet SET drag_table = 'G42'");
    // The shot is logged all the same, without a prediction.
    Log(600.0, 0.004);
    const auto shots = ListShots(db_, profile_).value();
    ASSERT_EQ(shots.size(), 1u);
    EXPECT_FALSE(shots[0].predicted_elevation_rad.has_value());
    EXPECT_EQ(ComputeTruing(db_, profile_).error,
              "The bullet does not reach one of the logged ranges.");
    EXPECT_EQ(ComputeDsf(db_, profile_).error,
              "The bullet does not reach one of the logged ranges.");
}

TEST_F(ApplogicEdges, TruingOfAShotBeyondTheFlight) {
    Log(600.0, ElevationAt(Loaded(), 600.0));
    Log(8000.0, 0.5);
    const auto shots = ListShots(db_, profile_).value();
    ASSERT_EQ(shots.size(), 2u);
    EXPECT_FALSE(shots[1].predicted_elevation_rad.has_value());
    EXPECT_EQ(ComputeTruing(db_, profile_).error,
              "The bullet does not reach one of the logged ranges.");
    EXPECT_EQ(ComputeDsf(db_, profile_).error,
              "The bullet does not reach one of the logged ranges.");
}

// A shot the bullet just reaches, but not 0.01 % slower: the velocity
// derivative cannot be taken there.
TEST_F(ApplogicEdges, TruingAtTheEdgeOfTheFlightVelocity) {
    const double reach = Reach(1.0, 1.0);
    const double slower = Reach(1.0 - 1e-4, 1.0);
    ASSERT_LT(slower, reach - 0.05);
    const double range = 0.5 * (reach + slower);
    Log(range, ElevationAt(Loaded(), range));
    const TruingResult r = ComputeTruing(db_, profile_);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.error, "The bullet does not reach one of the logged ranges.");
}

// Likewise with 0.01 % more drag (which shortens the flight more than
// 0.01 % less velocity does).
TEST_F(ApplogicEdges, TruingAtTheEdgeOfTheFlightDrag) {
    const double slower = Reach(1.0 - 1e-4, 1.0);
    const double draggier = Reach(1.0, 1.0 + 1e-4);
    ASSERT_LT(draggier, slower - 0.05);
    const double range = 0.5 * (slower + draggier);
    Log(range, ElevationAt(Loaded(), range));
    const TruingResult r = ComputeTruing(db_, profile_);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.error, "The bullet does not reach one of the logged ranges.");
}

// A hit near the end of the flight asking for a much slower bullet: the fit
// walks toward it until the bullet would no longer get there, and stops.
TEST_F(ApplogicEdges, TruingStopsWhereTheBulletStopsReaching) {
    const double range = Reach(1.0, 1.0) - 1.0;
    const double predicted = ElevationAt(Loaded(), range);
    Log(range, predicted + units::MradToRad(5.0));
    const TruingResult r = ComputeTruing(db_, profile_);
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_FALSE(r.drag_fitted);
    EXPECT_LT(r.velocity_scale, 1.0);
    EXPECT_GT(r.velocity_scale, 0.999);  // ~1 m of flight left: no further
    EXPECT_LE(r.rms_after_rad, r.rms_before_rad);
    EXPECT_GT(r.rms_after_rad, 0.0);
}

// ------------------------------------------------------------------- DSF

TEST_F(ApplogicEdges, DsfIgnoresShotsNotForTruing) {
    const storage::LoadedProfile p = Loaded();
    Log(1300.0, ElevationAt(p, 1300.0));
    const Id excluded = Log(1500.0, ElevationAt(p, 1500.0) + units::MradToRad(3.0));
    ASSERT_TRUE(SetShotUsedForTruing(db_, excluded, false).ok());
    const DsfResult r = ComputeDsf(db_, profile_);
    ASSERT_TRUE(r.ok) << r.error;
    ASSERT_EQ(r.shots.size(), 1u);
    EXPECT_DOUBLE_EQ(r.shots[0].range_m, 1300.0);
    // The hit is where the bare profile says: the fitted factors stay ~1.
    for (const DsfPoint& pt : r.points) {
        EXPECT_NEAR(pt.factor, 1.0, 0.01) << pt.mach;
    }
    EXPECT_LT(r.rms_after_rad, units::MradToRad(0.01));
}

// Two hits at almost one Mach that disagree: one DSF point serves both, and
// the joint fit settles between them.
TEST_F(ApplogicEdges, DsfSettlesBetweenHitsThatDisagree) {
    const storage::LoadedProfile p = Loaded();
    Log(1300.0, ElevationAt(p, 1300.0));
    Log(1320.0, ElevationAt(p, 1320.0) + units::MradToRad(0.06));
    const DsfResult r = ComputeDsf(db_, profile_);
    ASSERT_TRUE(r.ok) << r.error;
    ASSERT_EQ(r.shots.size(), 2u);
    EXPECT_TRUE(r.shots[0].used);
    EXPECT_FALSE(r.shots[1].used);  // too close in Mach to the first
    EXPECT_EQ(r.points.size(), 2u);
    // Neither hit is matched: one over, one under.
    EXPECT_GT(r.shots[0].predicted_after_rad, r.shots[0].observed_rad);
    EXPECT_LT(r.shots[1].predicted_after_rad, r.shots[1].observed_rad);
    EXPECT_LT(r.rms_after_rad, r.rms_before_rad);
}

// Far less elevation than the bare drag gives: even half the drag at that
// Mach does not explain it.
TEST_F(ApplogicEdges, DsfCannotGoBelowHalfTheDrag) {
    const storage::LoadedProfile p = Loaded();
    Log(1300.0, ElevationAt(p, 1300.0) - units::MradToRad(4.0));
    const DsfResult r = ComputeDsf(db_, profile_);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.error,
              "The DSF alone cannot explain these hits: true the velocity and drag first.");
    ASSERT_EQ(r.shots.size(), 1u);
    EXPECT_TRUE(r.shots[0].used);
    EXPECT_TRUE(r.shots[0].limited);
    // Half the drag gets closer, but not there.
    EXPECT_LT(r.shots[0].predicted_after_rad, r.shots[0].predicted_before_rad);
    EXPECT_GT(r.shots[0].predicted_after_rad, r.shots[0].observed_rad);
}

// More elevation than any reachable factor gives, at the end of the flight:
// twice the drag stops the bullet short, so the fit has nothing to start
// from and the shot stays unexplained.
TEST_F(ApplogicEdges, DsfOfAHitTheDragCannotReach) {
    const storage::LoadedProfile p = Loaded();
    const double range = Reach(p) - 1.0;
    Log(range, ElevationAt(p, range) + units::MradToRad(1.0));
    const DsfResult r = ComputeDsf(db_, profile_);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.error,
              "The DSF alone cannot explain these hits: true the velocity and drag first.");
    ASSERT_EQ(r.shots.size(), 1u);
    EXPECT_TRUE(r.shots[0].limited);
    // No fitted flight reaches the shot: it keeps its prediction.
    EXPECT_DOUBLE_EQ(r.shots[0].predicted_after_rad, r.shots[0].predicted_before_rad);
}

// A hit that needs exactly the largest factor, where the bullet only just
// gets with it: the joint refinement cannot take a derivative there and
// keeps the sequential fit.
TEST_F(ApplogicEdges, DsfAtTheEdgeOfTheFlightKeepsTheSequentialFit) {
    storage::LoadedProfile p = Loaded();
    double range = 3000.0;
    double mach = 0.0;
    for (int i = 0; i < 6; ++i) {
        mach = storage::Solve(p, ToConditions(SessionConditions{}), range + 1.0)
                   .value()
                   .trajectory.AtSlantRange(range)
                   ->mach;
        storage::LoadedProfile hi = p, beyond = p;
        hi.profile.dsf = {{mach, kDsfMaxFactor}, {kDsfAnchorMach, 1.0}};
        beyond.profile.dsf = {{mach, kDsfMaxFactor + 1e-4}, {kDsfAnchorMach, 1.0}};
        range = 0.5 * (Reach(hi) + Reach(beyond));
    }
    storage::LoadedProfile hi = p;
    hi.profile.dsf = {{mach, kDsfMaxFactor}, {kDsfAnchorMach, 1.0}};
    ASSERT_LT(mach, kDsfMaxMach);
    const double observed = ElevationAt(hi, range) + units::MradToRad(0.5);
    Log(range, observed);
    const DsfResult r = ComputeDsf(db_, profile_);
    ASSERT_EQ(r.shots.size(), 1u);
    EXPECT_TRUE(r.shots[0].used);
    EXPECT_GT(r.shots[0].predicted_after_rad, r.shots[0].predicted_before_rad);
    EXPECT_TRUE(r.shots[0].limited);
    EXPECT_FALSE(r.ok);
}

// ------------------------------------------------------------------- BC

TEST_F(ApplogicEdges, BcFromAHitChecksItsInput) {
    storage::LoadedProfile p = Loaded();
    const SessionConditions s;
    EXPECT_EQ(BcFromHit(p, "G9", 600.0, 0.004, s).error, "Unknown drag table.");
    // A zero far beyond any flight: no BC gives a solution at all.
    p.rifle.zero_range_m = 20000.0;
    const BcResult r = BcFromHit(p, "G7", 600.0, 0.004, s);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.error, "No BC between 0.02 and 2 gives this correction.");
}

}  // namespace
}  // namespace ballistics::applogic
