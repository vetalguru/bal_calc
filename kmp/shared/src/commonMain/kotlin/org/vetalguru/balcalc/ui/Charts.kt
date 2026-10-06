package org.vetalguru.balcalc.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.InputChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.log10
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.roundToInt
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.core.PairOption
import org.vetalguru.balcalc.core.RangeTable
import org.vetalguru.balcalc.core.TableRow
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** Colours of the compared rifles and cartridges, the current one first. */
private val SeriesColors = listOf(Transonic, Color(0xFF1E88E5), Color(0xFF8E24AA))

/** What the chart can show: one value per range-card row, in `unit`. */
private class Quantity(val key: String, val title: String, val unit: String, val decimals: Int, val value: (TableRow) -> Double)

private class Series(val label: String, val color: Color, val points: List<Pair<Double, Double>>)

/** The chart tab: a quantity along the range, for this rifle and cartridge and up to two more. */
@Composable
fun ChartPanel(model: AppModel, curve: RangeTable, maxRangeM: Double, modifier: Modifier) {
    val st = model.state
    val c = st.conditions
    val angle = stringResource(if (st.moa) Res.string.unit_moa else Res.string.unit_mrad)
    val cm = stringResource(Res.string.unit_cm)
    val quantities = buildList {
        add(Quantity("trajectory", stringResource(Res.string.q_trajectory), cm, 1) { it.dropCm })
        add(Quantity("height", stringResource(Res.string.q_height), cm, 1) { it.dropCm })
        add(Quantity("drift", stringResource(Res.string.q_drift), cm, 1) { it.windageCm })
        add(Quantity("elevation", stringResource(Res.string.q_elevation), angle, 2) { it.elevation })
        add(Quantity("windage", stringResource(Res.string.q_windage), angle, 2) { it.windage })
        add(Quantity("velocity", stringResource(Res.string.velocity), stringResource(Res.string.unit_mps), 0) { it.velocity })
        add(Quantity("energy", stringResource(Res.string.energy), stringResource(Res.string.unit_j), 0) { it.energy })
        add(Quantity("mach", stringResource(Res.string.mach), "", 2) { it.mach })
        add(Quantity("time", stringResource(Res.string.time_of_flight), stringResource(Res.string.unit_s), 2) { it.time })
        add(Quantity("spinDrift", stringResource(Res.string.spin_drift), cm, 1) { it.spinDriftCm })
        if (c.coriolis) {
            add(Quantity("coriolisDrift", stringResource(Res.string.q_coriolis_drift), cm, 1) { it.coriolisDriftCm })
            if (c.useAzimuth) add(Quantity("coriolisLift", stringResource(Res.string.q_coriolis_lift), cm, 1) { it.coriolisLiftCm })
        }
        if (c.targetSpeedMps > 0) add(Quantity("lead", stringResource(Res.string.q_lead), angle, 2) { it.lead })
    }
    var key by rememberSaveable { mutableStateOf("trajectory") }
    val q = quantities.firstOrNull { it.key == key } ?: quantities.first()

    // The other rifles and cartridges, kept while the app runs.
    val pairs = remember { mutableStateListOf<Pair<Long, Long>>() }
    var compared by remember { mutableStateOf(emptyList<RangeTable>()) }
    LaunchedEffect(model.revision, pairs.toList(), maxRangeM) {
        compared = if (pairs.isEmpty()) emptyList() else runCatching { model.compareCurves(maxRangeM, 250, pairs.toList()) }.getOrDefault(emptyList())
    }
    var choosing by remember { mutableStateOf(false) }
    if (choosing) {
        ComparePicker(model, onDismiss = { choosing = false }) { pair ->
            choosing = false
            val current = st.currentRifleId to st.currentCartridgeId
            if (pair != current && pair !in pairs) pairs.add(pair)
        }
    }

    val currentLabel = stringResource(Res.string.compare_current)
    val heightLabel = stringResource(Res.string.chart_height)
    val driftLabel = stringResource(Res.string.chart_drift)
    val series = buildList {
        fun points(t: RangeTable, f: (TableRow) -> Double) = if (t.ok) t.rows.map { it.rangeM to f(it) } else emptyList()
        if (q.key == "trajectory" && compared.isEmpty()) {
            add(Series(heightLabel, Transonic, points(curve) { it.dropCm }))
            add(Series(driftLabel, DriftColor, points(curve) { it.windageCm }))
        } else {
            add(Series(currentLabel, SeriesColors[0], points(curve, q.value)))
            compared.forEachIndexed { i, t -> if (t.ok) add(Series(t.label, SeriesColors[(i + 1) % SeriesColors.size], points(t, q.value))) }
        }
    }

    Column(modifier, verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(Modifier.fillMaxWidth().padding(horizontal = 4.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            ChoiceField(stringResource(Res.string.chart_show), quantities.map { it.key to it.title }, q.key, { key = it }, Modifier.weight(1f).testTag("chartQuantity"))
            if (pairs.size < 2) {
                OutlinedButton(onClick = { choosing = true }, modifier = Modifier.testTag("compare")) { Text(stringResource(Res.string.compare)) }
            }
        }
        if (pairs.isNotEmpty()) {
            Row(Modifier.padding(horizontal = 4.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                compared.forEachIndexed { i, t ->
                    InputChip(
                        selected = true,
                        onClick = { pairs.removeAll { it == t.rifleId to t.cartridgeId } },
                        label = { Text("● " + t.label + "  ✕", color = SeriesColors[(i + 1) % SeriesColors.size], maxLines = 1) },
                        modifier = Modifier.testTag("compared$i"),
                    )
                }
            }
        }
        LineChart(
            series, if (q.key == "trajectory") cm else q.unit, q.unit == cm || q.key == "trajectory", c.targetRangeM,
            Modifier.fillMaxSize(), withZero = q.key !in setOf("velocity", "energy", "mach"),
        )
    }
}

/** A rifle, then one of the cartridges of its calibre. */
@Composable
private fun ComparePicker(model: AppModel, onDismiss: () -> Unit, onChosen: (Pair<Long, Long>) -> Unit) {
    var options by remember { mutableStateOf(emptyList<PairOption>()) }
    LaunchedEffect(Unit) { options = runCatching { model.pairOptions() }.getOrDefault(emptyList()) }
    var rifle by remember { mutableStateOf(0L) }
    var picked by remember { mutableStateOf(0L) }
    LaunchedEffect(options) {
        if (rifle == 0L) rifle = model.state.currentRifleId.takeIf { id -> options.any { it.rifleId == id } } ?: options.firstOrNull()?.rifleId ?: 0L
    }
    val cartridges = options.firstOrNull { it.rifleId == rifle }?.cartridges.orEmpty()
    // The one picked, or another cartridge than the current one.
    val cartridge = picked.takeIf { id -> cartridges.any { it.id == id } }
        ?: cartridges.firstOrNull { it.id != model.state.currentCartridgeId }?.id ?: cartridges.firstOrNull()?.id ?: 0L
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(Res.string.compare_with)) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                ChoiceField(stringResource(Res.string.compare_rifle), options.map { it.rifleId to it.rifleName }, rifle, { rifle = it }, Modifier.fillMaxWidth())
                ChoiceField(stringResource(Res.string.compare_cartridge), cartridges.map { it.id to it.name }, cartridge, { picked = it }, Modifier.fillMaxWidth().testTag("compareCartridge"))
            }
        },
        confirmButton = {
            TextButton(onClick = { onChosen(rifle to cartridge) }, enabled = rifle != 0L && cartridge != 0L, modifier = Modifier.testTag("compareAdd")) {
                Text(stringResource(Res.string.compare_add))
            }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(Res.string.cancel)) } },
    )
}

/**
 * Lines along the range with a grid, the target marked and a legend.
 * `centimetres` switches the axis to metres when the values span more than 3 m.
 */
@Composable
private fun LineChart(
    series: List<Series>,
    unit: String,
    centimetres: Boolean,
    targetM: Double,
    modifier: Modifier,
    withZero: Boolean = true, // false: the axis spans the values only (velocity, energy)
) {
    val measurer = rememberTextMeasurer()
    val fg = MaterialTheme.colorScheme.onSurface
    val accent = MaterialTheme.colorScheme.primary
    val labelM = stringResource(Res.string.unit_m)
    val labelRange = stringResource(Res.string.chart_range)
    val style = TextStyle(fontSize = 12.sp, color = fg)
    Canvas(modifier.padding(8.dp).testTag("chart")) {
        val all = series.flatMap { it.points }
        if (all.size < 2) return@Canvas
        val maxR = all.maxOf { it.first }
        var minY = all.minOf { it.second }.let { if (withZero) min(0.0, it) else it }
        var maxY = all.maxOf { it.second }.let { if (withZero) max(0.0, it) else it }
        val meters = centimetres && maxY - minY > 300
        val scale = if (meters) 0.01 else 1.0
        val pad = (maxY - minY) * 0.08 + if (centimetres) 1.0 else 1e-3
        minY -= pad
        maxY += pad

        val left = 56.dp.toPx()
        val right = size.width - 12.dp.toPx()
        val top = 12.dp.toPx()
        val bottom = size.height - 36.dp.toPx()
        fun x(r: Double) = (left + (right - left) * r / maxR).toFloat()
        fun y(v: Double) = (top + (bottom - top) * (maxY - v) / (maxY - minY)).toFloat()
        fun label(text: String, at: Offset, color: Color = fg) = drawText(measurer, text, at, style.copy(color = color))

        val grid = fg.copy(alpha = 0.15f)
        val stepR = if (maxR > 1500) 250.0 else if (maxR > 600) 100.0 else 50.0
        var r = 0.0
        var lastLabelEnd = -1f
        while (r <= maxR + 1e-6) {
            drawLine(grid, Offset(x(r), top), Offset(x(r), bottom))
            // Centred under its line; skipped where it would overlap or overflow.
            val text = measurer.measure(r.roundToInt().toString(), style)
            val at = x(r) - text.size.width / 2f
            if (at > lastLabelEnd + 4f && at + text.size.width <= size.width) {
                drawText(text, topLeft = Offset(at, bottom + 4f))
                lastLabelEnd = at + text.size.width
            }
            r += stepR
        }
        val spanY = (maxY - minY) * scale
        var stepY = 10.0.pow(floor(log10(spanY / 5)))
        if (spanY / stepY > 10) stepY *= 2
        val decimals = if (stepY < 0.1) 2 else if (stepY < 1) 1 else 0
        var yv = ceil(minY * scale / stepY) * stepY
        while (yv <= maxY * scale) {
            drawLine(grid, Offset(left, y(yv / scale)), Offset(right, y(yv / scale)))
            label(yv.fixed(decimals), Offset(4f, y(yv / scale) - 8f))
            yv += stepY
        }
        label(if (meters) labelM else unit, Offset(4f, 0f))
        label(labelRange, Offset(right - 70f, bottom + 18f))

        // Zero line and target.
        if (minY < 0 && maxY > 0) {
            drawLine(fg.copy(alpha = 0.6f), Offset(x(0.0), y(0.0)), Offset(x(maxR), y(0.0)),
                pathEffect = PathEffect.dashPathEffect(floatArrayOf(18f, 12f)))
        }
        if (targetM <= maxR) drawLine(accent, Offset(x(targetM), top), Offset(x(targetM), bottom))

        series.forEachIndexed { i, s ->
            if (s.points.size < 2) return@forEachIndexed
            val p = Path()
            s.points.forEachIndexed { k, (range, v) ->
                if (k == 0) p.moveTo(x(range), y(v)) else p.lineTo(x(range), y(v))
            }
            drawPath(p, s.color, style = Stroke(width = 2.5.dp.toPx()))
            label("● ${s.label}", Offset(left + 8f, top + 4f + 18f * i), s.color)
        }
    }
}
