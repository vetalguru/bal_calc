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
        versionCode = 2 // the Qt version was 1; grow with every release
        versionName = rootProject.extra["appVersion"] as String

        ndk { abiFilters += listOf("arm64-v8a") }
        externalNativeBuild {
            cmake {
                arguments += listOf(
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

    // Release signing from the environment, never from the repository
    // (tools/android-release.ps1 sets these from the user's encrypted
    // settings). Without them the release APK stays unsigned.
    val keystore = providers.environmentVariable("BALCALC_KEYSTORE_PATH").orNull
    signingConfigs {
        if (keystore != null) {
            create("release") {
                storeFile = file(keystore)
                storePassword = providers.environmentVariable("BALCALC_KEYSTORE_PASSWORD").get()
                keyAlias = providers.environmentVariable("BALCALC_KEYSTORE_ALIAS").get()
                keyPassword = storePassword
            }
        }
    }
    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.findByName("release")
        }
    }

    buildFeatures { compose = true }

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
        exclude("sources/**") // collection inputs
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
