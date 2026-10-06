package org.vetalguru.balcalc

import java.awt.Color
import java.awt.image.BufferedImage
import java.io.ByteArrayOutputStream
import javax.imageio.ImageIO
import kotlin.random.Random
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** A QR code as a PNG, as a screen shows it: 4 px modules, quiet zone. */
fun qrPng(text: String): ByteArray {
    val m = qrMatrix(text)
    val px = 4
    val side = (m.size + 8) * px
    val img = BufferedImage(side, side, BufferedImage.TYPE_INT_RGB)
    val g = img.createGraphics()
    g.color = Color.WHITE
    g.fillRect(0, 0, side, side)
    g.color = Color.BLACK
    for (y in 0 until m.size) for (x in 0 until m.size) {
        if (m[x, y]) g.fillRect((x + 4) * px, (y + 4) * px, px, px)
    }
    g.dispose()
    return ByteArrayOutputStream().also { ImageIO.write(img, "png", it) }.toByteArray()
}

class QrShareTest {
    private val rifle = """{"format":"balcalc-rifle","version":1,"rifle":{"name":"Tikka T3x CTR","caliber":"6.5 Creedmoor"}}"""

    @Test
    fun smallFileIsOneCodeThroughAPicture() {
        val parts = QrShare.parts(rifle)
        assertEquals(1, parts.size)
        val read = decodeQrImage(qrPng(parts[0]))
        assertEquals(parts[0], read)
        val c = QrShare.Collector()
        assertTrue(c.add(read!!))
        assertTrue(c.complete)
        assertEquals(rifle, c.json())
    }

    @Test
    fun bigFileComesInPartsInAnyOrder() {
        // Random digits do not compress much: several codes.
        val random = Random(7)
        val json = """{"reticle":"${(1..6000).joinToString("") { random.nextInt(10).toString() }}"}"""
        val parts = QrShare.parts(json)
        assertTrue(parts.size > 1, "${parts.size} parts")
        assertTrue(parts.all { it.length <= QrShare.CHUNK + 20 })
        val c = QrShare.Collector()
        for (p in parts.reversed()) {
            assertFalse(c.complete)
            assertTrue(c.add(decodeQrImage(qrPng(p))!!))
        }
        assertTrue(c.add(parts[0])) // a part read twice changes nothing
        assertTrue(c.complete)
        assertEquals(json, c.json())
    }

    @Test
    fun foreignCodesAreRefused() {
        val c = QrShare.Collector()
        assertFalse(c.add("https://example.com"))
        assertFalse(c.add("BALCALC1:3:2:abc"))
        assertTrue(c.add("BALCALC1:1:2:abc"))
        assertFalse(c.add("BALCALC1:1:3:abc")) // another set of parts
        assertFalse(c.complete)
        assertNull(decodeQrImage(ByteArray(10)))
    }

    @Test
    fun aPlainJsonCodeIsComplete() {
        val c = QrShare.Collector()
        assertTrue(c.add(rifle))
        assertTrue(c.complete)
        assertEquals(rifle, c.json())
    }
}
