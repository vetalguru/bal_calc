package org.vetalguru.balcalc.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
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
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.roundToInt
import kotlin.math.sin
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** The lower part: range, wind and angle, each changed with the thumb. */
@Composable
internal fun Controller(model: AppModel, narrow: Boolean) {
    val c = model.state.conditions
    val angle = { d: Double -> model.updateConditions { it.copy(lookAngleDeg = (it.lookAngleDeg + d).coerceIn(-60.0, 60.0)) } }
    Card(
        Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 6.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerHigh),
    ) {
        Column(Modifier.padding(8.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(2.dp)) {
                if (!narrow) Step("−100") { model.setTargetRange(c.targetRangeM - 100) }
                Step("−10") { model.setTargetRange(c.targetRangeM - 10) }
                RangeField(c.targetRangeM, model::setTargetRange, Modifier.weight(1f), showUnit = !narrow)
                Step("+10") { model.setTargetRange(c.targetRangeM + 10) }
                if (!narrow) Step("+100") { model.setTargetRange(c.targetRangeM + 100) }
            }
            HorizontalDivider()
            QuickWind(
                speed = c.windSpeed,
                fromDeg = c.windFromDeg,
                onSpeed = { v -> model.updateConditions { it.copy(windSpeed = v.coerceIn(0.0, 40.0)) } },
                onDirection = { d -> model.updateConditions { it.copy(windFromDeg = d) } },
                zoneNote = if (c.windZones.isEmpty()) "" else stringResource(
                    Res.string.quick_zone_note,
                    c.windZones.size + 1,
                    c.windUntilM.roundToInt(),
                ),
            )
            HorizontalDivider()
            // Look angle: uphill positive.
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(2.dp)) {
                Text(
                    stringResource(Res.string.look_angle_short),
                    style = MaterialTheme.typography.labelLarge,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 1,
                    modifier = Modifier.padding(start = 4.dp).weight(1f),
                )
                if (!narrow) Step("−5") { angle(-5.0) }
                Step("−1") { angle(-1.0) }
                Text(
                    "${c.lookAngleDeg.roundToInt()}°",
                    style = MaterialTheme.typography.titleLarge,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.width(56.dp).testTag("lookAngle"),
                )
                Step("+1") { angle(1.0) }
                if (!narrow) Step("+5") { angle(5.0) }
            }
        }
    }
}

/** A small ± button of the controller. */
@Composable
internal fun Step(label: String, onClick: () -> Unit) {
    FilledTonalButton(
        onClick = onClick,
        contentPadding = PaddingValues(horizontal = 2.dp),
        modifier = Modifier.width(50.dp).height(40.dp),
    ) { Text(label, fontSize = 13.sp, maxLines = 1) }
}

/** The big distance input: whole metres, committed on Done or focus loss. */
@Composable
internal fun RangeField(rangeM: Double, onRange: (Double) -> Unit, modifier: Modifier, showUnit: Boolean = true) {
    var text by remember { mutableStateOf(rangeM.roundToInt().toString()) }
    var focused by remember { mutableStateOf(false) }
    LaunchedEffect(rangeM, focused) { if (!focused) text = rangeM.roundToInt().toString() }
    val focus = LocalFocusManager.current
    OutlinedTextField(
        value = text,
        onValueChange = { t -> if (t.length <= 4 && t.all(Char::isDigit)) text = t },
        singleLine = true,
        textStyle = MaterialTheme.typography.headlineMedium.copy(fontWeight = FontWeight.Bold, textAlign = TextAlign.Center),
        suffix = if (showUnit) ({ Text(stringResource(Res.string.unit_m)) }) else null,
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number, imeAction = ImeAction.Done),
        keyboardActions = KeyboardActions(onDone = { focus.clearFocus() }),
        modifier = modifier.testTag("range").onFocusChanged {
            if (focused && !it.isFocused) text.toIntOrNull()?.let { m -> onRange(m.toDouble()) }
            focused = it.isFocused
        },
    )
}

@Composable
internal fun QuickWind(
    speed: Double,
    fromDeg: Double,
    onSpeed: (Double) -> Unit,
    onDirection: (Double) -> Unit,
    zoneNote: String = "",
) {
    // The dial on the left; the speed with its steps and the clock beside it.
    Row(
        Modifier.fillMaxWidth().testTag("quickWind"),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        WindDial(fromDeg, onDirection, Modifier.size(76.dp))
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(2.dp)) {
                Step("−1") { onSpeed(speed - 1) }
                NumberField(
                    label = stringResource(Res.string.wind_speed),
                    value = speed,
                    onEdited = onSpeed,
                    unit = stringResource(Res.string.unit_mps),
                    from = 0.0,
                    to = 40.0,
                    modifier = Modifier.weight(1f),
                    tag = "windSpeed",
                )
                Step("+1") { onSpeed(speed + 1) }
            }
            Text(
                stringResource(Res.string.wind_from) + " " +
                    stringResource(Res.string.wind_clock, clockHour(fromDeg)) + " · ${fromDeg.roundToInt()}°",
                style = MaterialTheme.typography.bodyMedium,
                maxLines = 1,
            )
            if (zoneNote.isNotEmpty()) {
                Text(zoneNote, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.primary)
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
