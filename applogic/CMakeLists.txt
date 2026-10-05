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

# Toolkit-free logic behind the app screens (forms, session, summaries),
# so it builds and is unit-tested everywhere; the Qt app adapts it to QML.
add_library(ballistics_applogic STATIC
    src/library.cpp
    src/profile_form.cpp
    src/profile_io.cpp
    src/session.cpp
)
target_include_directories(ballistics_applogic PUBLIC include)

target_link_libraries(ballistics_applogic
    PUBLIC ballistics_storage
    PRIVATE ballistics_warnings nlohmann_json::nlohmann_json)

ballistics_enable_tidy(ballistics_applogic)
