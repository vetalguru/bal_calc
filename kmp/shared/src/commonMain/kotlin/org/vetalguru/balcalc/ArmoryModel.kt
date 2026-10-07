package org.vetalguru.balcalc

import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.double
import kotlinx.serialization.json.encodeToJsonElement
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.long
import kotlinx.serialization.json.put
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.core.CartridgeForm
import org.vetalguru.balcalc.core.ExportedJson
import org.vetalguru.balcalc.core.RifleForm

/** The user's rifles and cartridges: their forms, deleting, sharing. */
class ArmoryModel internal constructor(private val app: AppModel) {
    private val api get() = app.api

    suspend fun rifleForm(id: Long): RifleForm = api.get("rifleForm", idArgs(id))
    suspend fun cartridgeForm(id: Long): CartridgeForm = api.get("cartridgeForm", idArgs(id))

    suspend fun cartridgeFormWithBullet(form: CartridgeForm, bulletId: Long): CartridgeForm =
        api.get("cartridgeFormWithBullet", buildJsonObject {
            put("form", Api.json.encodeToJsonElement(form))
            put("bulletId", bulletId)
        })

    /**
     * Saves a form (and its picture when it changed); the saved record
     * becomes current. Returns the core's error or null.
     */
    suspend fun saveRifle(form: RifleForm, photo: PhotoChange? = null): String? =
        app.saveForm("saveRifle", formArgs(form)) { saved -> savePhoto("rifle", saved, photo) }

    suspend fun saveCartridge(form: CartridgeForm, photo: PhotoChange? = null): String? =
        app.saveForm("saveCartridge", formArgs(form)) { saved -> savePhoto("cartridge", saved, photo) }

    private suspend fun savePhoto(kind: String, saved: kotlinx.serialization.json.JsonElement, photo: PhotoChange?) {
        if (photo != null) app.photos.set(kind, saved.jsonObject.getValue("id").jsonPrimitive.long, photo.image)
    }

    fun deleteRifle(id: Long) = app.act { app.state = api.get("deleteRifle", idArgs(id)); app.recompute() }
    fun deleteCartridge(id: Long) = app.act { app.state = api.get("deleteCartridge", idArgs(id)); app.recompute() }

    fun addSample(rifleName: String, cartridgeName: String) = app.act {
        app.state = api.get("addSample", buildJsonObject {
            put("rifleName", rifleName)
            put("cartridgeName", cartridgeName)
        })
        app.recompute()
    }

    /** Miller Sg at standard air; 0 when an input is missing. */
    suspend fun stability(twistIn: Double, massGr: Double, diameterIn: Double, lengthIn: Double, velocityMps: Double): Double =
        api.call("stability", buildJsonObject {
            put("twistIn", twistIn)
            put("massGr", massGr)
            put("diameterIn", diameterIn)
            put("lengthIn", lengthIn)
            put("velocityMps", velocityMps)
        }).jsonObject.getValue("sg").jsonPrimitive.double

    /** A rifle or cartridge as a file: (JSON, suggested file name). */
    suspend fun exportJson(kind: String, id: Long): ExportedJson =
        api.get("exportJson", buildJsonObject {
            put("kind", kind)
            put("id", id)
        })

    /** A shared rifle/cartridge (or old profile) file; what it brought becomes current. */
    suspend fun importShared(text: String) = app.detached {
        app.state = api.get("importShared", buildJsonObject { put("text", text) })
        app.recompute()
    }
}
