package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.material3.MaterialTheme
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
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.loadOr
import org.vetalguru.balcalc.core.RifleForm
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

@Composable
internal fun CartridgeEditor(model: AppModel, route: Route.Cartridge, nav: ArmoryNav) {
    var f by route::form
    LaunchedEffect(route) { route.load(model, "cartridge", route.form.cartridgeId) }
    val photo = route.photo // read outside EditorPage's BoxWithConstraints (see TruingScreen)
    var error by remember { mutableStateOf<String?>(null) }
    val scope = rememberCoroutineScope()
    val fromLibrary = f.libraryBulletId > 0
    // Stability in the current rifle, as the bullet and velocity are edited.
    val currentRifle = model.state.currentRifleId
    var partner by remember { mutableStateOf<RifleForm?>(null) }
    LaunchedEffect(currentRifle) {
        partner = if (currentRifle > 0) loadOr(null) { model.armory.rifleForm(currentRifle) } else null
    }
    var sg by remember { mutableStateOf<Double?>(null) }
    LaunchedEffect(f.massGr, f.diameterIn, f.lengthIn, f.muzzleVelocity, partner) {
        sg = partner?.let { r -> loadOr(null) { model.armory.stability(r.twistIn, f.massGr, f.diameterIn, f.lengthIn, f.muzzleVelocity) } }
    }
    val stability = sg
    val partnerName = partner?.name.orEmpty()
    val inch = stringResource(Res.string.unit_in)
    EditorPage(
        title = stringResource(if (f.cartridgeId > 0) Res.string.edit_cartridge else Res.string.new_cartridge),
        error = error,
        onCancel = nav::back,
        onSave = { scope.launch { error = model.armory.saveCartridge(f, route.photoChange); if (error == null) nav.back() } },
    ) { wide ->
        Section(stringResource(Res.string.cartridge)) {
            TextInput(stringResource(Res.string.cartridge_name_hint), f.name, { f = f.copy(name = it) }, tag = "cartridgeName")
            TextInput(stringResource(Res.string.caliber_hint), f.caliber, { f = f.copy(caliber = it) }, tag = "cartridgeCaliber")
            PhotoRow(photo, route::change)
        }
        Section(stringResource(Res.string.bullet)) {
            // The bullet's name on its own line: beside the buttons a long one
            // was squeezed to a letter column on phones.
            Text(
                if (fromLibrary) stringResource(Res.string.from_library_named, f.bulletName) else stringResource(Res.string.own_bullet),
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.testTag("bulletSource"),
            )
            FlowRow {
                TextButton(onClick = {
                    nav.push(Route.Bullets { id -> model.act { f = model.armory.cartridgeFormWithBullet(f, id) } })
                }, modifier = Modifier.testTag("chooseBullet")) {
                    Text(stringResource(if (fromLibrary) Res.string.change else Res.string.from_library))
                }
                if (fromLibrary) TextButton(onClick = { f = f.copy(libraryBulletId = 0) }) { Text(stringResource(Res.string.edit_as_own)) }
            }
            TextInput(stringResource(Res.string.bullet_hint), f.bulletName, { f = f.copy(bulletName = it) }, enabled = !fromLibrary, tag = "bulletName")
            Column(Modifier.alpha(if (fromLibrary) 0.5f else 1f), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Fields(
                    wide,
                    { mod ->
                        ChoiceField(
                            stringResource(Res.string.drag_model),
                            listOf("G7", "G1", "G2", "G5", "G6", "G8", "GI", "GS", "RA4").map { it to it },
                            f.dragTable, { if (!fromLibrary) f = f.copy(dragTable = it) }, mod,
                        )
                    },
                    { mod -> NumberField(stringResource(Res.string.bc), f.bc, { if (!fromLibrary) f = f.copy(bc = it) }, mod, decimals = 3, from = 0.0, to = 2.0, tag = "bc") },
                    { mod -> NumberField(stringResource(Res.string.weight), f.massGr, { if (!fromLibrary) f = f.copy(massGr = it) }, mod, stringResource(Res.string.unit_gr), from = 0.0, to = 2000.0, tag = "mass") },
                    { mod -> NumberField(stringResource(Res.string.diameter), f.diameterIn, { if (!fromLibrary) f = f.copy(diameterIn = it) }, mod, inch, 3, 0.0, 1.0, tag = "diameter") },
                    { mod -> NumberField(stringResource(Res.string.length_spin), f.lengthIn, { if (!fromLibrary) f = f.copy(lengthIn = it) }, mod, inch, 3, 0.0, 4.0) },
                )
            }
            StabilityLine(stability, partnerName)
        }
        Section(stringResource(Res.string.velocity)) {
            Fields(
                wide,
                { mod -> NumberField(stringResource(Res.string.muzzle_velocity), f.muzzleVelocity, { f = f.copy(muzzleVelocity = it) }, mod, stringResource(Res.string.unit_mps), from = 0.0, to = 2000.0, tag = "muzzleVelocity") },
                { mod -> NumberField(stringResource(Res.string.measured_at_powder), f.powderReferenceC, { f = f.copy(powderReferenceC = it) }, mod, stringResource(Res.string.unit_c), from = -60.0, to = 80.0) },
                { mod -> NumberField(stringResource(Res.string.powder_sensitivity), f.powderSensitivity, { f = f.copy(powderSensitivity = it) }, mod, stringResource(Res.string.unit_pct_per_c), 3, -2.0, 2.0) },
            )
        }
    }
}
