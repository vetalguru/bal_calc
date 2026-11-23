package org.vetalguru.balcalc.ui

import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.layout.positionInParent
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import org.jetbrains.compose.resources.DrawableResource
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import org.jetbrains.compose.resources.vectorResource
import org.vetalguru.balcalc.BackHandler
import org.vetalguru.balcalc.coreText
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

// A thing (a rifle, a cartridge) edited on its drawing: the parts of the
// drawing are the page's parts, each with its fields; the last part is a
// summary. Shared by RifleWizard and CartridgeWizard.

/** A part of a page on a drawing; [name] tags its chip ("part:<name>"). */
internal interface PagePart {
    val name: String
    val title: StringResource
    val icon: DrawableResource?
    /** A sentence on what the part is for, above its fields. */
    val note: StringResource?
    val isSummary: Boolean
}

/**
 * The page: Cancel - [title] - Save; the [drawing] with the parts' chips
 * under it; the current part's [fields] (or the [summary]); Back and Next
 * (Save on the summary). Back (the key) goes to the part before; Save works
 * on any part.
 */
@Composable
internal fun <P : PagePart> PartsPage(
    title: String,
    parts: List<P>,
    step: Int,
    onStep: (Int) -> Unit,
    done: Set<P>,
    error: String?,
    onCancel: () -> Unit,
    onSave: () -> Unit,
    drawing: @Composable (Modifier) -> Unit,
    summary: @Composable () -> Unit,
    fields: @Composable ColumnScope.(part: P, wide: Boolean) -> Unit,
) {
    val current = parts[step]
    BackHandler(enabled = step > 0) { onStep(step - 1) }
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val wide = maxWidth >= 640.dp
        Column(Modifier.fillMaxSize()) {
            EditorBar(title, stringResource(Res.string.cancel), stringResource(Res.string.save), onCancel, onSave)
            Column(Modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally) {
                drawing(Modifier.widthIn(max = 560.dp).padding(horizontal = 12.dp))
                PartChips(parts, current, done) { onStep(parts.indexOf(it)) }
                Text(
                    stringResource(Res.string.wizard_step, step + 1, parts.size) + " · " + stringResource(current.title),
                    style = MaterialTheme.typography.labelMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(top = 2.dp).testTag("wizardStep"),
                )
            }
            Column(
                Modifier.weight(1f).fillMaxWidth().verticalScroll(rememberScrollState()).padding(vertical = 12.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                if (!error.isNullOrEmpty()) {
                    Text(coreText(error), color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(horizontal = 16.dp).testTag("formError"))
                }
                if (current.isSummary) {
                    summary()
                } else {
                    Section(stringResource(current.title), icon = current.icon) {
                        current.note?.let { Text(stringResource(it), color = MaterialTheme.colorScheme.onSurfaceVariant) }
                        fields(current, wide)
                    }
                }
            }
            Row(
                Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                OutlinedButton(onClick = { onStep(step - 1) }, enabled = step > 0, modifier = Modifier.testTag("wizardBack")) {
                    Text(stringResource(Res.string.back))
                }
                Spacer(Modifier.weight(1f))
                if (current.isSummary) {
                    Button(onClick = onSave, modifier = Modifier.testTag("wizardFinish")) {
                        ButtonIcon(Res.drawable.ic_save)
                        Text(stringResource(Res.string.save))
                    }
                } else {
                    Button(onClick = { onStep(step + 1) }, modifier = Modifier.testTag("wizardNext")) {
                        Text(stringResource(Res.string.next))
                    }
                }
            }
        }
    }
}

/** The parts as chips under the drawing: ✓ on the ones done, the current one selected and scrolled into view. */
@Composable
internal fun <P : PagePart> PartChips(parts: List<P>, selected: P, done: Set<P>, onSelect: (P) -> Unit) {
    // Every chip is there (a screen reader finds them all); the selected
    // one scrolls into view (after a tap on the drawing, Next).
    val row = rememberScrollState()
    val starts = remember { mutableMapOf<String, Int>() }
    LaunchedEffect(selected) { starts[selected.name]?.let { row.animateScrollTo((it - 48).coerceAtLeast(0)) } }
    Row(
        Modifier.fillMaxWidth().horizontalScroll(row).padding(horizontal = 12.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        parts.forEach { part ->
            FilterChip(
                selected = part == selected,
                onClick = { onSelect(part) },
                label = { Text(stringResource(part.title)) },
                leadingIcon = if (part in done && !part.isSummary) {
                    { Icon(vectorResource(Res.drawable.ic_check), contentDescription = null, Modifier.size(16.dp)) }
                } else {
                    null
                },
                modifier = Modifier.widthIn(min = 48.dp).testTag("part:${part.name}")
                    .onGloballyPositioned { starts[part.name] = it.positionInParent().x.toInt() },
            )
        }
    }
}

/** The summary: each part's stated values (label to value), with a way back to that part. */
@Composable
internal fun <P : PagePart> PartsSummary(rows: List<Pair<P, List<Pair<String, String>>>>, onChange: (P) -> Unit) {
    rows.forEach { (part, values) ->
        Section(stringResource(part.title), icon = part.icon) {
            if (values.isEmpty()) Text(stringResource(Res.string.not_stated), color = MaterialTheme.colorScheme.onSurfaceVariant)
            values.forEach { (label, value) ->
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(label, color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.weight(1f))
                    Text(value, modifier = Modifier.weight(1.4f))
                }
            }
            TextButton(onClick = { onChange(part) }, modifier = Modifier.align(Alignment.End).testTag("summaryChange:${part.name}")) {
                Text(stringResource(Res.string.change))
            }
        }
    }
}
