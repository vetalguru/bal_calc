# Installation and desktop packages.
#
#   Ubuntu:  cmake --build <dir> && (cd <dir> && cpack -G DEB)
#            -> balcalc_<version>_amd64.deb, using the system Qt 6
#   Windows: cmake --build <dir> --config Release
#            cpack --config <dir>/CPackConfig.cmake -C Release
#            -> BalCalc-<version>-win64.zip (portable) and, when NSIS is
#               installed, BalCalc-<version>-win64.exe (installer); both
#               carry Qt and the MSVC runtime.

include(GNUInstallDirs)

set(BALCALC_APP_ID "org.vetalguru.balcalc")

if(TARGET balcalc)
    if(WIN32)
        # Installed by app/CMakeLists.txt: Qt's deploy script must be made
        # where Qt was found.
    else()
        install(TARGETS balcalc RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        install(FILES ${PROJECT_SOURCE_DIR}/app/linux/${BALCALC_APP_ID}.desktop
                DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
        install(FILES ${PROJECT_SOURCE_DIR}/app/icons/balcalc.png
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
                RENAME ${BALCALC_APP_ID}.png)
        install(FILES ${PROJECT_SOURCE_DIR}/app/linux/${BALCALC_APP_ID}.metainfo.xml
                DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    endif()
endif()

if(TARGET bal-cli)
    if(WIN32)
        install(TARGETS bal-cli RUNTIME DESTINATION bin)
    else()
        install(TARGETS bal-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()

# The Visual C++ runtime (vcruntime140.dll, msvcp140.dll, ...) beside the
# exes, so the app runs on a PC without the VC++ redistributable.
if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION bin)
    include(InstallRequiredSystemLibraries)
endif()

set(balcalc_doc_dir ${CMAKE_INSTALL_DATADIR}/doc/balcalc) # Debian: /usr/share/doc/<package>
if(WIN32)
    set(balcalc_doc_dir doc)
endif()
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE
              ${PROJECT_SOURCE_DIR}/data/seed/README.md
              ${PROJECT_SOURCE_DIR}/data/seed/LICENSE-BallisticCalculator.txt
        DESTINATION ${balcalc_doc_dir})
if(NOT WIN32)
    install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION ${balcalc_doc_dir} RENAME copyright)
endif()

# --- CPack ---------------------------------------------------------------
set(CPACK_PACKAGE_NAME "balcalc")
set(CPACK_PACKAGE_VENDOR "vetalguru")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Ballistic calculator for small arms out to 2500 m")
set(CPACK_PACKAGE_DESCRIPTION
    "Point-mass trajectory solver with Coriolis, spin drift, aerodynamic jump, "
    "multi-BC and Doppler-radar drag curves, reticle holds, range tables and "
    "truing from logged shots. Data are stored in a local SQLite database.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/vetalguru/bal_calc")
set(CPACK_PACKAGE_CONTACT "VS <vetalguru@gmail.com>")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "BalCalc")
set(CPACK_PACKAGE_EXECUTABLES "balcalc" "BalCalc")
set(CPACK_STRIP_FILES ON)

if(WIN32)
    set(CPACK_PACKAGE_FILE_NAME "BalCalc-${PROJECT_VERSION}-win64")
    find_program(BALCALC_MAKENSIS makensis
        PATHS "$ENV{ProgramFiles}/NSIS" "$ENV{ProgramFiles\(x86\)}/NSIS")
    if(BALCALC_MAKENSIS)
        set(CPACK_GENERATOR "ZIP;NSIS")
    else()
        set(CPACK_GENERATOR "ZIP")
    endif()
    set(CPACK_NSIS_DISPLAY_NAME "BalCalc")
    set(CPACK_NSIS_PACKAGE_NAME "BalCalc")
    set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/app/icons/balcalc.ico")
    set(CPACK_NSIS_MUI_UNIICON "${PROJECT_SOURCE_DIR}/app/icons/balcalc.ico")
    # Start-menu shortcut from CPACK_PACKAGE_EXECUTABLES (exes are in bin\).
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\balcalc.exe")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_URL_INFO_ABOUT "${CPACK_PACKAGE_HOMEPAGE_URL}")
else()
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SECTION "science")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    # Shared libraries (Qt, libstdc++) are found by dpkg-shlibdeps; QML
    # modules are loaded at run time, so they are listed by hand.
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, \
qml6-module-qtquick-templates, qml6-module-qtquick-window, qml6-module-qtquick-dialogs, \
qml6-module-qtqml-workerscript, qml6-module-qtquick-shapes, qt6-qpa-plugins")
endif()

include(CPack)
