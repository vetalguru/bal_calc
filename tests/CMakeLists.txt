include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # MSVC: shared CRT like the rest
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)  # keep GoogleTest out of packages
FetchContent_MakeAvailable(googletest)

add_executable(core_tests
    applogic_import_test.cpp
    applogic_library_test.cpp
    applogic_reticle_test.cpp
    applogic_truing_test.cpp
    applogic_test.cpp
    bridge_test.cpp
    physics_analysis_test.cpp
    physics_basics_test.cpp
    physics_bc_test.cpp
    physics_drag_models_test.cpp
    physics_effects_test.cpp
    physics_gekht_test.cpp
    physics_reference_test.cpp
    physics_wez_test.cpp
    smoke_test.cpp
    storage_database_test.cpp
    storage_migration_test.cpp
    storage_solution_test.cpp
)
target_link_libraries(core_tests PRIVATE
    ballistics_core
    ballistics_storage
    ballistics_applogic
    ballistics_bridge
    nlohmann_json::nlohmann_json
    GTest::gtest_main
    GTest::gmock
    ballistics_warnings
)
ballistics_enable_tidy(core_tests)

target_include_directories(core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(core_tests PRIVATE
    BALLISTICS_SEED_DIR="${PROJECT_SOURCE_DIR}/data/seed"
    BALLISTICS_TEST_REFERENCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/reference")

if(TARGET bal_cli_lib)
    target_sources(core_tests PRIVATE cli_test.cpp)
    target_link_libraries(core_tests PRIVATE bal_cli_lib)
endif()

include(GoogleTest)
# PRE_TEST: list tests when ctest runs, not at build time, so a build
# never needs to execute the test binary (cross builds, locked-down hosts).
gtest_discover_tests(core_tests DISCOVERY_MODE PRE_TEST)
