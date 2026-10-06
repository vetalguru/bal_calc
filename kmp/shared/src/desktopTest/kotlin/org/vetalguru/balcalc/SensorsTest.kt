package org.vetalguru.balcalc

import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin
import kotlin.test.Test
import kotlin.test.assertEquals

class SensorsTest {
    /** What a phone on a rifle at these angles reads: the up direction in its frame. */
    private fun upInPhone(lookDeg: Double, cantDeg: Double): Triple<Double, Double, Double> {
        val l = lookDeg * PI / 180
        val c = cantDeg * PI / 180
        // Pitch the top edge up by `look`, then turn the rifle clockwise by `cant`.
        val y = 9.81 * sin(l)
        val horizontal = 9.81 * cos(l)
        return Triple(-horizontal * sin(c), y, horizontal * cos(c))
    }

    @Test
    fun levelPhoneReadsZero() {
        val (look, cant) = tiltFromGravity(0.0, 0.0, 9.81)
        assertEquals(0.0, look, 1e-12)
        assertEquals(0.0, cant, 1e-12)
    }

    @Test
    fun anglesComeBack() {
        for (look in listOf(-25.0, -5.0, 0.0, 7.5, 30.0)) {
            for (cant in listOf(-10.0, -2.0, 0.0, 3.0, 15.0)) {
                val (x, y, z) = upInPhone(look, cant)
                val (l, c) = tiltFromGravity(x, y, z)
                assertEquals(look, l, 1e-9, "look $look cant $cant")
                assertEquals(cant, c, 1e-9, "look $look cant $cant")
            }
        }
    }

    @Test
    fun uphillAndClockwiseArePositive() {
        // Top edge raised: up leans towards +y. Right edge lowered: up leans to -x.
        assertEquals(true, tiltFromGravity(0.0, 2.0, 9.6).first > 0)
        assertEquals(true, tiltFromGravity(-2.0, 0.0, 9.6).second > 0)
    }

    @Test
    fun azimuthsWrap() {
        assertEquals(10.0, normalizeDeg(370.0), 1e-12)
        assertEquals(350.0, normalizeDeg(-10.0), 1e-12)
    }

    @Test
    fun steadiness() {
        val (mean, spread) = steady(listOf(1.0, 1.2, 0.8, 1.0))
        assertEquals(1.0, mean, 1e-12)
        assertEquals(0.2, spread, 1e-12)
    }
}
