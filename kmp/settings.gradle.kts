rootProject.name = "balcalc"

pluginManagement {
    repositories {
        google()
        gradlePluginPortal()
        mavenCentral()
    }
}

plugins {
    // Downloads the JDK of jvmToolchain() where it is missing (WSL, CI).
    id("org.gradle.toolchains.foojay-resolver-convention") version "1.0.0"
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

// shared: Kotlin Multiplatform UI and app logic client (Android + desktop JVM)
// androidApp: the Android application; desktopApp: Windows/Linux packages.
include(":shared", ":androidApp", ":desktopApp")
