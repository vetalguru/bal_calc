package org.vetalguru.balcalc

import java.awt.image.BufferedImage
import java.io.ByteArrayOutputStream
import javax.imageio.ImageIO
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

/** A plain PNG of the given size (a phone photo stand-in). */
fun pngOf(width: Int, height: Int): ByteArray {
    val img = BufferedImage(width, height, BufferedImage.TYPE_INT_ARGB)
    val g = img.createGraphics()
    g.color = java.awt.Color(120, 90, 60)
    g.fillRect(0, 0, width, height)
    g.dispose()
    return ByteArrayOutputStream().also { ImageIO.write(img, "png", it) }.toByteArray()
}

class PhotosTest {
    @Test
    fun bigPicturesAreShrunkToJpeg() {
        val small = assertNotNull(shrinkImage(pngOf(2000, 1000)))
        val img = ImageIO.read(small.inputStream())
        assertEquals(640, img.width)
        assertEquals(320, img.height)
        assertEquals(0xFF.toByte(), small[0]) // JPEG starts FF D8
        assertEquals(0xD8.toByte(), small[1])
    }

    @Test
    fun smallPicturesKeepTheirSize() {
        val img = ImageIO.read(shrinkImage(pngOf(300, 200))!!.inputStream())
        assertEquals(300, img.width)
        assertEquals(200, img.height)
    }

    @Test
    fun otherFilesAreNotPictures() {
        assertNull(shrinkImage("not a picture".encodeToByteArray()))
    }
}
