package org.vetalguru.balcalc

import kotlin.math.abs
import kotlin.math.pow
import kotlin.math.roundToLong

/** [decimals] digits after the point, no "-0.00". */
fun Double.fixed(decimals: Int): String {
    val scale = 10.0.pow(decimals)
    val scaled = (this * scale).roundToLong()
    if (scaled == 0L) return if (decimals == 0) "0" else "0." + "0".repeat(decimals)
    val sign = if (scaled < 0) "-" else ""
    val digits = abs(scaled).toString().padStart(decimals + 1, '0')
    return if (decimals == 0) sign + digits
    else sign + digits.dropLast(decimals) + "." + digits.takeLast(decimals)
}
