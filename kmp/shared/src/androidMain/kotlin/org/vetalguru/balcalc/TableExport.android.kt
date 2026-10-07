package org.vetalguru.balcalc

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Typeface
import java.io.ByteArrayOutputStream

actual fun tablePng(title: String, header: List<String>, rows: List<List<String>>, highlight: Int): ByteArray {
    val cell = 220
    val line = 68
    val pad = 48
    val text = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.BLACK; textSize = 34f; textAlign = Paint.Align.RIGHT }
    val bold = Paint(text).apply { typeface = Typeface.DEFAULT_BOLD }
    val titlePaint = Paint(bold).apply { textSize = 40f; textAlign = Paint.Align.LEFT }
    // As wide as the columns or the title, whichever is wider.
    val width = pad * 2 + maxOf(cell * header.size.coerceAtLeast(1), titlePaint.measureText(title).toInt())
    val height = pad * 2 + line * (rows.size + 2) + 16
    val bmp = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
    val c = Canvas(bmp)
    c.drawColor(Color.WHITE)
    val fill = Paint()
    c.drawText(title, pad.toFloat(), pad + 44f, titlePaint)
    fun row(y: Float, cells: List<String>, paint: Paint) =
        cells.forEachIndexed { i, s -> c.drawText(s, (pad + cell * (i + 1) - 20).toFloat(), y, paint) }
    val top = pad + line + 16
    row(top + 44f, header, bold)
    c.drawLine(pad.toFloat(), (top + line).toFloat(), (width - pad).toFloat(), (top + line).toFloat(), text)
    rows.forEachIndexed { i, r ->
        val y = top + line * (i + 1)
        if (i == highlight || i % 2 == 1) {
            fill.color = if (i == highlight) 0xFFFFE0B2.toInt() else 0xFFF2F2F2.toInt()
            c.drawRect(pad.toFloat(), (y + 8).toFloat(), (width - pad).toFloat(), (y + 8 + line).toFloat(), fill)
        }
        row(y + 52f, r, if (i == highlight) bold else text)
    }
    return ByteArrayOutputStream().also { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) }.toByteArray()
}
