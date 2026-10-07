package org.vetalguru.balcalc.core

import android.content.Context
import java.io.File

/** libbalcalc_jni (jni/src/balcalc_jni.cpp). */
internal object Native {
    init {
        System.loadLibrary("balcalc_jni")
    }

    @JvmStatic external fun create(): Long
    @JvmStatic external fun destroy(handle: Long)
    @JvmStatic external fun call(handle: Long, method: String, args: String): String
}

/** The core for this Android app; one per process. */
fun androidEngine(): Engine {
    val handle = Native.create()
    return Engine { method, args -> Native.call(handle, method, args) }
}

/** Where the Qt version kept it too (QStandardPaths::AppDataLocation). */
fun androidDatabasePath(context: Context): String = File(context.filesDir, "holdmark.db").path

/** data/seed packed as assets under seed/. */
fun androidSeed(context: Context): List<SeedFile> {
    val assets = context.assets
    fun walk(dir: String): List<SeedFile> = assets.list(dir).orEmpty().flatMap { name ->
        val path = "$dir/$name"
        if (isSeedFile(name)) {
            listOf(SeedFile(name) { assets.open(path).use { it.readBytes().decodeToString() } })
        } else {
            walk(path)
        }
    }
    return walk("seed")
}
