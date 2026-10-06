add_library(ballistics_core STATIC
    src/analysis.cpp
    src/atmosphere.cpp
    src/bc.cpp
    src/drag.cpp
    src/effects.cpp
    src/solver.cpp
    src/standard_drag_tables.cpp
    src/version.cpp
    src/wez.cpp
)
target_include_directories(ballistics_core PUBLIC include)
target_compile_definitions(ballistics_core PRIVATE
    BALLISTICS_VERSION="${PROJECT_VERSION}")

target_link_libraries(ballistics_core PRIVATE ballistics_warnings)

ballistics_enable_tidy(ballistics_core)
