plugins {
    alias(libs.plugins.kotlin.multiplatform)
    alias(libs.plugins.android.kmp.library)
    alias(libs.plugins.kotlin.serialization)
    alias(libs.plugins.compose.compiler)
    alias(libs.plugins.compose.multiplatform)
}

kotlin {
    jvmToolchain(17)
    compilerOptions { freeCompilerArgs.add("-Xexpect-actual-classes") }

    android {
        namespace = "org.vetalguru.balcalc.shared"
        compileSdk = libs.versions.android.compileSdk.get().toInt()
        minSdk = libs.versions.android.minSdk.get().toInt()
        // Compose resources (strings) travel in the APK only with this on.
        androidResources.enable = true
    }
    jvm("desktop")

    sourceSets {
        commonMain.dependencies {
            api(libs.compose.runtime)
            api(libs.compose.foundation)
            api(libs.compose.ui)
            api(libs.compose.material3)
            api(libs.compose.resources)
            implementation(libs.kotlinx.serialization.json)
            implementation(libs.kotlinx.coroutines.core)
        }
        commonTest.dependencies {
            implementation(libs.kotlin.test)
        }
        androidMain.dependencies {
            implementation(libs.androidx.activity.compose)
        }
        getByName("desktopMain") {
            dependencies {
                implementation(libs.kotlinx.coroutines.swing)
            }
        }
        getByName("desktopTest") {
            dependencies {
                implementation(libs.compose.ui.test)
                implementation(compose.desktop.currentOs)
            }
        }
    }
}

// UI tests drive the real core: libbalcalc_jni and the starter library from
// the desktop app's staged resources.
tasks.named<Test>("desktopTest") {
    val resources = project(":desktopApp").layout.buildDirectory.dir("mergedResources")
    dependsOn(":desktopApp:mergedResources")
    environment("BALCALC_RESOURCES", resources.get().asFile.path)
    systemProperty("balcalc.screenshots", layout.buildDirectory.dir("screenshots").get().asFile.path)
    // The tests look for English texts: the system language must not matter.
    jvmArgs("-Duser.language=en", "-Duser.country=US")
}

compose.resources {
    packageOfResClass = "org.vetalguru.balcalc.res"
    publicResClass = true
}
