package org.vetalguru.balcalc.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.TextButton
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.rememberCoroutineScope
import kotlinx.coroutines.launch
import org.vetalguru.balcalc.LocalPlatform
import org.vetalguru.balcalc.core.UiPrefs
import org.vetalguru.balcalc.tableCsv
import org.vetalguru.balcalc.tablePng
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


private class TableCol(val key: String, val title: String, val decimals: Int, val angle: Boolean, val value: (TableRow) -> Double)

/** Every column the range card can show, in their order; key, decimals, angle?, value. */
private val allColumns: List<Triple<String, Pair<Int, Boolean>, (TableRow) -> Double>> = listOf(
    Triple("range", 0 to false) { it.rangeM },
    Triple("elev", 2 to true) { it.elevation },
    Triple("elevClicks", 0 to true) { it.elevationClicks },
    Triple("wind", 2 to true) { it.windage },
    Triple("windClicks", 0 to true) { it.windageClicks },
    Triple("lead", 2 to true) { it.lead },
    Triple("leadClicks", 0 to true) { it.leadClicks },
    Triple("v", 0 to false) { it.velocity },
    Triple("drop", 1 to false) { it.dropCm },
    Triple("drift", 1 to false) { it.windageCm },
    Triple("mach", 2 to false) { it.mach },
    Triple("energy", 0 to false) { it.energy },
    Triple("time", 3 to false) { it.time },
)

/** The keys shown when the user chose none: what fits the screen. */
private fun automaticColumns(table: RangeTable, widthDp: Float): List<String> {
    val wide = widthDp >= 720f
    val lead = table.rows.any { it.lead != 0.0 }
    return buildList {
        add("range")
        add("elev")
        if (table.hasScope) add("elevClicks")
        add("wind")
        if (table.hasScope) add("windClicks")
        // A moving target: its lead (instead of the velocity on a phone).
        if (lead) add("lead")
        if (lead && table.hasScope && wide) add("leadClicks")
        // Phones with a scope: five angle columns are enough.
        if ((widthDp >= 420f && (!lead || wide)) || !table.hasScope) add("v")
        if (wide) addAll(listOf("drop", "drift", "mach", "energy", "time"))
    }
}

@Composable
private fun columnTitles(moa: Boolean): Map<String, String> {
    val unit = stringResource(if (moa) Res.string.unit_moa else Res.string.unit_mrad)
    return mapOf(
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
}

/** The columns to show: the chosen ones (or the automatic ones), then a windage column per extra wind. */
@Composable
private fun tableColumns(table: RangeTable, prefs: UiPrefs, moa: Boolean, widthDp: Float): List<TableCol> {
    val titles = columnTitles(moa)
    val keys = prefs.tableColumns.ifEmpty { automaticColumns(table, widthDp) }
    val windTitle = stringResource(Res.string.col_wind_at, "%s")
    val mps = stringResource(Res.string.unit_mps)
    return allColumns.filter { it.first in keys }.map { (key, fmt, value) ->
        TableCol(key, titles.getValue(key), fmt.first, fmt.second, value)
    } + table.windSpeeds.mapIndexed { i, v ->
        TableCol("wind@$v", windTitle.replace("%s", "${v.fixed(0)} $mps"), 2, true) { it.windages.getOrElse(i) { 0.0 } }
    }
}

/** A cell as shown and exported: no hold at the muzzle. */
private fun cell(c: TableCol, row: TableRow) = if (row.rangeM == 0.0 && c.angle) "—" else c.value(row).fixed(c.decimals)

/** Range card and trajectory chart for the current rifle, cartridge and conditions. */
@Composable
fun TableScreen(model: AppModel, onRangeChosen: () -> Unit) {
    val st = model.state
    var tab by rememberSaveable { mutableStateOf(0) }
    var table by remember { mutableStateOf(RangeTable()) }
    var curve by remember { mutableStateOf(RangeTable()) }
    LaunchedEffect(model.revision, st.tableFromM, st.tableToM, st.tableStepM, st.prefs.tableWinds) {
        if (!model.ready) return@LaunchedEffect
        runCatching {
            table = model.rangeTable(st.prefs.tableWinds)
            curve = model.trajectory(max(st.tableToM, st.conditions.targetRangeM), 250)
        }
    }
    fun span(key: String, v: Double) = model.setSettings(buildJsonObject { put(key, v) })
    val m = stringResource(Res.string.unit_m)
    var choosing by remember { mutableStateOf(false) }
    if (choosing) ColumnsDialog(model) { choosing = false }

    Column(Modifier.fillMaxSize()) {
        PrimaryTabRow(selectedTabIndex = tab) {
            Tab(tab == 0, { tab = 0 }, text = { Text(stringResource(Res.string.tab_table)) })
            Tab(tab == 1, { tab = 1 }, text = { Text(stringResource(Res.string.tab_chart)) }, modifier = Modifier.testTag("chartTab"))
            Tab(tab == 2, { tab = 2 }, text = { Text(stringResource(Res.string.tab_hit)) }, modifier = Modifier.testTag("hitTab"))
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
            0 -> RangeCard(model, table, { choosing = true }) { r ->
                model.setTargetRange(r)
                onRangeChosen()
            }
            1 -> ChartPanel(model, curve, max(st.tableToM, st.conditions.targetRangeM), Modifier.fillMaxSize())
            else -> WezPanel(model, max(st.tableToM, st.conditions.targetRangeM), Modifier.fillMaxSize())
        }
    }
}

@Composable
private fun RangeCard(model: AppModel, table: RangeTable, onColumns: () -> Unit, onRow: (Double) -> Unit) {
    val st = model.state
    val targetM = st.conditions.targetRangeM
    val files = LocalPlatform.current.files
    val scope = rememberCoroutineScope()
    val saved = stringResource(Res.string.saved)
    val title = stringResource(
        Res.string.export_title,
        st.currentPair?.let { "${it.rifleName} · ${it.cartridgeName}" }.orEmpty(),
        st.conditions.temperatureC.fixed(0), st.conditions.windSpeed.fixed(1),
    )
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val cols = tableColumns(table, st.prefs, st.moa, maxWidth.value)
        val headerBg = MaterialTheme.colorScheme.surface
        val stripe = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.04f)
        val targetBg = MaterialTheme.colorScheme.primary.copy(alpha = 0.25f)
        var exportMenu by remember { mutableStateOf(false) }
        fun export(png: Boolean) = scope.launch {
            val header = cols.map { it.title.replace('\n', ' ') } // one line per title in a file
            val rows = table.rows.map { r -> cols.map { cell(it, r) } }
            val ok = if (png) {
                val target = table.rows.indexOfFirst { abs(it.rangeM - targetM) < 0.5 }
                files.saveBytes("range-card.png", "image/png", tablePng(title, header, rows, target))
            } else {
                files.saveBytes("range-card.csv", "text/csv", tableCsv(title, header, rows).encodeToByteArray())
            }
            if (ok) model.message = saved
        }
        Column(Modifier.fillMaxSize()) {
            Row(Modifier.fillMaxWidth().padding(horizontal = 8.dp), horizontalArrangement = Arrangement.End) {
                TextButton(onClick = onColumns, modifier = Modifier.testTag("tableColumns")) { Text(stringResource(Res.string.table_columns)) }
                Box {
                    TextButton(onClick = { exportMenu = true }, enabled = table.ok, modifier = Modifier.testTag("tableExport")) {
                        Text(stringResource(Res.string.table_export))
                    }
                    DropdownMenu(exportMenu, { exportMenu = false }) {
                        DropdownMenuItem({ Text(stringResource(Res.string.export_csv)) }, onClick = { exportMenu = false; export(png = false) },
                            modifier = Modifier.testTag("exportCsv"))
                        DropdownMenuItem({ Text(stringResource(Res.string.export_png)) }, onClick = { exportMenu = false; export(png = true) },
                            modifier = Modifier.testTag("exportPng"))
                    }
                }
            }
            LazyColumn(Modifier.fillMaxSize().testTag("rangeTable")) {
                stickyHeader {
                    Column(Modifier.background(headerBg)) {
                        Row(Modifier.fillMaxWidth().height(44.dp)) {
                            cols.forEach {
                                Text(
                                    it.title, Modifier.weight(1f).padding(end = 8.dp).testTag("col:${it.key}"),
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
                                    cell(c, row),
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
}

/** Which columns the range card shows, and the extra wind speeds. */
@Composable
private fun ColumnsDialog(model: AppModel, onClose: () -> Unit) {
    val prefs = model.state.prefs
    val titles = columnTitles(model.state.moa)
    var automatic by remember { mutableStateOf(prefs.tableColumns.isEmpty()) }
    val chosen = remember { mutableStateListOf<String>().apply { addAll(prefs.tableColumns.ifEmpty { listOf("range", "elev", "elevClicks", "wind", "windClicks") }) } }
    var winds by remember { mutableStateOf(prefs.tableWinds.joinToString(", ") { it.fixed(0) }) }
    AlertDialog(
        onDismissRequest = onClose,
        title = { Text(stringResource(Res.string.table_columns)) },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                SwitchRow(stringResource(Res.string.columns_automatic), automatic, { automatic = it }, Modifier.testTag("columnsAuto"))
                if (!automatic) {
                    allColumns.forEach { (key, _, _) ->
                        SwitchRow(titles.getValue(key), key in chosen, { on ->
                            if (on) chosen.add(key) else chosen.remove(key)
                        }, Modifier.testTag("column:$key"))
                    }
                }
                OutlinedTextField(
                    winds, { winds = it },
                    label = { Text(stringResource(Res.string.wind_columns)) },
                    supportingText = { Text(stringResource(Res.string.wind_columns_hint)) },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth().testTag("windColumns"),
                )
            }
        },
        confirmButton = {
            Button(onClick = {
                val speeds = winds.split(',', ';', ' ').mapNotNull { it.trim().replace(',', '.').toDoubleOrNull() }
                    .filter { it in 0.0..40.0 }.distinct().take(6)
                model.setPrefs { p ->
                    p.copy(tableColumns = if (automatic) emptyList() else allColumns.map { it.first }.filter { it in chosen }, tableWinds = speeds)
                }
                onClose()
            }, modifier = Modifier.testTag("columnsSave")) { Text(stringResource(Res.string.save)) }
        },
        dismissButton = { TextButton(onClick = onClose) { Text(stringResource(Res.string.cancel)) } },
    )
}
