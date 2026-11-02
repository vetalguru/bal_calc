package org.vetalguru.balcalc.core

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import java.io.File
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(shadows = [DeviceAssetListing::class])
class SeedTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    /** data/seed in the repository, where the assets come from (the build passes its path). */
    private val seedDir = File(System.getProperty("balcalc.seed")!!)

    /** The data files of data/seed as the build packs them: no docs, scripts or collection inputs. */
    private val expected: List<File> = seedDir.walkTopDown()
        .filter { it.isFile && isSeedFile(it.name) && !it.relativeTo(seedDir).path.startsWith("sources") }
        .toList()

    /** The shadow lists a folder as a phone does: its own entries, folders by name. */
    @Test
    fun assetsListLikeAPhone() {
        val top = context.assets.list("seed")!!.toSet()
        assertTrue(setOf("ammo", "drg", "reticle", "published_rifles.json").all { it in top }, "$top")
        assertTrue(top.none { '/' in it }, "$top")
    }

    /** Every data file of the starter library is found in the APK's assets, through all folders. */
    @Test
    fun seedWalksAllAssetFolders() {
        val seed = androidSeed(context)
        assertTrue(expected.size > 50, "${expected.size} files in $seedDir")
        assertEquals(expected.map { it.name }.sorted(), seed.map { it.name }.sorted())
        // Each kind is there: cartridges and drag tables in folders, reticles, catalogs at the top.
        for (ext in listOf(".ammo", ".drg", ".reticle", ".json")) assertTrue(seed.any { it.name.endsWith(ext) }, ext)
        assertTrue(seed.none { it.name.endsWith(".py") || it.name.endsWith(".md") || it.name.endsWith(".txt") })
    }

    /** A file's content is read from the asset, whole and as UTF-8. */
    @Test
    fun seedContentIsTheFile() {
        val seed = androidSeed(context).associateBy { it.name }
        for (file in listOf(expected.first { it.name.endsWith(".ammo") }, expected.first { it.name == "published_rifles.json" })) {
            assertEquals(file.readText(), seed.getValue(file.name).content, file.name)
        }
    }

    /** The database lives in the app's private files, under the name the Qt version used. */
    @Test
    fun databaseInPrivateFiles() {
        assertEquals(File(context.filesDir, "holdmark.db").path, androidDatabasePath(context))
    }
}
