package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import kotlin.math.roundToInt
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.core.DsfPointIn
import org.vetalguru.balcalc.core.DsfResult
import org.vetalguru.balcalc.coreText
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/**
 * The drag scale factor table of this rifle and cartridge: edited by hand,
 * or fitted from the hits logged in the transonic part.
 */
@Composable
fun DsfSection(model: AppModel, hasShots: Boolean, unit: String) {
    val stored = model.solution.dsf
    val scope = rememberCoroutineScope()
    // The table being edited; follows the stored one after each save.
    var rows by remember { mutableStateOf(stored) }
    LaunchedEffect(stored) { rows = stored }
    var error by remember { mutableStateOf<String?>(null) }
    var fit by remember { mutableStateOf<DsfResult?>(null) }
    val edited = rows != stored

    Section(stringResource(Res.string.dsf_title)) {
        Text(stringResource(Res.string.dsf_hint), color = MaterialTheme.colorScheme.onSurfaceVariant)
        if (rows.isEmpty()) Text(stringResource(Res.string.dsf_none), modifier = Modifier.testTag("dsfNone"))
        rows.forEachIndexed { i, p ->
            fun change(f: (DsfPointIn) -> DsfPointIn) { rows = rows.mapIndexed { j, old -> if (j == i) f(old) else old } }
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                NumberField(stringResource(Res.string.dsf_mach), p.mach, { v -> change { it.copy(mach = v) } },
                    Modifier.weight(1f), decimals = 2, from = 0.0, to = 5.0, tag = "dsfMach$i")
                NumberField(stringResource(Res.string.dsf_factor), p.factor, { v -> change { it.copy(factor = v) } },
                    Modifier.weight(1f), decimals = 3, from = 0.5, to = 2.0, tag = "dsfFactor$i")
                TextButton(onClick = { rows = rows.filterIndexed { j, _ -> j != i } }) { Text("✕") }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(
                onClick = { rows = rows + DsfPointIn(mach = ((rows.minOfOrNull { it.mach } ?: 1.2) - 0.1).coerceAtLeast(0.5), factor = 1.0) },
                modifier = Modifier.testTag("dsfAdd"),
            ) { Text(stringResource(Res.string.dsf_add)) }
            if (edited) {
                Button(onClick = { scope.launch { error = model.applyDsf(rows) } }, modifier = Modifier.testTag("dsfSave")) {
                    Text(stringResource(Res.string.dsf_save))
                }
            }
        }
        error?.let { Text(coreText(it), color = MaterialTheme.colorScheme.error) }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(
                onClick = { scope.launch { fit = runCatching { model.computeDsf() }.getOrNull() } },
                enabled = hasShots, modifier = Modifier.testTag("dsfFit"),
            ) { Text(stringResource(Res.string.dsf_fit)) }
            TextButton(onClick = { model.resetDsf(); fit = null }, enabled = stored.isNotEmpty(), modifier = Modifier.testTag("dsfReset")) {
                Text(stringResource(Res.string.dsf_reset))
            }
        }
        fit?.let { r ->
            DsfFitCard(r, unit) {
                scope.launch {
                    error = model.applyDsf()
                    if (error == null) fit = null
                }
            }
        }
    }
}

@Composable
private fun DsfFitCard(r: DsfResult, unit: String, onApply: () -> Unit) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            if (!r.ok) {
                Text(coreText(r.error), color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("dsfFitError"))
                return@Column
            }
            Text(
                r.points.joinToString("   ") { "M ${it.mach.fixed(2)}: ×${it.factor.fixed(3)}" },
                fontWeight = FontWeight.Bold, modifier = Modifier.testTag("dsfFitPoints"),
            )
            Text(stringResource(Res.string.avg_miss, r.rmsBefore.fixed(2), r.rmsAfter.fixed(2), unit))
            r.shots.forEach { s ->
                Text(
                    if (s.limited) {
                        stringResource(Res.string.dsf_limited, s.rangeM.roundToInt(), s.mach.fixed(2))
                    } else if (s.mach >= 1.3) {
                        stringResource(Res.string.dsf_supersonic, s.rangeM.roundToInt(), s.mach.fixed(2))
                    } else {
                        stringResource(Res.string.dsf_shot, s.rangeM.roundToInt(), s.mach.fixed(2), s.observed.fixed(2), s.before.fixed(2), s.after.fixed(2))
                    },
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Button(onClick = onApply, modifier = Modifier.testTag("dsfApply")) { Text(stringResource(Res.string.apply)) }
        }
    }
}
