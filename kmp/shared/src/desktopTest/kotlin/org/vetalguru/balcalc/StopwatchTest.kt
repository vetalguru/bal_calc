package org.vetalguru.balcalc

import kotlin.math.PI
import kotlin.test.Test
import kotlin.test.assertEquals
import org.vetalguru.balcalc.ui.stopwatchSpeed

class StopwatchTest {
    @Test
    fun metresPerSecond() = assertEquals(4.0, stopwatchSpeed(10.0, "m", 500.0, 2.5), 1e-12)

    @Test
    fun reticleMarksAtTheRange() {
        // 2 MRAD at 400 m = 0.8 m.
        assertEquals(0.8, stopwatchSpeed(2.0, "mrad", 400.0, 1.0), 1e-12)
        // 6 MOA at 300 m: 6 * 300 * pi / 10800.
        assertEquals(6 * 300 * PI / 10800, stopwatchSpeed(6.0, "moa", 300.0, 1.0), 1e-12)
    }

    @Test
    fun noTimeNoSpeed() = assertEquals(0.0, stopwatchSpeed(10.0, "m", 300.0, 0.0))
}
