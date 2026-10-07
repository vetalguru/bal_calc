package org.vetalguru.balcalc.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.core.Solution
import org.vetalguru.balcalc.core.Warning
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** Velocity, time of flight, energy, Mach and stability in one line; the rest on "More". */
@Composable
internal fun DetailsLine(sol: Solution, onMore: () -> Unit) {
    val parts = listOf(
        "${sol.velocity.fixed(0)} ${stringResource(Res.string.unit_mps)}",
        "${sol.time.fixed(2)} ${stringResource(Res.string.unit_s)}",
        "${sol.energy.fixed(0)} ${stringResource(Res.string.unit_j)}",
        "M ${sol.mach.fixed(2)}",
    ) + if (sol.stability > 0) listOf("Sg ${sol.stability.fixed(2)}") else emptyList()
    Row(
        Modifier.fillMaxWidth().clickable(onClick = onMore).padding(horizontal = 16.dp).testTag("detailsLine"),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            parts.joinToString("  ·  "),
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            maxLines = 2,
            modifier = Modifier.weight(1f),
        )
        Text("›", fontSize = 22.sp, color = MaterialTheme.colorScheme.primary)
    }
}

@Composable
internal fun Details(sol: Solution, targetHeightCm: Double, wide: Boolean) {
    val m = stringResource(Res.string.unit_mps)
    val cm = stringResource(Res.string.unit_cm)
    val metres = stringResource(Res.string.unit_m)
    val items = listOf(
        Triple(stringResource(Res.string.velocity), "${sol.velocity.fixed(0)} $m", false),
        Triple(stringResource(Res.string.energy), "${sol.energy.fixed(0)} ${stringResource(Res.string.unit_j)}", false),
        Triple(stringResource(Res.string.time_of_flight), "${sol.time.fixed(3)} ${stringResource(Res.string.unit_s)}", false),
        Triple(stringResource(Res.string.mach), sol.mach.fixed(2), false),
        Triple(stringResource(Res.string.drop), "${sol.dropCm.fixed(1)} $cm", false),
        Triple(stringResource(Res.string.drift), "${sol.windageCm.fixed(1)} $cm", false),
        Triple(stringResource(Res.string.spin_drift), "${sol.spinDriftCm.fixed(1)} $cm", false),
        Triple(stringResource(Res.string.muzzle_velocity), "${sol.muzzleVelocity.fixed(1)} $m", false),
        Triple(
            stringResource(Res.string.stability),
            if (sol.stability > 0) sol.stability.fixed(2) else "—",
            sol.stability > 0 && sol.stability < 1.3,
        ),
        Triple(
            stringResource(Res.string.apex),
            stringResource(Res.string.apex_value, sol.apexCm.fixed(1), sol.apexRangeM.roundToInt()),
            false,
        ),
        Triple(
            stringResource(Res.string.point_blank, targetHeightCm.roundToInt()),
            if (sol.pointBlankFarM > 0) {
                "${sol.pointBlankNearM.roundToInt()}–${sol.pointBlankFarM.roundToInt()} $metres"
            } else {
                "—"
            },
            false,
        ),
        Triple(stringResource(Res.string.density_altitude), "${sol.densityAltitudeM.roundToInt()} $metres", false),
    )
    Card(Modifier.fillMaxWidth().padding(horizontal = 12.dp)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            items.chunked(if (wide) 4 else 2).forEach { row ->
                Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                    row.forEach { (label, value, warn) -> Detail(label, value, warn, Modifier.weight(1f)) }
                    repeat((if (wide) 4 else 2) - row.size) { Spacer(Modifier.weight(1f)) }
                }
            }
        }
    }
}

/** What the shooter should know before trusting the numbers; nothing when all is well. */
@Composable
internal fun Warnings(warnings: List<Warning>) {
    if (warnings.isEmpty()) return
    Card(
        Modifier.fillMaxWidth().padding(horizontal = 12.dp).testTag("warnings"),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.errorContainer,
            contentColor = MaterialTheme.colorScheme.onErrorContainer,
        ),
    ) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            warnings.forEach { w ->
                val text = warningText(w) ?: return@forEach
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("⚠", fontWeight = FontWeight.Bold)
                    Text(text, modifier = Modifier.testTag("warning_${w.code}"))
                }
            }
        }
    }
}

@Composable
private fun warningText(w: Warning): String? {
    val signed = { v: Double, d: Int -> (if (v > 0) "+" else "") + v.fixed(d) }
    return when (w.code) {
        "unstable" -> stringResource(Res.string.warn_unstable, w.value.fixed(2))
        "lowStability" -> stringResource(Res.string.warn_low_stability, w.value.fixed(2))
        "subsonic" -> stringResource(Res.string.warn_subsonic, w.value.fixed(2))
        "transonic" -> stringResource(Res.string.warn_transonic, w.value.fixed(2))
        "zeroTemperature" -> stringResource(Res.string.warn_zero_temperature, signed(w.value, 0))
        "zeroPressure" -> stringResource(Res.string.warn_zero_pressure, signed(w.value, 0))
        "staleWeather" -> stringResource(Res.string.warn_stale_weather, w.value.roundToInt())
        else -> null // a newer core: not known to this app yet
    }
}
