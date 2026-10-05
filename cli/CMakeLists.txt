# Everything but main() lives in a library so the tests can drive it.
add_library(bal_cli_lib STATIC cli_app.cpp)
target_include_directories(bal_cli_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bal_cli_lib
    PUBLIC ballistics_core ballistics_storage ballistics_applogic
    PRIVATE ballistics_warnings)
ballistics_enable_tidy(bal_cli_lib)

add_executable(bal-cli main.cpp)
target_link_libraries(bal-cli PRIVATE bal_cli_lib ballistics_warnings)
ballistics_enable_tidy(bal-cli)
