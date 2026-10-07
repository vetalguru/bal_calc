package org.vetalguru.balcalc.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.BackHandler
import org.vetalguru.balcalc.PhotoChange
import org.vetalguru.balcalc.core.CartridgeForm
import org.vetalguru.balcalc.core.RifleForm
import org.vetalguru.balcalc.core.LibraryRifle
import org.vetalguru.balcalc.core.LibraryScope
import org.vetalguru.balcalc.core.ScopeClick
import org.vetalguru.balcalc.res.*

/** Inner pages of the Rifles tab, as a stack. */
internal sealed interface Route {
    data object Lists : Route
    class Rifle(form: RifleForm) : Route, WithPhoto() {
        var form by mutableStateOf(form)
    }
    /** The published scope or rifle catalogs; a pick fills the rifle form. */
    class LibraryScopes(val pick: (LibraryScope, ScopeClick) -> Unit) : Route
    class LibraryRifles(val pick: (LibraryRifle) -> Unit) : Route
    class Cartridge(form: CartridgeForm) : Route, WithPhoto() {
        var form by mutableStateOf(form)
    }
    class Factory : Route
    /** The bullet library; [pick] set: choose one for a cartridge. */
    class Bullets(val pick: ((Long) -> Unit)?) : Route
    class Bullet(val form: org.vetalguru.balcalc.core.BulletForm) : Route
    data object Truing : Route
    data object Group : Route
}

/** An editor's picture: loaded once, saved with the form only when changed. */
internal open class WithPhoto {
    var photo by mutableStateOf<ByteArray?>(null)
    var photoChanged by mutableStateOf(false)
    var photoLoaded = false

    fun change(image: ByteArray?) {
        photo = image
        photoChanged = true
    }

    val photoChange get() = if (photoChanged) PhotoChange(photo) else null

    suspend fun load(model: AppModel, kind: String, id: Long) {
        if (photoLoaded) return
        photoLoaded = true
        if (id <= 0) return
        val stored = model.photos.one(kind, id)
        // A picture chosen while this loaded wins: the stored one is older.
        if (!photoChanged) photo = stored
    }
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
        is Route.Rifle -> RifleEditor(model, r, nav)
        is Route.LibraryScopes -> LibraryScopes(model, onBack = nav::back) { s, c -> nav.back(); r.pick(s, c) }
        is Route.LibraryRifles -> LibraryRifles(model, onBack = nav::back) { rifle -> nav.back(); r.pick(rifle) }
        is Route.Cartridge -> CartridgeEditor(model, r, nav)
        is Route.Factory -> FactoryCartridges(model, onBack = nav::back) { id ->
            nav.back()
            model.act { nav.push(Route.Cartridge(model.library.cartridgeFormFromFactory(id))) }
        }
        is Route.Bullets -> BulletList(
            model,
            picker = r.pick != null,
            onBack = nav::back,
            onEdit = { id -> model.act { nav.push(Route.Bullet(model.library.bulletForm(id))) } },
        ) { id ->
            nav.back()
            r.pick?.invoke(id)
        }
        is Route.Bullet -> BulletEditor(model, r.form) { nav.back() }
        Route.Truing -> TruingScreen(model, onBack = nav::back, onGroup = { nav.push(Route.Group) })
        Route.Group -> GroupScreen(model, onBack = nav::back)
    }
}

/** Opens the shot log of the current rifle with this cartridge. */
fun ArmoryNav.openShotLog() = push(Route.Truing)
