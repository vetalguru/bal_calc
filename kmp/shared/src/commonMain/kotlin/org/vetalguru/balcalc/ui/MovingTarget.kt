package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableDoubleStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.roundToInt
import kotlin.time.TimeSource
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.core.Solution
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

private const val KMH = 3.6 // km/h per m/s

/** The fastest target the app takes: 150 km/h. */
private const val MAX_TARGET_MPS = 150.0 / KMH

/**
 * Speed of a target that covered `distance` in `seconds`: metres, or an
 * angle on the reticle (MRAD/MOA) seen at `rangeM`.
 */
fun stopwatchSpeed(distance: Double, unit: String, rangeM: Double, seconds: Double): Double {
    if (seconds <= 0.0) return 0.0
    val metres = when (unit) {
        "mrad" -> distance * rangeM / 1000.0
        "moa" -> distance * rangeM * PI / (180.0 * 60.0)
        else -> distance
    }
    return metres / seconds
}

/** Speed, direction and angle of a moving target, and the lead it needs. */
@Composable
fun MovingTargetCard(model: AppModel, sol: Solution, unit: String) {
    val c = model.state.conditions
    val speedUnit = WindUnit.of(c.targetSpeedUnit)
    // The heading as the shooter sees it: a side and an angle to the line of fire.
    val right = c.targetHeadingDeg <= 180.0
    val angle = if (right) c.targetHeadingDeg else 360.0 - c.targetHeadingDeg
    fun heading(toRight: Boolean, a: Double) = if (toRight) a else (360.0 - a) % 360.0
    var timing by remember { mutableStateOf(false) }
    if (timing) {
        StopwatchDialog(c.targetRangeM, onDismiss = { timing = false }) { mps ->
            timing = false
            model.updateConditions { it.copy(targetSpeedMps = mps) }
        }
    }

    Card(Modifier.fillMaxWidth().padding(horizontal = 12.dp).testTag("movingTarget")) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(stringResource(Res.string.moving_target), fontWeight = FontWeight.Bold)
            // The unit is in the field: tap it for km/h, m/s or mph.
            WindSpeedField(
                stringResource(Res.string.target_speed), c.targetSpeedMps,
                { v -> model.updateConditions { it.copy(targetSpeedMps = v) } },
                speedUnit, { u -> model.updateConditions { it.copy(targetSpeedUnit = u.key) } },
                tag = "targetSpeed", fieldMaxWidth = 200.dp, maxMps = MAX_TARGET_MPS,
            )
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilterChip(
                    selected = !right,
                    onClick = { model.updateConditions { it.copy(targetHeadingDeg = heading(false, angle)) } },
                    label = { Text(stringResource(Res.string.moves_left)) },
                    modifier = Modifier.testTag("movesLeft"),
                )
                FilterChip(
                    selected = right,
                    onClick = { model.updateConditions { it.copy(targetHeadingDeg = heading(true, angle)) } },
                    label = { Text(stringResource(Res.string.moves_right)) },
                    modifier = Modifier.testTag("movesRight"),
                )
            }
            StepperField(
                stringResource(Res.string.target_angle), angle,
                { a -> model.updateConditions { it.copy(targetHeadingDeg = heading(right, a)) } },
                unit = stringResource(Res.string.unit_deg), step = 15.0, from = 0.0, to = 180.0, tag = "targetAngle",
                fieldMaxWidth = 200.dp, hint = stringResource(Res.string.target_angle_hint),
                hintPicture = { HeadingPicture(right) }, decimals = 0,
            )
            OutlinedButton(onClick = { timing = true }, modifier = Modifier.testTag("stopwatch")) {
                Text(stringResource(Res.string.stopwatch))
            }

            if (sol.ok && sol.hasLead) {
                val side = @Composable { v: Double -> sideSuffix(v) }
                val clicks = @Composable { v: Double -> if (sol.hasScope) clicksSuffix(v) else "" }
                Text(
                    stringResource(Res.string.lead_value, "${abs(sol.lead).fixed(2)} $unit${side(sol.lead)}${clicks(sol.leadClicks)}"),
                    style = MaterialTheme.typography.titleMedium,
                    modifier = Modifier.testTag("lead"),
                )
                Text(
                    stringResource(
                        Res.string.lead_total,
                        "${abs(sol.leadTotalWindage).fixed(2)} $unit${side(sol.leadTotalWindage)}${clicks(sol.leadTotalWindageClicks)}",
                    ),
                    modifier = Modifier.testTag("leadTotal"),
                )
                Text(
                    stringResource(Res.string.lead_moves, (abs(sol.leadCm) / 100).fixed(1)),
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                if (abs(sol.leadRangeM - sol.rangeM) >= 1.0) {
                    Text(
                        stringResource(
                            Res.string.lead_meet,
                            sol.leadRangeM.roundToInt(),
                            "${sol.leadElevation.fixed(2)} $unit${clicks(sol.leadElevationClicks)}",
                        ),
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }
    }
}

/** Times the target between two marks and turns that into its speed. */
@Composable
private fun StopwatchDialog(rangeM: Double, onDismiss: () -> Unit, onSpeed: (Double) -> Unit) {
    var distance by remember { mutableDoubleStateOf(10.0) }
    var unit by remember { mutableStateOf("m") }
    var start by remember { mutableStateOf<TimeSource.Monotonic.ValueTimeMark?>(null) }
    var seconds by remember { mutableDoubleStateOf(0.0) }
    val mps = stopwatchSpeed(distance, unit, rangeM, seconds)
    val running = start != null

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(Res.string.stopwatch)) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(stringResource(Res.string.stopwatch_hint), color = MaterialTheme.colorScheme.onSurfaceVariant)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    NumberField(
                        stringResource(Res.string.stopwatch_distance), distance, { distance = it },
                        Modifier.weight(1f), from = 0.0, to = 1000.0, tag = "stopwatchDistance",
                    )
                    ChoiceField(
                        "",
                        listOf(
                            "m" to stringResource(Res.string.unit_m),
                            "mrad" to stringResource(Res.string.unit_mrad),
                            "moa" to stringResource(Res.string.unit_moa),
                        ),
                        unit, { unit = it }, Modifier.weight(0.8f),
                    )
                }
                Button(
                    onClick = {
                        val mark = start
                        if (mark != null) {
                            seconds = mark.elapsedNow().inWholeMilliseconds / 1000.0 // exact at the stop
                            start = null
                        } else {
                            seconds = 0.0
                            start = TimeSource.Monotonic.markNow()
                        }
                    },
                    modifier = Modifier.fillMaxWidth().testTag("stopwatchToggle"),
                ) {
                    Text(stringResource(if (running) Res.string.stopwatch_stop else Res.string.stopwatch_start))
                }
                if (running) {
                    Text("…", style = MaterialTheme.typography.titleMedium)
                } else Text(
                    stringResource(
                        Res.string.stopwatch_result,
                        seconds.fixed(2),
                        "${(mps * KMH).fixed(1)} ${stringResource(Res.string.unit_kmh)} · ${mps.fixed(1)} ${stringResource(Res.string.unit_mps)}",
                    ),
                    style = MaterialTheme.typography.titleMedium,
                    modifier = Modifier.testTag("stopwatchResult"),
                )
            }
        },
        confirmButton = {
            TextButton(onClick = { onSpeed(mps) }, enabled = !running && mps > 0.0, modifier = Modifier.testTag("stopwatchApply")) {
                Text(stringResource(Res.string.apply))
            }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(Res.string.cancel)) } },
    )
}

/** " RIGHT" / " LEFT" after a horizontal correction, nothing near zero. */
@Composable
private fun sideSuffix(v: Double): String = when {
    abs(v) < 0.005 -> ""
    v > 0 -> " " + stringResource(Res.string.right)
    else -> " " + stringResource(Res.string.left)
}

@Composable
private fun clicksSuffix(v: Double): String = ", " + stringResource(Res.string.clicks, abs(v).roundToInt())
