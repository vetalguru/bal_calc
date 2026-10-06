package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
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
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.core.BcCalc
import org.vetalguru.balcalc.core.CartridgeForm
import org.vetalguru.balcalc.coreText
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/**
 * The BC of the current cartridge from measurements: two chronograph
 * readings, or the elevation that hit at a range; it can be written into
 * the cartridge.
 */
@Composable
fun BcCalculatorDialog(model: AppModel, onClose: () -> Unit) {
    val scope = rememberCoroutineScope()
    val sol = model.solution
    var form by remember { mutableStateOf<CartridgeForm?>(null) }
    LaunchedEffect(model.state.currentCartridgeId) {
        form = runCatching { model.cartridgeForm(model.state.currentCartridgeId) }.getOrNull()
    }
    var hitMode by remember { mutableStateOf(false) }
    var table by remember { mutableStateOf("G7") }
    LaunchedEffect(form) { form?.let { if (it.dragTable == "G1" || it.dragTable == "G7") table = it.dragTable } }
    var vNear by remember { mutableDoubleStateOf(0.0) }
    var vFar by remember { mutableDoubleStateOf(0.0) }
    var distance by remember { mutableDoubleStateOf(100.0) }
    var range by remember { mutableDoubleStateOf(model.state.conditions.targetRangeM) }
    var elevation by remember { mutableDoubleStateOf(if (sol.ok) sol.elevation else 0.0) }
    var result by remember { mutableStateOf<BcCalc?>(null) }
    var saveError by remember { mutableStateOf<String?>(null) }
    val unit = stringResource(if (model.state.moa) Res.string.unit_moa else Res.string.unit_mrad)
    val mps = stringResource(Res.string.unit_mps)
    val m = stringResource(Res.string.unit_m)

    AlertDialog(
        onDismissRequest = onClose,
        title = { Text(stringResource(Res.string.bc_calc)) },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(stringResource(Res.string.bc_calc_hint), color = MaterialTheme.colorScheme.onSurfaceVariant)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    FilterChip(!hitMode, { hitMode = false; result = null }, { Text(stringResource(Res.string.bc_mode_chrono)) })
                    FilterChip(hitMode, { hitMode = true; result = null }, { Text(stringResource(Res.string.bc_mode_hit)) },
                        modifier = Modifier.testTag("bcModeHit"))
                }
                ChoiceField(stringResource(Res.string.bc_table), listOf("G1" to "G1", "G7" to "G7"), table,
                    { table = it; result = null }, Modifier.fillMaxWidth())
                if (!hitMode) {
                    NumberField(stringResource(Res.string.bc_v_near), vNear, { vNear = it }, Modifier.fillMaxWidth(), mps,
                        decimals = 1, from = 0.0, to = 2000.0, tag = "bcVNear")
                    NumberField(stringResource(Res.string.bc_v_far), vFar, { vFar = it }, Modifier.fillMaxWidth(), mps,
                        decimals = 1, from = 0.0, to = 2000.0, tag = "bcVFar")
                    NumberField(stringResource(Res.string.bc_distance), distance, { distance = it }, Modifier.fillMaxWidth(), m,
                        decimals = 0, from = 1.0, to = 2000.0, tag = "bcDistance")
                } else {
                    Text(stringResource(Res.string.bc_hit_note), color = MaterialTheme.colorScheme.onSurfaceVariant)
                    NumberField(stringResource(Res.string.distance), range, { range = it }, Modifier.fillMaxWidth(), m,
                        decimals = 0, from = 10.0, to = 3000.0, tag = "bcRange")
                    NumberField(stringResource(Res.string.bc_hit_elevation), elevation, { elevation = it }, Modifier.fillMaxWidth(), unit,
                        decimals = 2, from = -50.0, to = 200.0, tag = "bcElevation")
                }
                Button(
                    onClick = {
                        scope.launch {
                            saveError = null
                            result = runCatching {
                                if (hitMode) model.bcFromHit(table, range, elevation)
                                else model.bcFromChronograph(table, vNear, vFar, distance)
                            }.getOrNull()
                        }
                    },
                    modifier = Modifier.testTag("bcCalculate"),
                ) { Text(stringResource(Res.string.bc_calculate)) }

                result?.let { r ->
                    if (!r.ok) {
                        Text(coreText(r.error), color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("bcError"))
                    } else {
                        Text(stringResource(Res.string.bc_result, r.table, r.bc.fixed(3)),
                            style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold, modifier = Modifier.testTag("bcResult"))
                        form?.let { f -> Text(stringResource(Res.string.bc_current, "${f.dragTable} ${f.bc.fixed(3)}")) }
                        Text(stringResource(Res.string.bc_save_note), color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                }
                saveError?.let { Text(coreText(it), color = MaterialTheme.colorScheme.error) }
            }
        },
        confirmButton = {
            val r = result
            TextButton(
                enabled = r != null && r.ok && form != null,
                onClick = {
                    val f = form ?: return@TextButton
                    val bc = r ?: return@TextButton
                    scope.launch {
                        saveError = model.saveCartridge(f.copy(libraryBulletId = 0, dragTable = bc.table, bc = bc.bc))
                        if (saveError == null) onClose()
                    }
                },
                modifier = Modifier.testTag("bcSave"),
            ) { Text(stringResource(Res.string.bc_save)) }
        },
        dismissButton = { TextButton(onClick = onClose) { Text(stringResource(Res.string.cancel)) } },
    )
}
