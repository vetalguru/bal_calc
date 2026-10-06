package org.vetalguru.balcalc.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuAnchorType
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.roundToInt
import kotlin.math.sin
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.coreText
import org.vetalguru.balcalc.core.Solution
import org.vetalguru.balcalc.core.Warning
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** Firing solution for one target, large enough to read at arm's length. */
@Composable
fun SolutionScreen(model: AppModel, onEditArmory: () -> Unit) {
    val st = model.state
    val sol = model.solution
    val unit = stringResource(if (st.moa) Res.string.unit_moa else Res.string.unit_mrad)
    val sampleRifle = stringResource(Res.string.sample_rifle_name)
    val sampleCartridge = stringResource(Res.string.sample_cartridge_name)
    var logging by remember { mutableStateOf(false) }
    if (logging) LogShotDialog(model, st.conditions.targetRangeM, sol.elevation) { logging = false }

    BoxWithConstraints(Modifier.fillMaxSize()) {
        val wide = maxWidth >= 600.dp
        Column(Modifier.fillMaxSize()) {
            // Rifle and cartridge pickers.
            Row(
                Modifier.fillMaxWidth().padding(12.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Picker(
                    items = st.rifles.map { it.id to it.name },
                    selected = st.currentRifleId,
                    empty = stringResource(Res.string.no_rifles),
                    onSelect = model::selectRifle,
                    modifier = Modifier.weight(1f).testTag("rifleBox"),
                )
                Picker(
                    items = st.cartridges.map { it.id to it.name },
                    selected = st.currentCartridgeId,
                    empty = stringResource(Res.string.no_cartridges),
                    onSelect = model::selectCartridge,
                    modifier = Modifier.weight(1f).testTag("cartridgeBox"),
                )
            }

            Column(
                Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(bottom = 16.dp),
                verticalArrangement = Arrangement.spacedBy(16.dp),
            ) {
                DistanceCard(st.conditions.targetRangeM, model::setTargetRange)

                if (!sol.ok && model.ready) {
                    Text(
                        coreText(sol.error),
                        color = MaterialTheme.colorScheme.error,
                        fontSize = 16.sp,
                        modifier = Modifier.padding(horizontal = 16.dp),
                    )
                }
                if (model.ready && (st.rifles.isEmpty() || st.cartridges.isEmpty())) {
                    EmptyActions(
                        wide = wide,
                        addLabel = stringResource(
                            if (st.rifles.isEmpty()) Res.string.add_rifle else Res.string.add_cartridge,
                        ),
                        onAdd = onEditArmory,
                        onSample = { model.addSample(sampleRifle, sampleCartridge) },
                    )
                }

                if (sol.ok) {
                    Corrections(sol, unit, st.conditions.windGustMps, wide)
                    Warnings(sol.warnings)
                    Row(Modifier.padding(horizontal = 16.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            if (sol.velocityScale != 1.0 || sol.dragScale != 1.0) {
                                stringResource(Res.string.trued, sol.velocityScale.fixed(4), sol.dragScale.fixed(3))
                            } else {
                                stringResource(Res.string.not_trued)
                            },
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.weight(1f),
                        )
                        OutlinedButton(onClick = { logging = true }, modifier = Modifier.testTag("logHitSolution")) {
                            Text(stringResource(Res.string.log_hit))
                        }
                    }
                    ReticleCard(model, sol, wide)
                }

                QuickWind(
                    speed = st.conditions.windSpeed,
                    fromDeg = st.conditions.windFromDeg,
                    onSpeed = { v -> model.updateConditions { it.copy(windSpeed = v) } },
                    onDirection = { d -> model.updateConditions { it.copy(windFromDeg = d) } },
                    zoneNote = if (st.conditions.windZones.isEmpty()) "" else stringResource(
                        Res.string.quick_zone_note,
                        st.conditions.windZones.size + 1,
                        st.conditions.windUntilM.roundToInt(),
                    ),
                )

                MovingTargetCard(model, sol, unit)

                if (sol.ok) Details(sol, st.conditions.targetHeightCm, wide)
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun Picker(
    items: List<Pair<Long, String>>,
    selected: Long,
    empty: String,
    onSelect: (Long) -> Unit,
    modifier: Modifier = Modifier,
) {
    var open by remember { mutableStateOf(false) }
    val current = items.firstOrNull { it.first == selected }?.second ?: empty
    ExposedDropdownMenuBox(expanded = open, onExpandedChange = { open = it && items.isNotEmpty() }, modifier = modifier) {
        OutlinedTextField(
            value = current,
            onValueChange = {},
            readOnly = true,
            singleLine = true,
            enabled = items.isNotEmpty(),
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = open) },
            modifier = Modifier.menuAnchor(ExposedDropdownMenuAnchorType.PrimaryNotEditable).fillMaxWidth(),
        )
        ExposedDropdownMenu(expanded = open, onDismissRequest = { open = false }) {
            items.forEach { (id, name) ->
                DropdownMenuItem(text = { Text(name) }, onClick = {
                    open = false
                    if (id != selected) onSelect(id)
                })
            }
        }
    }
}

@Composable
private fun DistanceCard(rangeM: Double, onRange: (Double) -> Unit) {
    Card(Modifier.fillMaxWidth().padding(horizontal = 12.dp)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(stringResource(Res.string.distance), color = MaterialTheme.colorScheme.onSurfaceVariant)
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                RangeField(rangeM, onRange, Modifier.weight(1f))
                Text(stringResource(Res.string.unit_m), fontSize = 20.sp)
            }
            Row(Modifier.fillMaxWidth()) {
                for (step in listOf(-100, -10, 10, 100)) {
                    TextButton(onClick = { onRange(rangeM + step) }, modifier = Modifier.weight(1f)) {
                        Text((if (step > 0) "+" else "−") + abs(step))
                    }
                }
            }
            Slider(
                value = rangeM.toFloat().coerceIn(50f, 2500f),
                onValueChange = { onRange((it / 5f).roundToInt() * 5.0) },
                valueRange = 50f..2500f,
            )
        }
    }
}

/** The big distance input: whole metres, committed on Done or focus loss. */
@Composable
private fun RangeField(rangeM: Double, onRange: (Double) -> Unit, modifier: Modifier) {
    var text by remember { mutableStateOf(rangeM.roundToInt().toString()) }
    var focused by remember { mutableStateOf(false) }
    LaunchedEffect(rangeM, focused) { if (!focused) text = rangeM.roundToInt().toString() }
    val focus = LocalFocusManager.current
    OutlinedTextField(
        value = text,
        onValueChange = { t -> if (t.length <= 4 && t.all(Char::isDigit)) text = t },
        singleLine = true,
        textStyle = MaterialTheme.typography.displaySmall.copy(fontWeight = FontWeight.Bold, textAlign = TextAlign.Center),
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number, imeAction = ImeAction.Done),
        keyboardActions = KeyboardActions(onDone = { focus.clearFocus() }),
        modifier = modifier.testTag("range").onFocusChanged {
            if (focused && !it.isFocused) text.toIntOrNull()?.let { m -> onRange(m.toDouble()) }
            focused = it.isFocused
        },
    )
}

@Composable
private fun EmptyActions(wide: Boolean, addLabel: String, onAdd: () -> Unit, onSample: () -> Unit) {
    val buttons: @Composable (Modifier) -> Unit = { m ->
        Button(onClick = onAdd, modifier = m) { Text(addLabel) }
        OutlinedButton(onClick = onSample, modifier = m.testTag("sample")) { Text(stringResource(Res.string.try_sample)) }
    }
    if (wide) {
        Row(Modifier.fillMaxWidth().padding(horizontal = 16.dp), horizontalArrangement = Arrangement.spacedBy(12.dp, Alignment.CenterHorizontally)) {
            buttons(Modifier)
        }
    } else {
        Column(Modifier.fillMaxWidth().padding(horizontal = 16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            buttons(Modifier.fillMaxWidth())
        }
    }
}

@Composable
private fun Corrections(sol: Solution, unit: String, gustMps: Double, wide: Boolean) {
    val elevation: @Composable (Modifier) -> Unit = { m ->
        CorrectionTile(
            title = stringResource(Res.string.elevation),
            direction = stringResource(if (sol.elevation >= 0) Res.string.up else Res.string.down),
            value = abs(sol.elevation).fixed(2),
            unit = unit,
            clicks = if (sol.hasScope) stringResource(Res.string.clicks, abs(sol.elevationClicks).roundToInt()) else "",
            tag = "elevation",
            modifier = m,
        )
    }
    val windage: @Composable (Modifier) -> Unit = { m ->
        CorrectionTile(
            title = stringResource(Res.string.windage),
            direction = when {
                abs(sol.windage) < 0.005 -> ""
                sol.windage > 0 -> stringResource(Res.string.right)
                else -> stringResource(Res.string.left)
            },
            value = abs(sol.windage).fixed(2),
            unit = unit,
            clicks = if (sol.hasScope) stringResource(Res.string.clicks, abs(sol.windageClicks).roundToInt()) else "",
            tag = "windage",
            modifier = m,
            extra = if (sol.hasGust) {
                val side = when {
                    abs(sol.gustWindage) < 0.005 -> ""
                    sol.gustWindage > 0 -> " " + stringResource(Res.string.right)
                    else -> " " + stringResource(Res.string.left)
                }
                val clicks = if (sol.hasScope) ", " + stringResource(Res.string.clicks, abs(sol.gustWindageClicks).roundToInt()) else ""
                stringResource(Res.string.gust_windage, gustMps.fixed(1), "${abs(sol.gustWindage).fixed(2)} $unit$side$clicks")
            } else {
                ""
            },
        )
    }
    if (wide) {
        Row(Modifier.padding(horizontal = 12.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            elevation(Modifier.weight(1f))
            windage(Modifier.weight(1f))
        }
    } else {
        Column(Modifier.padding(horizontal = 12.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            elevation(Modifier.fillMaxWidth())
            windage(Modifier.fillMaxWidth())
        }
    }
}

@Composable
private fun CorrectionTile(
    title: String,
    direction: String,
    value: String,
    unit: String,
    clicks: String,
    tag: String,
    modifier: Modifier,
    extra: String = "",
) {
    Card(modifier, elevation = CardDefaults.cardElevation(defaultElevation = 3.dp)) {
        Column(Modifier.padding(16.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(title, fontSize = 15.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                Spacer(Modifier.weight(1f))
                Text(direction, fontSize = 18.sp, fontWeight = FontWeight.Bold, color = MaterialTheme.colorScheme.primary)
            }
            Row(verticalAlignment = Alignment.Bottom, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(value, fontSize = 56.sp, fontWeight = FontWeight.Bold, modifier = Modifier.testTag(tag))
                Text(unit, fontSize = 18.sp, modifier = Modifier.padding(bottom = 12.dp))
            }
            if (clicks.isNotEmpty()) Text(clicks, fontSize = 18.sp)
            if (extra.isNotEmpty()) {
                Text(extra, fontSize = 16.sp, color = MaterialTheme.colorScheme.primary, modifier = Modifier.testTag("${tag}Gust"))
            }
        }
    }
}

/** The reticle with the hold, the hold mode and the turret settings. */
@Composable
private fun ReticleCard(model: AppModel, sol: Solution, wide: Boolean) {
    val reticle: @Composable (Modifier) -> Unit = { m ->
        ReticleView(if (sol.hasReticle) sol.reticleDefinition else "", sol.targetX, sol.targetY, m.aspectRatio(1f))
    }
    val text: @Composable (Modifier) -> Unit = { m ->
        Column(m, verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(
                if (sol.hasReticle) sol.reticleName else stringResource(Res.string.no_reticle),
                fontWeight = FontWeight.Bold,
            )
            ChoiceField(
                stringResource(Res.string.hold_mode),
                listOf(
                    "dial_elevation" to stringResource(Res.string.hold_dial_elevation),
                    "hold" to stringResource(Res.string.hold_everything),
                    "dial" to stringResource(Res.string.dial_everything),
                ),
                model.state.holdMode, model::setHoldMode,
            )
            if (sol.dialElevationClicks != 0.0 || sol.dialWindageClicks != 0.0) {
                Text(
                    stringResource(
                        Res.string.turrets,
                        abs(sol.dialElevationClicks).roundToInt(),
                        stringResource(if (sol.dialElevationClicks >= 0) Res.string.up_lower else Res.string.down_lower),
                        abs(sol.dialWindageClicks).roundToInt(),
                        stringResource(if (sol.dialWindageClicks >= 0) Res.string.right_lower else Res.string.left_lower),
                    ),
                    fontSize = 16.sp,
                )
            }
            // Reticle marks are counted in the reticle's own units.
            val moa = sol.hasReticle && sol.reticleUnits == "moa"
            val perUnit = if (moa) 0.29088821 else 1.0 // mrad per unit
            Text(
                stringResource(
                    Res.string.target_on_mark,
                    (abs(sol.targetY) / perUnit).fixed(2),
                    stringResource(if (moa) Res.string.unit_moa else Res.string.unit_mrad),
                    stringResource(if (sol.targetY <= 0) Res.string.below else Res.string.above),
                    (abs(sol.targetX) / perUnit).fixed(2),
                    stringResource(if (sol.targetX <= 0) Res.string.left_lower else Res.string.right_lower),
                ),
                fontSize = 16.sp,
            )
            if (sol.focalPlane == "sfp" && sol.maxMagnification > sol.minMagnification) {
                Text(
                    stringResource(Res.string.sfp_magnification, sol.magnification.fixed(1)),
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Slider(
                    value = sol.magnification.toFloat(),
                    onValueChange = { v -> model.updateConditions { it.copy(magnification = (v * 2).roundToInt() / 2.0) } },
                    valueRange = sol.minMagnification.toFloat()..sol.maxMagnification.toFloat(),
                )
            }
        }
    }
    Card(Modifier.fillMaxWidth().padding(horizontal = 12.dp)) {
        if (wide) {
            Row(Modifier.padding(16.dp), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                reticle(Modifier.width(320.dp))
                text(Modifier.weight(1f))
            }
        } else {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                reticle(Modifier.fillMaxWidth())
                text(Modifier.fillMaxWidth())
            }
        }
    }
}

@Composable
private fun QuickWind(
    speed: Double,
    fromDeg: Double,
    onSpeed: (Double) -> Unit,
    onDirection: (Double) -> Unit,
    zoneNote: String = "",
) {
    Card(Modifier.fillMaxWidth().padding(horizontal = 12.dp).testTag("quickWind")) {
        // The dial on the left, everything else in the rest of the width. The
        // dial is a third of the card (84..112 dp), so a 320 dp phone, a
        // 412 dp one and a tablet all keep the text on its lines.
        BoxWithConstraints(Modifier.padding(16.dp)) {
            val dial = (maxWidth * 0.32f).coerceIn(84.dp, 112.dp)
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(16.dp),
            ) {
                WindDial(fromDeg, onDirection, Modifier.size(dial))
                Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                    NumberField(
                        label = stringResource(Res.string.wind_speed),
                        value = speed,
                        onEdited = onSpeed,
                        unit = stringResource(Res.string.unit_mps),
                        from = 0.0,
                        to = 40.0,
                        modifier = Modifier.fillMaxWidth().padding(bottom = 6.dp),
                        tag = "windSpeed",
                    )
                    Text(
                        stringResource(Res.string.wind_from),
                        style = MaterialTheme.typography.labelMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Text(
                        stringResource(Res.string.wind_clock, clockHour(fromDeg)) + " · ${fromDeg.roundToInt()}°",
                        style = MaterialTheme.typography.titleMedium,
                    )
                    Text(
                        stringResource(Res.string.wind_clock_hint),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    if (zoneNote.isNotEmpty()) {
                        Text(zoneNote, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.primary)
                    }
                }
            }
        }
    }
}

/** Clock hour of a direction (0° = 12 o'clock = from the target). */
fun clockHour(deg: Double): Int {
    val h = (((deg % 360) + 360) % 360 / 30).roundToInt() % 12
    return if (h == 0) 12 else h
}

/** Wind direction (where it blows from), set by touching or dragging the dial; 15° steps. */
@Composable
fun WindDial(fromDeg: Double, onChange: (Double) -> Unit, modifier: Modifier = Modifier) {
    val ring = MaterialTheme.colorScheme.outline
    val knob = MaterialTheme.colorScheme.primary
    fun angleAt(p: Offset, w: Float, h: Float): Double {
        val a = atan2((p.x - w / 2).toDouble(), (h / 2 - p.y).toDouble()) * 180 / PI
        return (((a + 360) % 360) / 15).roundToInt() * 15.0 % 360
    }
    Box(modifier.testTag("windDial")) {
        Canvas(
            Modifier.fillMaxSize()
                .pointerInput(Unit) {
                    detectTapGestures { onChange(angleAt(it, size.width.toFloat(), size.height.toFloat())) }
                }
                .pointerInput(Unit) {
                    detectDragGestures { change, _ ->
                        onChange(angleAt(change.position, size.width.toFloat(), size.height.toFloat()))
                    }
                },
        ) {
            val r = size.minDimension / 2 - 8.dp.toPx()
            drawCircle(ring, radius = r, style = Stroke(2.dp.toPx()))
            val a = fromDeg * PI / 180
            val p = Offset(center.x + (r * sin(a)).toFloat(), center.y - (r * cos(a)).toFloat())
            drawLine(knob, p, center, strokeWidth = 2.dp.toPx())
            drawCircle(knob, radius = 7.dp.toPx(), center = p)
        }
    }
}

@Composable
private fun Details(sol: Solution, targetHeightCm: Double, wide: Boolean) {
    val m = stringResource(Res.string.unit_mps)
    val cm = stringResource(Res.string.unit_cm)
    val metres = stringResource(Res.string.unit_m)
    val items = listOf(
        Triple(stringResource(Res.string.velocity), "${sol.velocity.fixed(0)} $m", false),
        Triple(stringResource(Res.string.energy), "${sol.energy.fixed(0)} ${stringResource(Res.string.unit_j)}", false),
        Triple(stringResource(Res.string.time_of_flight), "${sol.time.fixed(3)} ${stringResource(Res.string.unit_s)}", false),
        Triple(stringResource(Res.string.mach), sol.mach.fixed(2), false),
        Triple(stringResource(Res.string.drop), "${sol.dropCm.fixed(1)} $cm", false),
        Triple(stringResource(Res.string.drift), "${sol.windageCm.fixed(1)} $cm", false),
        Triple(stringResource(Res.string.spin_drift), "${sol.spinDriftCm.fixed(1)} $cm", false),
        Triple(stringResource(Res.string.muzzle_velocity), "${sol.muzzleVelocity.fixed(1)} $m", false),
        Triple(
            stringResource(Res.string.stability),
            if (sol.stability > 0) sol.stability.fixed(2) else "—",
            sol.stability > 0 && sol.stability < 1.3,
        ),
        Triple(
            stringResource(Res.string.apex),
            stringResource(Res.string.apex_value, sol.apexCm.fixed(1), sol.apexRangeM.roundToInt()),
            false,
        ),
        Triple(
            stringResource(Res.string.point_blank, targetHeightCm.roundToInt()),
            if (sol.pointBlankFarM > 0) {
                "${sol.pointBlankNearM.roundToInt()}–${sol.pointBlankFarM.roundToInt()} $metres"
            } else {
                "—"
            },
            false,
        ),
        Triple(stringResource(Res.string.density_altitude), "${sol.densityAltitudeM.roundToInt()} $metres", false),
    )
    Card(Modifier.fillMaxWidth().padding(horizontal = 12.dp)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            items.chunked(if (wide) 4 else 2).forEach { row ->
                Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                    row.forEach { (label, value, warn) -> Detail(label, value, warn, Modifier.weight(1f)) }
                    repeat((if (wide) 4 else 2) - row.size) { Spacer(Modifier.weight(1f)) }
                }
            }
        }
    }
}

/** What the shooter should know before trusting the numbers; nothing when all is well. */
@Composable
private fun Warnings(warnings: List<Warning>) {
    if (warnings.isEmpty()) return
    Card(
        Modifier.fillMaxWidth().padding(horizontal = 12.dp).testTag("warnings"),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.errorContainer,
            contentColor = MaterialTheme.colorScheme.onErrorContainer,
        ),
    ) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            warnings.forEach { w ->
                val text = warningText(w) ?: return@forEach
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("⚠", fontWeight = FontWeight.Bold)
                    Text(text, modifier = Modifier.testTag("warning_${w.code}"))
                }
            }
        }
    }
}

@Composable
private fun warningText(w: Warning): String? {
    val signed = { v: Double, d: Int -> (if (v > 0) "+" else "") + v.fixed(d) }
    return when (w.code) {
        "unstable" -> stringResource(Res.string.warn_unstable, w.value.fixed(2))
        "lowStability" -> stringResource(Res.string.warn_low_stability, w.value.fixed(2))
        "subsonic" -> stringResource(Res.string.warn_subsonic, w.value.fixed(2))
        "transonic" -> stringResource(Res.string.warn_transonic, w.value.fixed(2))
        "zeroTemperature" -> stringResource(Res.string.warn_zero_temperature, signed(w.value, 0))
        "zeroPressure" -> stringResource(Res.string.warn_zero_pressure, signed(w.value, 0))
        "staleWeather" -> stringResource(Res.string.warn_stale_weather, w.value.roundToInt())
        else -> null // a newer core: not known to this app yet
    }
}
