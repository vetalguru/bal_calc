add_library(ballistics_core STATIC
    src/version.cpp
)
target_include_directories(ballistics_core PUBLIC include)
target_compile_definitions(ballistics_core PRIVATE
    BALLISTICS_VERSION="${PROJECT_VERSION}")

target_link_libraries(ballistics_core PRIVATE ballistics_warnings)

ballistics_enable_tidy(ballistics_core)
