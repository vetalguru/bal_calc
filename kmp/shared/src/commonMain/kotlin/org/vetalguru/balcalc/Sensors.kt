package org.vetalguru.balcalc

import kotlin.math.PI
import kotlin.math.asin
import kotlin.math.atan2
import kotlin.math.sqrt
import kotlinx.coroutines.flow.Flow

/** A position from the phone's location (GPS). */
data class GeoFix(
    val latitudeDeg: Double,
    val longitudeDeg: Double,
    /** Above mean sea level; null when the phone has no altitude for it. */
    val altitudeM: Double?,
    /** Horizontal accuracy, m. */
    val accuracyM: Double,
)

/**
 * The phone laid on the rifle (screen up, top towards the target): the
 * shot angle (uphill +) and the cant (clockwise as the shooter sees it +),
 * averaged over the last second, and how much they moved meanwhile.
 */
data class Tilt(val lookAngleDeg: Double, val cantDeg: Double, val unsteadyDeg: Double)

/** Where the top of the phone points, clockwise from north. */
data class Heading(
    val azimuthDeg: Double,
    /** From true north (with the magnetic declination of [GeoFix]); else magnetic. */
    val trueNorth: Boolean,
    /** The compass reports a poor calibration: wave the phone in a figure 8. */
    val needsCalibration: Boolean,
)

/** The phone's own sensors; null on a desktop (see [Platform.sensors]). */
interface PhoneSensors {
    val hasBarometer: Boolean
    val hasLocation: Boolean
    val hasCompass: Boolean
    val hasTilt: Boolean

    /** Station (absolute) pressure averaged over a second, hPa; null if it fails. */
    suspend fun pressureHpa(): Double?

    /** One fix, asking for the location permission first if needed; null if refused or none. */
    suspend fun location(): GeoFix?

    /** The rifle's angles while collected. */
    fun tilt(): Flow<Tilt>

    /** The compass while collected; true north when [at] is given. */
    fun heading(at: GeoFix?): Flow<Heading>
}

private fun deg(rad: Double) = rad * 180.0 / PI

/**
 * Angles of a phone lying on the rifle from what its accelerometer (or
 * gravity sensor) reads at rest: the up direction in the phone's frame
 * (x right, y to the top edge, z out of the screen).
 */
fun tiltFromGravity(x: Double, y: Double, z: Double): Pair<Double, Double> {
    val g = sqrt(x * x + y * y + z * z)
    if (g == 0.0) return 0.0 to 0.0
    // The top edge rises above the horizon by the shot angle; turning the
    // rifle clockwise lowers the phone's right edge (up then leans to -x).
    val look = deg(asin((y / g).coerceIn(-1.0, 1.0)))
    val cant = deg(atan2(-x, z))
    return look to cant
}

/** An azimuth in 0..360. */
fun normalizeDeg(deg: Double): Double = ((deg % 360.0) + 360.0) % 360.0

/** Mean and the largest distance from it, for a second of readings. */
fun steady(values: List<Double>): Pair<Double, Double> {
    if (values.isEmpty()) return 0.0 to 0.0
    val mean = values.average()
    return mean to values.maxOf { kotlin.math.abs(it - mean) }
}
