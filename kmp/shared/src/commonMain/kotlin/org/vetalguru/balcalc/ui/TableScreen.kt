package org.vetalguru.balcalc.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.PrimaryTabRow
import androidx.compose.material3.Tab
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.log10
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.roundToInt
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.coreText
import org.vetalguru.balcalc.core.RangeTable
import org.vetalguru.balcalc.core.TableRow
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

val Transonic = Color(0xFFEF6C00) // below Mach 1.2
val DriftColor = Color(0xFF00897B)

private class TableCol(val title: String, val decimals: Int, val angle: Boolean, val value: (TableRow) -> Double)

/** Range card and trajectory chart for the current rifle, cartridge and conditions. */
@Composable
fun TableScreen(model: AppModel, onRangeChosen: () -> Unit) {
    val st = model.state
    var tab by rememberSaveable { mutableStateOf(0) }
    var table by remember { mutableStateOf(RangeTable()) }
    var curve by remember { mutableStateOf(RangeTable()) }
    LaunchedEffect(model.revision, st.tableFromM, st.tableToM, st.tableStepM) {
        if (!model.ready) return@LaunchedEffect
        runCatching {
            table = model.rangeTable()
            curve = model.trajectory(max(st.tableToM, st.conditions.targetRangeM), 250)
        }
    }
    fun span(key: String, v: Double) = model.setSettings(buildJsonObject { put(key, v) })
    val m = stringResource(Res.string.unit_m)

    Column(Modifier.fillMaxSize()) {
        PrimaryTabRow(selectedTabIndex = tab) {
            Tab(tab == 0, { tab = 0 }, text = { Text(stringResource(Res.string.tab_table)) })
            Tab(tab == 1, { tab = 1 }, text = { Text(stringResource(Res.string.tab_chart)) }, modifier = Modifier.testTag("chartTab"))
        }
        Row(Modifier.fillMaxWidth().padding(12.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            NumberField(stringResource(Res.string.from), st.tableFromM, { span("tableFromM", it) }, Modifier.weight(1f), m, 0, 0.0, 3000.0)
            NumberField(stringResource(Res.string.to), st.tableToM, { span("tableToM", it) }, Modifier.weight(1f), m, 0, 10.0, 3000.0)
            NumberField(stringResource(Res.string.step), st.tableStepM, { span("tableStepM", it) }, Modifier.weight(1f), m, 0, 5.0, 500.0)
        }
        if (!table.ok && model.ready) {
            Text(coreText(table.error), color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(12.dp))
        }
        when (tab) {
            0 -> RangeCard(table, st.moa, st.conditions.targetRangeM) { r ->
                model.setTargetRange(r)
                onRangeChosen()
            }
            else -> TrajectoryChart(curve, st.conditions.targetRangeM, Modifier.fillMaxSize().padding(8.dp))
        }
    }
}

@Composable
private fun RangeCard(table: RangeTable, moa: Boolean, targetM: Double, onRow: (Double) -> Unit) {
    val unit = stringResource(if (moa) Res.string.unit_moa else Res.string.unit_mrad)
    val titles = mapOf(
        "range" to stringResource(Res.string.col_range),
        "elev" to stringResource(Res.string.col_elev, unit),
        "elevClicks" to stringResource(Res.string.col_elev_clicks),
        "wind" to stringResource(Res.string.col_wind, unit),
        "windClicks" to stringResource(Res.string.col_wind_clicks),
        "v" to stringResource(Res.string.col_velocity),
        "drop" to stringResource(Res.string.col_drop),
        "drift" to stringResource(Res.string.col_drift),
        "mach" to stringResource(Res.string.col_mach),
        "energy" to stringResource(Res.string.col_energy),
        "time" to stringResource(Res.string.col_time),
    )
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val wide = maxWidth >= 720.dp
        val cols = buildList {
            add(TableCol(titles.getValue("range"), 0, false) { it.rangeM })
            add(TableCol(titles.getValue("elev"), 2, true) { it.elevation })
            if (table.hasScope) add(TableCol(titles.getValue("elevClicks"), 0, true) { it.elevationClicks })
            add(TableCol(titles.getValue("wind"), 2, true) { it.windage })
            if (table.hasScope) add(TableCol(titles.getValue("windClicks"), 0, true) { it.windageClicks })
            // Phones with a scope: five angle columns are enough.
            if (maxWidth >= 420.dp || !table.hasScope) add(TableCol(titles.getValue("v"), 0, false) { it.velocity })
            if (wide) {
                add(TableCol(titles.getValue("drop"), 1, false) { it.dropCm })
                add(TableCol(titles.getValue("drift"), 1, false) { it.windageCm })
                add(TableCol(titles.getValue("mach"), 2, false) { it.mach })
                add(TableCol(titles.getValue("energy"), 0, false) { it.energy })
                add(TableCol(titles.getValue("time"), 3, false) { it.time })
            }
        }
        val headerBg = MaterialTheme.colorScheme.surface
        val stripe = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.04f)
        val targetBg = MaterialTheme.colorScheme.primary.copy(alpha = 0.25f)
        LazyColumn(Modifier.fillMaxSize().testTag("rangeTable")) {
            stickyHeader {
                Column(Modifier.background(headerBg)) {
                    Row(Modifier.fillMaxWidth().height(44.dp)) {
                        cols.forEach {
                            Text(
                                it.title, Modifier.weight(1f).padding(end = 8.dp),
                                fontSize = 12.sp, textAlign = TextAlign.End, lineHeight = 14.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                    HorizontalDivider()
                }
            }
            if (table.ok) {
                itemsIndexed(table.rows) { i, row ->
                    val isTarget = abs(row.rangeM - targetM) < 0.5
                    val color = when {
                        row.mach < 1.0 -> MaterialTheme.colorScheme.error
                        row.mach < 1.2 -> Transonic
                        else -> Color.Unspecified
                    }
                    Row(
                        Modifier.fillMaxWidth().height(36.dp)
                            .background(if (isTarget) targetBg else if (i % 2 == 1) stripe else Color.Transparent)
                            .clickable { onRow(row.rangeM) },
                    ) {
                        cols.forEach { c ->
                            Text(
                                // No hold at the muzzle.
                                if (row.rangeM == 0.0 && c.angle) "—" else c.value(row).fixed(c.decimals),
                                Modifier.weight(1f).padding(end = 8.dp, top = 8.dp),
                                fontSize = 15.sp, textAlign = TextAlign.End, color = color,
                                fontWeight = if (isTarget) FontWeight.Bold else FontWeight.Normal,
                            )
                        }
                    }
                }
                item {
                    Text(
                        stringResource(Res.string.table_note), Modifier.padding(12.dp),
                        fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }
    }
}

/** Height over the line of sight and drift along the range, with the target marked. */
@Composable
private fun TrajectoryChart(curve: RangeTable, targetM: Double, modifier: Modifier) {
    val rows = if (curve.ok) curve.rows else emptyList()
    val measurer = rememberTextMeasurer()
    val fg = MaterialTheme.colorScheme.onSurface
    val accent = MaterialTheme.colorScheme.primary
    val labelM = stringResource(Res.string.unit_m)
    val labelCm = stringResource(Res.string.unit_cm)
    val labelRange = stringResource(Res.string.chart_range)
    val legendHeight = stringResource(Res.string.chart_height)
    val legendDrift = stringResource(Res.string.chart_drift)
    val style = TextStyle(fontSize = 12.sp, color = fg)
    Canvas(modifier.testTag("chart")) {
        if (rows.size < 2) return@Canvas
        val maxR = rows.last().rangeM
        var minY = 0.0
        var maxY = 0.0
        rows.forEach {
            minY = min(minY, min(it.dropCm, it.windageCm))
            maxY = max(maxY, max(it.dropCm, it.windageCm))
        }
        val meters = maxY - minY > 300
        val scale = if (meters) 0.01 else 1.0
        val pad = (maxY - minY) * 0.08 + 1
        minY -= pad
        maxY += pad

        val left = 56.dp.toPx()
        val right = size.width - 12.dp.toPx()
        val top = 12.dp.toPx()
        val bottom = size.height - 36.dp.toPx()
        fun x(r: Double) = (left + (right - left) * r / maxR).toFloat()
        fun y(v: Double) = (top + (bottom - top) * (maxY - v) / (maxY - minY)).toFloat()
        fun label(text: String, at: Offset, color: Color = fg) =
            drawText(measurer, text, at, style.copy(color = color))

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
        var yv = ceil(minY * scale / stepY) * stepY
        while (yv <= maxY * scale) {
            drawLine(grid, Offset(left, y(yv / scale)), Offset(right, y(yv / scale)))
            label(yv.fixed(if (stepY < 1) 1 else 0), Offset(4f, y(yv / scale) - 8f))
            yv += stepY
        }
        label(if (meters) labelM else labelCm, Offset(4f, 0f))
        label(labelRange, Offset(right - 70f, bottom + 18f))

        // Line of sight and target.
        drawLine(fg.copy(alpha = 0.6f), Offset(x(0.0), y(0.0)), Offset(x(maxR), y(0.0)),
            pathEffect = PathEffect.dashPathEffect(floatArrayOf(18f, 12f)))
        if (targetM <= maxR) drawLine(accent, Offset(x(targetM), top), Offset(x(targetM), bottom))

        fun curveOf(value: (TableRow) -> Double, color: Color) {
            val p = Path()
            rows.forEachIndexed { i, row ->
                if (i == 0) p.moveTo(x(row.rangeM), y(value(row))) else p.lineTo(x(row.rangeM), y(value(row)))
            }
            drawPath(p, color, style = Stroke(width = 2.5.dp.toPx()))
        }
        curveOf({ it.windageCm }, DriftColor)
        curveOf({ it.dropCm }, Transonic)
        label("● $legendHeight", Offset(left + 8f, top + 4f), Transonic)
        label("● $legendDrift", Offset(left + 8f, top + 22f), DriftColor)
    }
}
