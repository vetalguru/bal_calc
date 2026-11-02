// Repository, Database and Solution edge cases: every record type through
// the generic CRUD paths, child rows, and the failure paths of SQLite
// (closed connection, missing tables, oversized values, interrupted
// statements, a lock held by another connection, a broken file).
#include <ballistics/storage/database.h>
#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>
#include <ballistics/units.h>
#include <gtest/gtest.h>
#include <sqlite3.h>
#include <sqlite_manager/connection.h>
#include <sqlite_manager/statement.h>
#include <sqlite_manager/transaction.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace ballistics::storage {
namespace {

namespace fs = std::filesystem;
using sqlite_manager::ErrorCode;

class TempDbFile {
   public:
    TempDbFile()
        : path_(fs::temp_directory_path() /
                ("balcalc_repo_test_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) +
                 ".db")) {
        fs::remove(path_);
    }
    ~TempDbFile() {
        fs::remove(path_);
        fs::remove(path_.string() + "-journal");
    }
    std::string path() const { return path_.string(); }

   private:
    fs::path path_;
};

// Makes every statement whose SQL contains `needle` fail with
// SQLITE_INTERRUPT when stepped: a trace hook marks the statement as it
// starts, a progress handler then aborts it.
class StepFault {
   public:
    StepFault(Database& db, std::string needle) : db_(db.connection().raw()), needle_(needle) {
        sqlite3_trace_v2(db_, SQLITE_TRACE_STMT, &StepFault::OnStatement, this);
        sqlite3_progress_handler(db_, 1, &StepFault::OnProgress, this);
    }
    ~StepFault() {
        sqlite3_trace_v2(db_, 0, nullptr, nullptr);
        sqlite3_progress_handler(db_, 0, nullptr, nullptr);
    }
    StepFault(const StepFault&) = delete;
    StepFault& operator=(const StepFault&) = delete;

   private:
    static int OnStatement(unsigned /*type*/, void* self, void* /*stmt*/, void* sql) {
        auto* f = static_cast<StepFault*>(self);
        const char* text = static_cast<const char*>(sql);
        if (std::strncmp(text, "--", 2) != 0) {  // not a trigger sub-statement
            f->armed_ = std::strstr(text, f->needle_.c_str()) != nullptr;
        }
        return 0;
    }
    static int OnProgress(void* self) { return static_cast<StepFault*>(self)->armed_ ? 1 : 0; }

    sqlite3* db_;
    std::string needle_;
    bool armed_ = false;
};

// Lowers the longest string/blob SQLite accepts, so binding a longer value
// fails with SQLITE_TOOBIG.
class LengthLimit {
   public:
    LengthLimit(Database& db, int limit)
        : db_(db.connection().raw()), old_(sqlite3_limit(db_, SQLITE_LIMIT_LENGTH, limit)) {}
    ~LengthLimit() { sqlite3_limit(db_, SQLITE_LIMIT_LENGTH, old_); }
    LengthLimit(const LengthLimit&) = delete;
    LengthLimit& operator=(const LengthLimit&) = delete;

   private:
    sqlite3* db_;
    int old_;
};

constexpr int kSmallLimit = 500;
const std::string kTooLong(2000, 'x');

// Runs SQL on the database with foreign keys off (to leave dangling
// references or drop referenced tables).
void ExecUnchecked(Database& db, const std::string& sql) {
    ASSERT_TRUE(db.connection().Execute("PRAGMA foreign_keys = OFF").ok());
    ASSERT_TRUE(db.connection().Execute(sql).ok()) << sql;
    ASSERT_TRUE(db.connection().Execute("PRAGMA foreign_keys = ON").ok());
}

// --- Valid records of each type, with whatever they reference saved ------

BulletRecord SampleBullet() {
    BulletRecord b;
    b.name = "Sample SMK 175";
    b.caliber = ".308";
    b.diameter_m = units::InchToM(0.308);
    b.mass_kg = units::GrainToKg(175.0);
    b.bc = 0.243;
    return b;
}

CartridgeRecord SampleCartridge(Database& db) {
    BulletRecord b = SampleBullet();
    EXPECT_TRUE(Repository<BulletRecord>(db).Save(b).ok());
    CartridgeRecord c;
    c.name = "Sample M118LR";
    c.bullet_id = b.id;
    c.muzzle_velocity_mps = 790.0;
    return c;
}

ProfileRecord SampleProfile(Database& db) {
    CartridgeRecord c = SampleCartridge(db);
    EXPECT_TRUE(Repository<CartridgeRecord>(db).Save(c).ok());
    RifleRecord r;
    r.name = "M24";
    EXPECT_TRUE(Repository<RifleRecord>(db).Save(r).ok());
    ProfileRecord p;
    p.name = "Sample M24 / M118LR";
    p.rifle_id = r.id;
    p.cartridge_id = c.id;
    return p;
}

// Per record type: its table, a valid unsaved record, a text field to edit
// and, for types with child rows, the child table and its rows.
template <typename T>
struct Traits;

template <>
struct Traits<DragCurveRecord> {
    static constexpr const char* kTable = "drag_curve";
    static constexpr const char* kChildTable = "drag_point";
    static constexpr bool kHasName = true;
    static DragCurveRecord Make(Database&) {
        DragCurveRecord r;
        r.name = "Sample Doppler";
        return r;
    }
    static std::string& Label(DragCurveRecord& r) { return r.notes; }
    static void AddChild(DragCurveRecord& r) { r.points.push_back({0.5, 0.15}); }
    static void AddBadChild(DragCurveRecord& r) { r.points.push_back({0.8, -1.0}); }
    static std::size_t Children(const DragCurveRecord& r) { return r.points.size(); }
};

template <>
struct Traits<BulletRecord> {
    static constexpr const char* kTable = "bullet";
    static constexpr const char* kChildTable = "bullet_bc_band";
    static constexpr bool kHasName = true;
    static BulletRecord Make(Database&) { return SampleBullet(); }
    static std::string& Label(BulletRecord& r) { return r.notes; }
    static void AddChild(BulletRecord& r) { r.bc_bands.push_back({800.0, 0.25}); }
    static void AddBadChild(BulletRecord& r) { r.bc_bands.push_back({0.0, 0.25}); }
    static std::size_t Children(const BulletRecord& r) { return r.bc_bands.size(); }
};

template <>
struct Traits<CartridgeRecord> {
    static constexpr const char* kTable = "cartridge";
    static constexpr const char* kChildTable = "cartridge_velocity_point";
    static constexpr bool kHasName = true;
    static CartridgeRecord Make(Database& db) { return SampleCartridge(db); }
    static std::string& Label(CartridgeRecord& r) { return r.notes; }
    static void AddChild(CartridgeRecord& r) { r.velocity_points.push_back({288.15, 790.0}); }
    static void AddBadChild(CartridgeRecord& r) { r.velocity_points.push_back({300.0, 0.0}); }
    static std::size_t Children(const CartridgeRecord& r) { return r.velocity_points.size(); }
};

template <>
struct Traits<RifleRecord> {
    static constexpr const char* kTable = "rifle";
    static constexpr bool kHasName = true;
    static RifleRecord Make(Database&) {
        RifleRecord r;
        r.name = "Sample Tikka";
        return r;
    }
    static std::string& Label(RifleRecord& r) { return r.notes; }
};

template <>
struct Traits<ReticleRecord> {
    static constexpr const char* kTable = "reticle";
    static constexpr bool kHasName = true;
    static ReticleRecord Make(Database&) {
        ReticleRecord r;
        r.name = "Sample Mil-XT";
        return r;
    }
    static std::string& Label(ReticleRecord& r) { return r.source; }
};

template <>
struct Traits<ScopeRecord> {
    static constexpr const char* kTable = "scope";
    static constexpr bool kHasName = true;
    static ScopeRecord Make(Database&) {
        ScopeRecord r;
        r.name = "Sample PMII";
        r.click_vertical_rad = r.click_horizontal_rad = units::MradToRad(0.1);
        return r;
    }
    static std::string& Label(ScopeRecord& r) { return r.notes; }
};

template <>
struct Traits<ProfileRecord> {
    static constexpr const char* kTable = "profile";
    static constexpr const char* kChildTable = "profile_dsf";
    static constexpr bool kHasName = true;
    static ProfileRecord Make(Database& db) { return SampleProfile(db); }
    static std::string& Label(ProfileRecord& r) { return r.name; }
    static void AddChild(ProfileRecord& r) { r.dsf.push_back({1.0, 1.02}); }
    static void AddBadChild(ProfileRecord& r) { r.dsf.push_back({1.2, 0.0}); }
    static std::size_t Children(const ProfileRecord& r) { return r.dsf.size(); }
};

template <>
struct Traits<ConditionsRecord> {
    static constexpr const char* kTable = "conditions";
    static constexpr const char* kChildTable = "wind_zone";
    static constexpr bool kHasName = true;
    static ConditionsRecord Make(Database&) {
        ConditionsRecord r;
        r.name = "Sample range day";
        return r;
    }
    static std::string& Label(ConditionsRecord& r) { return r.name; }
    static void AddChild(ConditionsRecord& r) { r.winds.push_back({1e5, 3.0, 1.0, 0.0}); }
    static void AddBadChild(ConditionsRecord& r) { r.winds.push_back({1e5, -3.0, 1.0, 0.0}); }
    static std::size_t Children(const ConditionsRecord& r) { return r.winds.size(); }
};

template <>
struct Traits<DopeRecord> {
    static constexpr const char* kTable = "dope_log";
    static constexpr bool kHasName = false;
    static DopeRecord Make(Database& db) {
        ProfileRecord p = SampleProfile(db);
        EXPECT_TRUE(Repository<ProfileRecord>(db).Save(p).ok());
        DopeRecord d;
        d.profile_id = p.id;
        d.range_m = 600.0;
        d.observed_elevation_rad = units::MradToRad(4.1);
        return d;
    }
    static std::string& Label(DopeRecord& r) { return r.notes; }
};

// --- Every record type ----------------------------------------------------

template <typename T>
class StorageRepositoryAllTypes : public ::testing::Test {
   protected:
    void SetUp() override { ASSERT_TRUE(db_.Open(":memory:").ok()); }

    T Saved() {
        T r = Traits<T>::Make(db_);
        EXPECT_TRUE(Repository<T>(db_).Save(r).ok());
        return r;
    }

    Database db_;
};

using AllRecordTypes =
    ::testing::Types<DragCurveRecord, BulletRecord, CartridgeRecord, RifleRecord, ReticleRecord,
                     ScopeRecord, ProfileRecord, ConditionsRecord, DopeRecord>;
TYPED_TEST_SUITE(StorageRepositoryAllTypes, AllRecordTypes);

TYPED_TEST(StorageRepositoryAllTypes, SaveGetUpdateListRemove) {
    using T = TypeParam;
    Repository<T> repo(this->db_);
    T r = Traits<T>::Make(this->db_);
    Traits<T>::Label(r) = "first";
    auto id = repo.Save(r);
    ASSERT_TRUE(id.ok()) << id.error().message;
    EXPECT_GT(r.id, 0);
    EXPECT_EQ(id.value(), r.id);

    auto got = repo.Get(r.id);
    ASSERT_TRUE(got.ok()) << got.error().message;
    ASSERT_TRUE(got.value().has_value());
    EXPECT_EQ(Traits<T>::Label(*got.value()), "first");

    // Update in place: same id, new value.
    const Id first_id = r.id;
    Traits<T>::Label(r) = "edited";
    ASSERT_TRUE(repo.Save(r).ok());
    EXPECT_EQ(r.id, first_id);
    T again = *repo.Get(r.id).value();
    EXPECT_EQ(Traits<T>::Label(again), "edited");

    auto all = repo.List();
    ASSERT_TRUE(all.ok()) << all.error().message;
    ASSERT_EQ(all.value().size(), 1U);
    EXPECT_EQ(all.value()[0].id, r.id);
    EXPECT_EQ(Traits<T>::Label(all.value()[0]), "edited");

    ASSERT_TRUE(repo.Remove(r.id).ok());
    EXPECT_FALSE(repo.Get(r.id).value().has_value());
    EXPECT_TRUE(repo.List().value().empty());
    // Removing what is not there is not an error.
    EXPECT_TRUE(repo.Remove(r.id).ok());
}

TYPED_TEST(StorageRepositoryAllTypes, ListFilterMatchesNameOrIsIgnored) {
    using T = TypeParam;
    Repository<T> repo(this->db_);
    this->Saved();
    const auto miss = repo.List("no such thing");
    ASSERT_TRUE(miss.ok());
    if (Traits<T>::kHasName) {
        EXPECT_TRUE(miss.value().empty());
        EXPECT_EQ(repo.List("SAMPLE").value().size(), 1U);  // case-insensitive
        LengthLimit limit(this->db_, kSmallLimit);
        auto too_long = repo.List(kTooLong);
        ASSERT_FALSE(too_long.ok());
        EXPECT_EQ(too_long.error().code, ErrorCode::kTooBig);
    } else {
        // A table without names (the shot log) ignores the filter.
        EXPECT_EQ(miss.value().size(), 1U);
    }
}

TYPED_TEST(StorageRepositoryAllTypes, GetMissingIdIsEmpty) {
    using T = TypeParam;
    auto got = Repository<T>(this->db_).Get(4242);
    ASSERT_TRUE(got.ok());
    EXPECT_FALSE(got.value().has_value());
}

TYPED_TEST(StorageRepositoryAllTypes, UpdatingMissingRowFailsWithNotFound) {
    using T = TypeParam;
    Repository<T> repo(this->db_);
    T r = Traits<T>::Make(this->db_);
    r.id = 4242;
    auto id = repo.Save(r);
    ASSERT_FALSE(id.ok());
    EXPECT_EQ(id.error().code, ErrorCode::kNotFound);
    EXPECT_EQ(r.id, 4242);
    EXPECT_TRUE(repo.List().value().empty());
}

TYPED_TEST(StorageRepositoryAllTypes, ClosedDatabaseFailsEverything) {
    using T = TypeParam;
    T r = Traits<T>::Make(this->db_);
    Database closed;
    Repository<T> repo(closed);
    auto id = repo.Save(r);
    ASSERT_FALSE(id.ok());
    EXPECT_EQ(id.error().code, ErrorCode::kMisuse);
    EXPECT_EQ(r.id, 0);
    EXPECT_FALSE(repo.Get(1).ok());
    EXPECT_FALSE(repo.List().ok());
    EXPECT_FALSE(repo.Remove(1).ok());
}

TYPED_TEST(StorageRepositoryAllTypes, MissingTableFailsEverything) {
    using T = TypeParam;
    T r = Traits<T>::Make(this->db_);
    ExecUnchecked(this->db_, std::string("DROP TABLE ") + Traits<T>::kTable);
    Repository<T> repo(this->db_);
    auto id = repo.Save(r);
    ASSERT_FALSE(id.ok());
    EXPECT_EQ(id.error().code, ErrorCode::kError);
    EXPECT_EQ(r.id, 0);
    EXPECT_FALSE(repo.Get(1).ok());
    EXPECT_FALSE(repo.List().ok());
    EXPECT_FALSE(repo.Remove(1).ok());
    // The failed save did not leave its transaction open.
    EXPECT_FALSE(this->db_.connection().InTransaction());
}

TYPED_TEST(StorageRepositoryAllTypes, OversizedTextIsRefusedAndNothingIsStored) {
    using T = TypeParam;
    Repository<T> repo(this->db_);
    T r = Traits<T>::Make(this->db_);
    Traits<T>::Label(r) = kTooLong;
    {
        LengthLimit limit(this->db_, kSmallLimit);
        auto id = repo.Save(r);
        ASSERT_FALSE(id.ok());
        EXPECT_EQ(id.error().code, ErrorCode::kTooBig);
        EXPECT_EQ(r.id, 0);
    }
    EXPECT_TRUE(repo.List().value().empty());
}

TYPED_TEST(StorageRepositoryAllTypes, FailedInsertRollsBack) {
    using T = TypeParam;
    Repository<T> repo(this->db_);
    T r = Traits<T>::Make(this->db_);
    {
        StepFault fault(this->db_, std::string("INSERT INTO ") + Traits<T>::kTable);
        auto id = repo.Save(r);
        ASSERT_FALSE(id.ok());
        EXPECT_EQ(id.error().code, ErrorCode::kInterrupt);
        EXPECT_EQ(r.id, 0);
    }
    EXPECT_FALSE(this->db_.connection().InTransaction());
    EXPECT_TRUE(repo.List().value().empty());
}

TYPED_TEST(StorageRepositoryAllTypes, FailedReadIsReported) {
    using T = TypeParam;
    Repository<T> repo(this->db_);
    const T r = this->Saved();
    StepFault fault(this->db_, std::string("FROM ") + Traits<T>::kTable);
    auto got = repo.Get(r.id);
    ASSERT_FALSE(got.ok());
    EXPECT_EQ(got.error().code, ErrorCode::kInterrupt);
    auto all = repo.List();
    ASSERT_FALSE(all.ok());
    EXPECT_EQ(all.error().code, ErrorCode::kInterrupt);
}

TYPED_TEST(StorageRepositoryAllTypes, CommitBlockedByReaderKeepsRecordUnsaved) {
    using T = TypeParam;
    TempDbFile file;
    Database db;
    ASSERT_TRUE(db.Open(file.path()).ok());
    ASSERT_TRUE(db.connection().BusyTimeout(0).ok());
    Repository<T> repo(db);
    T r = Traits<T>::Make(db);

    // Another connection in the middle of a read transaction.
    sqlite_manager::Connection reader;
    ASSERT_TRUE(reader.Open(file.path()).ok());
    ASSERT_TRUE(reader.Execute("BEGIN; SELECT count(*) FROM sqlite_master;").ok());
    auto id = repo.Save(r);
    ASSERT_FALSE(id.ok());
    EXPECT_EQ(id.error().code, ErrorCode::kBusy);
    EXPECT_EQ(r.id, 0);
    ASSERT_TRUE(reader.Execute("COMMIT").ok());

    EXPECT_FALSE(db.connection().InTransaction());
    EXPECT_TRUE(repo.List().value().empty());
    ASSERT_TRUE(repo.Save(r).ok());  // once the reader is done
    EXPECT_EQ(repo.List().value().size(), 1U);
}

// --- Record types with child rows ------------------------------------------

template <typename T>
class StorageRepositoryChildren : public StorageRepositoryAllTypes<T> {};

using RecordTypesWithChildren = ::testing::Types<DragCurveRecord, BulletRecord, CartridgeRecord,
                                                 ProfileRecord, ConditionsRecord>;
TYPED_TEST_SUITE(StorageRepositoryChildren, RecordTypesWithChildren);

TYPED_TEST(StorageRepositoryChildren, ChildRowsAreReplacedAndListed) {
    using T = TypeParam;
    Repository<T> repo(this->db_);
    T r = Traits<T>::Make(this->db_);
    Traits<T>::AddChild(r);
    ASSERT_TRUE(repo.Save(r).ok());
    EXPECT_EQ(Traits<T>::Children(*repo.Get(r.id).value()), 1U);
    EXPECT_EQ(Traits<T>::Children(repo.List().value().at(0)), 1U);

    T emptied = Traits<T>::Make(this->db_);
    emptied.id = r.id;
    ASSERT_TRUE(repo.Save(emptied).ok());
    EXPECT_EQ(Traits<T>::Children(*repo.Get(r.id).value()), 0U);
}

TYPED_TEST(StorageRepositoryChildren, BadChildRowRollsBackTheParent) {
    using T = TypeParam;
    Repository<T> repo(this->db_);
    T r = Traits<T>::Make(this->db_);
    Traits<T>::AddChild(r);
    Traits<T>::AddBadChild(r);
    auto id = repo.Save(r);
    ASSERT_FALSE(id.ok());
    EXPECT_EQ(id.error().code, ErrorCode::kConstraint);
    EXPECT_EQ(r.id, 0);
    EXPECT_TRUE(repo.List().value().empty());
}

TYPED_TEST(StorageRepositoryChildren, MissingChildTableFailsSaveGetAndList) {
    using T = TypeParam;
    Repository<T> repo(this->db_);
    T r = Traits<T>::Make(this->db_);
    Traits<T>::AddChild(r);
    ASSERT_TRUE(repo.Save(r).ok());
    const Id id = r.id;
    ExecUnchecked(this->db_, std::string("DROP TABLE ") + Traits<T>::kChildTable);

    Traits<T>::Label(r) = "edited";
    EXPECT_FALSE(repo.Save(r).ok());
    EXPECT_EQ(r.id, id);
    EXPECT_FALSE(repo.Get(id).ok());
    EXPECT_FALSE(repo.List().ok());
}

TYPED_TEST(StorageRepositoryChildren, FailedChildReadIsReported) {
    using T = TypeParam;
    Repository<T> repo(this->db_);
    T r = Traits<T>::Make(this->db_);
    Traits<T>::AddChild(r);
    ASSERT_TRUE(repo.Save(r).ok());
    StepFault fault(this->db_, std::string("FROM ") + Traits<T>::kChildTable);
    auto got = repo.Get(r.id);
    ASSERT_FALSE(got.ok());
    EXPECT_EQ(got.error().code, ErrorCode::kInterrupt);
    EXPECT_FALSE(repo.List().ok());
}

// --- Column conversions and transactions ------------------------------------

class StorageRepositoryColumns : public ::testing::Test {
   protected:
    void SetUp() override { ASSERT_TRUE(db_.Open(":memory:").ok()); }
    Database db_;
};

TEST_F(StorageRepositoryColumns, OddlyTypedValuesReadAsDefaults) {
    Repository<DopeRecord> log(db_);
    DopeRecord d = Traits<DopeRecord>::Make(db_);
    d.notes = "kept";
    ASSERT_TRUE(log.Save(d).ok());
    // SQLite keeps values its column affinity cannot convert: a blob in a
    // text column, text in a REAL column, a fraction and text in INTEGER ones.
    ASSERT_TRUE(db_.connection()
                    .Execute("UPDATE dope_log SET notes = x'00ff', range_m = 'far', "
                             "use_for_truing = 'yes', look_angle_rad = 2 WHERE id = " +
                             std::to_string(d.id))
                    .ok());
    DopeRecord got = *log.Get(d.id).value();
    EXPECT_EQ(got.notes, "");
    EXPECT_DOUBLE_EQ(got.range_m, 0.0);
    EXPECT_FALSE(got.use_for_truing);
    EXPECT_DOUBLE_EQ(got.look_angle_rad, 2.0);

    ASSERT_TRUE(
        db_.connection()
            .Execute("UPDATE dope_log SET use_for_truing = 1.5 WHERE id = " + std::to_string(d.id))
            .ok());
    EXPECT_TRUE(log.Get(d.id).value()->use_for_truing);
    ASSERT_TRUE(
        db_.connection()
            .Execute("UPDATE dope_log SET use_for_truing = 0.5 WHERE id = " + std::to_string(d.id))
            .ok());
    EXPECT_FALSE(log.Get(d.id).value()->use_for_truing);  // truncated, as a C++ cast
}

TEST_F(StorageRepositoryColumns, ExplicitDatabaseDefaultsAndOptionalTextRoundTrip) {
    Repository<ConditionsRecord> conds(db_);
    ConditionsRecord c = Traits<ConditionsRecord>::Make(db_);
    c.created_at = "2020-01-02T03:04:05Z";
    c.powder_temp_k = 260.0;
    c.azimuth_rad = 1.5;
    ASSERT_TRUE(conds.Save(c).ok());
    const ConditionsRecord got = *conds.Get(c.id).value();
    EXPECT_EQ(got.created_at, "2020-01-02T03:04:05Z");
    EXPECT_DOUBLE_EQ(got.powder_temp_k.value_or(0.0), 260.0);
    EXPECT_DOUBLE_EQ(got.azimuth_rad.value_or(0.0), 1.5);

    Repository<ProfileRecord> profiles(db_);
    ProfileRecord p = Traits<ProfileRecord>::Make(db_);
    p.last_used_at = "2024-05-06T07:08:09Z";
    ASSERT_TRUE(profiles.Save(p).ok());
    ProfileRecord gotp = *profiles.Get(p.id).value();
    EXPECT_EQ(gotp.last_used_at.value_or(""), "2024-05-06T07:08:09Z");
    EXPECT_FALSE(gotp.created_at.empty());
    // Clearing the optional stores NULL; an empty created_at keeps the stored one.
    const std::string created = gotp.created_at;
    gotp.last_used_at.reset();
    gotp.created_at.clear();
    ASSERT_TRUE(profiles.Save(gotp).ok());
    gotp = *profiles.Get(p.id).value();
    EXPECT_FALSE(gotp.last_used_at.has_value());
    EXPECT_EQ(gotp.created_at, created);
}

TEST_F(StorageRepositoryColumns, SaveJoinsTheCallersTransaction) {
    Repository<RifleRecord> rifles(db_);
    {
        auto txn = sqlite_manager::Transaction::Begin(db_.connection());
        ASSERT_TRUE(txn.ok());
        RifleRecord a = Traits<RifleRecord>::Make(db_);
        RifleRecord b = Traits<RifleRecord>::Make(db_);
        ASSERT_TRUE(rifles.Save(a).ok());
        ASSERT_TRUE(rifles.Save(b).ok());
        EXPECT_TRUE(db_.connection().InTransaction());  // not committed by Save
        EXPECT_EQ(rifles.List().value().size(), 2U);
        ASSERT_TRUE(txn.value().Rollback().ok());
    }
    EXPECT_TRUE(rifles.List().value().empty());

    auto txn = sqlite_manager::Transaction::Begin(db_.connection());
    ASSERT_TRUE(txn.ok());
    RifleRecord c = Traits<RifleRecord>::Make(db_);
    ASSERT_TRUE(rifles.Save(c).ok());
    ASSERT_TRUE(txn.value().Commit().ok());
    EXPECT_EQ(rifles.List().value().size(), 1U);
}

TEST_F(StorageRepositoryColumns, ReferencedRowsCannotBeRemoved) {
    Repository<BulletRecord> bullets(db_);
    CartridgeRecord c = Traits<CartridgeRecord>::Make(db_);
    ASSERT_TRUE(Repository<CartridgeRecord>(db_).Save(c).ok());
    const Status s = bullets.Remove(c.bullet_id);
    ASSERT_FALSE(s.ok());
    EXPECT_EQ(s.error().code, ErrorCode::kConstraint);
    EXPECT_TRUE(bullets.Get(c.bullet_id).value().has_value());
}

// --- Settings and photos -----------------------------------------------------

TEST_F(StorageRepositoryColumns, SettingsOverwriteAndMiss) {
    EXPECT_FALSE(GetSetting(db_, "units").value().has_value());
    ASSERT_TRUE(SetSetting(db_, "units", "metric").ok());
    ASSERT_TRUE(SetSetting(db_, "units", "imperial").ok());
    EXPECT_EQ(GetSetting(db_, "units").value().value_or(""), "imperial");
}

TEST_F(StorageRepositoryColumns, SettingsFailures) {
    Database closed;
    EXPECT_EQ(GetSetting(closed, "k").error().code, ErrorCode::kMisuse);
    EXPECT_EQ(SetSetting(closed, "k", "v").error().code, ErrorCode::kMisuse);
    {
        LengthLimit limit(db_, kSmallLimit);
        EXPECT_EQ(GetSetting(db_, kTooLong).error().code, ErrorCode::kTooBig);
        EXPECT_EQ(SetSetting(db_, "k", kTooLong).error().code, ErrorCode::kTooBig);
    }
    ASSERT_TRUE(SetSetting(db_, "k", "v").ok());
    StepFault fault(db_, "FROM app_setting");
    EXPECT_EQ(GetSetting(db_, "k").error().code, ErrorCode::kInterrupt);
}

TEST_F(StorageRepositoryColumns, PhotosAreStoredReplacedAndRemoved) {
    RifleRecord r = Traits<RifleRecord>::Make(db_);
    ASSERT_TRUE(Repository<RifleRecord>(db_).Save(r).ok());
    const std::vector<std::uint8_t> jpeg = {0xff, 0xd8, 0xff, 0x00, 0x01};
    const std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G'};

    EXPECT_FALSE(GetPhoto(db_, "rifle", r.id).value().has_value());
    ASSERT_TRUE(SetPhoto(db_, "rifle", r.id, jpeg).ok());
    ASSERT_TRUE(SetPhoto(db_, "cartridge", 77, png).ok());
    EXPECT_EQ(GetPhoto(db_, "rifle", r.id).value().value_or(std::vector<std::uint8_t>{}), jpeg);
    ASSERT_TRUE(SetPhoto(db_, "rifle", r.id, png).ok());  // replaces
    EXPECT_EQ(GetPhoto(db_, "rifle", r.id).value().value_or(std::vector<std::uint8_t>{}), png);
    EXPECT_EQ(PhotoOwners(db_, "rifle").value(), std::vector<Id>{r.id});
    EXPECT_EQ(PhotoOwners(db_, "cartridge").value(), std::vector<Id>{77});

    // An empty image removes the picture; deleting the owner deletes it too.
    ASSERT_TRUE(SetPhoto(db_, "cartridge", 77, {}).ok());
    EXPECT_TRUE(PhotoOwners(db_, "cartridge").value().empty());
    ASSERT_TRUE(Repository<RifleRecord>(db_).Remove(r.id).ok());
    EXPECT_FALSE(GetPhoto(db_, "rifle", r.id).value().has_value());
    EXPECT_TRUE(PhotoOwners(db_, "rifle").value().empty());
}

TEST_F(StorageRepositoryColumns, PhotoFailures) {
    const std::vector<std::uint8_t> image = {1, 2, 3};
    Database closed;
    EXPECT_EQ(GetPhoto(closed, "rifle", 1).error().code, ErrorCode::kMisuse);
    EXPECT_EQ(SetPhoto(closed, "rifle", 1, image).error().code, ErrorCode::kMisuse);
    EXPECT_EQ(SetPhoto(closed, "rifle", 1, {}).error().code, ErrorCode::kMisuse);
    EXPECT_EQ(PhotoOwners(closed, "rifle").error().code, ErrorCode::kMisuse);

    // Only rifles and cartridges have pictures.
    EXPECT_EQ(SetPhoto(db_, "scope", 1, image).error().code, ErrorCode::kConstraint);
    {
        LengthLimit limit(db_, kSmallLimit);
        EXPECT_EQ(GetPhoto(db_, kTooLong, 1).error().code, ErrorCode::kTooBig);
        EXPECT_EQ(SetPhoto(db_, kTooLong, 1, image).error().code, ErrorCode::kTooBig);
        EXPECT_EQ(
            SetPhoto(db_, "rifle", 1, std::vector<std::uint8_t>(kTooLong.size(), 7)).error().code,
            ErrorCode::kTooBig);
        EXPECT_EQ(PhotoOwners(db_, kTooLong).error().code, ErrorCode::kTooBig);
    }
    EXPECT_TRUE(PhotoOwners(db_, "rifle").value().empty());

    ASSERT_TRUE(SetPhoto(db_, "rifle", 1, image).ok());
    {
        StepFault fault(db_, "FROM photo WHERE kind = ? AND");
        EXPECT_EQ(GetPhoto(db_, "rifle", 1).error().code, ErrorCode::kInterrupt);
    }
    StepFault fault(db_, "FROM photo WHERE kind = ? ORDER");
    auto owners = PhotoOwners(db_, "rifle");
    ASSERT_FALSE(owners.ok());
    EXPECT_EQ(owners.error().code, ErrorCode::kInterrupt) << owners.error().message;
}

// --- Database ----------------------------------------------------------------

TEST(StorageDatabaseErrors, OpenFailsInMissingDirectory) {
    Database db;
    const Status s = db.Open((fs::temp_directory_path() / "balcalc_no_such_dir" / "x.db").string());
    ASSERT_FALSE(s.ok());
    EXPECT_EQ(s.error().code, ErrorCode::kCantOpen);
    EXPECT_FALSE(db.IsOpen());
}

TEST(StorageDatabaseErrors, OpeningTwiceIsRefusedAndKeepsTheFirst) {
    Database db;
    ASSERT_TRUE(db.Open(":memory:").ok());
    ASSERT_TRUE(SetSetting(db, "k", "v").ok());
    EXPECT_FALSE(db.Open(":memory:").ok());
    EXPECT_TRUE(db.IsOpen());
    EXPECT_EQ(GetSetting(db, "k").value().value_or(""), "v");
}

TEST(StorageDatabaseErrors, NotADatabaseFileIsRefused) {
    TempDbFile file;
    {
        std::ofstream out(file.path(), std::ios::binary);
        out << std::string(4096, 'Z');
    }
    Database db;
    const Status s = db.Open(file.path());
    ASSERT_FALSE(s.ok());
    EXPECT_EQ(s.error().code, ErrorCode::kNotADb);
    EXPECT_FALSE(db.IsOpen());
}

TEST(StorageDatabaseErrors, SchemaVersionFailures) {
    Database closed;
    EXPECT_EQ(closed.SchemaVersion().error().code, ErrorCode::kMisuse);

    Database db;
    ASSERT_TRUE(db.Open(":memory:").ok());
    StepFault fault(db, "user_version");
    EXPECT_EQ(db.SchemaVersion().error().code, ErrorCode::kInterrupt);
}

TEST(StorageDatabaseErrors, FailedMigrationLeavesTheFileUntouched) {
    TempDbFile file;
    {
        Database db;
        ASSERT_TRUE(db.Open(file.path()).ok());
        // Claims an older version than its tables: replaying the last
        // migration fails (its table already exists).
        const std::string sql =
            "PRAGMA user_version = " + std::to_string(Database::LatestSchemaVersion() - 1);
        ASSERT_TRUE(db.connection().Execute(sql).ok());
    }
    Database db;
    const Status s = db.Open(file.path());
    ASSERT_FALSE(s.ok());
    EXPECT_EQ(s.error().code, ErrorCode::kError);
    EXPECT_FALSE(db.IsOpen());

    // Still at the old version.
    sqlite_manager::Connection raw;
    ASSERT_TRUE(raw.Open(file.path()).ok());
    auto st = sqlite_manager::Statement::Prepare(raw, "PRAGMA user_version");
    ASSERT_TRUE(st.ok());
    ASSERT_TRUE(st.value().Step().ok());
    EXPECT_EQ(st.value().ColumnInt64(0), Database::LatestSchemaVersion() - 1);
}

TEST(StorageDatabaseErrors, MigrationBlockedByReaderCanBeRetried) {
    TempDbFile file;
    {
        Database db;
        ASSERT_TRUE(db.Open(file.path()).ok());
        // Back to the previous version: undo the last migration (photos).
        ASSERT_TRUE(db.connection()
                        .Execute("DROP TRIGGER rifle_photo_gone; DROP TRIGGER cartridge_photo_gone;"
                                 "DROP TABLE photo; PRAGMA user_version = " +
                                 std::to_string(Database::LatestSchemaVersion() - 1))
                        .ok());
    }
    sqlite_manager::Connection reader;
    ASSERT_TRUE(reader.Open(file.path()).ok());
    ASSERT_TRUE(reader.Execute("BEGIN; SELECT count(*) FROM rifle;").ok());
    {
        Database db;
        const Status s = db.Open(file.path());  // waits out the busy timeout
        ASSERT_FALSE(s.ok());
        EXPECT_EQ(s.error().code, ErrorCode::kBusy);
        EXPECT_FALSE(db.IsOpen());
    }
    ASSERT_TRUE(reader.Execute("COMMIT").ok());

    Database db;
    ASSERT_TRUE(db.Open(file.path()).ok());
    EXPECT_EQ(db.SchemaVersion().value(), Database::LatestSchemaVersion());
    EXPECT_TRUE(PhotoOwners(db, "rifle").ok());
}

// --- Solution ----------------------------------------------------------------

class StorageSolutionEdges : public ::testing::Test {
   protected:
    void SetUp() override {
        ASSERT_TRUE(db_.Open(":memory:").ok());
        curve_.name = "Doppler";
        curve_.points = {{0.0, 0.30}, {0.8, 0.28}, {1.0, 0.40}, {1.5, 0.35}, {3.0, 0.25}};
        ASSERT_TRUE(Repository<DragCurveRecord>(db_).Save(curve_).ok());

        bullet_ = SampleBullet();
        bullet_.length_m = units::InchToM(1.24);
        bullet_.drag_kind = kDragKindCurve;
        bullet_.curve_id = curve_.id;
        ASSERT_TRUE(Repository<BulletRecord>(db_).Save(bullet_).ok());

        CartridgeRecord c;
        c.name = "Handload";
        c.bullet_id = bullet_.id;
        c.muzzle_velocity_mps = 790.0;
        ASSERT_TRUE(Repository<CartridgeRecord>(db_).Save(c).ok());
        cartridge_id_ = c.id;

        ScopeRecord s = Traits<ScopeRecord>::Make(db_);
        ASSERT_TRUE(Repository<ScopeRecord>(db_).Save(s).ok());
        scope_id_ = s.id;

        RifleRecord r;
        r.name = "M24";
        r.twist_m = units::InchToM(11.25);
        r.sight_height_m = 0.05;
        r.scope_id = s.id;
        ASSERT_TRUE(Repository<RifleRecord>(db_).Save(r).ok());
        rifle_id_ = r.id;

        ProfileRecord p;
        p.name = "M24 / handload";
        p.rifle_id = r.id;
        p.cartridge_id = c.id;
        ASSERT_TRUE(Repository<ProfileRecord>(db_).Save(p).ok());
        profile_id_ = p.id;
    }

    // LoadProfile's error after deleting a row the profile chain needs.
    std::string LoadErrorWithout(const std::string& table, Id id) {
        ExecUnchecked(db_, "DELETE FROM " + table + " WHERE id = " + std::to_string(id));
        auto p = LoadProfile(db_, profile_id_);
        EXPECT_FALSE(p.ok());
        return p.ok() ? std::string() : p.error().message;
    }

    Database db_;
    DragCurveRecord curve_;
    BulletRecord bullet_;
    Id cartridge_id_ = 0;
    Id scope_id_ = 0;
    Id rifle_id_ = 0;
    Id profile_id_ = 0;
};

TEST_F(StorageSolutionEdges, LoadsTheBulletsDragCurveAndSolves) {
    auto p = LoadProfile(db_, profile_id_);
    ASSERT_TRUE(p.ok()) << p.error().message;
    ASSERT_TRUE(p.value().curve.has_value());
    EXPECT_EQ(p.value().curve->points.size(), curve_.points.size());
    ASSERT_TRUE(p.value().scope.has_value());
    EXPECT_EQ(p.value().scope->id, scope_id_);

    auto sol = Solve(p.value(), ConditionsRecord{}, 510.0);
    ASSERT_TRUE(sol.ok()) << sol.error().message;
    EXPECT_LT(sol.value().trajectory.AtSlantRange(500.0)->drop_m, -0.5);  // below the line of sight
}

TEST_F(StorageSolutionEdges, MissingRifleIsReported) {
    EXPECT_EQ(LoadErrorWithout("rifle", rifle_id_),
              "rifle " + std::to_string(rifle_id_) + " not found");
}

TEST_F(StorageSolutionEdges, MissingScopeIsReported) {
    EXPECT_EQ(LoadErrorWithout("scope", scope_id_),
              "scope " + std::to_string(scope_id_) + " not found");
}

TEST_F(StorageSolutionEdges, MissingCartridgeIsReported) {
    EXPECT_EQ(LoadErrorWithout("cartridge", cartridge_id_),
              "cartridge " + std::to_string(cartridge_id_) + " not found");
}

TEST_F(StorageSolutionEdges, MissingBulletIsReported) {
    EXPECT_EQ(LoadErrorWithout("bullet", bullet_.id),
              "bullet " + std::to_string(bullet_.id) + " not found");
}

TEST_F(StorageSolutionEdges, MissingDragCurveIsReported) {
    EXPECT_EQ(LoadErrorWithout("drag_curve", curve_.id),
              "drag curve " + std::to_string(curve_.id) + " not found");
}

TEST_F(StorageSolutionEdges, ReadErrorsArePassedOn) {
    Database closed;
    auto p = LoadProfile(closed, profile_id_);
    ASSERT_FALSE(p.ok());
    EXPECT_EQ(p.error().code, ErrorCode::kMisuse);
}

TEST_F(StorageSolutionEdges, ReadErrorsOfEveryLinkArePassedOn) {
    for (const char* table : {"rifle", "scope", "cartridge", "bullet", "drag_curve"}) {
        StepFault fault(db_, std::string("FROM ") + table + " WHERE id");
        auto p = LoadProfile(db_, profile_id_);
        ASSERT_FALSE(p.ok()) << table;
        EXPECT_EQ(p.error().code, ErrorCode::kInterrupt) << table;
    }
    EXPECT_TRUE(LoadProfile(db_, profile_id_).ok());
}

TEST_F(StorageSolutionEdges, InconsistentBulletsAreReported) {
    BulletRecord b = SampleBullet();
    b.bc.reset();
    auto no_bc = MakeDragModel(b, nullptr);
    ASSERT_FALSE(no_bc.ok());
    EXPECT_NE(no_bc.error().message.find("has no BC"), std::string::npos);

    // Values the drag model rejects come back as errors, not exceptions.
    b.bc = -0.2;
    auto negative = MakeDragModel(b, nullptr);
    ASSERT_FALSE(negative.ok());
    EXPECT_NE(negative.error().message.find("Sample SMK 175"), std::string::npos);
    EXPECT_NE(negative.error().message.find("positive"), std::string::npos);

    b.drag_kind = kDragKindMultiBc;
    EXPECT_FALSE(MakeDragModel(b, nullptr).ok());  // no bands
    b.bc_bands = {{800.0, 0.25}, {500.0, 0.24}};
    EXPECT_TRUE(MakeDragModel(b, nullptr).ok());

    DragCurveRecord one_point;
    one_point.points = {{1.0, 0.3}};
    b.drag_kind = kDragKindCurve;
    EXPECT_FALSE(MakeDragModel(b, &one_point).ok());
    EXPECT_TRUE(MakeDragModel(b, &curve_).ok());
}

TEST_F(StorageSolutionEdges, SolveReportsBadDragAndUnreachableZero) {
    auto p = LoadProfile(db_, profile_id_);
    ASSERT_TRUE(p.ok());
    LoadedProfile no_curve = p.value();
    no_curve.curve.reset();
    EXPECT_FALSE(Solve(no_curve, ConditionsRecord{}, 300.0).ok());

    LoadedProfile far = p.value();
    far.rifle.zero_range_m = 1e6;  // the bullet never gets there
    auto sol = Solve(far, ConditionsRecord{}, 300.0);
    ASSERT_FALSE(sol.ok());
    EXPECT_NE(sol.error().message.find("could not zero"), std::string::npos);
}

TEST(StorageClicksEdges, NoClickValueGivesNoClicks) {
    EXPECT_DOUBLE_EQ(ToClicks(0.01, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(ToClicks(0.01, -0.0001), 0.0);
}

}  // namespace
}  // namespace ballistics::storage
