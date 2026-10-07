package org.vetalguru.balcalc.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.gestures.detectVerticalDragGestures
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
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.text.TextAutoSize
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.rotate
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.ceil
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.hypot
import kotlin.math.roundToInt
import kotlin.math.sin
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/**
 * The lower part of the solution screen: a ring with the target always at
 * the top and the wind's mark dragged round it, and inside it three wheels:
 * range, wind speed, look angle.
 */
@Composable
internal fun Controller(model: AppModel, size: Dp) {
    val c = model.state.conditions
    val windUnit = WindUnit.of(model.state.prefs.windUnit)
    var editing by remember { mutableStateOf<String?>(null) }
    val m = stringResource(Res.string.unit_m)
    // A small ring has room for one neighbour above and below each value.
    val compact = size < 270.dp

    Column(
        Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp).testTag("quickWind"),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        // Smaller rather than cut on narrow phones ("11 o'clock · 330°").
        val bodyMedium = MaterialTheme.typography.bodyMedium
        BasicText(
            stringResource(Res.string.wind_direction_clock, clockHour(c.windFromDeg)) + " · ${c.windFromDeg.roundToInt()}°",
            style = bodyMedium.copy(color = LocalContentColor.current),
            maxLines = 1,
            autoSize = TextAutoSize.StepBased(minFontSize = 9.sp, maxFontSize = bodyMedium.fontSize),
            modifier = Modifier.testTag("windDirectionText"),
        )
        Box(Modifier.size(size), contentAlignment = Alignment.Center) {
            Ring(c.windFromDeg, { d -> model.updateConditions { it.copy(windFromDeg = d) } }, Modifier.fillMaxSize())
            // The wheels sit inside the ring's band; they take their own touches.
            Row(
                Modifier.padding(horizontal = size * 0.2f),
                horizontalArrangement = Arrangement.spacedBy(6.dp),
                verticalAlignment = Alignment.Top,
            ) {
                Wheel(
                    value = c.targetRangeM, step = 10.0, from = 10.0, to = 3000.0,
                    text = { it.roundToInt().toString() },
                    onChange = model::setTargetRange, onEdit = { editing = "range" },
                    tag = "range", accent = MaterialTheme.colorScheme.primary, compact = compact,
                    header = { WheelHeader(m) }, label = stringResource(Res.string.distance),
                    modifier = Modifier.weight(1.15f),
                )
                Wheel(
                    value = c.windSpeed * windUnit.perMps, step = 1.0, from = 0.0, to = MAX_WIND_MPS * windUnit.perMps,
                    text = { formatNumber(it, 1) },
                    onChange = { v -> model.updateConditions { it.copy(windSpeed = (v / windUnit.perMps).coerceIn(0.0, MAX_WIND_MPS)) } },
                    onEdit = { editing = "windSpeed" },
                    tag = "windSpeed", accent = WindColor, compact = compact,
                    header = {
                        UnitPicker(
                            stringResource(windUnit.label),
                            windUnitMenu { u -> model.setPrefs { it.copy(windUnit = u.key) } },
                            "windSpeed",
                        )
                    },
                    label = stringResource(Res.string.wind),
                    modifier = Modifier.weight(1f),
                )
                Wheel(
                    value = c.lookAngleDeg, step = 1.0, from = -60.0, to = 60.0,
                    text = { a -> (if (a.roundToInt() > 0) "↑" else if (a.roundToInt() < 0) "↓" else "") + "${abs(a.roundToInt())}°" },
                    onChange = { v -> model.updateConditions { it.copy(lookAngleDeg = v) } },
                    onEdit = { editing = "lookAngle" },
                    tag = "lookAngle", accent = MaterialTheme.colorScheme.outline, compact = compact,
                    header = { WheelHeader("°") },
                    label = stringResource(Res.string.look_angle_short),
                    hint = stringResource(Res.string.shot_angle),
                    modifier = Modifier.weight(1f),
                )
            }
        }
        if (c.windZones.isNotEmpty()) {
            Text(
                stringResource(Res.string.quick_zone_note, c.windZones.size + 1, c.windUntilM.roundToInt()),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.primary,
            )
        }
    }

    when (editing) {
        "range" -> WheelEditDialog(stringResource(Res.string.distance), c.targetRangeM, m, 0, 10.0, 3000.0, "range",
            onDone = { editing = null }) { model.setTargetRange(it) }
        "windSpeed" -> WheelEditDialog(stringResource(Res.string.wind_speed), c.windSpeed * windUnit.perMps,
            stringResource(windUnit.label), 1, 0.0, MAX_WIND_MPS * windUnit.perMps, "windSpeed", onDone = { editing = null }) { v ->
            model.updateConditions { it.copy(windSpeed = v / windUnit.perMps) }
        }
        "lookAngle" -> WheelEditDialog(stringResource(Res.string.look_angle_short), c.lookAngleDeg, "°", 1, -60.0, 60.0,
            "lookAngle", hint = stringResource(Res.string.shot_angle_hint), onDone = { editing = null }) { v ->
            model.updateConditions { it.copy(lookAngleDeg = v) }
        }
    }
}

/** The wind's colour on the ring and its wheel. */
private val WindColor = Color(0xFF1D9E75)

@Composable
private fun WheelHeader(text: String) {
    Text(text, style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
}

/**
 * The ring: the target fixed at the top with the line of fire, the wind's
 * mark where it blows from (an arrow from it to the centre: where it goes).
 * A touch or drag on the ring sets the wind in 15° steps.
 */
@Composable
private fun Ring(fromDeg: Double, onChange: (Double) -> Unit, modifier: Modifier) {
    val track = MaterialTheme.colorScheme.outlineVariant
    val target = MaterialTheme.colorScheme.primary
    val onMark = MaterialTheme.colorScheme.surface
    val change by rememberUpdatedState(onChange)
    fun angleAt(p: Offset, w: Float, h: Float): Double? {
        val dx = p.x - w / 2
        val dy = h / 2 - p.y
        // The centre is the wheels': only the ring's band turns the wind.
        if (hypot(dx, dy) < minOf(w, h) / 2 * 0.62f) return null
        val a = atan2(dx.toDouble(), dy.toDouble()) * 180 / PI
        return (((a + 360) % 360) / 15).roundToInt() * 15.0 % 360
    }
    Canvas(
        modifier.testTag("windDial")
            .pointerInput(Unit) {
                detectTapGestures { p -> angleAt(p, size.width.toFloat(), size.height.toFloat())?.let(change) }
            }
            .pointerInput(Unit) {
                detectDragGestures { ch, _ -> angleAt(ch.position, size.width.toFloat(), size.height.toFloat())?.let(change) }
            },
    ) {
        val r = size.minDimension / 2 - 32.dp.toPx()
        val c = center
        drawCircle(track, r, c, style = Stroke(10.dp.toPx()))
        // The target above the ring, out of the wind mark's way; a notch on
        // the ring points to it (the line of fire).
        drawTarget(Offset(c.x, c.y - r - 19.dp.toPx()), target, onMark)
        val notch = Path().apply {
            moveTo(c.x, c.y - r - 5.dp.toPx())
            lineTo(c.x - 7.dp.toPx(), c.y - r + 6.dp.toPx())
            lineTo(c.x + 7.dp.toPx(), c.y - r + 6.dp.toPx())
            close()
        }
        drawPath(notch, target)
        // The wind: its mark on the ring, its arrow to the centre.
        val a = fromDeg * PI / 180
        val dir = Offset(sin(a).toFloat(), -cos(a).toFloat())
        val mark = c + dir * r
        val from = c + dir * (r - 18.dp.toPx())
        val to = c + dir * (r * 0.71f) // short of the wheels
        val thick = 3.dp.toPx()
        drawLine(WindColor, from, to, thick)
        val back = (from - to) / (from - to).getDistance() * 11.dp.toPx()
        val side = Offset(-back.y, back.x) * 0.55f
        drawLine(WindColor, to, to + back + side, thick)
        drawLine(WindColor, to, to + back - side, thick)
        drawWindMark(mark, onMark)
    }
}

/** The target: a crosshair in a circle. */
private fun DrawScope.drawTarget(at: Offset, color: Color, inner: Color) {
    val r = 12.dp.toPx()
    drawCircle(color, r, at)
    drawCircle(inner, r * 0.5f, at, style = Stroke(1.5.dp.toPx()))
    drawLine(inner, Offset(at.x, at.y - r * 0.8f), Offset(at.x, at.y + r * 0.8f), 1.5.dp.toPx())
    drawLine(inner, Offset(at.x - r * 0.8f, at.y), Offset(at.x + r * 0.8f, at.y), 1.5.dp.toPx())
}

/** The wind's mark on the ring: three wavy lines in a circle. */
private fun DrawScope.drawWindMark(at: Offset, lines: Color) {
    val r = 15.dp.toPx()
    drawCircle(WindColor, r, at)
    val w = r * 1.1f
    val stroke = Stroke(1.8.dp.toPx())
    for (i in -1..1) {
        val y = at.y + i * r * 0.42f
        val p = Path().apply {
            moveTo(at.x - w / 2, y)
            quadraticTo(at.x - w / 4, y - r * 0.22f, at.x, y)
            quadraticTo(at.x + w / 4, y + r * 0.22f, at.x + w / 2 - (if (i == 1) r * 0.2f else 0f), y)
        }
        drawPath(p, lines, style = stroke)
    }
}

/**
 * A wheel: the value in a box, its neighbours above and below. Dragged up
 * or down it steps by [step]; a tap on a neighbour steps to it; a tap on the
 * value opens the keyboard ([onEdit]).
 */
@Composable
private fun Wheel(
    value: Double,
    step: Double,
    from: Double,
    to: Double,
    text: (Double) -> String,
    onChange: (Double) -> Unit,
    onEdit: () -> Unit,
    tag: String,
    accent: Color,
    header: @Composable () -> Unit,
    label: String,
    modifier: Modifier = Modifier,
    hint: String? = null,
    compact: Boolean = false,
) {
    val current by rememberUpdatedState(value)
    val change by rememberUpdatedState(onChange)
    // The next value on the step grid: 305 m goes to 310 or 300.
    fun stepped(v: Double, dir: Int): Double {
        val k = v / step
        val n = if (dir > 0) floor(k + 1e-9) + 1 else ceil(k - 1e-9) - 1
        return (n * step).coerceIn(from, to)
    }
    val rowH = 26.dp
    val faded = MaterialTheme.colorScheme.onSurfaceVariant
    Column(
        modifier.pointerInput(Unit) {
            var acc = 0f
            val px = rowH.toPx()
            detectVerticalDragGestures(onDragEnd = { acc = 0f }, onDragCancel = { acc = 0f }) { ch, dy ->
                ch.consume()
                acc += dy
                // Up brings the larger values in from below.
                while (acc <= -px) { acc += px; change(stepped(current, +1)) }
                while (acc >= px) { acc -= px; change(stepped(current, -1)) }
            }
        },
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Box(Modifier.height(20.dp), contentAlignment = Alignment.Center) { header() }
        val below = stepped(value, -1)
        val above = stepped(value, +1)
        val far = @Composable { v: Double, show: Boolean ->
            Text(
                if (show) text(v) else "", Modifier.height(rowH), color = faded.copy(alpha = 0.45f),
                style = MaterialTheme.typography.bodySmall, maxLines = 1,
            )
        }
        if (!compact) far(stepped(below, -1), below > from)
        Text(
            if (value > from) text(below) else "",
            Modifier.height(rowH).clickable(enabled = value > from) { change(below) }.testTag("${tag}Prev"),
            color = faded, style = MaterialTheme.typography.bodyMedium, maxLines = 1,
        )
        Box(
            Modifier.fillMaxWidth().height(40.dp)
                .background(accent.copy(alpha = 0.12f), RoundedCornerShape(8.dp))
                .border(1.5.dp, accent, RoundedCornerShape(8.dp))
                .clickable(onClick = onEdit)
                .testTag(tag),
            contentAlignment = Alignment.Center,
        ) {
            BasicText(
                text(value),
                style = MaterialTheme.typography.titleLarge.copy(
                    color = LocalContentColor.current, fontWeight = FontWeight.Bold, textAlign = TextAlign.Center,
                ),
                maxLines = 1,
                autoSize = TextAutoSize.StepBased(minFontSize = 12.sp, maxFontSize = MaterialTheme.typography.titleLarge.fontSize),
            )
        }
        Text(
            if (value < to) text(above) else "",
            Modifier.height(rowH).padding(top = 4.dp).clickable(enabled = value < to) { change(above) }.testTag("${tag}Next"),
            color = faded, style = MaterialTheme.typography.bodyMedium, maxLines = 1,
        )
        if (!compact) far(stepped(above, +1), above < to)
        val labelText = @Composable {
            Text(
                label, style = MaterialTheme.typography.labelSmall, color = faded, maxLines = 2,
                textAlign = TextAlign.Center, modifier = Modifier.width(80.dp),
            )
        }
        if (hint != null) Hint(hint, Modifier.testTag("${tag}Hint")) { labelText() } else labelText()
    }
}

/** A wheel's value typed: the keyboard opens on the field; Done or OK sets it. */
@Composable
private fun WheelEditDialog(
    title: String,
    value: Double,
    unit: String,
    decimals: Int,
    from: Double,
    to: Double,
    tag: String,
    hint: String? = null,
    onDone: () -> Unit,
    onValue: (Double) -> Unit,
) {
    val focus = LocalFocusManager.current
    // The keyboard comes up with the dialog.
    val field = remember { FocusRequester() }
    LaunchedEffect(Unit) { field.requestFocus() }
    AlertDialog(
        onDismissRequest = onDone,
        title = { Text(title) },
        text = {
            NumberField(
                title, value, onValue, Modifier.fillMaxWidth().focusRequester(field), unit, decimals, from, to,
                "${tag}Input", hint = hint, onCommitted = onDone,
            )
        },
        // Losing the focus commits the field, which closes the dialog.
        confirmButton = { TextButton(onClick = { focus.clearFocus(); onDone() }, modifier = Modifier.testTag("wheelOk")) { Text("OK") } },
        dismissButton = { TextButton(onClick = onDone) { Text(stringResource(Res.string.cancel)) } },
    )
}
