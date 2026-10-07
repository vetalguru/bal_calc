package org.vetalguru.balcalc.ui

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.unit_kmh
import org.vetalguru.balcalc.res.unit_mph
import org.vetalguru.balcalc.res.unit_mps

/** How wind speeds are shown and typed; the core always keeps m/s. */
enum class WindUnit(val key: String, val perMps: Double, val label: StringResource) {
    MPS("mps", 1.0, Res.string.unit_mps),
    KMH("kmh", 3.6, Res.string.unit_kmh),
    MPH("mph", 3600.0 / 1609.344, Res.string.unit_mph),
    ;

    companion object {
        fun of(key: String) = entries.firstOrNull { it.key == key } ?: MPS
    }
}

/** The fastest wind the app takes (m/s). */
const val MAX_WIND_MPS = 40.0

/** A wind speed in [unit], with its unit: "18 km/h". */
@Composable
fun windSpeedText(mps: Double, unit: WindUnit): String = "${formatNumber(mps * unit.perMps, 1)} ${stringResource(unit.label)}"

/** The unit list a wind-speed field opens when its unit is tapped. */
@Composable
fun windUnitMenu(onUnit: (WindUnit) -> Unit) =
    UnitMenu(WindUnit.entries.map { it.key to stringResource(it.label) }) { onUnit(WindUnit.of(it)) }

/**
 * A wind speed typed in [unit] (tap the unit to change it), kept in m/s;
 * with − and + of one [unit] when [steps].
 */
@Composable
fun WindSpeedField(
    label: String,
    mps: Double,
    onMps: (Double) -> Unit,
    unit: WindUnit,
    onUnit: (WindUnit) -> Unit,
    modifier: Modifier = Modifier,
    tag: String? = null,
    steps: Boolean = true,
    fieldMaxWidth: Dp = Dp.Unspecified,
    stepWidth: Dp = 50.dp,
    hint: String? = null,
    maxMps: Double = MAX_WIND_MPS,
) {
    val shown = mps * unit.perMps
    val max = maxMps * unit.perMps
    val edited = { v: Double -> onMps((v / unit.perMps).coerceIn(0.0, maxMps)) }
    val menu = windUnitMenu(onUnit)
    if (steps) {
        StepperField(label, shown, edited, modifier, stringResource(unit.label), from = 0.0, to = max, tag = tag,
            unitMenu = menu, fieldMaxWidth = fieldMaxWidth, stepWidth = stepWidth, hint = hint)
    } else {
        NumberField(label, shown, edited, modifier, stringResource(unit.label), from = 0.0, to = max, tag = tag, unitMenu = menu, hint = hint)
    }
}
