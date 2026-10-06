# libbalcalc_jni: the C++ core as one shared library for the Kotlin app
# (Android: built by Gradle's externalNativeBuild; desktop: by the Gradle
# task buildNative). JNI headers come from the NDK or the host JDK.
if(NOT ANDROID)
    find_package(JNI REQUIRED COMPONENTS JVM)
endif()

add_library(balcalc_jni SHARED src/balcalc_jni.cpp)
target_link_libraries(balcalc_jni PRIVATE ballistics_bridge ballistics_warnings)
if(NOT ANDROID)
    target_include_directories(balcalc_jni PRIVATE ${JNI_INCLUDE_DIRS})
endif()
if(ANDROID)
    # 16 KB pages (Android 15+ devices).
    target_link_options(balcalc_jni PRIVATE "-Wl,-z,max-page-size=16384")
endif()
ballistics_enable_tidy(balcalc_jni)
