package org.vetalguru.balcalc

import java.awt.RenderingHints
import java.awt.image.BufferedImage
import java.io.ByteArrayOutputStream
import javax.imageio.ImageIO
import kotlin.math.max
import kotlin.math.roundToInt

actual fun shrinkImage(image: ByteArray, maxSide: Int): ByteArray? {
    val src = runCatching { ImageIO.read(image.inputStream()) }.getOrNull() ?: return null
    val k = minOf(1.0, maxSide.toDouble() / max(src.width, src.height))
    val w = (src.width * k).roundToInt().coerceAtLeast(1)
    val h = (src.height * k).roundToInt().coerceAtLeast(1)
    val out = BufferedImage(w, h, BufferedImage.TYPE_INT_RGB) // JPEG: no alpha
    val g = out.createGraphics()
    g.setRenderingHint(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BILINEAR)
    g.color = java.awt.Color.WHITE
    g.fillRect(0, 0, w, h)
    g.drawImage(src, 0, 0, w, h, null)
    g.dispose()
    return ByteArrayOutputStream().also { ImageIO.write(out, "jpg", it) }.toByteArray()
}
