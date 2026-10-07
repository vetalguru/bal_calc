package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.core.RifleForm
import org.vetalguru.balcalc.core.LibraryRifle
import org.vetalguru.balcalc.core.LibraryScope
import org.vetalguru.balcalc.core.ScopeClick
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** "From library" at the right of an editor section. */
@Composable
internal fun LibraryButton(tag: String, onClick: () -> Unit) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
        TextButton(onClick = onClick, modifier = Modifier.testTag(tag)) { Text(stringResource(Res.string.from_library)) }
    }
}

/** A library rifle into the form: its twist and caliber, and its name if none yet. */
internal fun RifleForm.withRifle(r: LibraryRifle) = copy(
    name = name.ifBlank { "${r.maker} ${r.model} ${r.caliber}" },
    caliber = r.caliber,
    twistIn = r.twistIn,
)

/**
 * A library scope into the form. An SFP reticle is taken as true at the
 * highest power unless the maker states another.
 */
internal fun RifleForm.withScope(s: LibraryScope, click: ScopeClick) = copy(
    clickUnits = click.units,
    clickValue = click.value,
    focalPlane = s.focalPlane,
    minMagnification = s.minMagnification,
    maxMagnification = s.maxMagnification,
    sfpReferenceMagnification = when {
        s.focalPlane != "sfp" -> 0.0
        s.sfpReferenceMagnification > 0 -> s.sfpReferenceMagnification
        else -> s.maxMagnification
    },
)

@Composable
internal fun LibraryRifles(model: AppModel, onBack: () -> Unit, onPick: (LibraryRifle) -> Unit) {
    SearchList<LibraryRifle>(
        stringResource(Res.string.library_rifles),
        stringResource(Res.string.library_search_rifle),
        stringResource(Res.string.catalog_empty),
        model.library::rifles,
        onBack,
        header = { LibraryNote() },
    ) { r ->
        TwoLines(
            "${r.maker} ${r.model} · ${r.caliber}",
            stringResource(Res.string.library_rifle_line, r.twistIn.trimmed(), r.barrelsIn.joinToString(", ") { it.trimmed() }),
            "libraryRifle:${r.maker} ${r.model} ${r.caliber} ${r.twistIn.trimmed()}",
        ) { onPick(r) }
    }
}

/** One row per scope and click value it is sold with. */
@Composable
internal fun LibraryScopes(model: AppModel, onBack: () -> Unit, onPick: (LibraryScope, ScopeClick) -> Unit) {
    val ffp = stringResource(Res.string.ffp)
    val sfp = stringResource(Res.string.sfp)
    val mrad = stringResource(Res.string.unit_mrad)
    val moa = stringResource(Res.string.unit_moa)
    SearchList<Pair<LibraryScope, ScopeClick>>(
        stringResource(Res.string.library_scopes),
        stringResource(Res.string.library_search_scope),
        stringResource(Res.string.catalog_empty),
        { filter -> model.library.scopes(filter).flatMap { s -> s.clicks.map { s to it } } },
        onBack,
        header = { LibraryNote() },
    ) { (s, c) ->
        TwoLines(
            "${s.maker} ${s.model}",
            stringResource(
                Res.string.library_scope_line,
                s.minMagnification.trimmed(), s.maxMagnification.trimmed(),
                if (s.focalPlane == "sfp") sfp else ffp,
                c.value.trimmed(), if (c.units == "mrad") mrad else moa,
            ),
            "libraryScope:${s.maker} ${s.model} ${c.units}${c.value.trimmed()}",
        ) { onPick(s, c) }
    }
}

@Composable
private fun LibraryNote() = Text(
    stringResource(Res.string.library_note),
    color = MaterialTheme.colorScheme.onSurfaceVariant,
    modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp),
)

/** 8.0 -> "8", 9.375 -> "9.375". */
private fun Double.trimmed(): String = if (this == kotlin.math.floor(this)) toLong().toString() else toString()
