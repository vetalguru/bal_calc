#ifndef BALLISTICS_STORAGE_DATABASE_H
#define BALLISTICS_STORAGE_DATABASE_H

#include <cstdint>
#include <string>

#include <sqlite_manager/connection.h>
#include <sqlite_manager/result.h>

namespace ballistics::storage {

using sqlite_manager::Result;
using sqlite_manager::Status;

// The application database: one SQLite file holding rifles, scopes,
// cartridges, profiles and logs. Owns the connection; repositories
// borrow it.
class Database final {
public:
    // Opens (creating if missing) the database at `path`, enables foreign
    // keys and a busy timeout. ":memory:" gives a throwaway database.
    Status Open(const std::string& path);

    bool IsOpen() const { return conn_.IsOpen(); }

    // Schema version stored in PRAGMA user_version (0 for a fresh file).
    Result<std::int64_t> SchemaVersion();

    sqlite_manager::Connection& connection() { return conn_; }

private:
    sqlite_manager::Connection conn_;
};

// Version of the bundled SQLite library, e.g. "3.46.1".
const char* SqliteVersion();

} // namespace ballistics::storage

#endif // BALLISTICS_STORAGE_DATABASE_H
