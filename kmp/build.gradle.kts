plugins {
    alias(libs.plugins.kotlin.multiplatform) apply false
    alias(libs.plugins.kotlin.jvm) apply false
    alias(libs.plugins.kotlin.serialization) apply false
    alias(libs.plugins.compose.compiler) apply false
    alias(libs.plugins.compose.multiplatform) apply false
    alias(libs.plugins.android.application) apply false
    alias(libs.plugins.android.kmp.library) apply false
}

// The C++ project (core, storage, applogic, bridge, jni) at the repository root.
val repoRoot: File = rootDir.parentFile
extra["repoRoot"] = repoRoot

// project(VERSION) of the top-level CMakeLists.txt: the one app version.
extra["appVersion"] = Regex("""project\(ballistics VERSION ([0-9.]+)""")
    .find(repoRoot.resolve("CMakeLists.txt").readText())!!.groupValues[1]
