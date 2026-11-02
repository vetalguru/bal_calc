package org.vetalguru.balcalc

import kotlin.test.assertEquals
import kotlin.test.assertNotEquals
import kotlin.test.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.GraphicsMode

@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE) // real drawing and PNG encoding
class TableExportAndroidTest {
    private val rows = List(12) { listOf("${it * 100}", "1.0", "0.2") }

    /** The card is a PNG with room for every row and a column each. */
    @Test
    fun pngIsAPictureWithAColumnEach() {
        val png = tablePng("Card", listOf("Range", "Elev", "Wind"), rows, highlight = 3)
        assertTrue(isPng(png))
        val img = decode(png)
        assertEquals(2 * 48 + 3 * 220, img.width) // pad, cell
        assertEquals(2 * 48 + 68 * (12 + 2) + 16, img.height) // pad, line
    }

    /** One more column makes the picture one cell wider. */
    @Test
    fun widthGrowsWithColumns() {
        val three = decode(tablePng("Card", listOf("A", "B", "C"), rows, highlight = -1)).width
        val four = decode(tablePng("Card", listOf("A", "B", "C", "D"), rows.map { it + "x" }, highlight = -1)).width
        assertEquals(220, four - three)
    }

    /** A title longer than the columns widens the picture to fit it. */
    @Test
    fun longTitleWidensThePicture() {
        val short = decode(tablePng("Card", listOf("A"), rows.map { it.take(1) }, highlight = -1)).width
        val long = decode(tablePng("A".repeat(60) + " drop card", listOf("A"), rows.map { it.take(1) }, highlight = -1)).width
        assertTrue(long > short, "$long vs $short")
    }

    /** The highlighted row has its orange band; the next odd row is grey, an even one white. */
    @Test
    fun highlightedRowIsShaded() {
        val img = decode(tablePng("Card", listOf("Range", "Elev", "Wind"), rows, highlight = 4))
        // Left edge of a row band, inside the padding of the cells (no text there).
        fun band(i: Int) = img.getPixel(48 + 4, 48 + 68 + 16 + 68 * (i + 1) + 8 + 34)
        assertEquals(0xFFFFE0B2.toInt(), band(4))
        assertEquals(0xFFF2F2F2.toInt(), band(3))
        assertEquals(0xFFFFFFFF.toInt(), band(2))
        assertNotEquals(band(4), decode(tablePng("Card", listOf("Range", "Elev", "Wind"), rows, highlight = -1)).let {
            it.getPixel(48 + 4, 48 + 68 + 16 + 68 * 5 + 8 + 34)
        })
    }
}
