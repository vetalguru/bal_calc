# JSON for profile import/export (header-only, MIT).
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# XML for .ammo/.reticle files (tinyxml2, zlib licence). Only its single
# source file is built; its own CMake project is not used.
FetchContent_Declare(
    tinyxml2
    URL https://github.com/leethomason/tinyxml2/archive/refs/tags/10.0.0.tar.gz
    URL_HASH SHA256=3bdf15128ba16686e69bce256cc468e76c7b94ff2c7f391cc5ec09e40bff3839
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR no-cmake
)
FetchContent_MakeAvailable(tinyxml2)
add_library(ballistics_tinyxml2 STATIC ${tinyxml2_SOURCE_DIR}/tinyxml2.cpp)
target_include_directories(ballistics_tinyxml2 SYSTEM PUBLIC ${tinyxml2_SOURCE_DIR})

# Toolkit-free logic behind the app screens (forms, session, summaries),
# so it builds and is unit-tested everywhere; the Qt app adapts it to QML.
add_library(ballistics_applogic STATIC
    src/importers.cpp
    src/library.cpp
    src/profile_form.cpp
    src/profile_io.cpp
    src/session.cpp
)
target_include_directories(ballistics_applogic PUBLIC include)

target_link_libraries(ballistics_applogic
    PUBLIC ballistics_storage
    PRIVATE ballistics_warnings nlohmann_json::nlohmann_json ballistics_tinyxml2)

ballistics_enable_tidy(ballistics_applogic)
