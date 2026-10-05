#include <ballistics/storage/database.h>

#include <sqlite3.h>
#include <sqlite_manager/statement.h>

namespace ballistics::storage {

namespace {
constexpr int kBusyTimeoutMs = 2000;
} // namespace

Status Database::Open(const std::string& path) {
    if (Status s = conn_.Open(path); !s) {
        return s;
    }
    if (Status s = conn_.Execute("PRAGMA foreign_keys = ON"); !s) {
        return s;
    }
    return conn_.BusyTimeout(kBusyTimeoutMs);
}

Result<std::int64_t> Database::SchemaVersion() {
    auto stmt = sqlite_manager::Statement::Prepare(conn_, "PRAGMA user_version");
    if (!stmt) {
        return stmt.error();
    }
    auto step = stmt.value().Step();
    if (!step) {
        return step.error();
    }
    return stmt.value().ColumnInt64(0);
}

const char* SqliteVersion() {
    return sqlite3_libversion();
}

} // namespace ballistics::storage
