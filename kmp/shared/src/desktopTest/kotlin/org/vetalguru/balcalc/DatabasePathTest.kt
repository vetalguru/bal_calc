package org.vetalguru.balcalc

import java.io.File
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import org.vetalguru.balcalc.core.databaseIn

class DatabasePathTest {
    /** The app was BalCalc: its database is copied to the Holdmark folder once, the old one stays. */
    @Test
    fun balCalcDatabaseIsCopiedOnce() {
        val base = File.createTempFile("holdmark-base", "").apply { delete(); mkdirs() }
        try {
            val old = File(base, "vetalguru/BalCalc/balcalc.db").apply { parentFile.mkdirs(); writeText("old data") }
            File(old.path + "-wal").writeText("journal")
            val path = databaseIn(base)
            assertEquals(File(base, "vetalguru/Holdmark/holdmark.db").path, path)
            assertEquals("old data", File(path).readText())
            assertEquals("journal", File("$path-wal").readText())
            assertTrue(old.exists())

            // Later starts keep what Holdmark has written.
            File(path).writeText("new data")
            databaseIn(base)
            assertEquals("new data", File(path).readText())
        } finally {
            base.deleteRecursively()
        }
    }

    /** A first install: an empty folder, nothing copied. */
    @Test
    fun freshInstallStartsEmpty() {
        val base = File.createTempFile("holdmark-base", "").apply { delete(); mkdirs() }
        try {
            val path = databaseIn(base)
            assertTrue(File(path).parentFile.isDirectory)
            assertTrue(!File(path).exists())
        } finally {
            base.deleteRecursively()
        }
    }
}
