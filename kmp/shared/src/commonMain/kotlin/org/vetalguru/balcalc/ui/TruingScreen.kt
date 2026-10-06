package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableDoubleStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.coreText
import org.vetalguru.balcalc.core.Shot
import org.vetalguru.balcalc.core.TruingResult
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** Records the correction that actually hit, at the current conditions. */
@Composable
fun LogShotDialog(model: AppModel, rangeGuess: Double, elevationGuess: Double, onClose: () -> Unit) {
    var range by remember { mutableDoubleStateOf(rangeGuess) }
    var elevation by remember { mutableDoubleStateOf((elevationGuess * 100).roundToInt() / 100.0) }
    var withWindage by remember { mutableStateOf(false) }
    var windage by remember { mutableDoubleStateOf(0.0) }
    var notes by remember { mutableStateOf("") }
    var error by remember { mutableStateOf<String?>(null) }
    val scope = rememberCoroutineScope()
    val unit = stringResource(if (model.state.moa) Res.string.unit_moa else Res.string.unit_mrad)
    AlertDialog(
        onDismissRequest = onClose,
        title = { Text(stringResource(Res.string.log_hit)) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(stringResource(Res.string.log_note), color = MaterialTheme.colorScheme.onSurfaceVariant)
                NumberField(stringResource(Res.string.distance), range, { range = it }, Modifier.fillMaxWidth(), stringResource(Res.string.unit_m), 0, 10.0, 3000.0)
                NumberField(stringResource(Res.string.elevation_hit), elevation, { elevation = it }, Modifier.fillMaxWidth(), unit, 2, -100.0, 200.0, tag = "hitElevation")
                SwitchRow(stringResource(Res.string.also_windage), withWindage, { withWindage = it })
                if (withWindage) {
                    NumberField(stringResource(Res.string.windage_hit), windage, { windage = it }, Modifier.fillMaxWidth(), unit, 2, -100.0, 100.0)
                }
                TextInput(stringResource(Res.string.notes_hint), notes, { notes = it }, tag = "hitNotes")
                error?.let { Text(coreText(it), color = MaterialTheme.colorScheme.error) }
            }
        },
        confirmButton = {
            TextButton(onClick = {
                scope.launch {
                    error = model.logShot(range, elevation, if (withWindage) windage else null, notes)
                    if (error == null) onClose()
                }
            }, modifier = Modifier.testTag("saveHit")) { Text(stringResource(Res.string.save)) }
        },
        dismissButton = { TextButton(onClick = onClose) { Text(stringResource(Res.string.cancel)) } },
    )
}

/**
 * Shot log of the current rifle + cartridge, its point-of-impact shift and
 * truing (fitting muzzle velocity and drag to the corrections that hit).
 */
@Composable
fun TruingScreen(model: AppModel, onBack: () -> Unit) {
    val st = model.state
    val pair = st.currentPair
    val sol = model.solution
    var shots by remember { mutableStateOf(emptyList<Shot>()) }
    var result by remember { mutableStateOf<TruingResult?>(null) }
    var logging by remember { mutableStateOf(false) }
    var bcCalc by remember { mutableStateOf(false) }
    var offsetError by remember { mutableStateOf<String?>(null) }
    val scope = rememberCoroutineScope()
    LaunchedEffect(model.shotsRevision, st.currentProfileId, st.angleUnit) {
        shots = runCatching { model.shots() }.getOrDefault(emptyList())
        result = null
    }
    val unit = stringResource(if (st.moa) Res.string.unit_moa else Res.string.unit_mrad)
    val cm = stringResource(Res.string.unit_cm)
    val trued = sol.ok && (sol.velocityScale != 1.0 || sol.dragScale != 1.0)
    // Read here, not only inside BoxWithConstraints: its subcomposition alone
    // does not recompose for state written by the LaunchedEffect above.
    val shotList = shots
    val truing = result

    BoxWithConstraints(Modifier.fillMaxSize()) {
        val wide = maxWidth >= 560.dp
        Column(Modifier.fillMaxSize()) {
            Row(Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                TextButton(onClick = onBack) { Text(stringResource(Res.string.back)) }
                Text(stringResource(Res.string.shot_log), style = MaterialTheme.typography.titleLarge, maxLines = 1,
                    overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f))
                Button(onClick = { logging = true }, enabled = st.hasPair, modifier = Modifier.testTag("logHit")) {
                    Text(stringResource(Res.string.log_hit))
                }
            }
            Column(
                Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(vertical = 8.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                Text(
                    if (pair != null && st.hasPair) "${pair.rifleName} / ${pair.cartridgeName}" else stringResource(Res.string.choose_pair),
                    fontSize = 16.sp, fontWeight = FontWeight.Bold, modifier = Modifier.padding(horizontal = 12.dp),
                )
                Text(stringResource(Res.string.truing_intro), color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(horizontal = 12.dp))

                if (pair != null && st.hasPair) {
                    Section(stringResource(Res.string.poi_title, pair.zeroRangeM.roundToInt())) {
                        fun set(up: Double, right: Double) = scope.launch { offsetError = model.setZeroOffset(up, right) }
                        Fields(
                            wide,
                            { m -> NumberField(stringResource(Res.string.poi_up), pair.offsetUpCm, { set(it, pair.offsetRightCm) }, m, cm, from = -100.0, to = 100.0, tag = "offsetUp") },
                            { m -> NumberField(stringResource(Res.string.poi_right), pair.offsetRightCm, { set(pair.offsetUpCm, it) }, m, cm, from = -100.0, to = 100.0) },
                        )
                        offsetError?.let { Text(coreText(it), color = MaterialTheme.colorScheme.error) }
                    }
                }

                if (shotList.isEmpty()) {
                    Text(stringResource(Res.string.no_shots), textAlign = TextAlign.Center,
                        color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.fillMaxWidth().padding(12.dp))
                }
                shotList.forEach { s ->
                    Row(Modifier.fillMaxWidth().padding(horizontal = 4.dp).testTag("shotRow"), verticalAlignment = Alignment.CenterVertically) {
                        Checkbox(s.used, { model.setShotUsed(s.id, it) })
                        Column(Modifier.weight(1f)) {
                            Text(stringResource(Res.string.shot_line, s.rangeM.roundToInt(), s.observed.fixed(2), unit), fontSize = 16.sp)
                            Text(
                                listOfNotNull(
                                    s.predicted?.let { stringResource(Res.string.predicted, it.fixed(2)) },
                                    "${s.temperatureC.fixed(0)} °C",
                                    s.shotAt.replace("T", " ").replace("Z", ""),
                                    s.notes.ifEmpty { null },
                                ).joinToString("  ·  "),
                                color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1, overflow = TextOverflow.Ellipsis,
                            )
                        }
                        TextButton(onClick = { model.deleteShot(s.id) }) { Text("✕") }
                    }
                }

                Row(Modifier.padding(horizontal = 12.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = { scope.launch { result = runCatching { model.computeTruing() }.getOrNull() } },
                        enabled = shotList.isNotEmpty(), modifier = Modifier.testTag("computeTruing")) {
                        Text(stringResource(Res.string.calculate_truing))
                    }
                    TextButton(onClick = { model.resetTruing() }, enabled = trued, modifier = Modifier.testTag("resetTruing")) {
                        Text(stringResource(Res.string.reset_truing))
                    }
                }
                if (trued) {
                    Text(stringResource(Res.string.trued_now, sol.velocityScale.fixed(4), sol.dragScale.fixed(3)),
                        modifier = Modifier.padding(horizontal = 12.dp))
                }

                truing?.let { r -> TruingResultCard(r, unit) {
                    scope.launch {
                        val error = model.applyTruing()
                        result = if (error == null) null else r.copy(ok = false, error = error)
                    }
                } }

                if (st.hasPair) DsfSection(model, shotList.isNotEmpty(), unit)

                if (st.hasPair) {
                    OutlinedButton(onClick = { bcCalc = true }, modifier = Modifier.padding(horizontal = 12.dp).testTag("bcCalc")) {
                        Text(stringResource(Res.string.bc_calc))
                    }
                }
            }
        }
    }
    if (logging) {
        LogShotDialog(model, st.conditions.targetRangeM, sol.elevation) { logging = false }
    }
    if (bcCalc) BcCalculatorDialog(model) { bcCalc = false }
}

@Composable
private fun TruingResultCard(r: TruingResult, unit: String, onApply: () -> Unit) {
    Card(Modifier.fillMaxWidth().padding(horizontal = 12.dp)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            if (!r.ok) {
                Text(coreText(r.error), color = MaterialTheme.colorScheme.error)
                return@Column
            }
            Text(stringResource(Res.string.mv_change, r.velocityBefore.fixed(1), r.velocityAfter.fixed(1)),
                fontSize = 16.sp, fontWeight = FontWeight.Bold, modifier = Modifier.testTag("truingResult"))
            Text(if (r.dragFitted) stringResource(Res.string.drag_change, r.dragScale.fixed(3)) else stringResource(Res.string.drag_unchanged))
            Text(stringResource(Res.string.avg_miss, r.rmsBefore.fixed(2), r.rmsAfter.fixed(2), unit))
            r.points.forEach {
                Text(stringResource(Res.string.truing_point, it.rangeM.roundToInt(), it.observed.fixed(2), it.before.fixed(2), it.after.fixed(2)),
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Button(onClick = onApply, modifier = Modifier.testTag("applyTruing")) { Text(stringResource(Res.string.apply)) }
        }
    }
}
