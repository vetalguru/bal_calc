package org.vetalguru.balcalc

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flowOf

/** Sensors that read what the test says. */
class FakeSensors(
    var pressure: Double? = 987.6,
    var fix: GeoFix? = GeoFix(49.84, 24.03, 296.4, 6.0),
    var tilt: Tilt = Tilt(4.25, -1.34, 0.1),
    var headingMagnetic: Double = 117.4,
    var declination: Double = 6.3,
) : PhoneSensors {
    override val hasBarometer = true
    override val hasLocation = true
    override val hasCompass = true
    override val hasTilt = true
    override suspend fun pressureHpa() = pressure
    override suspend fun location() = fix
    override fun tilt(): Flow<Tilt> = flowOf(tilt)
    override fun heading(at: GeoFix?): Flow<Heading> =
        flowOf(Heading(normalizeDeg(headingMagnetic + if (at != null) declination else 0.0), at != null, false))
}
