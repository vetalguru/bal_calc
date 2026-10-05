find_package(Qt6 REQUIRED COMPONENTS QuickTest)

qt_add_executable(ui_tests tst_ui.cpp)
set_target_properties(ui_tests PROPERTIES AUTOMOC ON)
target_link_libraries(ui_tests PRIVATE balcalc_ui balcalc_uiplugin Qt6::QuickTest)
target_compile_definitions(ui_tests PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# Headless: offscreen platform, software rendering.
add_test(NAME ui_tests COMMAND ui_tests)
set_tests_properties(ui_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
