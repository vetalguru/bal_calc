package org.vetalguru.balcalc

import kotlin.test.Test
import kotlin.test.assertEquals

class MarkRangesTest {
    // A made-up straight curve: 1 mrad more every 100 m from 100 m (0 mrad).
    private val curve = (1..10).map { k -> k * 100.0 to (k - 1) * 1.0 }

    @Test
    fun milMarksWithNothingDialled() {
        val r = markRanges(curve, dialed = 0.0, angleUnit = "mrad", reticleUnit = "mrad", step = 1.0, last = 3.0)
        assertEquals(listOf(1.0 to 200.0, 2.0 to 300.0, 3.0 to 400.0), r)
    }

    @Test
    fun dialledElevationMovesTheMarksOut() {
        // 2.5 dialled: the 1 mark is where 3.5 is needed, between 400 and 500 m.
        val r = markRanges(curve, dialed = 2.5, angleUnit = "mrad", reticleUnit = "mrad", step = 1.0, last = 1.0)
        assertEquals(450.0, r.single().second, 1e-9)
    }

    @Test
    fun moaReticleAndSecondFocalPlane() {
        // A 2 MOA mark is 0.5818 mrad; at half the reference zoom it is twice that.
        val r = markRanges(curve, dialed = 0.0, angleUnit = "mrad", reticleUnit = "moa", step = 2.0, last = 2.0, scale = 2.0)
        assertEquals(100.0 + 2 * 0.581776417 * 100, r.single().second, 1e-6)
    }

    @Test
    fun theMuzzleHumpIsNotAMark() {
        // Near the muzzle the bullet is below the sight line: lots of elevation.
        val real = listOf(5.0 to 9.0, 50.0 to 0.5, 100.0 to 0.0) + curve.drop(1)
        assertEquals(200.0, markRanges(real, 0.0, "mrad", "mrad", 1.0, 1.0).single().second, 1e-9)
    }

    @Test
    fun marksBeyondTheCurveAreLeftOut() {
        assertEquals(9, markRanges(curve, 0.0, "mrad", "mrad", 1.0, 20.0).size)
    }
}
