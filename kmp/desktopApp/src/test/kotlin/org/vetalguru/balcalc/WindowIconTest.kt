package org.vetalguru.balcalc

import androidx.compose.ui.geometry.Size
import java.io.File
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

/** The window icon: the shipped PNG, or no icon (not a crash) when it is missing or broken. */
class WindowIconTest {
    private val dir = File.createTempFile("holdmark-icon", "").apply { delete(); mkdirs() }

    @AfterTest
    fun cleanUp() {
        dir.deleteRecursively()
    }

    /** The packages' icon is read at its own size. */
    @Test
    fun shippedIcon() {
        File(System.getProperty("balcalc.icon")).copyTo(File(dir, "balcalc.png"))
        val icon = assertNotNull(windowIcon(dir))
        assertEquals(Size(512f, 512f), icon.intrinsicSize)
    }

    /** A package without the icon, or with a damaged one, opens without it. */
    @Test
    fun missingOrBrokenIcon() {
        assertNull(windowIcon(dir))
        File(dir, "balcalc.png").writeText("not a picture")
        assertNull(windowIcon(dir))
        File(dir, "balcalc.png").delete()
        File(dir, "balcalc.png").mkdir() // a folder of that name
        assertNull(windowIcon(dir))
    }
}
