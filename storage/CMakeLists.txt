add_library(ballistics_storage STATIC
    src/database.cpp
    src/solution.cpp
    src/repository.cpp
    src/schema.cpp
)
target_include_directories(ballistics_storage PUBLIC include)

target_link_libraries(ballistics_storage
    PUBLIC ballistics_core sqlite_manager
    PRIVATE ballistics_warnings)

ballistics_enable_tidy(ballistics_storage)
