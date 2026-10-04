#include <ballistics/storage/database.h>
#include <ballistics/version.h>

#include <cstdio>

int main() {
    std::printf("ballistics %s (SQLite %s)\n", ballistics::version(),
                ballistics::storage::SqliteVersion());
    return 0;
}
