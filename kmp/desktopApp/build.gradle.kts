import org.jetbrains.compose.desktop.application.dsl.TargetFormat

plugins {
    alias(libs.plugins.kotlin.jvm)
    alias(libs.plugins.compose.compiler)
    alias(libs.plugins.compose.multiplatform)
}

val repoRoot = rootProject.extra["repoRoot"] as File
val windows = System.getProperty("os.name").startsWith("Windows")

kotlin { jvmToolchain(17) }

dependencies {
    implementation(project(":shared"))
    implementation(compose.desktop.currentOs)
    implementation(libs.kotlinx.coroutines.swing)
}

// libbalcalc_jni for this OS, built from the repository's CMake project.
val nativeBuildDir = layout.buildDirectory.dir("native").get().asFile
val configureNative = tasks.register<Exec>("configureNative") {
    workingDir = repoRoot
    val generator = if (windows) listOf("-G", "Visual Studio 17 2022", "-A", "x64") else listOf("-G", "Ninja")
    commandLine(
        listOf("cmake", "-S", repoRoot.path, "-B", nativeBuildDir.path) + generator + listOf(
            "-DCMAKE_BUILD_TYPE=Release",
            "-DBALLISTICS_BUILD_APP=OFF",
            "-DBALLISTICS_BUILD_CLI=OFF",
            "-DBALLISTICS_BUILD_TESTS=OFF",
            "-DBALLISTICS_BUILD_JNI=ON",
        ),
    )
    outputs.file(nativeBuildDir.resolve("CMakeCache.txt"))
}
val buildNative = tasks.register<Exec>("buildNative") {
    dependsOn(configureNative)
    commandLine("cmake", "--build", nativeBuildDir.path, "--config", "Release", "--target", "balcalc_jni")
    // Always ask CMake; it knows what is up to date.
    outputs.upToDateWhen { false }
}

// App resources: <os>/libbalcalc_jni (+ common/seed), see Compose's
// appResourcesRootDir layout.
val appResources = layout.buildDirectory.dir("appResources")
val stageResources = tasks.register<Sync>("stageResources") {
    dependsOn(buildNative)
    into(appResources)
    from(nativeBuildDir.resolve("jni")) {
        include("balcalc_jni.dll", "Release/balcalc_jni.dll", "libbalcalc_jni.so")
        val target = if (System.getProperty("os.name").startsWith("Windows")) "windows" else "linux"
        eachFile { path = "$target/$name" }
        includeEmptyDirs = false
    }
    from(repoRoot.resolve("data/seed")) {
        include("**/*.ammo", "**/*.drg", "**/*.reticle", "**/*.json")
        into("common/seed")
    }
}

compose.desktop {
    application {
        mainClass = "org.vetalguru.balcalc.MainKt"
        nativeDistributions {
            appResourcesRootDir.set(appResources)
            targetFormats(TargetFormat.Msi, TargetFormat.Deb)
            packageName = "BalCalc"
            packageVersion = rootProject.extra["appVersion"] as String
            vendor = "vetalguru"
        }
    }
}

tasks.matching { it.name in setOf("prepareAppResources", "run", "createDistributable") }
    .configureEach { dependsOn(stageResources) }

// The same files as one directory (what a package's resources dir holds),
// for tests: BALCALC_RESOURCES points here.
val mergedResources = tasks.register<Sync>("mergedResources") {
    dependsOn(stageResources)
    val os = if (System.getProperty("os.name").startsWith("Windows")) "windows" else "linux"
    from(appResources.map { it.dir(os) })
    from(appResources.map { it.dir("common") })
    into(layout.buildDirectory.dir("mergedResources"))
}
