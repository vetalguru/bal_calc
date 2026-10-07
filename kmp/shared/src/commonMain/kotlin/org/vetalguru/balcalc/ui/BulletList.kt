package org.vetalguru.balcalc.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.loadOr
import org.vetalguru.balcalc.LocalPlatform
import org.vetalguru.balcalc.core.CartridgeItem
import org.vetalguru.balcalc.core.BulletItem
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** A searchable list page with a Back bar. */
@Composable
internal fun <T> SearchList(
    title: String,
    hint: String,
    empty: String,
    load: suspend (String) -> List<T>,
    onBack: () -> Unit,
    reloadKey: Any? = null,
    header: @Composable () -> Unit = {},
    actions: @Composable () -> Unit = {},
    row: @Composable (T) -> Unit,
) {
    var filter by rememberSaveable { mutableStateOf("") }
    var items by remember { mutableStateOf(emptyList<T>()) }
    LaunchedEffect(filter, reloadKey) { items = loadOr(emptyList()) { load(filter) } }
    Column(Modifier.fillMaxSize()) {
        Row(Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
            TextButton(onClick = onBack) { Text(stringResource(Res.string.back)) }
            Text(title, style = MaterialTheme.typography.titleLarge, maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f))
            actions()
        }
        TextInput(hint, filter, { filter = it }, Modifier.padding(horizontal = 12.dp), tag = "search")
        header()
        if (items.isEmpty()) {
            Text(
                if (filter.isNotEmpty()) stringResource(Res.string.nothing_found) else empty,
                textAlign = TextAlign.Center,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.fillMaxWidth().padding(24.dp),
            )
        }
        LazyColumn(Modifier.fillMaxSize()) {
            items(items) { item ->
                row(item)
                HorizontalDivider()
            }
        }
    }
}

@Composable
internal fun TwoLines(title: String, subtitle: String, tag: String, onClick: () -> Unit) {
    Column(Modifier.fillMaxWidth().clickable(onClick = onClick).padding(horizontal = 16.dp, vertical = 10.dp).testTag(tag)) {
        Text(title, fontSize = 16.sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
        Text(subtitle, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}

@Composable
internal fun FactoryCartridges(model: AppModel, onBack: () -> Unit, onPick: (Long) -> Unit) {
    val mps = stringResource(Res.string.mps_value, "%s")
    SearchList<CartridgeItem>(
        stringResource(Res.string.factory_cartridges),
        stringResource(Res.string.search_name_caliber),
        stringResource(Res.string.no_factory),
        model.library::factoryCartridges,
        onBack,
    ) { c ->
        TwoLines(
            c.name,
            listOf(c.caliber, c.bulletName, mps.replace("%s", c.muzzleVelocity.roundToInt().toString()))
                .filter { it.isNotEmpty() }.joinToString(" · "),
            "factory:${c.name}",
        ) { onPick(c.id) }
    }
}

/**
 * The bullet library: search, add, edit, import data files; in picker
 * mode tapping a bullet chooses it (Edit still edits).
 */
@Composable
fun BulletList(model: AppModel, picker: Boolean, onBack: () -> Unit, onEdit: (Long) -> Unit, onPick: (Long) -> Unit) {
    val gr = stringResource(Res.string.gr_value, "%s")
    val ownCurve = stringResource(Res.string.own_curve)
    val bands = stringResource(Res.string.bands_count, "%s")
    val importedFiles = stringResource(Res.string.files_imported, "%s")
    val platform = LocalPlatform.current
    var report by remember { mutableStateOf<String?>(null) }
    SearchList<BulletItem>(
        stringResource(if (picker) Res.string.choose_bullet else Res.string.bullet_library),
        stringResource(Res.string.search_bullets),
        stringResource(Res.string.library_empty),
        model.library::bullets,
        onBack,
        reloadKey = model.revision,
        actions = {
            TextButton(onClick = {
                model.act {
                    val files = platform.files.openTexts(listOf("ammo", "drg", "reticle", "json"), multiple = true)
                    if (files.isNotEmpty()) {
                        val r = model.library.importFiles(files)
                        report = (listOf(importedFiles.replace("%s", r.imported.toString())) +
                            r.problems.map { "${it.file}: ${it.message}" }).joinToString("\n")
                    }
                }
            }) { Text(stringResource(Res.string.import_action)) }
            Button(onClick = { onEdit(0) }, modifier = Modifier.testTag("newBullet")) { Text(stringResource(Res.string.new_action)) }
        },
    ) { b ->
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(Modifier.weight(1f)) {
                TwoLines(
                    b.name,
                    listOf(
                        b.manufacturer, b.caliber, gr.replace("%s", b.massGr.fixed(1)),
                        if (b.dragKind == "curve") ownCurve
                        else "${b.dragTable} ${b.bc.fixed(3)}" + if (b.bcBands > 1) " " + bands.replace("%s", b.bcBands.toString()) else "",
                    ).filter { it.isNotEmpty() }.joinToString("  ·  "),
                    "bullet:${b.name}",
                ) { if (picker) onPick(b.id) else onEdit(b.id) }
            }
            if (picker) TextButton(onClick = { onEdit(b.id) }) { Text(stringResource(Res.string.edit)) }
        }
    }
    report?.let {
        AlertDialog(
            onDismissRequest = { report = null },
            title = { Text(stringResource(Res.string.import_action)) },
            text = { Text(it) },
            confirmButton = { TextButton(onClick = { report = null }) { Text("OK") } },
        )
    }
}
