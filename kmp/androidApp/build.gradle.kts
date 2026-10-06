import com.android.build.api.variant.ApplicationAndroidComponentsExtension

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.compose.compiler)
}

val repoRoot = rootProject.extra["repoRoot"] as File

android {
    namespace = "org.vetalguru.balcalc"
    compileSdk = libs.versions.android.compileSdk.get().toInt()
    ndkVersion = libs.versions.android.ndk.get()

    defaultConfig {
        // Same id as the Qt version: installs as its update and keeps the data.
        applicationId = "org.vetalguru.balcalc"
        minSdk = libs.versions.android.minSdk.get().toInt()
        targetSdk = libs.versions.android.targetSdk.get().toInt()
        versionCode = 2 // the Qt version was 1
        versionName = rootProject.extra["appVersion"] as String

        ndk { abiFilters += listOf("arm64-v8a") }
        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DBALLISTICS_BUILD_APP=OFF",
                    "-DBALLISTICS_BUILD_CLI=OFF",
                    "-DBALLISTICS_BUILD_TESTS=OFF",
                    "-DBALLISTICS_BUILD_JNI=ON",
                )
                targets += "balcalc_jni"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = repoRoot.resolve("CMakeLists.txt")
            version = libs.versions.android.cmake.get()
        }
    }

    buildFeatures { compose = true }

    sourceSets["main"].res.directories += repoRoot.resolve("app/android/res").path // icons of the Qt app

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

kotlin { jvmToolchain(17) }

// data/seed as assets/seed (data files only).
val seedAssets = tasks.register<Sync>("seedAssets") {
    from(repoRoot.resolve("data/seed")) {
        include("**/*.ammo", "**/*.drg", "**/*.reticle", "**/*.json")
    }
    into(layout.buildDirectory.dir("generated/seedAssets/seed"))
}
extensions.getByType<ApplicationAndroidComponentsExtension>().onVariants { variant ->
    variant.sources.assets?.addStaticSourceDirectory(
        layout.buildDirectory.dir("generated/seedAssets").get().asFile.path,
    )
}
tasks.named("preBuild") { dependsOn(seedAssets) }

dependencies {
    implementation(project(":shared"))
    implementation(libs.androidx.activity.compose)
}
