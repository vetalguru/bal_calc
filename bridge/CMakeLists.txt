# JSON facade over applogic for UIs that are not C++ (the Kotlin app via
# JNI); the session logic the Qt Backend used to hold, toolkit-free.
add_library(ballistics_bridge STATIC src/api.cpp)
target_include_directories(ballistics_bridge PUBLIC include)
target_link_libraries(ballistics_bridge
    PUBLIC ballistics_applogic
    PRIVATE ballistics_warnings nlohmann_json::nlohmann_json)
ballistics_enable_tidy(ballistics_bridge)
