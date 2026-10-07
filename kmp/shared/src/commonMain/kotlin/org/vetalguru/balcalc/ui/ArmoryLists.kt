package org.vetalguru.balcalc.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.PrimaryTabRow
import androidx.compose.material3.Tab
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.draw.alpha
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
import org.vetalguru.balcalc.QrShare
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

@Composable
internal fun Lists(model: AppModel, nav: ArmoryNav, onChosen: () -> Unit) {
    val st = model.state
    val platform = LocalPlatform.current
    var importMenu by remember { mutableStateOf(false) }
    var newMenu by remember { mutableStateOf(false) }
    var deleting by remember { mutableStateOf<Triple<String, Long, String>?>(null) }
    // The pictures of the shown list, as small images.
    val photoKind = if (nav.tab == 0) "rifle" else "cartridge"
    var thumbs by remember { mutableStateOf(emptyMap<Long, ImageBitmap>()) }
    LaunchedEffect(photoKind, model.photos.revision, st.rifles, st.cartridges) {
        thumbs = loadOr(emptyMap()) {
            model.photos.all(photoKind).mapNotNull { (id, b) -> pictureOf(b)?.let { id to it } }.toMap()
        }
    }
    val imported = stringResource(Res.string.imported)
    val copied = stringResource(Res.string.copied)
    val saved = stringResource(Res.string.saved)

    var qr by remember { mutableStateOf<Pair<String, List<String>>?>(null) }
    var qrImport by remember { mutableStateOf(false) }
    qr?.let { (title, parts) -> QrShowDialog(title, parts) { qr = null } }
    if (qrImport) QrImportDialog(model, onImported = { qrImport = false; model.message = imported }) { qrImport = false }

    fun share(kind: String, id: Long, toFile: Boolean) = model.act {
        val e = model.armory.exportJson(kind, id)
        if (toFile) {
            if (platform.files.saveText(e.fileName, e.json)) model.message = saved
        } else {
            platform.clipboard.copyText(e.json)
            model.message = copied
        }
    }

    fun showQr(kind: String, id: Long, title: String) = model.act {
        qr = title to QrShare.parts(model.armory.exportJson(kind, id).json)
    }

    // Read here, not only inside BoxWithConstraints: its subcomposition alone
    // does not recompose for state written by the LaunchedEffect above.
    val thumbList = thumbs

    BoxWithConstraints(Modifier.fillMaxSize()) {
        val showTitle = maxWidth >= 520.dp // the tabs say it on a phone
        Column(Modifier.fillMaxSize()) {
            Row(Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                Text(
                    if (showTitle) stringResource(Res.string.nav_armory) else "",
                    style = MaterialTheme.typography.titleLarge,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.weight(1f),
                )
                TextButton(onClick = { nav.push(Route.Bullets(null)) }) { Text(stringResource(Res.string.bullets)) }
                Box {
                    TextButton(onClick = { importMenu = true }) { Text(stringResource(Res.string.import_action)) }
                    DropdownMenu(importMenu, { importMenu = false }) {
                        DropdownMenuItem({ Text(stringResource(Res.string.from_file)) }, onClick = {
                            importMenu = false
                            model.act {
                                platform.files.openTexts(listOf("json"), multiple = false).firstOrNull()?.let {
                                    model.armory.importShared(it.content)
                                    model.message = imported
                                }
                            }
                        })
                        DropdownMenuItem({ Text(stringResource(Res.string.from_clipboard)) }, onClick = {
                            importMenu = false
                            model.act {
                                model.armory.importShared(platform.clipboard.pasteText().orEmpty())
                                model.message = imported
                            }
                        }, modifier = Modifier.testTag("importClipboard"))
                        DropdownMenuItem({ Text(stringResource(Res.string.qr_show)) }, onClick = {
                            importMenu = false
                            qrImport = true
                        }, modifier = Modifier.testTag("importQr"))
                    }
                }
                Box {
                    Button(onClick = {
                        if (nav.tab == 0) model.act { nav.push(Route.Rifle(model.armory.rifleForm(0))) } else newMenu = true
                    }, modifier = Modifier.testTag("new")) { Text(stringResource(Res.string.new_action)) }
                    DropdownMenu(newMenu, { newMenu = false }) {
                        DropdownMenuItem({ Text(stringResource(Res.string.empty_cartridge)) }, onClick = {
                            newMenu = false
                            model.act { nav.push(Route.Cartridge(model.armory.cartridgeForm(0))) }
                        }, modifier = Modifier.testTag("newEmptyCartridge"))
                        DropdownMenuItem({ Text(stringResource(Res.string.copy_factory)) }, onClick = {
                            newMenu = false
                            nav.push(Route.Factory())
                        }, modifier = Modifier.testTag("newFactoryCartridge"))
                    }
                }
            }
            PrimaryTabRow(selectedTabIndex = nav.tab) {
                Tab(nav.tab == 0, { nav.tab = 0 }, text = { Text(stringResource(Res.string.rifles_count, st.rifles.size)) },
                    modifier = Modifier.testTag("riflesTab"))
                Tab(nav.tab == 1, { nav.tab = 1 }, text = { Text(stringResource(Res.string.cartridges_count, st.cartridges.size)) },
                    modifier = Modifier.testTag("cartridgesTab"))
            }
            if (nav.tab == 0) {
                ItemList(
                    empty = stringResource(Res.string.no_rifles_hint),
                    items = st.rifles.map { Row4(it.id, it.name, it.caliber, true, it.id == st.currentRifleId) },
                    kind = "rifle",
                    onClick = { id ->
                        model.selectRifle(id)
                        nav.tab = 1 // now the cartridge
                    },
                    onEdit = { id -> model.act { nav.push(Route.Rifle(model.armory.rifleForm(id))) } },
                    onShare = ::share,
                    onQr = ::showQr,
                    thumbs = thumbList,
                    onDelete = { id, name -> deleting = Triple("rifle", id, name) },
                )
            } else {
                val mps = stringResource(Res.string.mps_value, "%s")
                ItemList(
                    empty = stringResource(Res.string.no_cartridges_hint),
                    items = st.cartridges.map { c ->
                        Row4(
                            c.id, c.name,
                            listOf(c.caliber, c.bulletName, mps.replace("%s", c.muzzleVelocity.roundToInt().toString()))
                                .filter { it.isNotEmpty() }.joinToString(" · "),
                            // Another calibre than the current rifle's: dimmed.
                            c.matches || st.rifles.isEmpty(),
                            c.id == st.currentCartridgeId,
                        )
                    },
                    kind = "cartridge",
                    onClick = { id ->
                        model.selectCartridge(id)
                        onChosen()
                    },
                    onEdit = { id -> model.act { nav.push(Route.Cartridge(model.armory.cartridgeForm(id))) } },
                    onShare = ::share,
                    onQr = ::showQr,
                    thumbs = thumbList,
                    onDelete = { id, name -> deleting = Triple("cartridge", id, name) },
                    onShotLog = { id ->
                        model.selectCartridge(id)
                        nav.openShotLog()
                    },
                )
            }
        }
    }

    deleting?.let { (kind, id, name) ->
        AlertDialog(
            onDismissRequest = { deleting = null },
            title = { Text(stringResource(if (kind == "rifle") Res.string.delete_rifle_q else Res.string.delete_cartridge_q)) },
            text = {
                Text(name + "\n\n" + stringResource(if (kind == "rifle") Res.string.delete_rifle_note else Res.string.delete_cartridge_note))
            },
            confirmButton = {
                TextButton(onClick = {
                    deleting = null
                    if (kind == "rifle") model.armory.deleteRifle(id) else model.armory.deleteCartridge(id)
                }, modifier = Modifier.testTag("confirmDelete")) { Text(stringResource(Res.string.yes)) }
            },
            dismissButton = { TextButton(onClick = { deleting = null }) { Text(stringResource(Res.string.no)) } },
        )
    }
}

private class Row4(val id: Long, val title: String, val subtitle: String, val bright: Boolean, val current: Boolean)

@Composable
private fun ItemList(
    empty: String,
    items: List<Row4>,
    kind: String,
    onClick: (Long) -> Unit,
    onEdit: (Long) -> Unit,
    onShare: (String, Long, Boolean) -> Unit,
    onQr: (String, Long, String) -> Unit,
    thumbs: Map<Long, ImageBitmap> = emptyMap(),
    onDelete: (Long, String) -> Unit,
    onShotLog: ((Long) -> Unit)? = null,
) {
    if (items.isEmpty()) {
        Box(Modifier.fillMaxSize().padding(24.dp), contentAlignment = Alignment.Center) {
            Text(empty, textAlign = TextAlign.Center, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        return
    }
    val selected = MaterialTheme.colorScheme.primary.copy(alpha = 0.12f)
    LazyColumn(Modifier.fillMaxSize().testTag("${kind}List")) {
        items(items, key = { it.id }) { item ->
            var menu by remember { mutableStateOf(false) }
            Row(
                Modifier.fillMaxWidth()
                    .then(if (item.current) Modifier.background(selected) else Modifier)
                    .clickable { onClick(item.id) }
                    .padding(start = 16.dp, top = 8.dp, bottom = 8.dp)
                    .testTag("$kind:${item.title}"),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                thumbs[item.id]?.let { Thumbnail(it, 44.dp, Modifier.padding(end = 12.dp).testTag("thumb:${item.title}")) }
                Column(Modifier.weight(1f).alpha(if (item.bright) 1f else 0.55f)) {
                    Text(item.title, fontSize = 16.sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    if (item.subtitle.isNotEmpty()) {
                        Text(item.subtitle, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    }
                }
                TextButton(onClick = { onEdit(item.id) }) { Text(stringResource(Res.string.edit)) }
                Box {
                    TextButton(onClick = { menu = true }, modifier = Modifier.testTag("more:${item.title}")) { Text("⋮", fontSize = 18.sp) }
                    DropdownMenu(menu, { menu = false }) {
                        if (onShotLog != null) {
                            DropdownMenuItem({ Text(stringResource(Res.string.shot_log)) }, onClick = {
                                menu = false
                                onShotLog(item.id)
                            }, modifier = Modifier.testTag("shotLog"))
                        }
                        DropdownMenuItem({ Text(stringResource(Res.string.export_file)) }, onClick = {
                            menu = false
                            onShare(kind, item.id, true)
                        })
                        DropdownMenuItem({ Text(stringResource(Res.string.copy_clipboard)) }, onClick = {
                            menu = false
                            onShare(kind, item.id, false)
                        }, modifier = Modifier.testTag("copy"))
                        DropdownMenuItem({ Text(stringResource(Res.string.qr_show)) }, onClick = {
                            menu = false
                            onQr(kind, item.id, item.title)
                        }, modifier = Modifier.testTag("showQr"))
                        DropdownMenuItem({ Text(stringResource(Res.string.delete)) }, onClick = {
                            menu = false
                            onDelete(item.id, item.title)
                        }, modifier = Modifier.testTag("delete"))
                    }
                }
            }
            HorizontalDivider()
        }
    }
}
