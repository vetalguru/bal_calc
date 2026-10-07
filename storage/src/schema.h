#ifndef BALLISTICS_STORAGE_SCHEMA_H
#define BALLISTICS_STORAGE_SCHEMA_H

#include <cstdint>
#include <vector>

namespace ballistics::storage::detail {

struct Migration {
    std::int64_t version;  // PRAGMA user_version after applying `sql`
    const char* sql;
};

const std::vector<Migration>& Migrations();

}  // namespace ballistics::storage::detail

#endif  // BALLISTICS_STORAGE_SCHEMA_H
