package org.vetalguru.balcalc

import android.graphics.Bitmap
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.GraphicsMode

@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE) // real JPEG/PNG encoding and decoding
class PhotosAndroidTest {
    /** A big landscape picture comes back as a JPEG with its longer side at the limit. */
    @Test
    fun bigPicturesAreShrunkToJpeg() {
        val small = assertNotNull(shrinkImage(bitmapBytes(2000, 1000)))
        assertTrue(isJpeg(small))
        val img = decode(small)
        assertEquals(640, img.width)
        assertEquals(320, img.height)
    }

    /** Portrait keeps its aspect too; the limit is on the longer side, whichever it is. */
    @Test
    fun portraitKeepsItsAspect() {
        val img = decode(shrinkImage(bitmapBytes(900, 3000, Bitmap.CompressFormat.JPEG), maxSide = 300)!!)
        assertEquals(300, img.height)
        assertEquals(90, img.width)
    }

    /** A camera-sized picture is subsampled on reading and still lands exactly on the limit. */
    @Test
    fun hugePicturesAreSubsampledFirst() {
        val img = decode(shrinkImage(bitmapBytes(4000, 3000))!!)
        assertEquals(640, img.width)
        assertEquals(480, img.height)
    }

    /** A picture already within the limit keeps its size (re-encoded as JPEG). */
    @Test
    fun smallPicturesKeepTheirSize() {
        val out = shrinkImage(bitmapBytes(300, 200))!!
        assertTrue(isJpeg(out))
        val img = decode(out)
        assertEquals(300, img.width)
        assertEquals(200, img.height)
    }

    /** Bytes that are no picture give null. */
    @Test
    fun otherFilesAreNotPictures() {
        assertNull(shrinkImage("not a picture".encodeToByteArray()))
        assertNull(shrinkImage(ByteArray(0)))
    }
}
