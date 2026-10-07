package org.vetalguru.balcalc.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import kotlin.math.abs
import kotlin.math.roundToInt
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.core.TargetItem
import org.vetalguru.balcalc.coreText
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/**
 * The target card: named targets with their range, angle and wind, each
 * with its corrections; a tap makes one current. Edited as a list.
 */
@Composable
fun TargetCard(model: AppModel, targets: List<TargetItem>, unit: String) {
    var editing by remember { mutableStateOf(false) }
    if (editing) TargetsDialog(model, targets) { editing = false }
    val c = model.state.conditions
    val selected = MaterialTheme.colorScheme.primary.copy(alpha = 0.12f)
    Card(Modifier.fillMaxWidth().padding(horizontal = 12.dp).testTag("targetCard")) {
        Column(Modifier.padding(vertical = 8.dp)) {
            if (targets.isEmpty()) {
                Text(
                    stringResource(Res.string.targets_empty),
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                )
            }
            targets.forEachIndexed { i, t ->
                val current = abs(t.rangeM - c.targetRangeM) < 0.5 && abs(t.lookAngleDeg - c.lookAngleDeg) < 0.05 &&
                    abs(t.windSpeed - c.windSpeed) < 0.05 && c.windZones.isEmpty()
                Row(
                    Modifier.fillMaxWidth()
                        .then(if (current) Modifier.background(selected) else Modifier)
                        .clickable { model.targets.select(i) }
                        .padding(horizontal = 16.dp, vertical = 8.dp)
                        .testTag("target:${t.name}"),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Column(Modifier.weight(1f)) {
                        Text(t.name, fontWeight = FontWeight.Bold, maxLines = 1)
                        Text(
                            "${t.rangeM.roundToInt()} ${stringResource(Res.string.unit_m)} · ${t.lookAngleDeg.roundToInt()}° · " +
                                "${t.windSpeed.fixed(1)} ${stringResource(Res.string.unit_mps)} " +
                                stringResource(Res.string.wind_clock, clockHour(t.windFromDeg)),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            maxLines = 1,
                        )
                    }
                    if (t.ok) {
                        Column(horizontalAlignment = Alignment.End) {
                            Text("${if (t.elevation >= 0) "↑" else "↓"} ${abs(t.elevation).fixed(2)}", fontWeight = FontWeight.Bold)
                            Text(
                                "${if (t.windage >= 0) "→" else "←"} ${abs(t.windage).fixed(2)} $unit",
                                style = MaterialTheme.typography.bodySmall,
                            )
                        }
                    }
                }
                HorizontalDivider()
            }
            Row(Modifier.padding(horizontal = 12.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                TextButton(onClick = { editing = true }, modifier = Modifier.testTag("editTargets")) {
                    Text(stringResource(Res.string.targets_edit))
                }
            }
        }
    }
}

/** The list being edited: name, range, angle, wind speed and clock of each target. */
@Composable
private fun TargetsDialog(model: AppModel, initial: List<TargetItem>, onClose: () -> Unit) {
    val scope = rememberCoroutineScope()
    val rows = remember { mutableStateListOf<TargetItem>().apply { addAll(initial) } }
    var error by remember { mutableStateOf<String?>(null) }
    val c = model.state.conditions
    val defaultName = stringResource(Res.string.target_n, "%d")
    AlertDialog(
        onDismissRequest = onClose,
        title = { Text(stringResource(Res.string.targets)) },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                rows.forEachIndexed { i, t ->
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        OutlinedTextField(
                            t.name, { rows[i] = t.copy(name = it) },
                            label = { Text(stringResource(Res.string.situation_name)) },
                            singleLine = true,
                            modifier = Modifier.weight(1f).testTag("targetName$i"),
                        )
                        TextButton(onClick = { rows.removeAt(i) }, modifier = Modifier.testTag("targetRemove$i")) { Text("✕") }
                    }
                    Fields(
                        false,
                        { m -> NumberField(stringResource(Res.string.distance), t.rangeM, { rows[i] = rows[i].copy(rangeM = it) }, m,
                            stringResource(Res.string.unit_m), 0, 10.0, 3000.0, tag = "targetRange$i") },
                        { m -> NumberField(stringResource(Res.string.look_angle_short), t.lookAngleDeg, { rows[i] = rows[i].copy(lookAngleDeg = it) }, m,
                            "°", 0, -60.0, 60.0) },
                        { m -> NumberField(stringResource(Res.string.wind_speed), t.windSpeed, { rows[i] = rows[i].copy(windSpeed = it) }, m,
                            stringResource(Res.string.unit_mps), 1, 0.0, 40.0) },
                        { m ->
                            ChoiceField(
                                stringResource(Res.string.wind_from),
                                (1..12).map { h -> (h % 12) * 30.0 to stringResource(Res.string.wind_clock, h) },
                                (clockHour(t.windFromDeg) % 12) * 30.0, { rows[i] = rows[i].copy(windFromDeg = it) }, m,
                            )
                        },
                    )
                    HorizontalDivider()
                }
                if (rows.size < 20) {
                    OutlinedButton(onClick = {
                        // A new target starts as the current conditions.
                        rows.add(
                            TargetItem(
                                name = defaultName.replace("%d", (rows.size + 1).toString()),
                                rangeM = c.targetRangeM, lookAngleDeg = c.lookAngleDeg,
                                windSpeed = c.windSpeed, windFromDeg = c.windFromDeg,
                            ),
                        )
                    }, modifier = Modifier.testTag("addTarget")) { Text(stringResource(Res.string.target_add)) }
                }
                error?.let { Text(coreText(it), color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("targetsError")) }
            }
        },
        confirmButton = {
            Button(onClick = {
                scope.launch {
                    error = model.targets.save(rows.toList())
                    if (error == null) onClose()
                }
            }, modifier = Modifier.testTag("saveTargets")) { Text(stringResource(Res.string.save)) }
        },
        dismissButton = { TextButton(onClick = onClose) { Text(stringResource(Res.string.cancel)) } },
    )
}
