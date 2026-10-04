add_executable(bal-cli main.cpp)
target_link_libraries(bal-cli PRIVATE ballistics_core ballistics_storage ballistics_warnings)

ballistics_enable_tidy(bal-cli)
