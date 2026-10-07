package org.vetalguru.balcalc.core

import java.io.File

/** libbalcalc_jni (jni/src/balcalc_jni.cpp), shipped in the app's resources. */
internal object Native {
    init {
        System.load(File(resourcesDir(), System.mapLibraryName("balcalc_jni")).absolutePath)
    }

    @JvmStatic external fun create(): Long
    @JvmStatic external fun destroy(handle: Long)
    @JvmStatic external fun call(handle: Long, method: String, args: String): String
}

/**
 * The packaged app's resources (Compose Desktop sets the property for both
 * `run` and installed packages); BALCALC_RESOURCES overrides it for tests.
 */
fun resourcesDir(): File = File(
    System.getenv("BALCALC_RESOURCES")
        ?: System.getProperty("compose.application.resources.dir")
        ?: error("compose.application.resources.dir is not set"),
)

fun desktopEngine(): Engine {
    val handle = Native.create()
    return Engine { method, args -> Native.call(handle, method, args) }
}

/**
 * Same file as the Qt version (QStandardPaths::AppDataLocation for
 * organisation "vetalguru", application "BalCalc"); BALCALC_DB overrides.
 */
fun desktopDatabasePath(): String {
    System.getenv("BALCALC_DB")?.let { return it }
    val windows = System.getProperty("os.name").startsWith("Windows")
    val base = if (windows) {
        File(System.getenv("APPDATA"))
    } else {
        System.getenv("XDG_DATA_HOME")?.let(::File)
            ?: File(System.getProperty("user.home"), ".local/share")
    }
    val dir = File(base, "vetalguru/BalCalc")
    dir.mkdirs()
    return File(dir, "balcalc.db").path
}

/** data/seed copied into the resources under seed/. */
fun desktopSeed(): List<SeedFile> =
    File(resourcesDir(), "seed").walkTopDown()
        .filter { it.isFile && isSeedFile(it.name) }
        .map { f -> SeedFile(f.name) { f.readText() } }
        .toList()
