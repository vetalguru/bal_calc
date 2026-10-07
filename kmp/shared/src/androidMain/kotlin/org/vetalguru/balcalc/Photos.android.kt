package org.vetalguru.balcalc

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import java.io.ByteArrayOutputStream
import kotlin.math.max
import kotlin.math.roundToInt

actual fun shrinkImage(image: ByteArray, maxSide: Int): ByteArray? {
    // Read big photos at a fraction of their size first: phone cameras give 50+ Mpx.
    val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
    BitmapFactory.decodeByteArray(image, 0, image.size, bounds)
    if (bounds.outWidth <= 0 || bounds.outHeight <= 0) return null
    var sample = 1
    while (max(bounds.outWidth, bounds.outHeight) / (sample * 2) >= maxSide) sample *= 2
    val src = BitmapFactory.decodeByteArray(image, 0, image.size, BitmapFactory.Options().apply { inSampleSize = sample })
        ?: return null
    val k = minOf(1.0, maxSide.toDouble() / max(src.width, src.height))
    val scaled = Bitmap.createScaledBitmap(
        src, (src.width * k).roundToInt().coerceAtLeast(1), (src.height * k).roundToInt().coerceAtLeast(1), true,
    )
    return ByteArrayOutputStream().also { scaled.compress(Bitmap.CompressFormat.JPEG, 85, it) }.toByteArray()
}
