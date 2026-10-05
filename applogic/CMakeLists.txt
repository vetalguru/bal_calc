# Toolkit-free logic behind the app screens (forms, session, summaries),
# so it builds and is unit-tested everywhere; the Qt app adapts it to QML.
add_library(ballistics_applogic STATIC
    src/profile_form.cpp
    src/session.cpp
)
target_include_directories(ballistics_applogic PUBLIC include)

target_link_libraries(ballistics_applogic
    PUBLIC ballistics_storage
    PRIVATE ballistics_warnings)

ballistics_enable_tidy(ballistics_applogic)
