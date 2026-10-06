package org.vetalguru.balcalc

import kotlin.math.sqrt
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

class GroupAnalysisTest {
    // 10 px per cm: the scale points are 100 px apart, said to be 10 cm.
    private val a = PhotoPoint(0.0, 0.0)
    private val b = PhotoPoint(100.0, 0.0)
    private val aim = PhotoPoint(500.0, 500.0)

    @Test
    fun squareGroup() {
        // Four hits on a 4 cm square centred 3 cm above and 2 cm right of the aim.
        val hits = listOf(
            PhotoPoint(500.0, 450.0), PhotoPoint(540.0, 450.0),
            PhotoPoint(500.0, 490.0), PhotoPoint(540.0, 490.0),
        )
        val s = analyzeGroup(hits, aim, a, b, 10.0, 100.0)!!
        assertEquals(4, s.shots)
        assertEquals(sqrt(32.0), s.extremeSpreadCm, 1e-9) // the diagonal
        assertEquals(3.0, s.centreUpCm, 1e-9)
        assertEquals(2.0, s.centreRightCm, 1e-9)
        assertEquals(sqrt(8.0), s.meanRadiusCm, 1e-9)
        // Radii^2 = 8 each: sigma^2 = 4 * 8 / (2 * 3).
        assertEquals(sqrt(32.0 / 6.0), s.sigmaCm, 1e-9)
        // At 100 m, 1 cm = 0.1 MRAD = 0.3438 MOA.
        assertEquals(0.1 * sqrt(32.0), s.mrad(s.extremeSpreadCm), 1e-9)
        assertEquals(s.moa(s.sigmaCm * 3.07), s.equivalentFiveShotMoa, 1e-12)
    }

    @Test
    fun belowAndLeftAreNegative() {
        val hits = listOf(PhotoPoint(480.0, 520.0), PhotoPoint(480.0, 540.0))
        val s = analyzeGroup(hits, aim, a, b, 10.0, 300.0)!!
        assertEquals(-3.0, s.centreUpCm, 1e-9)
        assertEquals(-2.0, s.centreRightCm, 1e-9)
        assertEquals(2.0, s.extremeSpreadCm, 1e-9)
    }

    @Test
    fun needsTwoHitsAndAScale() {
        assertNull(analyzeGroup(listOf(PhotoPoint(1.0, 1.0)), aim, a, b, 10.0, 100.0))
        assertNull(analyzeGroup(listOf(aim, aim), aim, a, a, 10.0, 100.0))
    }
}
