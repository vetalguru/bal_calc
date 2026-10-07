package org.vetalguru.balcalc.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.rememberTransformableState
import androidx.compose.foundation.gestures.transformable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Slider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.abs
import kotlin.math.roundToInt
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.loadOr
import org.vetalguru.balcalc.core.Solution
import org.vetalguru.balcalc.markRanges
import org.vetalguru.balcalc.mradPer
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** The reticle with the hold, the hold mode and the turret settings. */
@Composable
internal fun ReticleCard(model: AppModel, sol: Solution, wide: Boolean, marks: List<ReticleMark> = emptyList()) {
    // The range at each mark below the centre, for the elevation on the turret.
    var showRanges by remember { mutableStateOf(false) }
    var dialedClicks by remember { mutableStateOf<Double?>(null) } // null: as solved
    var curve by remember { mutableStateOf(emptyList<Pair<Double, Double>>()) }
    LaunchedEffect(showRanges, model.revision) {
        if (showRanges) {
            curve = loadOr(null) { model.trajectory(2500.0, 250) }
                ?.takeIf { it.ok }?.rows?.map { it.rangeM to it.elevation }.orEmpty()
        }
    }
    val reticleUnit = if (sol.hasReticle) sol.reticleUnits else "mrad"
    val clicks = dialedClicks ?: sol.dialElevationClicks
    val ranges = if (showRanges && curve.isNotEmpty()) {
        markRanges(
            curve, clicks * sol.clickElevation, model.state.angleUnit, reticleUnit,
            step = if (reticleUnit == "moa") 2.0 else 1.0, last = if (reticleUnit == "moa") 40.0 else 12.0,
            scale = sol.subtensionScale,
        )
    } else {
        emptyList()
    }
    val metres = stringResource(Res.string.unit_m)
    val rangeMarks = ranges.map { (mark, r) -> ReticleMark(0.25, -mark * mradPer(reticleUnit), "${r.roundToInt()} $metres", ring = false) }
    var fullscreen by remember { mutableStateOf(false) }
    if (fullscreen) {
        ReticleFullscreen(if (sol.hasReticle) sol.reticleDefinition else "", sol.targetX, sol.targetY, marks + rangeMarks) {
            fullscreen = false
        }
    }
    val reticle: @Composable (Modifier) -> Unit = { m ->
        ReticleView(
            if (sol.hasReticle) sol.reticleDefinition else "", sol.targetX, sol.targetY,
            m.aspectRatio(1f).clickable { fullscreen = true }, marks + rangeMarks,
        )
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
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = { fullscreen = true }, modifier = Modifier.testTag("reticleFullscreen")) {
                    Text(stringResource(Res.string.reticle_fullscreen))
                }
            }
            SwitchRow(stringResource(Res.string.ranges_at_marks), showRanges, { showRanges = it }, Modifier.testTag("rangesAtMarks"))
            if (showRanges) {
                if (sol.hasScope) {
                    NumberField(
                        stringResource(Res.string.dialed_clicks), clicks,
                        { dialedClicks = it }, Modifier.fillMaxWidth(), stringResource(Res.string.clicks_unit),
                        decimals = 0, from = -500.0, to = 500.0, tag = "dialedClicks",
                    )
                }
                Text(
                    ranges.joinToString("   ") { (mark, r) -> "${mark.fixed(0)} → ${r.roundToInt()} $metres" },
                    modifier = Modifier.testTag("markRanges"),
                )
            }
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

/** The reticle on the whole screen: pinch (or the buttons) to zoom, tap Close to go back. */
@Composable
private fun ReticleFullscreen(
    definition: String,
    targetX: Double,
    targetY: Double,
    marks: List<ReticleMark>,
    onClose: () -> Unit,
) {
    var zoom by remember { mutableStateOf(1f) }
    val pinch = rememberTransformableState { change, _, _ -> zoom = (zoom * change).coerceIn(0.5f, 8f) }
    Dialog(onDismissRequest = onClose, properties = DialogProperties(usePlatformDefaultWidth = false)) {
        Box(Modifier.fillMaxSize().background(MaterialTheme.colorScheme.surface).testTag("reticleScreen")) {
            ReticleView(definition, targetX, targetY, Modifier.fillMaxSize().transformable(pinch).padding(8.dp), marks, zoom)
            Row(
                Modifier.align(Alignment.BottomCenter).padding(16.dp),
                horizontalArrangement = Arrangement.spacedBy(12.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                FilledTonalButton(onClick = { zoom = (zoom / 1.5f).coerceAtLeast(0.5f) }, modifier = Modifier.testTag("reticleZoomOut")) { Text("−") }
                Text("×${zoom.toDouble().fixed(1)}", modifier = Modifier.testTag("reticleZoom"))
                FilledTonalButton(onClick = { zoom = (zoom * 1.5f).coerceAtMost(8f) }, modifier = Modifier.testTag("reticleZoomIn")) { Text("+") }
                Button(onClick = onClose, modifier = Modifier.testTag("reticleClose")) { Text(stringResource(Res.string.close)) }
            }
        }
    }
}
