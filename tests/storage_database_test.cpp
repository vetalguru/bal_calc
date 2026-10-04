#include <ballistics/storage/database.h>

#include <gtest/gtest.h>

#include <string>

namespace ballistics::storage {
namespace {

TEST(StorageDatabase, OpensInMemoryWithFreshSchema) {
    Database db;
    ASSERT_TRUE(db.Open(":memory:").ok());
    EXPECT_TRUE(db.IsOpen());

    auto version = db.SchemaVersion();
    ASSERT_TRUE(version.ok());
    EXPECT_EQ(version.value(), 0);
}

TEST(StorageDatabase, ReportsBundledSqliteVersion) {
    EXPECT_EQ(std::string(SqliteVersion()).rfind("3.", 0), 0U);
}

} // namespace
} // namespace ballistics::storage
