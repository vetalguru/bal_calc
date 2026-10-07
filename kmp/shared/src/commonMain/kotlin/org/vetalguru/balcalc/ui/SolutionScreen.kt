package org.vetalguru.balcalc.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.rememberTransformableState
import androidx.compose.foundation.gestures.transformable
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
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
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.SecondaryTabRow
import androidx.compose.material3.Slider
import androidx.compose.material3.Tab
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuAnchorType
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
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
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.loadOr
import org.vetalguru.balcalc.coreText
import org.vetalguru.balcalc.CorrectionText
import org.vetalguru.balcalc.core.Solution
import org.vetalguru.balcalc.core.TargetItem
import org.vetalguru.balcalc.core.UiPrefs
import org.vetalguru.balcalc.formatCorrection
import org.vetalguru.balcalc.markRanges
import org.vetalguru.balcalc.mradPer
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
    var logging by remember { mutableStateOf(false) }
    if (logging) LogShotDialog(model, st.conditions.targetRangeM, sol.elevation) { logging = false }
    var situations by remember { mutableStateOf(false) }
    if (situations) SituationsDialog(model) { situations = false }

    // "Viewer on top, controller below": what to read above, what the thumb
    // changes below. Side by side on wide screens.
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val wide = maxWidth >= 600.dp
        // Small phones (under 380 dp): the controller drops its big steps.
        val narrow = maxWidth < 380.dp
        Column(Modifier.fillMaxSize()) {
            Pickers(model)
            if (wide) {
                Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Viewer(model, sol, unit, wide, onEditArmory, { logging = true }, { situations = true }, Modifier.weight(1.3f))
                    Column(Modifier.weight(1f).verticalScroll(rememberScrollState())) { Controller(model, narrow = false) }
                }
            } else {
                Viewer(model, sol, unit, wide, onEditArmory, { logging = true }, { situations = true }, Modifier.weight(1f))
                Controller(model, narrow)
            }
        }
    }
}

/** Rifle and cartridge pickers. */
@Composable
private fun Pickers(model: AppModel) {
    val st = model.state
    Row(
        Modifier.fillMaxWidth().padding(start = 12.dp, end = 12.dp, top = 8.dp, bottom = 4.dp),
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
}

private class ViewPage(val title: StringResource, val tag: String)

private val viewPages = listOf(
    ViewPage(Res.string.view_corrections, "viewCorrections"),
    ViewPage(Res.string.view_reticle, "viewReticle"),
    ViewPage(Res.string.view_targets, "viewTargets"),
    ViewPage(Res.string.view_more, "viewMore"),
)

/** The upper part: corrections, the reticle, the rest; swiped or chosen by the tabs. */
@Composable
private fun Viewer(
    model: AppModel,
    sol: Solution,
    unit: String,
    wide: Boolean,
    onEditArmory: () -> Unit,
    onLogHit: () -> Unit,
    onSituations: () -> Unit,
    modifier: Modifier,
) {
    val st = model.state
    val pager = rememberPagerState { viewPages.size }
    val scope = rememberCoroutineScope()
    val sampleRifle = stringResource(Res.string.sample_rifle_name)
    val sampleCartridge = stringResource(Res.string.sample_cartridge_name)
    // The target card, with the solution: holds follow the current target.
    var targets by remember { mutableStateOf(emptyList<TargetItem>()) }
    LaunchedEffect(model.revision) { targets = loadOr(emptyList()) { model.targets() } }
    // The others: the current target is the big mark already.
    val marks = targets.filter { it.ok && (abs(it.holdX - sol.targetX) > 0.05 || abs(it.holdY - sol.targetY) > 0.05) }
        .map { ReticleMark(it.holdX, it.holdY, it.name) }
    Column(modifier) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            SecondaryTabRow(selectedTabIndex = pager.currentPage, modifier = Modifier.weight(1f)) {
                viewPages.forEachIndexed { i, p ->
                    Tab(
                        selected = pager.currentPage == i,
                        onClick = { scope.launch { pager.animateScrollToPage(i) } },
                        text = { Text(stringResource(p.title), maxLines = 1, style = MaterialTheme.typography.labelMedium) },
                        modifier = Modifier.testTag(p.tag),
                    )
                }
            }
        }
        // Every page stays composed: switching is instant and keeps its scroll.
        HorizontalPager(pager, Modifier.fillMaxWidth().weight(1f), beyondViewportPageCount = viewPages.size - 1) { page ->
            Column(
                Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(vertical = 12.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                when (page) {
                    0 -> {
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
                            Corrections(sol, st.prefs, st.moa, st.conditions.windGustMps)
                            DetailsLine(sol) { scope.launch { pager.animateScrollToPage(viewPages.lastIndex) } }
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
                                OutlinedButton(onClick = onLogHit, modifier = Modifier.testTag("logHitSolution")) {
                                    Text(stringResource(Res.string.log_hit))
                                }
                            }
                        }
                    }
                    1 -> if (sol.ok) ReticleCard(model, sol, wide, marks)
                    2 -> {
                        // Saved situations live by the targets: both switch what is shot at.
                        if (st.rifles.isNotEmpty()) {
                            OutlinedButton(onClick = onSituations, modifier = Modifier.padding(horizontal = 12.dp).testTag("situations")) {
                                Text(stringResource(Res.string.situations))
                            }
                        }
                        TargetCard(model, targets, unit)
                    }
                    else -> {
                        MovingTargetCard(model, sol, unit)
                        if (sol.ok) Details(sol, st.conditions.targetHeightCm, wide)
                    }
                }
            }
        }
    }
}

/** The lower part: range, wind and angle, each changed with the thumb. */
@Composable
private fun Controller(model: AppModel, narrow: Boolean) {
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
private fun Step(label: String, onClick: () -> Unit) {
    FilledTonalButton(
        onClick = onClick,
        contentPadding = PaddingValues(horizontal = 2.dp),
        modifier = Modifier.width(50.dp).height(40.dp),
    ) { Text(label, fontSize = 13.sp, maxLines = 1) }
}

/** Velocity, time of flight, energy, Mach and stability in one line; the rest on "More". */
@Composable
private fun DetailsLine(sol: Solution, onMore: () -> Unit) {
    val parts = listOf(
        "${sol.velocity.fixed(0)} ${stringResource(Res.string.unit_mps)}",
        "${sol.time.fixed(2)} ${stringResource(Res.string.unit_s)}",
        "${sol.energy.fixed(0)} ${stringResource(Res.string.unit_j)}",
        "M ${sol.mach.fixed(2)}",
    ) + if (sol.stability > 0) listOf("Sg ${sol.stability.fixed(2)}") else emptyList()
    Row(
        Modifier.fillMaxWidth().clickable(onClick = onMore).padding(horizontal = 16.dp).testTag("detailsLine"),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            parts.joinToString("  ·  "),
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            maxLines = 2,
            modifier = Modifier.weight(1f),
        )
        Text("›", fontSize = 22.sp, color = MaterialTheme.colorScheme.primary)
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

/** The big distance input: whole metres, committed on Done or focus loss. */
@Composable
private fun RangeField(rangeM: Double, onRange: (Double) -> Unit, modifier: Modifier, showUnit: Boolean = true) {
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
private fun Corrections(sol: Solution, prefs: UiPrefs, moa: Boolean, gustMps: Double) {
    val unit = stringResource(if (moa) Res.string.unit_moa else Res.string.unit_mrad)
    val other = stringResource(if (moa) Res.string.unit_mrad else Res.string.unit_moa)
    val cm = stringResource(Res.string.unit_cm)
    // The direction words, arrows or signs for up / down and right / left.
    val (up, down, right, left) = when (prefs.correctionStyle) {
        "arrows" -> listOf("↑", "↓", "→", "←")
        "signs" -> listOf("+", "−", "+", "−")
        else -> listOf(
            stringResource(Res.string.up), stringResource(Res.string.down),
            stringResource(Res.string.right), stringResource(Res.string.left),
        )
    }
    
    val elevation = formatCorrection(
        sol.elevation, sol.clickElevation.takeIf { sol.hasScope }, moa, sol.rangeM, prefs.roundToClicks, up, down, other, cm,
    )
    val windage = formatCorrection(
        sol.windage, sol.clickWindage.takeIf { sol.hasScope }, moa, sol.rangeM, prefs.roundToClicks, right, left, other, cm,
    )
    val gust = if (sol.hasGust) {
        val g = formatCorrection(
            sol.gustWindage, sol.clickWindage.takeIf { sol.hasScope }, moa, sol.rangeM, prefs.roundToClicks, right, left, other, cm,
        )
        val clicks = g.clicks?.let { ", " + stringResource(Res.string.clicks, it) }.orEmpty()
        stringResource(Res.string.gust_windage, gustMps.fixed(1), "${g.direction} ${g.value} $unit$clicks".trim())
    } else {
        ""
    }
    // Side by side: read together. Tagged with the range it was solved for,
    // so tests can wait for it.
    Row(Modifier.padding(horizontal = 12.dp).testTag("solvedFor:${sol.rangeM.roundToInt()}"), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        CorrectionTile(stringResource(Res.string.elevation), elevation, unit, prefs.showSecondUnit, "elevation", Modifier.weight(1f))
        CorrectionTile(stringResource(Res.string.windage), windage, unit, prefs.showSecondUnit, "windage", Modifier.weight(1f), gust)
    }
}

@Composable
private fun CorrectionTile(
    title: String,
    c: CorrectionText,
    unit: String,
    showSecond: Boolean,
    tag: String,
    modifier: Modifier,
    extra: String = "",
) {
    Card(modifier, elevation = CardDefaults.cardElevation(defaultElevation = 3.dp)) {
        Column(Modifier.padding(12.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(title, fontSize = 15.sp, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1, modifier = Modifier.weight(1f))
                Text(
                    c.direction, fontSize = 20.sp, fontWeight = FontWeight.Bold, color = MaterialTheme.colorScheme.primary,
                    maxLines = 1, modifier = Modifier.testTag("${tag}Direction"),
                )
            }
            Text(c.value, fontSize = 48.sp, fontWeight = FontWeight.Bold, maxLines = 1, modifier = Modifier.testTag(tag))
            // The unit with the clicks: the number keeps the whole width.
            Text(
                if (c.clicks != null) "$unit · ${stringResource(Res.string.clicks, c.clicks)}" else unit,
                fontSize = 16.sp, maxLines = 1,
            )
            if (showSecond) {
                Text(
                    c.second, fontSize = 13.sp, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1,
                    modifier = Modifier.testTag("${tag}Second"),
                )
            }
            if (extra.isNotEmpty()) {
                Text(extra, fontSize = 16.sp, color = MaterialTheme.colorScheme.primary, modifier = Modifier.testTag("${tag}Gust"))
            }
        }
    }
}

/** The reticle with the hold, the hold mode and the turret settings. */
@Composable
private fun ReticleCard(model: AppModel, sol: Solution, wide: Boolean, marks: List<ReticleMark> = emptyList()) {
    // The range at each mark below the centre, for the elevation on the turret.
    var showRanges by remember { mutableStateOf(false) }
    var dialedClicks by remember { mutableStateOf<Double?>(null) } // null: as solved
    var curve by remember { mutableStateOf(emptyList<Pair<Double, Double>>()) }
    LaunchedEffect(showRanges, model.revision) {
        if (showRanges) {
            curve = runCatching { model.trajectory(2500.0, 250) }.getOrNull()
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

@Composable
private fun QuickWind(
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
