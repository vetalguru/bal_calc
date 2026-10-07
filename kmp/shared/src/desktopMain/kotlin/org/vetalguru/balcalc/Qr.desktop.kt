package org.vetalguru.balcalc

import com.google.zxing.BarcodeFormat
import com.google.zxing.BinaryBitmap
import com.google.zxing.DecodeHintType
import com.google.zxing.EncodeHintType
import com.google.zxing.RGBLuminanceSource
import com.google.zxing.common.GlobalHistogramBinarizer
import com.google.zxing.common.HybridBinarizer
import com.google.zxing.qrcode.QRCodeReader
import com.google.zxing.qrcode.QRCodeWriter
import com.google.zxing.qrcode.decoder.ErrorCorrectionLevel
import java.io.ByteArrayOutputStream
import java.util.zip.Deflater
import java.util.zip.DeflaterOutputStream
import java.util.zip.InflaterInputStream
import javax.imageio.ImageIO

actual fun qrMatrix(text: String): QrMatrix {
    val hints = mapOf(EncodeHintType.ERROR_CORRECTION to ErrorCorrectionLevel.M, EncodeHintType.MARGIN to 0)
    val bits = QRCodeWriter().encode(text, BarcodeFormat.QR_CODE, 0, 0, hints)
    return QrMatrix(bits.width, BooleanArray(bits.width * bits.height) { bits[it % bits.width, it / bits.width] })
}

actual fun decodeQrImage(image: ByteArray): String? {
    val img = ImageIO.read(image.inputStream()) ?: return null
    val pixels = img.getRGB(0, 0, img.width, img.height, null, 0, img.width)
    return decodeQrPixels(img.width, img.height, pixels)
}

actual fun deflate(data: ByteArray): ByteArray =
    ByteArrayOutputStream().also { out ->
        DeflaterOutputStream(out, Deflater(Deflater.BEST_COMPRESSION)).use { it.write(data) }
    }.toByteArray()

actual fun inflate(data: ByteArray): ByteArray = InflaterInputStream(data.inputStream()).use { it.readBytes() }

/** The local binarizer first (photos), then the global one and a clean-image read. */
private fun decodeQrPixels(width: Int, height: Int, pixels: IntArray): String? {
    val source = RGBLuminanceSource(width, height, pixels)
    val tries = listOf(
        BinaryBitmap(HybridBinarizer(source)) to mapOf(DecodeHintType.TRY_HARDER to true),
        BinaryBitmap(GlobalHistogramBinarizer(source)) to mapOf(DecodeHintType.TRY_HARDER to true),
        BinaryBitmap(HybridBinarizer(source)) to mapOf(DecodeHintType.PURE_BARCODE to true),
    )
    for ((bitmap, hints) in tries) {
        runCatching { return QRCodeReader().decode(bitmap, hints).text }
    }
    return null
}
