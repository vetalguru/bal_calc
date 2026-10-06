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
            else -> ChartPanel(model, curve, max(st.tableToM, st.conditions.targetRangeM), Modifier.fillMaxSize())
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
        "lead" to stringResource(Res.string.col_lead, unit),
        "leadClicks" to stringResource(Res.string.col_lead_clicks),
    )
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val wide = maxWidth >= 720.dp
        val cols = buildList {
            add(TableCol(titles.getValue("range"), 0, false) { it.rangeM })
            add(TableCol(titles.getValue("elev"), 2, true) { it.elevation })
            if (table.hasScope) add(TableCol(titles.getValue("elevClicks"), 0, true) { it.elevationClicks })
            add(TableCol(titles.getValue("wind"), 2, true) { it.windage })
            if (table.hasScope) add(TableCol(titles.getValue("windClicks"), 0, true) { it.windageClicks })
            // A moving target: its lead (instead of the velocity on a phone).
            val lead = table.rows.any { it.lead != 0.0 }
            if (lead) add(TableCol(titles.getValue("lead"), 2, true) { it.lead })
            if (lead && table.hasScope && wide) add(TableCol(titles.getValue("leadClicks"), 0, true) { it.leadClicks })
            // Phones with a scope: five angle columns are enough.
            if ((maxWidth >= 420.dp && (!lead || wide)) || !table.hasScope) add(TableCol(titles.getValue("v"), 0, false) { it.velocity })
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

