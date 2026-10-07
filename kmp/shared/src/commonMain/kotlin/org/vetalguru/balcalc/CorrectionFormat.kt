package org.vetalguru.balcalc

import kotlin.math.abs
import kotlin.math.roundToInt

/** MOA in one milliradian. */
const val MOA_PER_MRAD = 3.437746770784939

/** A correction as the solution screen shows it. */
data class CorrectionText(
    /** The direction: a word, an arrow or a sign ("" when there is nothing to correct). */
    val direction: String,
    /** The size in the main unit, without a sign. */
    val value: String,
    /** Whole clicks, or null without a scope. */
    val clicks: Int?,
    /** The size in the other angle unit and in cm at the target, e.g. "5.53 MOA · 16.1 cm". */
    val second: String,
)

/**
 * Formats a correction of [angle] (in the main unit: MOA when [moa], else
 * mrad; positive = up or right) for a scope whose [click] is that size (in
 * the same unit; null without a scope) at [rangeM].
 *
 * [positive] / [negative] say each way: words, arrows or signs. With
 * [roundToClicks] the value is what whole clicks give (the turret cannot
 * set more exactly); without a scope it stays exact. [otherUnit] names the
 * second angle unit.
 */
fun formatCorrection(
    angle: Double,
    click: Double?,
    moa: Boolean,
    rangeM: Double,
    roundToClicks: Boolean,
    positive: String,
    negative: String,
    otherUnit: String,
    cm: String,
): CorrectionText {
    val whole = click?.takeIf { it > 0 }?.let { (angle / it).roundToInt() }
    val shown = if (roundToClicks && whole != null) whole * click else angle
    val mrad = if (moa) shown / MOA_PER_MRAD else shown
    val other = if (moa) mrad else mrad * MOA_PER_MRAD
    val atTarget = abs(mrad) * rangeM / 10.0 // 1 mrad is 0.1 cm per metre
    val direction = when {
        abs(shown) < 0.005 -> ""
        shown > 0 -> positive
        else -> negative
    }
    return CorrectionText(
        direction = direction,
        value = abs(shown).fixed(2),
        clicks = whole?.let { abs(it) },
        second = "${abs(other).fixed(2)} $otherUnit · ${atTarget.fixed(1)} $cm",
    )
}
