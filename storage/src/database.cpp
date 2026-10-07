#include <ballistics/storage/database.h>
#include <sqlite3.h>
#include <sqlite_manager/statement.h>
#include <sqlite_manager/transaction.h>

#include <string>

#include "schema.h"

namespace ballistics::storage {

namespace {
constexpr int kBusyTimeoutMs = 2000;
}  // namespace

Status Database::Open(const std::string& path) {
    if (Status s = conn_.Open(path); !s) {
        return s;
    }
    if (Status s = conn_.Execute("PRAGMA foreign_keys = ON"); !s) {
        return s;
    }
    if (Status s = conn_.BusyTimeout(kBusyTimeoutMs); !s) {
        return s;
    }
    if (Status s = Migrate(); !s) {
        conn_.Close().ok();
        return s;
    }
    return sqlite_manager::Ok();
}

std::int64_t Database::LatestSchemaVersion() { return detail::Migrations().back().version; }

Status Database::Migrate() {
    auto current = SchemaVersion();
    if (!current) {
        return current.error();
    }
    if (current.value() > LatestSchemaVersion()) {
        return sqlite_manager::Error(sqlite_manager::ErrorCode::kSchema, 0,
                                     "database schema v" + std::to_string(current.value()) +
                                         " is newer than this app supports (v" +
                                         std::to_string(LatestSchemaVersion()) + ")");
    }
    for (const detail::Migration& m : detail::Migrations()) {
        if (m.version <= current.value()) {
            continue;
        }
        auto txn = sqlite_manager::Transaction::Begin(conn_);
        if (!txn) {
            return txn.error();
        }
        if (Status s = conn_.Execute(m.sql); !s) {
            return s;
        }
        if (Status s = conn_.Execute("PRAGMA user_version = " + std::to_string(m.version)); !s) {
            return s;
        }
        if (Status s = txn.value().Commit(); !s) {
            return s;
        }
    }
    return sqlite_manager::Ok();
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

const char* SqliteVersion() { return sqlite3_libversion(); }

}  // namespace ballistics::storage
