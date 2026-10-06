package org.vetalguru.balcalc.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
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
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.BackHandler
import org.vetalguru.balcalc.LocalPlatform
import org.vetalguru.balcalc.core.CartridgeForm
import org.vetalguru.balcalc.core.CartridgeItem
import org.vetalguru.balcalc.core.RifleForm
import org.vetalguru.balcalc.core.BulletItem
import org.vetalguru.balcalc.core.ReticleItem
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** Inner pages of the Rifles tab, as a stack. */
internal sealed interface Route {
    data object Lists : Route
    class Rifle(val form: RifleForm) : Route
    class Cartridge(form: CartridgeForm) : Route {
        var form by mutableStateOf(form)
    }
    class Factory : Route
    /** The bullet library; [pick] set: choose one for a cartridge. */
    class Bullets(val pick: ((Long) -> Unit)?) : Route
    class Bullet(val form: org.vetalguru.balcalc.core.BulletForm) : Route
    data object Truing : Route
}

/** The open inner page of the Rifles tab, so Back can close it first. */
class ArmoryNav {
    internal val stack = mutableStateListOf<Any>(Route.Lists)
    var tab by mutableStateOf(0)
    val canGoBack get() = stack.size > 1
    fun back() { if (canGoBack) stack.removeAt(stack.lastIndex) }
    internal fun push(r: Route) { stack.add(r) }
    internal val top: Route get() = stack.last() as Route
}

/** Rifles and cartridges: two lists to choose from, create, edit, delete and share. */
@Composable
fun ArmoryScreen(model: AppModel, nav: ArmoryNav, onChosen: () -> Unit) {
    BackHandler(nav.canGoBack) { nav.back() }
    when (val r = nav.top) {
        Route.Lists -> Lists(model, nav, onChosen)
        is Route.Rifle -> RifleEditor(model, r.form) { nav.back() }
        is Route.Cartridge -> CartridgeEditor(model, r, nav)
        is Route.Factory -> FactoryCartridges(model, onBack = nav::back) { id ->
            nav.back()
            model.act { nav.push(Route.Cartridge(model.cartridgeFormFromLibrary(id))) }
        }
        is Route.Bullets -> BulletList(
            model,
            picker = r.pick != null,
            onBack = nav::back,
            onEdit = { id -> model.act { nav.push(Route.Bullet(model.bulletForm(id))) } },
        ) { id ->
            nav.back()
            r.pick?.invoke(id)
        }
        is Route.Bullet -> BulletEditor(model, r.form) { nav.back() }
        Route.Truing -> TruingScreen(model, onBack = nav::back)
    }
}

/** Opens the shot log of the current rifle with this cartridge. */
fun ArmoryNav.openShotLog() = push(Route.Truing)

@Composable
private fun Lists(model: AppModel, nav: ArmoryNav, onChosen: () -> Unit) {
    val st = model.state
    val platform = LocalPlatform.current
    var importMenu by remember { mutableStateOf(false) }
    var newMenu by remember { mutableStateOf(false) }
    var deleting by remember { mutableStateOf<Triple<String, Long, String>?>(null) }
    val imported = stringResource(Res.string.imported)
    val copied = stringResource(Res.string.copied)
    val saved = stringResource(Res.string.saved)

    fun share(kind: String, id: Long, toFile: Boolean) = model.act {
        val e = model.exportJson(kind, id)
        if (toFile) {
            if (platform.saveText(e.fileName, e.json)) model.message = saved
        } else {
            platform.copyText(e.json)
            model.message = copied
        }
    }

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
                                platform.openTexts(listOf("json"), multiple = false).firstOrNull()?.let {
                                    model.importShared(it.content)
                                    model.message = imported
                                }
                            }
                        })
                        DropdownMenuItem({ Text(stringResource(Res.string.from_clipboard)) }, onClick = {
                            importMenu = false
                            model.act {
                                model.importShared(platform.pasteText().orEmpty())
                                model.message = imported
                            }
                        }, modifier = Modifier.testTag("importClipboard"))
                    }
                }
                Box {
                    Button(onClick = {
                        if (nav.tab == 0) model.act { nav.push(Route.Rifle(model.rifleForm(0))) } else newMenu = true
                    }, modifier = Modifier.testTag("new")) { Text(stringResource(Res.string.new_action)) }
                    DropdownMenu(newMenu, { newMenu = false }) {
                        DropdownMenuItem({ Text(stringResource(Res.string.empty_cartridge)) }, onClick = {
                            newMenu = false
                            model.act { nav.push(Route.Cartridge(model.cartridgeForm(0))) }
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
                    onEdit = { id -> model.act { nav.push(Route.Rifle(model.rifleForm(id))) } },
                    onShare = ::share,
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
                    onEdit = { id -> model.act { nav.push(Route.Cartridge(model.cartridgeForm(id))) } },
                    onShare = ::share,
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
                    if (kind == "rifle") model.deleteRifle(id) else model.deleteCartridge(id)
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


// ---- Editors ----------------------------------------------------------------

/** One editor screen: bar, error line, scrolled sections, Save at the bottom. */
@Composable
private fun EditorPage(
    title: String,
    error: String?,
    onCancel: () -> Unit,
    onSave: () -> Unit,
    content: @Composable (wide: Boolean) -> Unit,
) {
    val cancel = stringResource(Res.string.cancel)
    val save = stringResource(Res.string.save)
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val wide = maxWidth >= 640.dp
        Column(Modifier.fillMaxSize()) {
            EditorBar(title, cancel, save, onCancel, onSave)
            Column(
                Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(vertical = 12.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                if (!error.isNullOrEmpty()) {
                    Text(error, color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(horizontal = 16.dp).testTag("formError"))
                }
                content(wide)
                Button(onClick = onSave, modifier = Modifier.align(Alignment.CenterHorizontally).padding(bottom = 24.dp)) { Text(save) }
            }
        }
    }
}

@Composable
private fun RifleEditor(model: AppModel, initial: RifleForm, onDone: () -> Unit) {
    var f by remember { mutableStateOf(initial) }
    var error by remember { mutableStateOf<String?>(null) }
    var reticles by remember { mutableStateOf(emptyList<ReticleItem>()) }
    LaunchedEffect(Unit) { reticles = model.reticles() }
    val reticleList = reticles // read outside the editor's BoxWithConstraints (see TruingScreen)
    val scope = rememberCoroutineScope()
    val cm = stringResource(Res.string.unit_cm)
    val inch = stringResource(Res.string.unit_in)
    val m = stringResource(Res.string.unit_m)
    val c = stringResource(Res.string.unit_c)
    val x = "×"
    EditorPage(
        title = stringResource(if (f.rifleId > 0) Res.string.edit_rifle else Res.string.new_rifle),
        error = error,
        onCancel = onDone,
        onSave = { scope.launch { error = model.saveRifle(f); if (error == null) onDone() } },
    ) { wide ->
        Section(stringResource(Res.string.rifle)) {
            TextInput(stringResource(Res.string.rifle_name_hint), f.name, { f = f.copy(name = it) }, tag = "rifleName")
            TextInput(stringResource(Res.string.caliber_hint), f.caliber, { f = f.copy(caliber = it) }, tag = "rifleCaliber")
            Fields(
                wide,
                { mod -> NumberField(stringResource(Res.string.sight_height), f.sightHeightCm, { f = f.copy(sightHeightCm = it) }, mod, cm, from = 0.0, to = 20.0) },
                { mod -> NumberField(stringResource(Res.string.twist), f.twistIn, { f = f.copy(twistIn = it) }, mod, inch, 2, 0.0, 60.0) },
            )
            SwitchRow(stringResource(Res.string.left_twist), f.twistLeft, { f = f.copy(twistLeft = it) })
        }
        Section(stringResource(Res.string.scope)) {
            Fields(
                wide,
                { mod ->
                    ChoiceField(
                        stringResource(Res.string.turret_units),
                        listOf(
                            "mrad" to stringResource(Res.string.unit_mrad),
                            "moa" to stringResource(Res.string.unit_moa),
                            "smoa" to stringResource(Res.string.click_smoa),
                            "cm100m" to stringResource(Res.string.click_cm100m),
                        ),
                        f.clickUnits, { f = f.copy(clickUnits = it) }, mod,
                    )
                },
                { mod -> NumberField(stringResource(Res.string.one_click), f.clickValue, { f = f.copy(clickValue = it) }, mod, decimals = 4, from = 0.0, to = 10.0) },
                { mod ->
                    ChoiceField(
                        stringResource(Res.string.reticle),
                        listOf(0L to stringResource(Res.string.reticle_none)) + reticleList.map { it.id to it.name },
                        f.reticleId, { f = f.copy(reticleId = it) }, mod,
                    )
                },
                { mod ->
                    ChoiceField(
                        stringResource(Res.string.focal_plane),
                        listOf("ffp" to stringResource(Res.string.ffp), "sfp" to stringResource(Res.string.sfp)),
                        f.focalPlane, { f = f.copy(focalPlane = it) }, mod,
                    )
                },
                { mod -> NumberField(stringResource(Res.string.mag_from), f.minMagnification, { f = f.copy(minMagnification = it) }, mod, x, from = 0.0, to = 100.0) },
                { mod -> NumberField(stringResource(Res.string.mag_to), f.maxMagnification, { f = f.copy(maxMagnification = it) }, mod, x, from = 0.0, to = 100.0) },
            )
            if (f.focalPlane == "sfp") {
                NumberField(stringResource(Res.string.sfp_reference), f.sfpReferenceMagnification, { f = f.copy(sfpReferenceMagnification = it) }, unit = x, from = 0.0, to = 100.0)
            }
        }
        Section(stringResource(Res.string.zero)) {
            Text(stringResource(Res.string.zero_note), color = MaterialTheme.colorScheme.onSurfaceVariant)
            Fields(
                wide,
                { mod -> NumberField(stringResource(Res.string.zero_distance), f.zeroRangeM, { f = f.copy(zeroRangeM = it) }, mod, m, 0, 10.0, 1000.0, tag = "zeroRange") },
                { mod ->
                    NumberField(stringResource(Res.string.zero_temperature), f.zeroTemperatureC,
                        { f = f.copy(zeroTemperatureC = it, zeroPowderC = it) }, mod, c, from = -60.0, to = 60.0)
                },
                { mod -> NumberField(stringResource(Res.string.zero_pressure), f.zeroPressureHpa, { f = f.copy(zeroPressureHpa = it) }, mod, stringResource(Res.string.unit_hpa), from = 300.0, to = 1200.0) },
                { mod -> NumberField(stringResource(Res.string.zero_altitude), f.zeroAltitudeM, { f = f.copy(zeroAltitudeM = it) }, mod, m, 0, -500.0, 6000.0) },
                { mod -> NumberField(stringResource(Res.string.zero_humidity), f.zeroHumidityPct, { f = f.copy(zeroHumidityPct = it) }, mod, "%", 0, 0.0, 100.0) },
            )
        }
    }
}

@Composable
private fun CartridgeEditor(model: AppModel, route: Route.Cartridge, nav: ArmoryNav) {
    var f by route::form
    var error by remember { mutableStateOf<String?>(null) }
    val scope = rememberCoroutineScope()
    val fromLibrary = f.libraryBulletId > 0
    val inch = stringResource(Res.string.unit_in)
    EditorPage(
        title = stringResource(if (f.cartridgeId > 0) Res.string.edit_cartridge else Res.string.new_cartridge),
        error = error,
        onCancel = nav::back,
        onSave = { scope.launch { error = model.saveCartridge(f); if (error == null) nav.back() } },
    ) { wide ->
        Section(stringResource(Res.string.cartridge)) {
            TextInput(stringResource(Res.string.cartridge_name_hint), f.name, { f = f.copy(name = it) }, tag = "cartridgeName")
            TextInput(stringResource(Res.string.caliber_hint), f.caliber, { f = f.copy(caliber = it) }, tag = "cartridgeCaliber")
        }
        Section(stringResource(Res.string.bullet)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    if (fromLibrary) stringResource(Res.string.from_library_named, f.bulletName) else stringResource(Res.string.own_bullet),
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.weight(1f),
                )
                TextButton(onClick = {
                    nav.push(Route.Bullets { id -> model.act { f = model.cartridgeFormWithBullet(f, id) } })
                }, modifier = Modifier.testTag("chooseBullet")) {
                    Text(stringResource(if (fromLibrary) Res.string.change else Res.string.from_library))
                }
                if (fromLibrary) TextButton(onClick = { f = f.copy(libraryBulletId = 0) }) { Text(stringResource(Res.string.edit_as_own)) }
            }
            TextInput(stringResource(Res.string.bullet_hint), f.bulletName, { f = f.copy(bulletName = it) }, enabled = !fromLibrary, tag = "bulletName")
            Column(Modifier.alpha(if (fromLibrary) 0.5f else 1f), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Fields(
                    wide,
                    { mod ->
                        ChoiceField(
                            stringResource(Res.string.drag_model),
                            listOf("G7", "G1", "G2", "G5", "G6", "G8", "GI", "GS", "RA4").map { it to it },
                            f.dragTable, { if (!fromLibrary) f = f.copy(dragTable = it) }, mod,
                        )
                    },
                    { mod -> NumberField(stringResource(Res.string.bc), f.bc, { if (!fromLibrary) f = f.copy(bc = it) }, mod, decimals = 3, from = 0.0, to = 2.0, tag = "bc") },
                    { mod -> NumberField(stringResource(Res.string.weight), f.massGr, { if (!fromLibrary) f = f.copy(massGr = it) }, mod, stringResource(Res.string.unit_gr), from = 0.0, to = 2000.0, tag = "mass") },
                    { mod -> NumberField(stringResource(Res.string.diameter), f.diameterIn, { if (!fromLibrary) f = f.copy(diameterIn = it) }, mod, inch, 3, 0.0, 1.0, tag = "diameter") },
                    { mod -> NumberField(stringResource(Res.string.length_spin), f.lengthIn, { if (!fromLibrary) f = f.copy(lengthIn = it) }, mod, inch, 3, 0.0, 4.0) },
                )
            }
        }
        Section(stringResource(Res.string.velocity)) {
            Fields(
                wide,
                { mod -> NumberField(stringResource(Res.string.muzzle_velocity), f.muzzleVelocity, { f = f.copy(muzzleVelocity = it) }, mod, stringResource(Res.string.unit_mps), from = 0.0, to = 2000.0, tag = "muzzleVelocity") },
                { mod -> NumberField(stringResource(Res.string.measured_at_powder), f.powderReferenceC, { f = f.copy(powderReferenceC = it) }, mod, stringResource(Res.string.unit_c), from = -60.0, to = 80.0) },
                { mod -> NumberField(stringResource(Res.string.powder_sensitivity), f.powderSensitivity, { f = f.copy(powderSensitivity = it) }, mod, stringResource(Res.string.unit_pct_per_c), 3, -2.0, 2.0) },
            )
        }
    }
}

// ---- Factory cartridges and bullets ---------------------------------------------

/** A searchable list page with a Back bar. */
@Composable
private fun <T> SearchList(
    title: String,
    hint: String,
    empty: String,
    load: suspend (String) -> List<T>,
    onBack: () -> Unit,
    reloadKey: Any? = null,
    actions: @Composable () -> Unit = {},
    row: @Composable (T) -> Unit,
) {
    var filter by rememberSaveable { mutableStateOf("") }
    var items by remember { mutableStateOf(emptyList<T>()) }
    LaunchedEffect(filter, reloadKey) { items = runCatching { load(filter) }.getOrDefault(emptyList()) }
    Column(Modifier.fillMaxSize()) {
        Row(Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
            TextButton(onClick = onBack) { Text(stringResource(Res.string.back)) }
            Text(title, style = MaterialTheme.typography.titleLarge, maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f))
            actions()
        }
        TextInput(hint, filter, { filter = it }, Modifier.padding(horizontal = 12.dp), tag = "search")
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
private fun TwoLines(title: String, subtitle: String, tag: String, onClick: () -> Unit) {
    Column(Modifier.fillMaxWidth().clickable(onClick = onClick).padding(horizontal = 16.dp, vertical = 10.dp).testTag(tag)) {
        Text(title, fontSize = 16.sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
        Text(subtitle, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}

@Composable
private fun FactoryCartridges(model: AppModel, onBack: () -> Unit, onPick: (Long) -> Unit) {
    val mps = stringResource(Res.string.mps_value, "%s")
    SearchList<CartridgeItem>(
        stringResource(Res.string.factory_cartridges),
        stringResource(Res.string.search_name_caliber),
        stringResource(Res.string.no_factory),
        model::libraryCartridges,
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
        model::libraryBullets,
        onBack,
        reloadKey = model.revision,
        actions = {
            TextButton(onClick = {
                model.act {
                    val files = platform.openTexts(listOf("ammo", "drg", "reticle", "json"), multiple = true)
                    if (files.isNotEmpty()) {
                        val r = model.importFiles(files)
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

