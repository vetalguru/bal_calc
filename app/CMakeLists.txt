find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(balcalc WIN32 MACOSX_BUNDLE
    main.cpp
)

qt_add_qml_module(balcalc
    URI BalCalc
    VERSION 1.0
    QML_FILES
        qml/Main.qml
    SOURCES
        app_info.h
        app_info.cpp
)

set_target_properties(balcalc PROPERTIES
    QT_ANDROID_PACKAGE_NAME "org.vetalguru.balcalc"
    QT_ANDROID_APP_NAME "BalCalc"
    QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
    QT_ANDROID_VERSION_CODE 1
    QT_ANDROID_MIN_SDK_VERSION 28
    QT_ANDROID_TARGET_SDK_VERSION 35
    QT_ANDROID_COMPILE_SDK_VERSION 35
)

target_link_libraries(balcalc PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    ballistics_core
    ballistics_storage
)

# Windows: copy the Qt runtime (DLLs, QML modules, plugins) next to the
# executable after each build, so build/.../balcalc.exe runs on its own.
if(WIN32)
    get_target_property(qt_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(qt_bin_dir "${qt_qmake}" DIRECTORY)
    find_program(WINDEPLOYQT windeployqt HINTS "${qt_bin_dir}" NO_DEFAULT_PATH)
    if(WINDEPLOYQT)
        add_custom_command(TARGET balcalc POST_BUILD
            COMMAND "${WINDEPLOYQT}" --no-translations --no-compiler-runtime
                    --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                    "$<TARGET_FILE:balcalc>"
            COMMENT "Deploying Qt runtime next to balcalc.exe"
            VERBATIM)
    endif()
endif()
