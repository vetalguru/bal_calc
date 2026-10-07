cmake_minimum_required(VERSION 3.22)
project(ballistics VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Desktop builds get everything; cross builds (Android) get only what ships
# in the app, since the CLI and tests cannot run on the build host there.
if(ANDROID)
    set(ballistics_host_tools OFF)
else()
    set(ballistics_host_tools ON)
endif()
option(BALLISTICS_BUILD_CLI "Build the bal-cli tool" ${ballistics_host_tools})
option(BALLISTICS_BUILD_TESTS "Build the unit tests" ${ballistics_host_tools})
option(BALLISTICS_BUILD_JNI "Build libbalcalc_jni for the Kotlin app" OFF)
if(BALLISTICS_BUILD_JNI)
    # Static libraries end up in a shared one.
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

option(ENABLE_CLANG_TIDY "Run clang-tidy during build" OFF)
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE clang-tidy REQUIRED)
endif()

# Attach clang-tidy to a project target (no-op if disabled)
function(ballistics_enable_tidy target)
    if(ENABLE_CLANG_TIDY)
        set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    endif()
endfunction()

# Warning flags for project's own targets (not third-party deps)
add_library(ballistics_warnings INTERFACE)
target_compile_options(ballistics_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Wconversion -Wshadow>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /utf-8>
)

# SQLite + the sqlite_manager C++ wrapper (library only when embedded)
add_subdirectory(third_party/sqlite_manager EXCLUDE_FROM_ALL)

add_subdirectory(core)
add_subdirectory(storage)
add_subdirectory(applogic)
add_subdirectory(bridge)

if(BALLISTICS_BUILD_JNI)
    add_subdirectory(jni)
endif()

if(BALLISTICS_BUILD_CLI)
    add_subdirectory(cli)
endif()

if(BALLISTICS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
