package org.vetalguru.balcalc

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

class CorrectionFormatTest {
    private fun mrad(angle: Double, click: Double?, round: Boolean = false, rangeM: Double = 600.0) =
        formatCorrection(angle, click, moa = false, rangeM, round, "UP", "DOWN", "MOA", "cm")

    @Test
    fun exactWithTheOtherUnitAndCentimetres() {
        val c = mrad(5.43, 0.1)
        assertEquals("UP", c.direction)
        assertEquals("5.43", c.value)
        assertEquals(54, c.clicks)
        // 5.43 mrad = 18.67 MOA; at 600 m, 0.1 cm per mrad per metre: 325.8 cm.
        assertEquals("18.67 MOA · 325.8 cm", c.second)
    }

    @Test
    fun roundedToWholeClicks() {
        val c = mrad(5.43, 0.1, round = true)
        assertEquals("5.40", c.value) // 54 clicks of 0.1
        assertEquals("18.56 MOA · 324.0 cm", c.second)
    }

    @Test
    fun theOtherWayAndNothing() {
        assertEquals("DOWN", mrad(-0.3, 0.1).direction)
        assertEquals("0.30", mrad(-0.3, 0.1).value)
        assertEquals("", mrad(0.001, 0.1).direction)
    }

    @Test
    fun withoutAScopeThereAreNoClicksAndNoRounding() {
        val c = mrad(1.234, null, round = true)
        assertNull(c.clicks)
        assertEquals("1.23", c.value)
    }

    @Test
    fun moaMainUnit() {
        // 1/4 MOA clicks: 18.67 MOA is 75 clicks.
        val c = formatCorrection(18.67, 0.25, moa = true, 300.0, false, "UP", "DOWN", "MRAD", "cm")
        assertEquals("18.67", c.value)
        assertEquals("5.43 MRAD · 162.9 cm", c.second)
    }
}
