package org.vetalguru.balcalc

import java.awt.Color
import java.awt.Font
import java.awt.RenderingHints
import java.awt.image.BufferedImage
import java.io.ByteArrayOutputStream
import javax.imageio.ImageIO

actual fun tablePng(title: String, header: List<String>, rows: List<List<String>>, highlight: Int): ByteArray {
    val cell = 110
    val line = 34
    val pad = 24
    val titleFont = Font(Font.SANS_SERIF, Font.BOLD, 20)
    // As wide as the columns or the title, whichever is wider.
    val probe = BufferedImage(1, 1, BufferedImage.TYPE_INT_RGB).createGraphics()
    val titleWidth = probe.getFontMetrics(titleFont).stringWidth(title)
    probe.dispose()
    val width = pad * 2 + maxOf(cell * header.size.coerceAtLeast(1), titleWidth)
    val height = pad * 2 + line * (rows.size + 2) + 8
    val img = BufferedImage(width, height, BufferedImage.TYPE_INT_RGB)
    val g = img.createGraphics()
    g.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON)
    g.color = Color.WHITE
    g.fillRect(0, 0, width, height)
    g.color = Color.BLACK
    g.font = titleFont
    g.drawString(title, pad, pad + 22)
    fun row(y: Int, cells: List<String>, bold: Boolean) {
        g.font = Font(Font.SANS_SERIF, if (bold) Font.BOLD else Font.PLAIN, 17)
        cells.forEachIndexed { i, s ->
            val right = pad + cell * (i + 1) - 10
            g.drawString(s, right - g.fontMetrics.stringWidth(s), y)
        }
    }
    val top = pad + line + 8
    row(top + 22, header, bold = true)
    g.drawLine(pad, top + line, width - pad, top + line)
    rows.forEachIndexed { i, r ->
        val y = top + line * (i + 1)
        if (i == highlight) {
            g.color = Color(0xFFE0B2)
            g.fillRect(pad, y + 4, width - pad * 2, line)
            g.color = Color.BLACK
        } else if (i % 2 == 1) {
            g.color = Color(0xF2F2F2)
            g.fillRect(pad, y + 4, width - pad * 2, line)
            g.color = Color.BLACK
        }
        row(y + 26, r, bold = i == highlight)
    }
    g.dispose()
    return ByteArrayOutputStream().also { ImageIO.write(img, "png", it) }.toByteArray()
}
