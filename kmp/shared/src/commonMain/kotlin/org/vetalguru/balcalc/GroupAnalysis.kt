package org.vetalguru.balcalc

import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.sqrt

/** A point on the target photo, in its pixels (y down). */
data class PhotoPoint(val x: Double, val y: Double)

/** A group of hits measured on a photo of the target, at [rangeM]. */
data class GroupStats(
    val shots: Int,
    /** Extreme spread: the two hits farthest apart, centre to centre. */
    val extremeSpreadCm: Double,
    /** Mean distance of the hits from their centre. */
    val meanRadiusCm: Double,
    /** Centre of the group (mean point of impact) from the aim point. */
    val centreUpCm: Double,
    val centreRightCm: Double,
    /** Dispersion per axis (1 sigma), from the spread of the hits. */
    val sigmaCm: Double,
    val rangeM: Double,
) {
    /** An angle seen at the range: cm -> MRAD. */
    fun mrad(cm: Double) = cm * 10.0 / rangeM

    fun moa(cm: Double) = mrad(cm) * MOA_PER_MRAD

    /**
     * The 5-shot group (MOA) this dispersion corresponds to: what the hit
     * probability takes (its extreme spread is about 3.07 sigma).
     */
    val equivalentFiveShotMoa get() = moa(sigmaCm * 3.07)

    companion object {
        const val MOA_PER_MRAD = 3.4377467707849396
    }
}

/**
 * Hits, the aim point and a scale (two points [scaleCm] apart on the
 * target) from a photo -> the group. Null with fewer than two hits or no
 * scale.
 */
fun analyzeGroup(
    hits: List<PhotoPoint>,
    aim: PhotoPoint,
    scaleA: PhotoPoint,
    scaleB: PhotoPoint,
    scaleCm: Double,
    rangeM: Double,
): GroupStats? {
    val px = hypot(scaleB.x - scaleA.x, scaleB.y - scaleA.y)
    if (hits.size < 2 || px <= 0.0 || scaleCm <= 0.0 || rangeM <= 0.0) return null
    val cmPerPx = scaleCm / px
    // Up and right of the aim point, cm.
    val pts = hits.map { (it.x - aim.x) * cmPerPx to -(it.y - aim.y) * cmPerPx }
    val right = pts.map { it.first }.average()
    val up = pts.map { it.second }.average()
    var es = 0.0
    for (i in pts.indices) for (j in i + 1 until pts.size) {
        es = max(es, hypot(pts[i].first - pts[j].first, pts[i].second - pts[j].second))
    }
    val radii = pts.map { hypot(it.first - right, it.second - up) }
    // Per axis, unbiased: the squared radii share two axes.
    val sigma = sqrt(radii.sumOf { it * it } / (2.0 * (pts.size - 1)))
    return GroupStats(pts.size, es, radii.average(), up, right, sigma, rangeM)
}
