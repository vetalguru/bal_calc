package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.coreText
import org.vetalguru.balcalc.core.BcBand
import org.vetalguru.balcalc.core.BulletForm
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** One library bullet: identity, size and drag (one BC or BC bands). */
@Composable
fun BulletEditor(model: AppModel, initial: BulletForm, onDone: () -> Unit) {
    var f by remember { mutableStateOf(initial) }
    var banded by remember { mutableStateOf(initial.bands.isNotEmpty()) }
    var error by remember { mutableStateOf<String?>(null) }
    val scope = rememberCoroutineScope()
    val inch = stringResource(Res.string.unit_in)
    val mps = stringResource(Res.string.unit_mps)
    fun save() = scope.launch {
        error = model.saveBullet(f.copy(bands = if (banded) f.bands else emptyList()))
        if (error == null) onDone()
    }
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val wide = maxWidth >= 640.dp
        Column(Modifier.fillMaxSize()) {
            EditorBar(
                stringResource(if (f.id > 0) Res.string.edit_bullet else Res.string.new_bullet),
                stringResource(Res.string.cancel), stringResource(Res.string.save), onDone, ::save,
            )
            Column(
                Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(vertical = 12.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                error?.let { Text(coreText(it), color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(horizontal = 16.dp).testTag("formError")) }
                Section(stringResource(Res.string.bullet)) {
                    TextInput(stringResource(Res.string.bullet_name_hint), f.name, { f = f.copy(name = it) }, tag = "bulletEditName")
                    Fields(
                        wide,
                        { m -> TextInput(stringResource(Res.string.manufacturer), f.manufacturer, { f = f.copy(manufacturer = it) }, m) },
                        { m -> TextInput(stringResource(Res.string.caliber_hint), f.caliber, { f = f.copy(caliber = it) }, m) },
                        { m -> NumberField(stringResource(Res.string.weight), f.massGr, { f = f.copy(massGr = it) }, m, stringResource(Res.string.unit_gr), from = 0.0, to = 2000.0, tag = "bulletMass") },
                        { m -> NumberField(stringResource(Res.string.diameter), f.diameterIn, { f = f.copy(diameterIn = it) }, m, inch, 3, 0.0, 1.0, tag = "bulletDiameter") },
                        { m -> NumberField(stringResource(Res.string.length_spin), f.lengthIn, { f = f.copy(lengthIn = it) }, m, inch, 3, 0.0, 4.0) },
                    )
                }
                Section(stringResource(Res.string.drag), Modifier.alpha(if (f.hasCustomCurve) 0.6f else 1f)) {
                    if (f.hasCustomCurve) Text(stringResource(Res.string.own_curve_note))
                    ChoiceField(
                        stringResource(Res.string.drag_model),
                        listOf("G7", "G1", "G2", "G5", "G6", "G8", "GI", "GS", "RA4").map { it to it },
                        f.dragTable, { if (!f.hasCustomCurve) f = f.copy(dragTable = it) },
                    )
                    SwitchRow(stringResource(Res.string.banded), banded, { on ->
                        if (f.hasCustomCurve) return@SwitchRow
                        banded = on
                        if (on && f.bands.isEmpty()) f = f.copy(bands = listOf(BcBand(800.0, if (f.bc > 0) f.bc else 0.3)))
                    })
                    if (!banded) {
                        NumberField(stringResource(Res.string.bc), f.bc, { f = f.copy(bc = it) }, decimals = 3, from = 0.0, to = 2.0, tag = "bulletBc")
                    } else {
                        f.bands.forEachIndexed { i, band ->
                            Row(horizontalArrangement = Arrangement.spacedBy(12.dp), verticalAlignment = Alignment.CenterVertically) {
                                NumberField(stringResource(Res.string.above_velocity), band.velocity,
                                    { v -> f = f.copy(bands = f.bands.toMutableList().also { it[i] = band.copy(velocity = v) }) },
                                    Modifier.weight(1f), mps, 0, 0.0, 2000.0, tag = "bandVelocity$i")
                                NumberField(stringResource(Res.string.bc_short), band.bc,
                                    { v -> f = f.copy(bands = f.bands.toMutableList().also { it[i] = band.copy(bc = v) }) },
                                    Modifier.weight(1f), decimals = 3, from = 0.0, to = 2.0, tag = "bandBc$i")
                                TextButton(onClick = {
                                    val rest = f.bands.toMutableList().also { it.removeAt(i) }
                                    f = f.copy(bands = rest)
                                    if (rest.isEmpty()) banded = false
                                }) { Text("✕") }
                            }
                        }
                        TextButton(onClick = {
                            val last = f.bands.lastOrNull() ?: BcBand(800.0, 0.3)
                            f = f.copy(bands = f.bands + BcBand(maxOf(100.0, last.velocity - 200), last.bc))
                        }, modifier = Modifier.testTag("addBand")) { Text(stringResource(Res.string.add_band)) }
                    }
                }
                Section("") {
                    OutlinedTextField(
                        f.notes, { f = f.copy(notes = it) },
                        label = { Text(stringResource(Res.string.notes_source)) },
                        minLines = 2, modifier = Modifier.fillMaxWidth(),
                    )
                }
                Row(Modifier.fillMaxWidth().padding(horizontal = 12.dp, vertical = 8.dp)) {
                    if (f.id > 0) {
                        TextButton(onClick = {
                            scope.launch {
                                error = model.deleteBullet(f.id)
                                if (error == null) onDone()
                            }
                        }) { Text(stringResource(Res.string.delete), color = MaterialTheme.colorScheme.error) }
                    }
                    Spacer(Modifier.weight(1f))
                    Button(onClick = { save() }) { Text(stringResource(Res.string.save)) }
                }
            }
        }
    }
}
