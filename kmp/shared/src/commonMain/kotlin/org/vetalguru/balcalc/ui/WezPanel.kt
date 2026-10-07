package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.loadOr
import org.vetalguru.balcalc.core.WezResult
import org.vetalguru.balcalc.core.WezSettings
import org.vetalguru.balcalc.coreText
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** Names of the error sources (WezModel's "source" keys). */
private val SourceNames: Map<String, StringResource> = mapOf(
    "range" to Res.string.src_range,
    "windSpeed" to Res.string.src_windSpeed,
    "windDirection" to Res.string.src_windDirection,
    "muzzleVelocity" to Res.string.src_muzzleVelocity,
    "drag" to Res.string.src_drag,
    "temperature" to Res.string.src_temperature,
    "pressure" to Res.string.src_pressure,
    "humidity" to Res.string.src_humidity,
    "lookAngle" to Res.string.src_lookAngle,
    "cant" to Res.string.src_cant,
    "azimuth" to Res.string.src_azimuth,
    "latitude" to Res.string.src_latitude,
    "dispersion" to Res.string.src_dispersion,
)

private fun percent(p: Double) = "${(p * 100).roundToInt()} %"

/**
 * Hit probability: at the target range, along the range, what spreads the
 * shots most, and the errors and target it is computed for.
 */
@Composable
fun WezPanel(model: AppModel, toM: Double, modifier: Modifier) {
    var result by remember { mutableStateOf<WezResult?>(null) }
    // Settings the shooter changed and not yet sent; null = load the stored ones.
    var edit by remember { mutableStateOf<WezSettings?>(null) }
    LaunchedEffect(model.revision, toM, edit) {
        if (!model.ready) return@LaunchedEffect
        result = loadOr(null) { model.wez(edit, toM, 50.0) }
    }
    val r = result
    val s = r?.settings ?: WezSettings()
    fun change(f: (WezSettings) -> WezSettings) { edit = f(s) }
    val cm = stringResource(Res.string.unit_cm)
    val deg = stringResource(Res.string.unit_deg)
    val coriolis = model.state.conditions.coriolis

    BoxWithConstraints(modifier) {
        val wide = maxWidth >= 600.dp
        Column(
            Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(vertical = 8.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text(stringResource(Res.string.wez_hint), color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(horizontal = 12.dp))
            if (r != null && !r.ok) {
                Text(coreText(r.error), color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(horizontal = 12.dp))
            }
            if (r != null && r.ok) {
                Card(Modifier.fillMaxWidth().padding(horizontal = 12.dp)) {
                    Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                        Row(verticalAlignment = Alignment.Bottom, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                            Text(percent(r.atTarget.probability), fontSize = 52.sp, fontWeight = FontWeight.Bold,
                                modifier = Modifier.testTag("hitChance"))
                            Text(stringResource(Res.string.wez_at_target, r.atTarget.rangeM.roundToInt()), fontSize = 18.sp,
                                modifier = Modifier.padding(bottom = 10.dp))
                        }
                        Text(stringResource(Res.string.wez_shots, r.shots50, r.shots80, r.shots95), modifier = Modifier.testTag("shotsToHit"))
                        Text(stringResource(Res.string.wez_spread, r.atTarget.sigmaUpCm.fixed(1), r.atTarget.sigmaRightCm.fixed(1)),
                            color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                }
                LineChart(
                    listOf(Series(stringResource(Res.string.tab_hit), Transonic, r.rows.map { it.rangeM to it.probability * 100 })),
                    "%", false, model.state.conditions.targetRangeM,
                    Modifier.fillMaxWidth().height(260.dp).padding(horizontal = 4.dp).testTag("hitChart"),
                    withZero = true,
                )
                Section(stringResource(Res.string.wez_sources)) {
                    r.parts.take(6).forEach { p ->
                        Row(Modifier.fillMaxWidth().testTag("part:${p.source}")) {
                            Text(SourceNames[p.source]?.let { stringResource(it) } ?: p.source, modifier = Modifier.weight(1f))
                            Text(stringResource(Res.string.part_line, kotlin.math.abs(p.upCm).fixed(1), kotlin.math.abs(p.rightCm).fixed(1)),
                                color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                    }
                }
            }

            Section(stringResource(Res.string.wez_target)) {
                ChoiceField(
                    stringResource(Res.string.wez_target),
                    listOf(
                        "rectangle" to stringResource(Res.string.target_rect),
                        "ellipse" to stringResource(Res.string.target_ellipse),
                        "figure" to stringResource(Res.string.target_figure),
                    ),
                    s.targetKind, { k -> change { it.copy(targetKind = k) } }, Modifier.fillMaxWidth().testTag("targetKind"),
                )
                Fields(
                    wide,
                    { m -> NumberField(stringResource(Res.string.target_width), s.targetWidthCm, { v -> change { it.copy(targetWidthCm = v) } }, m, cm, decimals = 0, from = 1.0, to = 1000.0, tag = "targetWidth") },
                    { m -> NumberField(stringResource(Res.string.target_height_wez), s.targetHeightCm, { v -> change { it.copy(targetHeightCm = v) } }, m, cm, decimals = 0, from = 1.0, to = 1000.0) },
                )
            }

            Section(stringResource(Res.string.wez_errors)) {
                val m = stringResource(Res.string.unit_m)
                val mps = stringResource(Res.string.unit_mps)
                Fields(
                    wide,
                    { mod -> NumberField(stringResource(Res.string.src_range), s.rangeM, { v -> change { it.copy(rangeM = v) } }, mod, m, from = 0.0, to = 200.0, tag = "errRange") },
                    { mod -> NumberField(stringResource(Res.string.src_windSpeed), s.windSpeedMps, { v -> change { it.copy(windSpeedMps = v) } }, mod, mps, from = 0.0, to = 20.0, tag = "errWind") },
                )
                Fields(
                    wide,
                    { mod -> NumberField(stringResource(Res.string.src_windDirection), s.windDirectionDeg, { v -> change { it.copy(windDirectionDeg = v) } }, mod, deg, decimals = 0, from = 0.0, to = 180.0) },
                    { mod -> NumberField(stringResource(Res.string.err_mv), s.muzzleVelocityMps, { v -> change { it.copy(muzzleVelocityMps = v) } }, mod, mps, from = 0.0, to = 100.0) },
                )
                Fields(
                    wide,
                    { mod -> NumberField(stringResource(Res.string.src_drag), s.bcPercent, { v -> change { it.copy(bcPercent = v) } }, mod, "%", from = 0.0, to = 50.0) },
                    { mod -> NumberField(stringResource(Res.string.err_group), s.groupMoa, { v -> change { it.copy(groupMoa = v) } }, mod, stringResource(Res.string.unit_moa), decimals = 2, from = 0.0, to = 20.0, tag = "errGroup") },
                )
                Fields(
                    wide,
                    { mod -> NumberField(stringResource(Res.string.src_temperature), s.temperatureC, { v -> change { it.copy(temperatureC = v) } }, mod, stringResource(Res.string.unit_c), from = 0.0, to = 30.0) },
                    { mod -> NumberField(stringResource(Res.string.src_pressure), s.pressureHpa, { v -> change { it.copy(pressureHpa = v) } }, mod, stringResource(Res.string.unit_hpa), from = 0.0, to = 100.0) },
                )
                Fields(
                    wide,
                    { mod -> NumberField(stringResource(Res.string.src_humidity), s.humidityPct, { v -> change { it.copy(humidityPct = v) } }, mod, stringResource(Res.string.unit_percent), decimals = 0, from = 0.0, to = 100.0) },
                    { mod -> NumberField(stringResource(Res.string.src_lookAngle), s.lookAngleDeg, { v -> change { it.copy(lookAngleDeg = v) } }, mod, deg, from = 0.0, to = 30.0) },
                )
                NumberField(stringResource(Res.string.src_cant), s.cantDeg, { v -> change { it.copy(cantDeg = v) } }, Modifier.fillMaxWidth(), deg, from = 0.0, to = 30.0)
                if (coriolis) {
                    Fields(
                        wide,
                        { mod -> NumberField(stringResource(Res.string.src_azimuth), s.azimuthDeg, { v -> change { it.copy(azimuthDeg = v) } }, mod, deg, decimals = 0, from = 0.0, to = 180.0) },
                        { mod -> NumberField(stringResource(Res.string.src_latitude), s.latitudeDeg, { v -> change { it.copy(latitudeDeg = v) } }, mod, deg, from = 0.0, to = 30.0) },
                    )
                }
            }

            if (r != null && r.ok && r.rows.isNotEmpty()) {
                Section("") {
                    Row(Modifier.fillMaxWidth()) {
                        for (title in listOf(stringResource(Res.string.col_range), stringResource(Res.string.col_hit),
                            stringResource(Res.string.col_sigma_up), stringResource(Res.string.col_sigma_right))) {
                            Text(title, fontSize = 12.sp, textAlign = TextAlign.End, modifier = Modifier.weight(1f),
                                color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                    }
                    r.rows.filter { (it.rangeM.roundToInt() % 100) == 0 }.forEach { row ->
                        Row(Modifier.fillMaxWidth()) {
                            for (cell in listOf(row.rangeM.roundToInt().toString(), percent(row.probability),
                                row.sigmaUpCm.fixed(1), row.sigmaRightCm.fixed(1))) {
                                Text(cell, textAlign = TextAlign.End, modifier = Modifier.weight(1f))
                            }
                        }
                    }
                }
            }
        }
    }
}
