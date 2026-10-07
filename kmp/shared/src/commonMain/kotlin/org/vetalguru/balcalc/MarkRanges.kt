package org.vetalguru.balcalc

/** mrad in one unit of an angle: "moa" or anything else (mrad). */
fun mradPer(unit: String): Double = if (unit == "moa") 1.0 / MOA_PER_MRAD else 1.0

/**
 * The range at each mark below the centre of a reticle: where the bullet
 * meets the line of a mark when the turret holds [dialed].
 *
 * [curve] is (range m, elevation needed) in [angleUnit], increasing range;
 * marks are every [step] in [reticleUnit] down to [last]; [scale] is the
 * subtension scale (SFP: how many times larger a mark is than its value at
 * this zoom). A mark whose hold the curve does not reach is left out.
 * Only the far part counts, from where the curve is lowest: near the muzzle
 * the bullet is below the line of sight and needs a lot of elevation too.
 * Returns (mark value in reticle units, range m).
 */
fun markRanges(
    curve: List<Pair<Double, Double>>,
    dialed: Double,
    angleUnit: String,
    reticleUnit: String,
    step: Double,
    last: Double,
    scale: Double = 1.0,
): List<Pair<Double, Double>> {
    val out = mutableListOf<Pair<Double, Double>>()
    val perMark = mradPer(reticleUnit) * scale / mradPer(angleUnit) // angle units per reticle unit
    val lowest = curve.indices.minByOrNull { curve[it].second } ?: return out
    var mark = step
    while (mark <= last + 1e-9) {
        val hold = dialed + mark * perMark
        // The first range past the lowest point where the needed elevation reaches the mark's hold.
        val i = (lowest until curve.size).firstOrNull { curve[it].second >= hold } ?: -1
        if (i > lowest) {
            val (r0, e0) = curve[i - 1]
            val (r1, e1) = curve[i]
            out += mark to r0 + (r1 - r0) * (hold - e0) / (e1 - e0)
        }
        mark += step
    }
    return out
}
