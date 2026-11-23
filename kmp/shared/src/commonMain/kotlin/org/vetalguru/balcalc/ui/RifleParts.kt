package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.AlertDialog
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
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import kotlin.math.PI
import kotlin.math.round
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.loadOr
import org.vetalguru.balcalc.core.CartridgeForm
import org.vetalguru.balcalc.core.ReticleItem
import org.vetalguru.balcalc.core.RifleForm
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

// The rifle's fields in parts, one per part of the rifle's drawing (RifleWizard).

/** Radians in one of a turret's or a rail's units (as the core's ClickToRad); 0 for unknown units. */
internal fun radPer(units: String): Double = when (units) {
    "mrad" -> 1e-3
    "moa" -> PI / (180 * 60)
    "smoa" -> 0.0254 / (100 * 0.9144)
    "cm100m" -> 1e-4
    else -> 0.0
}

/** [value] in [from] units as [to] units: a turret's travel stays the same angle when its units change. */
internal fun convertAngle(value: Double, from: String, to: String): Double {
    val f = radPer(from)
    val t = radPer(to)
    return if (f == 0.0 || t == 0.0) value else value * f / t
}

/** The turret units changed: the travel keeps its angle; the clicks are the new scope's to enter. */
internal fun RifleForm.withClickUnits(units: String) = copy(
    clickUnits = units,
    elevationTravel = round6(convertAngle(elevationTravel, clickUnits, units)),
    windageTravel = round6(convertAngle(windageTravel, clickUnits, units)),
)

private fun round6(v: Double) = round(v * 1e6) / 1e6

/**
 * How far the turret dials up from the zero, in the click units: half the
 * travel (zeroed near its middle) plus the rail's slope; null when the
 * travel is not stated. The core warns on the same reach.
 */
internal fun RifleForm.reachUp(): Double? =
    if (elevationTravel <= 0.0) null else elevationTravel / 2 + convertAngle(railSlope, railSlopeUnits, clickUnits)

/** A device is stated (not "not stated", not "none"). */
internal val RifleForm.hasMuzzleDevice get() = muzzleDevice.isNotEmpty() && muzzleDevice != "none"

/**
 * Where a muzzle device or a suppressor moves the point of impact (up and
 * right positive, in [units]), whether it is [on] now and was on [atZero].
 */
internal data class DeviceShift(val up: Double, val right: Double, val units: String, val on: Boolean, val atZero: Boolean) {
    /**
     * What the solution counts now: the shift when the device is on but was
     * off at the zero, back by it in the other case, none when it is as at
     * the zero. The core does the same (MuzzleDeviceShift).
     */
    fun now(): Pair<Double, Double> {
        if (on == atZero) return 0.0 to 0.0
        val sign = if (on) 1.0 else -1.0
        return sign * up + 0.0 to sign * right + 0.0 // + 0.0: no "−0" for a side without a shift
    }
}

internal val RifleForm.muzzleShift
    get() = DeviceShift(muzzlePoiUp, muzzlePoiRight, muzzlePoiUnits, muzzleDeviceOn, zeroedWithDevice)

internal fun RifleForm.withMuzzleShift(d: DeviceShift) =
    copy(muzzlePoiUp = d.up, muzzlePoiRight = d.right, muzzlePoiUnits = d.units, muzzleDeviceOn = d.on, zeroedWithDevice = d.atZero)

internal val RifleForm.suppressorShift
    get() = DeviceShift(suppressorPoiUp, suppressorPoiRight, suppressorPoiUnits, suppressorOn, zeroedWithSuppressor)

internal fun RifleForm.withSuppressorShift(d: DeviceShift) = copy(
    suppressorPoiUp = d.up, suppressorPoiRight = d.right, suppressorPoiUnits = d.units, suppressorOn = d.on, zeroedWithSuppressor = d.atZero,
)

/** The muzzle device's shift the solution counts now (none without a device). */
internal fun RifleForm.muzzleShiftNow(): Pair<Double, Double> = if (hasMuzzleDevice) muzzleShift.now() else 0.0 to 0.0

/** The suppressor's shift the solution counts now (none without a suppressor). */
internal fun RifleForm.suppressorShiftNow(): Pair<Double, Double> = if (hasSuppressor) suppressorShift.now() else 0.0 to 0.0

/** "↓1.5 MOA →0.5" for a shift; "" for none. */
internal fun shiftText(up: Double, right: Double, units: String): String = listOfNotNull(
    up.takeIf { it != 0.0 }?.let { (if (it > 0) "↑" else "↓") + formatNumber(kotlin.math.abs(it), 2) },
    right.takeIf { it != 0.0 }?.let { (if (it > 0) "→" else "←") + formatNumber(kotlin.math.abs(it), 2) },
).joinToString(" ").let { if (it.isEmpty()) "" else "$it $units" }

/**
 * Sight height in cm from a caliper's measures in mm: the bolt's axis is
 * taken as the bore's, so the height is the bolt's radius plus the bolt top
 * to tube top distance less the tube's radius. Null when they cannot be.
 */
internal fun sightHeightCm(boltMm: Double, topToTopMm: Double, tubeMm: Double): Double? {
    if (boltMm <= 0.0 || tubeMm <= 0.0 || topToTopMm <= 0.0) return null
    val mm = boltMm / 2 + topToTopMm - tubeMm / 2
    return if (mm > 0.0 && mm < 300.0) mm / 10 else null
}

/** Sg of [twistIn] with the current cartridge, and that cartridge's name ("" without one). */
@Composable
internal fun rememberStability(model: AppModel, twistIn: Double): Pair<Double?, String> {
    val current = model.state.currentCartridgeId
    var partner by remember { mutableStateOf<CartridgeForm?>(null) }
    LaunchedEffect(current) {
        partner = if (current > 0) loadOr(null) { model.armory.cartridgeForm(current) } else null
    }
    var sg by remember { mutableStateOf<Double?>(null) }
    LaunchedEffect(twistIn, partner) {
        sg = partner?.let { p -> loadOr(null) { model.armory.stability(twistIn, p.massGr, p.diameterIn, p.lengthIn, p.muzzleVelocity) } }
    }
    return sg to partner?.name.orEmpty()
}

@Composable
private fun clickUnitText(units: String) = stringResource(
    when (units) {
        "moa" -> Res.string.unit_moa
        "smoa" -> Res.string.click_smoa
        "cm100m" -> Res.string.click_cm100m
        else -> Res.string.unit_mrad
    },
)

@Composable
internal fun riflingOptions() = listOf(
    "" to stringResource(Res.string.not_stated),
    "conventional" to stringResource(Res.string.rifling_conventional),
    "polygonal" to stringResource(Res.string.rifling_polygonal),
    "5r" to stringResource(Res.string.rifling_5r),
    "other" to stringResource(Res.string.rifling_other),
)

@Composable
internal fun muzzleOptions() = listOf(
    "" to stringResource(Res.string.not_stated),
    "none" to stringResource(Res.string.muzzle_none),
    "brake" to stringResource(Res.string.muzzle_brake),
    "compensator" to stringResource(Res.string.muzzle_compensator),
    "flash_hider" to stringResource(Res.string.muzzle_flash_hider),
)

/** Name, maker, model, calibre and photo; a pick from the catalog fills them. */
@Composable
internal fun RifleIdentityFields(
    f: RifleForm,
    set: (RifleForm) -> Unit,
    wide: Boolean,
    photo: ByteArray?,
    onPhoto: (ByteArray?) -> Unit,
    onLibrary: () -> Unit,
) {
    LibraryButton("rifleFromLibrary", onLibrary)
    TextInput(stringResource(Res.string.rifle_name_hint), f.name, { set(f.copy(name = it)) }, tag = "rifleName")
    Fields(
        wide,
        { mod -> TextInput(stringResource(Res.string.maker), f.maker, { set(f.copy(maker = it)) }, mod, tag = "rifleMaker") },
        { mod -> TextInput(stringResource(Res.string.model), f.model, { set(f.copy(model = it)) }, mod, tag = "rifleModel") },
    )
    TextInput(stringResource(Res.string.caliber_hint), f.caliber, { set(f.copy(caliber = it)) }, tag = "rifleCaliber")
    PhotoRow(photo, onPhoto)
}

/** Length, twist and its hand, rifling, muzzle device; Sg with the current cartridge. */
@Composable
internal fun BarrelFields(f: RifleForm, set: (RifleForm) -> Unit, wide: Boolean, stability: Double?, partnerName: String) {
    val inch = stringResource(Res.string.unit_in)
    Fields(
        wide,
        { mod ->
            NumberField(stringResource(Res.string.barrel_length), f.barrelIn, { set(f.copy(barrelIn = it)) }, mod, inch, 1, 0.0, 40.0,
                tag = "barrel", hint = stringResource(Res.string.barrel_length_hint))
        },
        { mod -> NumberField(stringResource(Res.string.twist), f.twistIn, { set(f.copy(twistIn = it)) }, mod, inch, 2, 0.0, 60.0, tag = "twist") },
        { mod -> ChoiceField(stringResource(Res.string.rifling_type), riflingOptions(), f.riflingType, { set(f.copy(riflingType = it)) }, mod) },
        { mod ->
            NumberField(stringResource(Res.string.rifling_grooves), f.riflingGrooves.toDouble(), { set(f.copy(riflingGrooves = it.toInt())) },
                mod, decimals = 0, from = 0.0, to = 16.0, tag = "grooves")
        },
        { mod -> ChoiceField(stringResource(Res.string.muzzle_device), muzzleOptions(), f.muzzleDevice, { set(f.copy(muzzleDevice = it)) }, mod) },
    )
    if (f.hasMuzzleDevice) {
        ShiftFields(f.muzzleShift, { set(f.withMuzzleShift(it)) }, wide, "muzzle", stringResource(Res.string.zeroed_with_device))
    }
    SwitchRow(stringResource(Res.string.left_twist), f.twistLeft, { set(f.copy(twistLeft = it)) })
    StabilityLine(stability, partnerName)
}

/**
 * A muzzle device's or suppressor's shift: where it moves the point of
 * impact, whether it is on now and was at the zero, and what the solution
 * counts now. Tags start with [tag] ("muzzlePoiUp", "suppressorOn", ...).
 */
@Composable
private fun ShiftFields(d: DeviceShift, set: (DeviceShift) -> Unit, wide: Boolean, tag: String, atZeroLabel: String) {
    val unitOptions = listOf("moa" to stringResource(Res.string.unit_moa), "mrad" to stringResource(Res.string.unit_mrad))
    val units = unitOptions.first { it.first == (if (d.units == "mrad") "mrad" else "moa") }.second
    Fields(
        wide,
        { mod ->
            Row(mod, verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                NumberField(stringResource(Res.string.muzzle_poi_up), d.up, { set(d.copy(up = it)) }, Modifier.weight(1f),
                    decimals = 2, from = -40.0, to = 40.0, tag = "${tag}PoiUp", hint = stringResource(Res.string.muzzle_poi_hint))
                ChoiceField("", unitOptions, d.units, { set(d.copy(units = it)) }, Modifier.weight(0.6f))
            }
        },
        { mod ->
            NumberField(stringResource(Res.string.muzzle_poi_right), d.right, { set(d.copy(right = it)) }, mod, units,
                decimals = 2, from = -40.0, to = 40.0, tag = "${tag}PoiRight", hint = stringResource(Res.string.muzzle_poi_hint))
        },
    )
    SwitchRow(stringResource(Res.string.muzzle_device_on), d.on, { set(d.copy(on = it)) }, Modifier.testTag("${tag}On"))
    SwitchRow(atZeroLabel, d.atZero, { set(d.copy(atZero = it)) }, Modifier.testTag("${tag}AtZero"))
    val (up, right) = d.now()
    val shift = shiftText(up, right, units)
    Text(
        if (shift.isEmpty()) stringResource(Res.string.muzzle_shift_none) else stringResource(Res.string.muzzle_shift_now, shift),
        color = if (shift.isEmpty()) MaterialTheme.colorScheme.onSurfaceVariant else MaterialTheme.colorScheme.primary,
        modifier = Modifier.testTag("${tag}ShiftNow"),
    )
}

/** The suppressor, if the rifle has one: its name and its shift. */
@Composable
internal fun SuppressorFields(f: RifleForm, set: (RifleForm) -> Unit, wide: Boolean) {
    SwitchRow(stringResource(Res.string.has_suppressor), f.hasSuppressor, { set(f.copy(hasSuppressor = it)) }, Modifier.testTag("hasSuppressor"))
    if (!f.hasSuppressor) return
    TextInput(stringResource(Res.string.suppressor_name), f.suppressorName, { set(f.copy(suppressorName = it)) }, tag = "suppressorName")
    ShiftFields(f.suppressorShift, { set(f.withSuppressorShift(it)) }, wide, "suppressor", stringResource(Res.string.zeroed_with_suppressor))
}

/** The scope: what it is, its clicks and turrets, magnification, reticle. */
@Composable
internal fun ScopeFields(f: RifleForm, set: (RifleForm) -> Unit, wide: Boolean, reticles: List<ReticleItem>, onLibrary: () -> Unit) {
    val x = "×"
    val mm = stringResource(Res.string.unit_mm)
    val clicks = clickUnitText(f.clickUnits)
    LibraryButton("scopeFromLibrary", onLibrary)
    Fields(
        wide,
        { mod -> TextInput(stringResource(Res.string.maker), f.scopeMaker, { set(f.copy(scopeMaker = it)) }, mod, tag = "scopeMaker") },
        { mod -> TextInput(stringResource(Res.string.model), f.scopeModel, { set(f.copy(scopeModel = it)) }, mod, tag = "scopeModel") },
        { mod ->
            ChoiceField(
                stringResource(Res.string.turret_units),
                listOf(
                    "mrad" to stringResource(Res.string.unit_mrad),
                    "moa" to stringResource(Res.string.unit_moa),
                    "smoa" to stringResource(Res.string.click_smoa),
                    "cm100m" to stringResource(Res.string.click_cm100m),
                ),
                f.clickUnits, { set(f.withClickUnits(it)) }, mod,
            )
        },
        { mod -> NumberField(stringResource(Res.string.one_click), f.clickValue, { set(f.copy(clickValue = it)) }, mod, decimals = 4, from = 0.0, to = 10.0, tag = "clickValue") },
        { mod ->
            NumberField(stringResource(Res.string.click_horizontal), f.clickValueHorizontal, { set(f.copy(clickValueHorizontal = it)) }, mod,
                decimals = 4, from = 0.0, to = 10.0, tag = "clickHorizontal", hint = stringResource(Res.string.click_horizontal_hint))
        },
        { mod ->
            NumberField(stringResource(Res.string.elevation_travel), f.elevationTravel, { set(f.copy(elevationTravel = it)) }, mod, clicks,
                decimals = 1, from = 0.0, to = 400.0, tag = "elevationTravel", hint = stringResource(Res.string.travel_hint))
        },
        { mod ->
            NumberField(stringResource(Res.string.windage_travel), f.windageTravel, { set(f.copy(windageTravel = it)) }, mod, clicks,
                decimals = 1, from = 0.0, to = 400.0, tag = "windageTravel", hint = stringResource(Res.string.travel_hint))
        },
        { mod ->
            ChoiceField(
                stringResource(Res.string.focal_plane),
                listOf("ffp" to stringResource(Res.string.ffp), "sfp" to stringResource(Res.string.sfp)),
                f.focalPlane, { set(f.copy(focalPlane = it)) }, mod,
            )
        },
        { mod ->
            ChoiceField(
                stringResource(Res.string.reticle),
                listOf(0L to stringResource(Res.string.reticle_none)) + reticles.map { it.id to it.name },
                f.reticleId, { set(f.copy(reticleId = it)) }, mod,
            )
        },
        { mod -> NumberField(stringResource(Res.string.mag_from), f.minMagnification, { set(f.copy(minMagnification = it)) }, mod, x, from = 0.0, to = 100.0) },
        { mod -> NumberField(stringResource(Res.string.mag_to), f.maxMagnification, { set(f.copy(maxMagnification = it)) }, mod, x, from = 0.0, to = 100.0, tag = "magTo") },
        { mod -> NumberField(stringResource(Res.string.tube_diameter), f.tubeDiameterMm, { set(f.copy(tubeDiameterMm = it)) }, mod, mm, 1, 0.0, 50.0, tag = "tube") },
        { mod -> NumberField(stringResource(Res.string.objective), f.objectiveMm, { set(f.copy(objectiveMm = it)) }, mod, mm, 0, 0.0, 100.0, tag = "objective") },
    )
    if (f.focalPlane == "sfp") {
        NumberField(stringResource(Res.string.sfp_reference), f.sfpReferenceMagnification, { set(f.copy(sfpReferenceMagnification = it)) }, unit = x, from = 0.0, to = 100.0)
    }
}

/** The rail's slope, the rings and the sight height (with its calculator); how far the turret dials up. */
@Composable
internal fun MountFields(f: RifleForm, set: (RifleForm) -> Unit, wide: Boolean) {
    val cm = stringResource(Res.string.unit_cm)
    val mm = stringResource(Res.string.unit_mm)
    var calc by remember { mutableStateOf(false) }
    Fields(
        wide,
        { mod ->
            Row(mod, verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                NumberField(stringResource(Res.string.rail_slope), f.railSlope, { set(f.copy(railSlope = it)) }, Modifier.weight(1f),
                    decimals = 1, from = 0.0, to = 60.0, tag = "railSlope", hint = stringResource(Res.string.rail_slope_hint))
                ChoiceField(
                    "",
                    listOf("moa" to stringResource(Res.string.unit_moa), "mrad" to stringResource(Res.string.unit_mrad)),
                    f.railSlopeUnits, { set(f.copy(railSlopeUnits = it)) }, Modifier.weight(0.7f),
                )
            }
        },
        { mod -> NumberField(stringResource(Res.string.ring_height), f.ringHeightMm, { set(f.copy(ringHeightMm = it)) }, mod, mm, 1, 0.0, 100.0, tag = "ringHeight") },
    )
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        NumberField(stringResource(Res.string.sight_height), f.sightHeightCm, { set(f.copy(sightHeightCm = it)) }, Modifier.weight(1f),
            cm, 2, 0.0, 20.0, tag = "sightHeight", hint = stringResource(Res.string.sight_height_hint))
        OutlinedButton(onClick = { calc = true }, modifier = Modifier.testTag("sightCalc")) {
            ButtonIcon(Res.drawable.ic_calculate)
            Text(stringResource(Res.string.calculate))
        }
    }
    f.reachUp()?.let { up ->
        Text(stringResource(Res.string.reach_up, up.fixed(1), clickUnitText(f.clickUnits)),
            color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.testTag("reachUp"))
    }
    if (calc) SightHeightCalculator(f.tubeDiameterMm, onUse = { set(f.copy(sightHeightCm = it)); calc = false }, onDismiss = { calc = false })
}

@Composable
private fun SightHeightCalculator(tubeMm: Double, onUse: (Double) -> Unit, onDismiss: () -> Unit) {
    val mm = stringResource(Res.string.unit_mm)
    var bolt by remember { mutableDoubleStateOf(0.0) }
    var topToTop by remember { mutableDoubleStateOf(0.0) }
    var tube by remember { mutableDoubleStateOf(if (tubeMm > 0) tubeMm else 30.0) }
    val result = sightHeightCm(bolt, topToTop, tube)
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(Res.string.calc_sight_title)) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(stringResource(Res.string.calc_sight_note), color = MaterialTheme.colorScheme.onSurfaceVariant)
                NumberField(stringResource(Res.string.calc_bolt), bolt, { bolt = it }, Modifier.fillMaxWidth(), mm, 1, 0.0, 50.0, tag = "calcBolt")
                NumberField(stringResource(Res.string.calc_top_to_top), topToTop, { topToTop = it }, Modifier.fillMaxWidth(), mm, 1, 0.0, 300.0, tag = "calcTopToTop")
                NumberField(stringResource(Res.string.tube_diameter), tube, { tube = it }, Modifier.fillMaxWidth(), mm, 1, 0.0, 50.0, tag = "calcTube")
                if (result != null) {
                    Text(stringResource(Res.string.calc_sight_result, result.fixed(2)), style = MaterialTheme.typography.titleMedium,
                        modifier = Modifier.testTag("calcResult"))
                }
            }
        },
        confirmButton = {
            TextButton(enabled = result != null, onClick = { result?.let { onUse(round(it * 100) / 100) } }, modifier = Modifier.testTag("calcUse")) {
                Text(stringResource(Res.string.sensor_use))
            }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(Res.string.cancel)) } },
    )
}

/** Where and in what air the rifle was zeroed. */
@Composable
internal fun ZeroFields(f: RifleForm, set: (RifleForm) -> Unit, wide: Boolean, model: AppModel) {
    val m = stringResource(Res.string.unit_m)
    val c = stringResource(Res.string.unit_c)
    Text(stringResource(Res.string.zero_note), color = MaterialTheme.colorScheme.onSurfaceVariant)
    Fields(
        wide,
        { mod -> NumberField(stringResource(Res.string.zero_distance), f.zeroRangeM, { set(f.copy(zeroRangeM = it)) }, mod, m, 0, 10.0, 1000.0, tag = "zeroRange") },
        { mod ->
            NumberField(stringResource(Res.string.zero_temperature), f.zeroTemperatureC,
                { set(f.copy(zeroTemperatureC = it, zeroPowderC = it)) }, mod, c, from = -60.0, to = 60.0)
        },
        { mod ->
            PressureField(
                stringResource(Res.string.zero_pressure), f.zeroPressureHpa, { set(f.copy(zeroPressureHpa = it)) },
                PressureUnit.of(model.state.prefs.pressureUnit), { u -> model.setPrefs { it.copy(pressureUnit = u.key) } },
                mod, decimals = PressureUnit.of(model.state.prefs.pressureUnit).decimals + 1,
            )
        },
        { mod -> NumberField(stringResource(Res.string.zero_altitude), f.zeroAltitudeM, { set(f.copy(zeroAltitudeM = it)) }, mod, m, 0, -500.0, 6000.0) },
        { mod -> NumberField(stringResource(Res.string.zero_humidity), f.zeroHumidityPct, { set(f.copy(zeroHumidityPct = it)) }, mod, "%", 0, 0.0, 100.0) },
    )
}
