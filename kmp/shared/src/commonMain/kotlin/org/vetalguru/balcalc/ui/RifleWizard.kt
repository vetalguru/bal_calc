package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
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
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.DrawableResource
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.BackHandler
import org.vetalguru.balcalc.coreText
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.core.ReticleItem
import org.vetalguru.balcalc.core.RifleForm
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** The wizard's steps, in order; the last one is the summary. */
internal enum class WizardStep(val title: StringResource, val icon: DrawableResource?, val note: StringResource?) {
    Rifle(Res.string.rifle, Res.drawable.ic_rifle, Res.string.wizard_rifle_note),
    Barrel(Res.string.barrel_section, null, Res.string.wizard_barrel_note),
    Suppressor(Res.string.suppressor_section, null, Res.string.wizard_suppressor_note),
    Scope(Res.string.scope, Res.drawable.ic_scope, Res.string.wizard_scope_note),
    Mount(Res.string.mount_section, Res.drawable.ic_straighten, Res.string.wizard_mount_note),
    Zero(Res.string.zero, Res.drawable.ic_my_location, null),
    Summary(Res.string.summary, null, null),
}

/**
 * A rifle on its drawing: a tap on a part (or its chip) opens that part's
 * fields; Next and Back go through them in order, to a summary. A new rifle
 * starts at the first part, with the parts not seen yet faint; Save works on
 * any part (only the name is required).
 */
@Composable
internal fun RifleWizard(model: AppModel, route: Route.RifleWizard, nav: ArmoryNav) {
    var f by route::form
    var step by route::step
    LaunchedEffect(route) { route.load(model, "rifle", route.form.rifleId) }
    val photo = route.photo
    var error by remember { mutableStateOf<String?>(null) }
    var reticles by remember { mutableStateOf(emptyList<ReticleItem>()) }
    LaunchedEffect(Unit) { reticles = model.library.reticles() }
    val reticleList = reticles
    val (stability, partnerName) = rememberStability(model, f.twistIn)
    val scope = rememberCoroutineScope()
    val steps = WizardStep.entries
    val current = steps[step]
    LaunchedEffect(current) { route.visited = route.visited + current }
    val done = route.visited
    val set: (RifleForm) -> Unit = { f = it }
    val save: () -> Unit = {
        scope.launch {
            error = model.armory.saveRifle(f, route.photoChange)
            if (error == null) nav.back() else if (f.name.isBlank()) step = 0
        }
    }
    BackHandler(enabled = step > 0) { step -= 1 }

    BoxWithConstraints(Modifier.fillMaxSize()) {
        val wide = maxWidth >= 640.dp
        Column(Modifier.fillMaxSize()) {
            EditorBar(
                stringResource(if (f.rifleId > 0) Res.string.edit_rifle else Res.string.new_rifle),
                stringResource(Res.string.cancel), stringResource(Res.string.save), nav::back, save,
            )
            Column(Modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally) {
                RifleDiagram(
                    f, current, done, { step = it.ordinal },
                    Modifier.widthIn(max = 560.dp).padding(horizontal = 12.dp),
                )
                PartChips(current, done) { step = it.ordinal }
                Text(
                    stringResource(Res.string.wizard_step, step + 1, steps.size) + " · " + stringResource(current.title),
                    style = MaterialTheme.typography.labelMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(top = 2.dp).testTag("wizardStep"),
                )
            }
            Column(
                Modifier.weight(1f).fillMaxWidth().verticalScroll(rememberScrollState()).padding(vertical = 12.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                if (!error.isNullOrEmpty()) {
                    Text(coreText(error!!), color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(horizontal = 16.dp).testTag("formError"))
                }
                if (current == WizardStep.Summary) {
                    WizardSummary(f, reticleList) { step = it.ordinal }
                } else {
                    Section(stringResource(current.title), icon = current.icon) {
                        current.note?.let { Text(stringResource(it), color = MaterialTheme.colorScheme.onSurfaceVariant) }
                        when (current) {
                            WizardStep.Rifle -> RifleIdentityFields(f, set, wide, photo, route::change) {
                                nav.push(Route.LibraryRifles { r, barrel -> f = f.withRifle(r, barrel) })
                            }
                            WizardStep.Barrel -> BarrelFields(f, set, wide, stability, partnerName)
                            WizardStep.Suppressor -> SuppressorFields(f, set, wide)
                            WizardStep.Scope -> ScopeFields(f, set, wide, reticleList) {
                                nav.push(Route.LibraryScopes { s, c -> f = f.withScope(s, c) })
                            }
                            WizardStep.Mount -> MountFields(f, set, wide)
                            WizardStep.Zero -> ZeroFields(f, set, wide, model)
                            WizardStep.Summary -> Unit
                        }
                    }
                }
            }
            Row(
                Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                OutlinedButton(onClick = { step -= 1 }, enabled = step > 0, modifier = Modifier.testTag("wizardBack")) {
                    Text(stringResource(Res.string.back))
                }
                Spacer(Modifier.weight(1f))
                if (current == WizardStep.Summary) {
                    Button(onClick = save, modifier = Modifier.testTag("wizardFinish")) {
                        ButtonIcon(Res.drawable.ic_save)
                        Text(stringResource(Res.string.save))
                    }
                } else {
                    Button(onClick = { step += 1 }, modifier = Modifier.testTag("wizardNext")) {
                        Text(stringResource(Res.string.next))
                    }
                }
            }
        }
    }
}

/** "↓1.5 MOA, On the rifle now" for a device's shift and state. */
@Composable
private fun deviceText(d: DeviceShift) = listOfNotNull(
    shiftText(d.up, d.right, stringResource(if (d.units == "mrad") Res.string.unit_mrad else Res.string.unit_moa)).ifEmpty { null },
    stringResource(if (d.on) Res.string.muzzle_device_on else Res.string.muzzle_device_off),
).joinToString(", ")

/** Each step's stated values (what is left at "not stated" is not listed), with a way back to it. */
@Composable
private fun WizardSummary(f: RifleForm, reticles: List<ReticleItem>, onChange: (WizardStep) -> Unit) {
    val inch = stringResource(Res.string.unit_in)
    val cm = stringResource(Res.string.unit_cm)
    val mm = stringResource(Res.string.unit_mm)
    val clicks = stringResource(if (f.clickUnits == "moa") Res.string.unit_moa else if (f.clickUnits == "mrad") Res.string.unit_mrad
        else if (f.clickUnits == "smoa") Res.string.click_smoa else Res.string.click_cm100m)
    val rail = stringResource(if (f.railSlopeUnits == "mrad") Res.string.unit_mrad else Res.string.unit_moa)
    val rifling = riflingOptions().firstOrNull { it.first == f.riflingType }?.second.orEmpty()
    val muzzle = muzzleOptions().firstOrNull { it.first == f.muzzleDevice }?.second.orEmpty()
    fun n(v: Double, d: Int = 1) = formatNumber(v, d)
    val parts: List<Pair<WizardStep, List<Pair<String, String>>>> = listOf(
        WizardStep.Rifle to listOfNotNull(
            stringResource(Res.string.label_name) to f.name,
            (stringResource(Res.string.maker) to listOf(f.maker, f.model).filter { it.isNotBlank() }.joinToString(" ")).takeIf { it.second.isNotBlank() },
            (stringResource(Res.string.label_caliber) to f.caliber).takeIf { f.caliber.isNotBlank() },
        ),
        WizardStep.Barrel to listOfNotNull(
            (stringResource(Res.string.barrel_length) to "${n(f.barrelIn)} $inch").takeIf { f.barrelIn > 0 },
            (stringResource(Res.string.label_twist) to "1:${n(f.twistIn, 2)} $inch" + if (f.twistLeft) " ↺" else "").takeIf { f.twistIn > 0 },
            (stringResource(Res.string.rifling_type) to listOfNotNull(rifling.takeIf { f.riflingType.isNotEmpty() },
                f.riflingGrooves.takeIf { it > 0 }?.toString()).joinToString(", ")).takeIf { it.second.isNotEmpty() },
            (stringResource(Res.string.muzzle_device) to muzzle).takeIf { f.muzzleDevice.isNotEmpty() },
            (stringResource(Res.string.muzzle_poi_up) to deviceText(f.muzzleShift)).takeIf { f.hasMuzzleDevice },
        ),
        WizardStep.Suppressor to listOfNotNull(
            (stringResource(Res.string.suppressor_section) to f.suppressorName).takeIf { f.hasSuppressor && f.suppressorName.isNotBlank() },
            (stringResource(Res.string.muzzle_poi_up) to deviceText(f.suppressorShift)).takeIf { f.hasSuppressor },
        ),
        WizardStep.Scope to listOfNotNull(
            (stringResource(Res.string.model) to listOf(f.scopeMaker, f.scopeModel).filter { it.isNotBlank() }.joinToString(" ")).takeIf { it.second.isNotBlank() },
            stringResource(Res.string.one_click) to "${n(f.clickValue, 4)} $clicks" +
                if (f.clickValueHorizontal > 0) " / ${n(f.clickValueHorizontal, 4)}" else "",
            (stringResource(Res.string.elevation_travel) to "${n(f.elevationTravel)} / ${n(f.windageTravel)} $clicks").takeIf { f.elevationTravel > 0 || f.windageTravel > 0 },
            stringResource(Res.string.focal_plane) to stringResource(if (f.focalPlane == "sfp") Res.string.sfp else Res.string.ffp) +
                if (f.maxMagnification > 0) ", ${n(f.minMagnification)}–${n(f.maxMagnification)}×" else "",
            (stringResource(Res.string.reticle) to reticles.firstOrNull { it.id == f.reticleId }?.name.orEmpty()).takeIf { f.reticleId > 0 },
            (stringResource(Res.string.tube_diameter) to "${n(f.tubeDiameterMm)} $mm" + if (f.objectiveMm > 0) ", ${n(f.objectiveMm, 0)} $mm" else "")
                .takeIf { f.tubeDiameterMm > 0 },
        ),
        WizardStep.Mount to listOfNotNull(
            (stringResource(Res.string.rail_slope) to "${n(f.railSlope)} $rail").takeIf { f.railSlope > 0 },
            (stringResource(Res.string.ring_height) to "${n(f.ringHeightMm)} $mm").takeIf { f.ringHeightMm > 0 },
            stringResource(Res.string.sight_height) to "${n(f.sightHeightCm, 2)} $cm",
            f.reachUp()?.let { "" to stringResource(Res.string.reach_up, it.fixed(1), clicks) },
        ),
        WizardStep.Zero to listOf(
            stringResource(Res.string.zero_distance) to "${n(f.zeroRangeM, 0)} ${stringResource(Res.string.unit_m)}",
            stringResource(Res.string.zero_temperature) to "${n(f.zeroTemperatureC, 0)} ${stringResource(Res.string.unit_c)}",
        ),
    )
    parts.forEach { (step, rows) ->
        Section(stringResource(step.title), icon = step.icon) {
            if (rows.isEmpty()) Text(stringResource(Res.string.not_stated), color = MaterialTheme.colorScheme.onSurfaceVariant)
            rows.forEach { (label, value) ->
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(label, color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.weight(1f))
                    Text(value, modifier = Modifier.weight(1.4f))
                }
            }
            TextButton(onClick = { onChange(step) }, modifier = Modifier.align(Alignment.End).testTag("summaryChange:${step.name}")) {
                Text(stringResource(Res.string.change))
            }
        }
    }
}
