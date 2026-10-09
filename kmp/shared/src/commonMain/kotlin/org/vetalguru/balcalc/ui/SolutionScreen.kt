package org.vetalguru.balcalc.ui

import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.SecondaryTabRow
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
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.abs
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.loadOr
import org.vetalguru.balcalc.coreText
import org.vetalguru.balcalc.core.Solution
import org.vetalguru.balcalc.core.TargetItem
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** Firing solution for one target, large enough to read at arm's length. */
@Composable
fun SolutionScreen(model: AppModel, onEditArmory: () -> Unit, onEditConditions: () -> Unit = {}) {
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
        // The ring: as wide as the screen allows, but at most about half its
        // height so the corrections above keep their room.
        // (Short screens keep a little more for the corrections.)
        val ring = minOf(maxWidth - 8.dp, maxHeight * (if (maxHeight < 700.dp) 0.42f else 0.48f), 420.dp)
        Column(Modifier.fillMaxSize()) {
            Pickers(model)
            if (wide) {
                Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Viewer(model, sol, unit, wide, onEditArmory, { logging = true }, { situations = true }, Modifier.weight(1.3f))
                    Column(Modifier.weight(1f).verticalScroll(rememberScrollState())) { Controller(model, 400.dp, onEditConditions) }
                }
            } else {
                Viewer(model, sol, unit, wide, onEditArmory, { logging = true }, { situations = true }, Modifier.weight(1f))
                Controller(model, ring, onEditConditions)
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
    LaunchedEffect(model.revision) { targets = loadOr(emptyList()) { model.targets.list() } }
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
                                onSample = { model.armory.addSample(sampleRifle, sampleCartridge) },
                            )
                        }
                        if (sol.ok) {
                            Corrections(sol, st)
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
