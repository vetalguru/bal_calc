find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 QuickDialogs2 LinguistTools)
qt_standard_project_setup(REQUIRES 6.8 I18N_TRANSLATED_LANGUAGES uk ru)

# The UI (QML + Backend) is a static QML module so that both the app and
# the UI tests (tests/ui) link the very same code.
# Generated QML modules in one place, so qmllint and tools find them.
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)
qt_add_library(balcalc_ui STATIC)
qt_add_qml_module(balcalc_ui
    URI BalCalc
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/NumberField.qml
        qml/SolutionPage.qml
        qml/ConditionsPage.qml
        qml/ProfilesPage.qml
        qml/ProfileEditor.qml
        qml/SettingsPage.qml
        qml/TablePage.qml
        qml/LibraryPage.qml
        qml/BulletEditor.qml
        qml/SwitchRow.qml
        qml/ReticleView.qml
        qml/TruingPage.qml
        qml/LogShotDialog.qml
    SOURCES
        backend.h
        backend.cpp
)
target_link_libraries(balcalc_ui PUBLIC
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    ballistics_core
    ballistics_storage
    ballistics_applogic
)

# The starter library (data/seed) is compiled in and imported into a new
# database on first start.
file(GLOB_RECURSE seed_files CONFIGURE_DEPENDS
    RELATIVE ${PROJECT_SOURCE_DIR}/data/seed
    ${PROJECT_SOURCE_DIR}/data/seed/ammo/*.ammo
    ${PROJECT_SOURCE_DIR}/data/seed/drg/*.drg
    ${PROJECT_SOURCE_DIR}/data/seed/reticle/*.reticle
    ${PROJECT_SOURCE_DIR}/data/seed/published_bullets.json)
list(TRANSFORM seed_files PREPEND ${PROJECT_SOURCE_DIR}/data/seed/)
qt_add_resources(balcalc_ui "seed"
    PREFIX "/seed"
    BASE ${PROJECT_SOURCE_DIR}/data/seed
    FILES ${seed_files})

qt_add_resources(balcalc_ui "icons"
    PREFIX "/icons"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/icons
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/icons/balcalc.png)

qt_add_executable(balcalc WIN32 MACOSX_BUNDLE
    main.cpp
)
if(WIN32)
    target_sources(balcalc PRIVATE balcalc.rc) # exe icon
endif()
target_link_libraries(balcalc PRIVATE balcalc_ui balcalc_uiplugin)

# English is the source language; uk/ru are compiled into :/i18n/.
# Refresh the .ts files after changing UI strings with:
#   cmake --build <build dir> --target update_translations
qt_add_translations(balcalc
    SOURCE_TARGETS balcalc_ui balcalc
    TS_FILE_DIR i18n
    TS_FILE_BASE balcalc
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -no-obsolete
)

set_target_properties(balcalc PROPERTIES
    QT_ANDROID_PACKAGE_NAME "org.vetalguru.balcalc"
    QT_ANDROID_APP_NAME "BalCalc"
    QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
    QT_ANDROID_VERSION_CODE 1
    QT_ANDROID_MIN_SDK_VERSION 28
    QT_ANDROID_TARGET_SDK_VERSION 35
    QT_ANDROID_COMPILE_SDK_VERSION 35
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
    QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
)

# Release APK/AAB signing from the environment (never from the source tree):
#   QT_ANDROID_KEYSTORE_PATH, QT_ANDROID_KEYSTORE_ALIAS,
#   QT_ANDROID_KEYSTORE_STORE_PASS, QT_ANDROID_KEYSTORE_KEY_PASS
if(ANDROID AND DEFINED ENV{QT_ANDROID_KEYSTORE_PATH})
    set(QT_ANDROID_SIGN_APK ON CACHE BOOL "" FORCE)
    set(QT_ANDROID_SIGN_AAB ON CACHE BOOL "" FORCE)
endif()


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

# Windows install: bin/balcalc.exe with the Qt libraries next to it and
# QML modules / plugins beside bin/ (Qt's deploy layout, bin/qt.conf
# points at them); packaged by cmake/Packaging.cmake.
if(WIN32)
    install(TARGETS balcalc RUNTIME DESTINATION bin)
    qt_generate_deploy_qml_app_script(
        TARGET balcalc
        OUTPUT_SCRIPT balcalc_deploy_script
        NO_TRANSLATIONS)
    install(SCRIPT ${balcalc_deploy_script})
endif()
