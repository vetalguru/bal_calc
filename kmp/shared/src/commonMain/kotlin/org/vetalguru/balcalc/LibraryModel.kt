package org.vetalguru.balcalc

import kotlinx.serialization.json.addJsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonArray
import org.vetalguru.balcalc.core.ApiException
import org.vetalguru.balcalc.core.BulletForm
import org.vetalguru.balcalc.core.BulletItem
import org.vetalguru.balcalc.core.CartridgeForm
import org.vetalguru.balcalc.core.CartridgeItem
import org.vetalguru.balcalc.core.ImportReport
import org.vetalguru.balcalc.core.LibraryRifle
import org.vetalguru.balcalc.core.LibraryScope
import org.vetalguru.balcalc.core.NamedText
import org.vetalguru.balcalc.core.ReticleItem

/** The library: bullets, factory cartridges, reticles, scope and rifle catalogs, imports. */
class LibraryModel internal constructor(private val app: AppModel) {
    private val api get() = app.api
    private fun filterArgs(filter: String) = buildJsonObject { put("filter", filter) }

    suspend fun bullets(filter: String): List<BulletItem> = api.get("libraryBullets", filterArgs(filter))
    suspend fun factoryCartridges(filter: String): List<CartridgeItem> = api.get("libraryCartridges", filterArgs(filter))
    suspend fun scopes(filter: String): List<LibraryScope> = api.get("libraryScopes", filterArgs(filter))
    suspend fun rifles(filter: String): List<LibraryRifle> = api.get("libraryRifles", filterArgs(filter))
    suspend fun reticles(): List<ReticleItem> = api.get("reticles")

    /** A factory cartridge as the form of a new cartridge of the user's. */
    suspend fun cartridgeFormFromFactory(id: Long): CartridgeForm = api.get("cartridgeFormFromLibrary", idArgs(id))

    suspend fun bulletForm(id: Long): BulletForm = api.get("bulletForm", idArgs(id))

    suspend fun saveBullet(form: BulletForm): String? = app.saveForm("saveBullet", formArgs(form))

    suspend fun deleteBullet(id: Long): String? = app.detached {
        try {
            api.call("deleteBullet", idArgs(id))
            app.recompute() // lists reload on the revision
            null
        } catch (e: ApiException) {
            e.message
        }
    }

    /** .ammo / .drg / .reticle / bullet-list / rifle / cartridge files into the library. */
    suspend fun importFiles(files: List<NamedText>): ImportReport = app.detached {
        val report: ImportReport = api.get("importFiles", buildJsonObject {
            putJsonArray("files") {
                files.forEach { f ->
                    addJsonObject {
                        put("name", f.name)
                        put("content", f.content)
                    }
                }
            }
        })
        app.state = api.get("state")
        app.recompute()
        report
    }
}
