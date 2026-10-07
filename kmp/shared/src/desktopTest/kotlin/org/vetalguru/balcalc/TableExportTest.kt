package org.vetalguru.balcalc

import javax.imageio.ImageIO
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class TableExportTest {
    @Test
    fun csvQuotesWhatNeedsIt() {
        val csv = tableCsv("Card, \"A\"", listOf("Range, m", "Elev"), listOf(listOf("100", "0.25"), listOf("200", "—")))
        assertEquals("\"Card, \"\"A\"\"\"\n\"Range, m\",Elev\n100,0.25\n200,—\n", csv.replace("\r\n", "\n"))
    }

    @Test
    fun pngIsAPictureWithAColumnEach() {
        val png = tablePng("Card", listOf("Range", "Elev", "Wind"), List(12) { listOf("${it * 100}", "1.0", "0.2") }, highlight = 3)
        val img = ImageIO.read(png.inputStream())
        assertTrue(img.width >= 3 * 100, "${img.width}")
        assertTrue(img.height > 12 * 30, "${img.height}")
    }
}
