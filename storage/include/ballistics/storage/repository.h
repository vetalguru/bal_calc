#ifndef BALLISTICS_STORAGE_REPOSITORY_H
#define BALLISTICS_STORAGE_REPOSITORY_H

#include <optional>
#include <string>
#include <vector>

#include <ballistics/storage/database.h>
#include <ballistics/storage/records.h>

namespace ballistics::storage {

// CRUD for one record type. Instantiated for every record in records.h.
//
//   Repository<RifleRecord> rifles(db);
//   RifleRecord r{.name = "AI AX308", ...};
//   if (auto id = rifles.Save(r); !id) { /* id.error() */ }   // r.id is set
//
// Saving a record also replaces its child rows, in one transaction (the
// caller's, if one is open).
template <typename T>
class Repository final {
public:
    explicit Repository(Database& db) : db_(db) {}

    // Inserts (id == 0) or updates the record; returns and sets its id.
    Result<Id> Save(T& record);

    // The record, or nullopt if there is no row with this id.
    Result<std::optional<T>> Get(Id id);

    // All records ordered by name (or id), optionally those whose name
    // contains `name_filter` (case-insensitive for ASCII).
    Result<std::vector<T>> List(const std::string& name_filter = "");

    // Deletes the record (children cascade). Fails with kConstraint while
    // other records still reference it.
    Status Remove(Id id);

private:
    Database& db_;
};

// Key-value application settings.
Result<std::optional<std::string>> GetSetting(Database& db, const std::string& key);
Status SetSetting(Database& db, const std::string& key, const std::string& value);

} // namespace ballistics::storage

#endif // BALLISTICS_STORAGE_REPOSITORY_H
