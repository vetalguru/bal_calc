package org.vetalguru.balcalc

import java.io.File
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.put
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.core.SeedFile
import org.vetalguru.balcalc.core.desktopEngine
import org.vetalguru.balcalc.core.desktopSeed

class StartupTest {
    /** A seeded database: the next start reads only the catalogs, and the library is all there. */
    @Test
    fun laterStartReadsOnlyTheCatalogs() = runBlocking {
        val db = File.createTempFile("balcalc-start", ".db").apply { delete(); deleteOnExit() }
        val read = mutableListOf<String>()
        fun counted() = desktopSeed().map { f -> SeedFile(f.name) { read += f.name; f.content } }
        val filter = buildJsonObject { put("filter", "") }

        Api(desktopEngine()).start(db.path, ::counted)
        assertTrue(read.size > 50, "first start read ${read.size} files")

        read.clear()
        val api = Api(desktopEngine())
        api.start(db.path, ::counted)
        assertEquals(setOf("published_scopes.json", "published_rifles.json"), read.toSet())
        assertTrue(api.call("libraryScopes", filter).jsonArray.isNotEmpty())
        assertTrue(api.call("libraryRifles", filter).jsonArray.isNotEmpty())
        assertTrue(api.call("libraryBullets", filter).jsonArray.size > 300)
        assertTrue(api.call("reticles").jsonArray.size >= 14)
    }
}
