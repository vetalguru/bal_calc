package org.vetalguru.balcalc

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Color
import java.io.ByteArrayOutputStream

/** A plain picture of the given size, encoded as [format] (a phone photo stand-in). */
fun bitmapBytes(width: Int, height: Int, format: Bitmap.CompressFormat = Bitmap.CompressFormat.PNG): ByteArray {
    val bmp = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
    bmp.eraseColor(Color.rgb(120, 90, 60))
    return ByteArrayOutputStream().also { bmp.compress(format, 90, it) }.toByteArray()
}

/** A QR code as a PNG, as a screen shows it: 4 px modules, quiet zone. */
fun qrPng(text: String): ByteArray {
    val m = qrMatrix(text)
    val px = 4
    val side = (m.size + 8) * px
    val bmp = Bitmap.createBitmap(side, side, Bitmap.Config.ARGB_8888)
    bmp.eraseColor(Color.WHITE)
    for (y in 0 until m.size) for (x in 0 until m.size) {
        if (!m[x, y]) continue
        for (dy in 0 until px) for (dx in 0 until px) bmp.setPixel((x + 4) * px + dx, (y + 4) * px + dy, Color.BLACK)
    }
    return ByteArrayOutputStream().also { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) }.toByteArray()
}

/** The decoded picture, or a failure naming what was read. */
fun decode(bytes: ByteArray): Bitmap =
    BitmapFactory.decodeByteArray(bytes, 0, bytes.size) ?: error("not a picture: ${bytes.size} bytes")

fun isJpeg(bytes: ByteArray) = bytes.size > 2 && bytes[0] == 0xFF.toByte() && bytes[1] == 0xD8.toByte()

fun isPng(bytes: ByteArray) =
    bytes.size > 8 && bytes.copyOf(8).contentEquals(byteArrayOf(0x89.toByte(), 'P'.code.toByte(), 'N'.code.toByte(), 'G'.code.toByte(), 13, 10, 26, 10))
