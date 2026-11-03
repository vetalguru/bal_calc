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
fun resourcesDir(
    env: (String) -> String? = System::getenv,
    property: (String) -> String? = System::getProperty,
): File = File(
    env("BALCALC_RESOURCES")
        ?: property("compose.application.resources.dir")
        ?: error("compose.application.resources.dir is not set"),
)

fun desktopEngine(): Engine {
    val handle = Native.create()
    return Engine { method, args -> Native.call(handle, method, args) }
}

/**
 * The app's data folder (%APPDATA% on Windows, XDG_DATA_HOME on Linux), in
 * vetalguru/Holdmark; HOLDMARK_DB (or the older BALCALC_DB) overrides.
 */
fun desktopDatabasePath(
    env: (String) -> String? = System::getenv,
    property: (String) -> String? = System::getProperty,
): String {
    (env("HOLDMARK_DB") ?: env("BALCALC_DB"))?.let { return it }
    val windows = property("os.name").orEmpty().startsWith("Windows")
    val base = if (windows) {
        File(env("APPDATA"))
    } else {
        // XDG: unset, empty or relative means the default.
        env("XDG_DATA_HOME")?.let(::File)?.takeIf { it.isAbsolute }
            ?: File(property("user.home"), ".local/share")
    }
    return databaseIn(base)
}

/**
 * vetalguru/Holdmark/holdmark.db under [base]. The app was BalCalc before:
 * the first start copies its database (and SQLite's journal files) over, so
 * rifles and cartridges stay; the old folder is left as it was.
 */
fun databaseIn(base: File): String {
    val dir = File(base, "vetalguru/Holdmark")
    dir.mkdirs()
    val db = File(dir, "holdmark.db")
    val old = File(base, "vetalguru/BalCalc/balcalc.db")
    if (!db.exists() && old.exists()) {
        for (suffix in listOf("", "-wal", "-shm")) {
            File(old.path + suffix).takeIf { it.exists() }?.copyTo(File(db.path + suffix))
        }
    }
    return db.path
}

/** data/seed copied into the resources under seed/. */
fun desktopSeed(): List<SeedFile> =
    File(resourcesDir(), "seed").walkTopDown()
        .filter { it.isFile && isSeedFile(it.name) }
        .map { f -> SeedFile(f.name) { f.readText() } }
        .toList()
