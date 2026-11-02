package org.vetalguru.balcalc

import kotlin.random.Random
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.GraphicsMode

@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE) // real PNG encoding and decoding
class QrAndroidTest {
    private val rifle = """{"format":"balcalc-rifle","version":1,"rifle":{"name":"Tikka T3x CTR","caliber":"6.5 Creedmoor"}}"""

    /** A code drawn from the matrix and saved as a picture reads back as the same text. */
    @Test
    fun matrixRendersAndDecodesBack() {
        val parts = QrShare.parts(rifle)
        assertEquals(1, parts.size)
        assertEquals(parts[0], decodeQrImage(qrPng(parts[0])))
    }

    /** Every part of a big file survives the picture round trip and the collector rebuilds it. */
    @Test
    fun bigFileComesBackThroughPictures() {
        val random = Random(7)
        val json = """{"reticle":"${(1..3000).joinToString("") { random.nextInt(10).toString() }}"}"""
        val parts = QrShare.parts(json)
        assertTrue(parts.size > 1, "${parts.size} parts")
        val c = QrShare.Collector()
        for (p in parts.reversed()) assertTrue(c.add(decodeQrImage(qrPng(p))!!))
        assertTrue(c.complete)
        assertEquals(json, c.json())
    }

    /** The matrix is at least version 1 (21 modules), no margin: the finder patterns sit in the corners. */
    @Test
    fun matrixHasFinderPatterns() {
        val m = qrMatrix("hello")
        assertTrue(m.size >= 21, "${m.size}")
        // Each finder: dark outer ring, light ring, dark 3x3 centre.
        for ((x0, y0) in listOf(0 to 0, m.size - 7 to 0, 0 to m.size - 7)) {
            assertTrue(m[x0, y0] && m[x0 + 6, y0 + 6], "outer ring at $x0,$y0")
            assertTrue(!m[x0 + 1, y0 + 1] && !m[x0 + 5, y0 + 5], "light ring at $x0,$y0")
            assertTrue(m[x0 + 3, y0 + 3], "centre at $x0,$y0")
        }
    }

    /** Deflated data inflates back unchanged, and repetitive text shrinks. */
    @Test
    fun deflateRoundTrip() {
        val data = rifle.repeat(20).encodeToByteArray()
        val packed = deflate(data)
        assertTrue(packed.size < data.size / 5, "${packed.size} of ${data.size}")
        assertContentEquals(data, inflate(packed))
        assertContentEquals(ByteArray(0), inflate(deflate(ByteArray(0))))
    }

    /** Bytes that are no picture, and a picture without a code, give null. */
    @Test
    fun noCodeIsNull() {
        assertNull(decodeQrImage(ByteArray(10)))
        assertNull(decodeQrImage("not a picture".encodeToByteArray()))
        assertNull(decodeQrImage(bitmapBytes(200, 200)))
    }
}
