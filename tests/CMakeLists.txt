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
    applogic_armory_test.cpp
    applogic_import_test.cpp
    applogic_library_test.cpp
    applogic_profile_io_test.cpp
    applogic_reticle_test.cpp
    applogic_session_test.cpp
    applogic_test.cpp
    applogic_truing_test.cpp
    bridge_api_test.cpp
    bridge_test.cpp
    physics_analysis_test.cpp
    physics_basics_test.cpp
    physics_bc_test.cpp
    physics_drag_models_test.cpp
    physics_edge_test.cpp
    physics_effects_test.cpp
    physics_gekht_test.cpp
    physics_reference_test.cpp
    physics_wez_test.cpp
    smoke_test.cpp
    storage_database_test.cpp
    storage_migration_test.cpp
    storage_repository_test.cpp
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
    target_sources(core_tests PRIVATE cli_test.cpp cli_edge_test.cpp)
    target_link_libraries(core_tests PRIVATE bal_cli_lib)
endif()

include(GoogleTest)
# PRE_TEST: list tests when ctest runs, not at build time, so a build
# never needs to execute the test binary (cross builds, locked-down hosts).
gtest_discover_tests(core_tests DISCOVERY_MODE PRE_TEST)

# The bal-cli executable end to end, as a shell runs it: exit codes, stdout
# and stderr (cli_run.cmake). The unit tests above drive the same code
# through balcli::Run; these cover main() and the process around it.
if(TARGET bal-cli)
    function(bal_cli_test name)
        cmake_parse_arguments(T "NO_OUT;NO_ERR" "CODE;OUT;ERR" "ARGS" ${ARGN})
        # "|" between the arguments: add_test would split a ";" list.
        string(REPLACE ";" "|" args "${T_ARGS}")
        set(defs -DCLI=$<TARGET_FILE:bal-cli> "-DARGS=${args}" -DCODE=${T_CODE})
        if(DEFINED T_OUT)
            list(APPEND defs "-DOUT=${T_OUT}")
        endif()
        if(DEFINED T_ERR)
            list(APPEND defs "-DERR=${T_ERR}")
        endif()
        if(T_NO_OUT)
            list(APPEND defs -DNO_OUT=1)
        endif()
        if(T_NO_ERR)
            list(APPEND defs -DNO_ERR=1)
        endif()
        add_test(NAME cli.${name}
                 COMMAND ${CMAKE_COMMAND} ${defs} -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_run.cmake)
    endfunction()

    bal_cli_test(help ARGS --help CODE 0 OUT "Usage: bal-cli" NO_ERR)
    bal_cli_test(no_command CODE 2 OUT "Usage: bal-cli" NO_ERR)
    bal_cli_test(unknown_command ARGS fly CODE 2 ERR "^unknown command: fly\nbal-cli" NO_OUT)
    bal_cli_test(missing_value ARGS quick --bc CODE 2 ERR "^missing value for --bc\n")
    bal_cli_test(version ARGS version CODE 0 OUT "^ballistics [0-9]+\\.[0-9]+.* \\(SQLite 3\\.")
    bal_cli_test(quick_text
        ARGS quick --bc 0.243 --v0 800 --to 300 --wind 4@3h
        CODE 0 OUT "Range m +Elev MRAD +Wind MRAD" NO_ERR)
    bal_cli_test(quick_csv
        ARGS quick --bc 0.243 --v0 800 --to 200 --csv --units moa
        CODE 0 OUT "^V0 800\\.0 m/s, [^\n]*zero elevation [0-9.]+ MOA[^\n]*\n\nRange m,Elev MOA,Wind MOA,Drop cm,[^\n]*\n0,-,-,[^\n]*\n100,[^\n]*\n200," NO_ERR)
    bal_cli_test(quick_bad_number ARGS quick --bc fast --v0 800
        CODE 2 ERR "^--bc: not a number: fast\n" NO_OUT)
    bal_cli_test(quick_bad_wind ARGS quick --bc 0.3 --v0 800 --wind 4
        CODE 2 ERR "^--wind: expected SPEED@DIR\\[:UNTIL\\], got 4\n" NO_OUT)
    bal_cli_test(needs_db ARGS rifles CODE 2 ERR "^rifles needs --db FILE\n" NO_OUT)

    # One database through the commands that use it, in order.
    set(db ${CMAKE_CURRENT_BINARY_DIR}/cli_e2e.db)
    add_test(NAME cli.db.clean COMMAND ${CMAKE_COMMAND} -E rm -f ${db})
    bal_cli_test(db.demo ARGS --db ${db} demo
        CODE 0 OUT "^demo rifle 1, cartridge 1 created \\(profile 1\\)\n$" NO_ERR)
    bal_cli_test(db.rifles ARGS --db ${db} rifles CODE 0 OUT "M24 \\(demo\\)" NO_ERR)
    bal_cli_test(db.cartridges ARGS --db ${db} cartridges CODE 0 OUT "M118LR \\(demo\\)" NO_ERR)
    bal_cli_test(db.profiles ARGS --db ${db} profiles
        CODE 0 OUT "^1  M24 \\(demo\\) / M118LR \\(demo\\)" NO_ERR)
    bal_cli_test(db.table ARGS --db ${db} table --profile 1 --to 500 --csv
        CODE 0 OUT "Elev clk,Wind clk" NO_ERR)
    bal_cli_test(db.table_missing ARGS --db ${db} table --rifle 9 --cartridge 9 CODE 1 NO_OUT)
    bal_cli_test(db.import ARGS --db ${db} import ${PROJECT_SOURCE_DIR}/data/seed/reticle/mildot.reticle
        CODE 0 OUT "mildot.reticle: imported \\(id [0-9]+\\)" NO_ERR)
    bal_cli_test(db.import_missing ARGS --db ${db} import ${db}.none
        CODE 1 ERR "cli_e2e.db.none: cannot read")
    bal_cli_test(db.seed ARGS --db ${db} seed ${PROJECT_SOURCE_DIR}/data/seed
        CODE 0 OUT "^[0-9]+ imported, [0-9]+ skipped\n")
    add_test(NAME cli.db.cleanup COMMAND ${CMAKE_COMMAND} -E rm -f ${db})

    set_tests_properties(cli.db.clean PROPERTIES FIXTURES_SETUP cli_db_clean)
    set_tests_properties(cli.db.demo PROPERTIES FIXTURES_REQUIRED cli_db_clean
                                                FIXTURES_SETUP cli_db)
    set_tests_properties(cli.db.rifles cli.db.cartridges cli.db.profiles cli.db.table
                         cli.db.table_missing PROPERTIES FIXTURES_REQUIRED cli_db)
    # The imports after the reads: the lists above count the demo only.
    set_tests_properties(cli.db.import cli.db.import_missing cli.db.seed PROPERTIES
        FIXTURES_REQUIRED cli_db
        DEPENDS "cli.db.rifles;cli.db.cartridges;cli.db.profiles;cli.db.table;cli.db.table_missing")
    set_tests_properties(cli.db.seed PROPERTIES DEPENDS "cli.db.import;cli.db.import_missing")
    set_tests_properties(cli.db.cleanup PROPERTIES FIXTURES_CLEANUP cli_db)
endif()
