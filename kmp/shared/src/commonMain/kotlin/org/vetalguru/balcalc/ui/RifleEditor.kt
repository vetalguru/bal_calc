package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.loadOr
import org.vetalguru.balcalc.core.CartridgeForm
import org.vetalguru.balcalc.core.ReticleItem
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

@Composable
internal fun RifleEditor(model: AppModel, route: Route.Rifle, nav: ArmoryNav) {
    var f by route::form
    LaunchedEffect(route) { route.load(model, "rifle", route.form.rifleId) }
    val photo = route.photo // read outside EditorPage's BoxWithConstraints (see TruingScreen)
    val onDone = nav::back
    var error by remember { mutableStateOf<String?>(null) }
    var reticles by remember { mutableStateOf(emptyList<ReticleItem>()) }
    LaunchedEffect(Unit) { reticles = model.library.reticles() }
    val reticleList = reticles // read outside the editor's BoxWithConstraints (see TruingScreen)
    // Stability with the current cartridge, as the twist is edited.
    val currentCartridge = model.state.currentCartridgeId
    var partner by remember { mutableStateOf<CartridgeForm?>(null) }
    LaunchedEffect(currentCartridge) {
        partner = if (currentCartridge > 0) loadOr(null) { model.armory.cartridgeForm(currentCartridge) } else null
    }
    var sg by remember { mutableStateOf<Double?>(null) }
    LaunchedEffect(f.twistIn, partner) {
        sg = partner?.let { p -> loadOr(null) { model.armory.stability(f.twistIn, p.massGr, p.diameterIn, p.lengthIn, p.muzzleVelocity) } }
    }
    val stability = sg
    val partnerName = partner?.name.orEmpty()
    val scope = rememberCoroutineScope()
    val cm = stringResource(Res.string.unit_cm)
    val inch = stringResource(Res.string.unit_in)
    val m = stringResource(Res.string.unit_m)
    val c = stringResource(Res.string.unit_c)
    val x = "×"
    EditorPage(
        title = stringResource(if (f.rifleId > 0) Res.string.edit_rifle else Res.string.new_rifle),
        error = error,
        onCancel = onDone,
        onSave = { scope.launch { error = model.armory.saveRifle(f, route.photoChange); if (error == null) onDone() } },
    ) { wide ->
        Section(stringResource(Res.string.rifle)) {
            LibraryButton("rifleFromLibrary") {
                nav.push(Route.LibraryRifles { r -> f = f.withRifle(r) })
            }
            TextInput(stringResource(Res.string.rifle_name_hint), f.name, { f = f.copy(name = it) }, tag = "rifleName")
            TextInput(stringResource(Res.string.caliber_hint), f.caliber, { f = f.copy(caliber = it) }, tag = "rifleCaliber")
            PhotoRow(photo, route::change)
            Fields(
                wide,
                { mod -> NumberField(stringResource(Res.string.sight_height), f.sightHeightCm, { f = f.copy(sightHeightCm = it) }, mod, cm, from = 0.0, to = 20.0) },
                { mod -> NumberField(stringResource(Res.string.twist), f.twistIn, { f = f.copy(twistIn = it) }, mod, inch, 2, 0.0, 60.0, tag = "twist") },
            )
            SwitchRow(stringResource(Res.string.left_twist), f.twistLeft, { f = f.copy(twistLeft = it) })
            StabilityLine(stability, partnerName)
        }
        Section(stringResource(Res.string.scope)) {
            LibraryButton("scopeFromLibrary") {
                nav.push(Route.LibraryScopes { s, c -> f = f.withScope(s, c) })
            }
            Fields(
                wide,
                { mod ->
                    ChoiceField(
                        stringResource(Res.string.turret_units),
                        listOf(
                            "mrad" to stringResource(Res.string.unit_mrad),
                            "moa" to stringResource(Res.string.unit_moa),
                            "smoa" to stringResource(Res.string.click_smoa),
                            "cm100m" to stringResource(Res.string.click_cm100m),
                        ),
                        f.clickUnits, { f = f.copy(clickUnits = it) }, mod,
                    )
                },
                { mod -> NumberField(stringResource(Res.string.one_click), f.clickValue, { f = f.copy(clickValue = it) }, mod, decimals = 4, from = 0.0, to = 10.0, tag = "clickValue") },
                { mod ->
                    ChoiceField(
                        stringResource(Res.string.reticle),
                        listOf(0L to stringResource(Res.string.reticle_none)) + reticleList.map { it.id to it.name },
                        f.reticleId, { f = f.copy(reticleId = it) }, mod,
                    )
                },
                { mod ->
                    ChoiceField(
                        stringResource(Res.string.focal_plane),
                        listOf("ffp" to stringResource(Res.string.ffp), "sfp" to stringResource(Res.string.sfp)),
                        f.focalPlane, { f = f.copy(focalPlane = it) }, mod,
                    )
                },
                { mod -> NumberField(stringResource(Res.string.mag_from), f.minMagnification, { f = f.copy(minMagnification = it) }, mod, x, from = 0.0, to = 100.0) },
                { mod -> NumberField(stringResource(Res.string.mag_to), f.maxMagnification, { f = f.copy(maxMagnification = it) }, mod, x, from = 0.0, to = 100.0, tag = "magTo") },
            )
            if (f.focalPlane == "sfp") {
                NumberField(stringResource(Res.string.sfp_reference), f.sfpReferenceMagnification, { f = f.copy(sfpReferenceMagnification = it) }, unit = x, from = 0.0, to = 100.0)
            }
        }
        Section(stringResource(Res.string.zero)) {
            Text(stringResource(Res.string.zero_note), color = MaterialTheme.colorScheme.onSurfaceVariant)
            Fields(
                wide,
                { mod -> NumberField(stringResource(Res.string.zero_distance), f.zeroRangeM, { f = f.copy(zeroRangeM = it) }, mod, m, 0, 10.0, 1000.0, tag = "zeroRange") },
                { mod ->
                    NumberField(stringResource(Res.string.zero_temperature), f.zeroTemperatureC,
                        { f = f.copy(zeroTemperatureC = it, zeroPowderC = it) }, mod, c, from = -60.0, to = 60.0)
                },
                { mod -> NumberField(stringResource(Res.string.zero_pressure), f.zeroPressureHpa, { f = f.copy(zeroPressureHpa = it) }, mod, stringResource(Res.string.unit_hpa), from = 300.0, to = 1200.0) },
                { mod -> NumberField(stringResource(Res.string.zero_altitude), f.zeroAltitudeM, { f = f.copy(zeroAltitudeM = it) }, mod, m, 0, -500.0, 6000.0) },
                { mod -> NumberField(stringResource(Res.string.zero_humidity), f.zeroHumidityPct, { f = f.copy(zeroHumidityPct = it) }, mod, "%", 0, 0.0, 100.0) },
            )
        }
    }
}
