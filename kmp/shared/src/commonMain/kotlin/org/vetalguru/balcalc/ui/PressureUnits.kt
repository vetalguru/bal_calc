package org.vetalguru.balcalc.ui

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.unit_hpa
import org.vetalguru.balcalc.res.unit_inhg
import org.vetalguru.balcalc.res.unit_mmhg

/** How pressures are shown and typed; the core always keeps hPa. */
enum class PressureUnit(val key: String, val perHpa: Double, val label: StringResource, val decimals: Int) {
    HPA("hpa", 1.0, Res.string.unit_hpa, 0),
    MMHG("mmhg", 760.0 / 1013.25, Res.string.unit_mmhg, 0),
    INHG("inhg", 1.0 / 33.8638866667, Res.string.unit_inhg, 2),
    ;

    companion object {
        fun of(key: String) = entries.firstOrNull { it.key == key } ?: HPA
    }
}

/** A pressure in [unit], with its unit: "760 mmHg". */
@Composable
fun pressureText(hpa: Double, unit: PressureUnit): String =
    "${formatNumber(hpa * unit.perHpa, unit.decimals)} ${stringResource(unit.label)}"

/** The unit list a pressure field opens when its unit is tapped. */
@Composable
fun pressureUnitMenu(onUnit: (PressureUnit) -> Unit) =
    UnitMenu(PressureUnit.entries.map { it.key to stringResource(it.label) }) { onUnit(PressureUnit.of(it)) }

/** A pressure typed in [unit] (tap the unit to change it), kept in hPa; [fromHpa]..[toHpa] in hPa. */
@Composable
fun PressureField(
    label: String,
    hpa: Double,
    onHpa: (Double) -> Unit,
    unit: PressureUnit,
    onUnit: (PressureUnit) -> Unit,
    modifier: Modifier = Modifier,
    fromHpa: Double = 300.0,
    toHpa: Double = 1200.0,
    tag: String? = null,
    hint: String? = null,
    decimals: Int = unit.decimals,
) {
    NumberField(
        label, hpa * unit.perHpa, { v -> onHpa(v / unit.perHpa) }, modifier, stringResource(unit.label), decimals,
        fromHpa * unit.perHpa, toHpa * unit.perHpa, tag, pressureUnitMenu(onUnit), hint,
    )
}
