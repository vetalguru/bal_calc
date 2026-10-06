package org.vetalguru.balcalc

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.async
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.launch
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.decodeFromJsonElement
import kotlinx.serialization.json.encodeToJsonElement
import kotlinx.serialization.json.double
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import kotlinx.serialization.json.addJsonObject
import kotlinx.serialization.json.putJsonArray
import org.vetalguru.balcalc.core.ImportReport
import org.vetalguru.balcalc.core.Info
import org.vetalguru.balcalc.core.NamedText
import org.vetalguru.balcalc.core.PairOption
import org.vetalguru.balcalc.core.SeedReport
import org.vetalguru.balcalc.core.Shot
import org.vetalguru.balcalc.core.TruingResult
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.core.ApiException
import org.vetalguru.balcalc.core.BulletForm
import org.vetalguru.balcalc.core.BulletItem
import org.vetalguru.balcalc.core.CartridgeForm
import org.vetalguru.balcalc.core.CartridgeItem
import org.vetalguru.balcalc.core.ExportedJson
import org.vetalguru.balcalc.core.ReticleItem
import org.vetalguru.balcalc.core.RifleForm
import org.vetalguru.balcalc.core.AppState
import org.vetalguru.balcalc.core.Conditions
import org.vetalguru.balcalc.core.RangeTable
import org.vetalguru.balcalc.core.Solution

/** A typed call: the result decoded as [T]. */
suspend inline fun <reified T> Api.get(method: String, args: JsonObject = JsonObject(emptyMap())): T =
    Api.json.decodeFromJsonElement(call(method, args))

/**
 * What the screens show and change: the core's state (selection, settings,
 * conditions) and the solution for it. Edits update the screen at once and
 * reach the core in order; the solution follows the latest edit only.
 */
class AppModel(val api: Api, private val scope: CoroutineScope) {
    var ready by mutableStateOf(false)
        private set
    var state by mutableStateOf(AppState())
        private set
    var solution by mutableStateOf(Solution())
        private set
    /** Bumps whenever results may have changed (tables reload on it). */
    var revision by mutableIntStateOf(0)
        private set
    /** The last failure of an action, for a snackbar; the screen clears it. */
    var message by mutableStateOf<String?>(null)

    // Solution requests, conflated: one fetch at a time, and one more after
    // the last request. A fetch reads the core as it is, so the solution
    // shown is never older than the last change, whichever thread asks.
    private val solutionRequests = Channel<Unit>(Channel.CONFLATED)

    init {
        scope.launch {
            for (request in solutionRequests) {
                try {
                    solution = api.get("solution")
                    revision++
                } catch (e: Exception) {
                    if (e is kotlinx.coroutines.CancellationException) throw e
                    message = e.message
                }
            }
        }
    }

    fun start(startup: suspend Api.() -> Unit) = act {
        api.startup()
        state = api.get("state")
        ready = true
        recompute()
    }

    /** Runs [block] off the UI thread, reporting a failure as [message]. */
    fun act(block: suspend () -> Unit): Job = scope.launch {
        try {
            block()
        } catch (e: Exception) {
            if (e is kotlinx.coroutines.CancellationException) throw e
            message = e.message
        }
    }

    private fun recompute() {
        solutionRequests.trySend(Unit)
    }

    /**
     * Runs a change in the model's scope: it completes (and refreshes the
     * solution) even if the screen that asked is gone meanwhile, e.g. a tab
     * switched right after Save.
     */
    private suspend fun <T> detached(block: suspend () -> T): T = scope.async { block() }.await()

    fun updateConditions(change: (Conditions) -> Conditions) {
        val c = change(state.conditions)
        state = state.copy(conditions = c)
        act {
            api.call("setConditions", Api.json.encodeToJsonElement(Conditions.serializer(), c).jsonObject)
            recompute()
        }
    }

    fun setTargetRange(m: Double) = updateConditions { it.copy(targetRangeM = m.coerceIn(10.0, 3000.0)) }

    fun selectRifle(id: Long) = act {
        state = api.get("select", buildJsonObject { put("rifleId", id) })
        recompute()
    }

    fun selectCartridge(id: Long) = act {
        state = api.get("select", buildJsonObject { put("cartridgeId", id) })
        recompute()
    }

    fun setSettings(args: JsonObject) = act {
        state = api.get("setSettings", args)
        recompute()
    }

    fun addSample(rifleName: String, cartridgeName: String) = act {
        state = api.get("addSample", buildJsonObject {
            put("rifleName", rifleName)
            put("cartridgeName", cartridgeName)
        })
        recompute()
    }

    // ---- Rifles, cartridges, library -----------------------------------

    private fun id(value: Long) = buildJsonObject { put("id", value) }
    private inline fun <reified T> formArgs(form: T) =
        buildJsonObject { put("form", Api.json.encodeToJsonElement(form)) }

    suspend fun rifleForm(id: Long): RifleForm = api.get("rifleForm", id(id))
    suspend fun cartridgeForm(id: Long): CartridgeForm = api.get("cartridgeForm", id(id))
    suspend fun bulletForm(id: Long): BulletForm = api.get("bulletForm", id(id))
    suspend fun reticles(): List<ReticleItem> = api.get("reticles")
    suspend fun libraryBullets(filter: String): List<BulletItem> =
        api.get("libraryBullets", buildJsonObject { put("filter", filter) })
    suspend fun libraryCartridges(filter: String): List<CartridgeItem> =
        api.get("libraryCartridges", buildJsonObject { put("filter", filter) })
    suspend fun cartridgeFormFromLibrary(id: Long): CartridgeForm = api.get("cartridgeFormFromLibrary", id(id))
    suspend fun cartridgeFormWithBullet(form: CartridgeForm, bulletId: Long): CartridgeForm =
        api.get("cartridgeFormWithBullet", buildJsonObject {
            put("form", Api.json.encodeToJsonElement(form))
            put("bulletId", bulletId)
        })

    /**
     * Saves a form; the saved record becomes current. Returns the core's
     * error (a validation sentence) or null.
     */
    suspend fun saveRifle(form: RifleForm): String? = saveForm("saveRifle", formArgs(form))
    suspend fun saveCartridge(form: CartridgeForm): String? = saveForm("saveCartridge", formArgs(form))
    suspend fun saveBullet(form: BulletForm): String? = saveForm("saveBullet", formArgs(form))

    private suspend fun saveForm(method: String, args: JsonObject): String? = detached {
        try {
            api.call(method, args)
            state = api.get("state")
            recompute()
            null
        } catch (e: ApiException) {
            e.message
        }
    }

    fun deleteRifle(id: Long) = act { state = api.get("deleteRifle", id(id)); recompute() }
    fun deleteCartridge(id: Long) = act { state = api.get("deleteCartridge", id(id)); recompute() }
    suspend fun deleteBullet(id: Long): String? = detached {
        try {
            api.call("deleteBullet", id(id))
            recompute() // lists reload on the revision
            null
        } catch (e: ApiException) {
            e.message
        }
    }

    /** A rifle or cartridge as a file: (JSON, suggested file name). */
    suspend fun exportJson(kind: String, id: Long): ExportedJson =
        api.get("exportJson", buildJsonObject {
            put("kind", kind)
            put("id", id)
        })

    /** A shared rifle/cartridge (or old profile) file; what it brought becomes current. */
    suspend fun importShared(text: String) = detached {
        state = api.get("importShared", buildJsonObject { put("text", text) })
        recompute()
    }

    // ---- Shot log and truing (the current rifle + cartridge) -------------

    /** Bumps when the shot log or the pair's truing changed. */
    var shotsRevision by mutableIntStateOf(0)
        private set

    suspend fun shots(): List<Shot> = api.get("shots")

    /** Angles in the current unit; returns the core's error or null. */
    suspend fun logShot(rangeM: Double, elevation: Double, windage: Double?, notes: String): String? = detached {
        try {
            api.call("logShot", buildJsonObject {
                put("rangeM", rangeM)
                put("elevation", elevation)
                put("hasWindage", windage != null)
                put("windage", windage ?: 0.0)
                put("notes", notes)
            })
            shotsRevision++
            null
        } catch (e: ApiException) {
            e.message
        }
    }

    fun deleteShot(id: Long) = act { api.call("deleteShot", id(id)); shotsRevision++ }

    fun setShotUsed(id: Long, used: Boolean) = act {
        api.call("setShotUsed", buildJsonObject {
            put("id", id)
            put("used", used)
        })
        shotsRevision++
    }

    suspend fun computeTruing(): TruingResult = api.get("computeTruing")

    suspend fun applyTruing(): String? = detached {
        try {
            api.call("applyTruing")
            shotsRevision++
            recompute()
            null
        } catch (e: ApiException) {
            e.message
        }
    }

    fun resetTruing() = act { api.call("resetTruing"); shotsRevision++; recompute() }

    /** Where this cartridge hits at the rifle's zero; returns the core's error or null. */
    suspend fun setZeroOffset(upCm: Double, rightCm: Double): String? = detached {
        try {
            api.call("setZeroOffset", buildJsonObject {
                put("upCm", upCm)
                put("rightCm", rightCm)
            })
            state = api.get("state")
            recompute()
            null
        } catch (e: ApiException) {
            e.message
        }
    }

    // ---- Files, settings, about -------------------------------------------

    /** .ammo / .drg / .reticle / bullet-list / rifle / cartridge files into the library. */
    suspend fun importFiles(files: List<NamedText>): ImportReport = detached {
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
        state = api.get("state")
        recompute()
        report
    }

    suspend fun info(): Info = api.get("info")

    val seedReport: SeedReport?
        get() = api.seedResult?.let { Api.json.decodeFromJsonElement(SeedReport.serializer(), it) }

    fun setAngleUnit(unit: String) = setSettings(buildJsonObject { put("angleUnit", unit) })
    fun setHoldMode(mode: String) = setSettings(buildJsonObject { put("holdMode", mode) })
    fun setLanguage(language: String) = setSettings(buildJsonObject { put("language", language) })

    suspend fun rangeTable(): RangeTable = api.get("rangeTable")

    suspend fun trajectory(maxRangeM: Double, points: Int): RangeTable =
        api.get("trajectoryCurve", buildJsonObject {
            put("maxRangeM", maxRangeM)
            put("points", points)
        })

    /** Curves of other rifle + cartridge pairs in the current conditions. */
    suspend fun compareCurves(maxRangeM: Double, points: Int, pairs: List<Pair<Long, Long>>): List<RangeTable> =
        api.get("compareCurves", buildJsonObject {
            put("maxRangeM", maxRangeM)
            put("points", points)
            put("pairs", kotlinx.serialization.json.buildJsonArray {
                pairs.forEach { (rifle, cartridge) ->
                    add(buildJsonObject {
                        put("rifleId", rifle)
                        put("cartridgeId", cartridge)
                    })
                }
            })
        })

    suspend fun pairOptions(): List<PairOption> = api.get("pairOptions")

    suspend fun stationPressure(qnhHpa: Double, altitudeM: Double): Double =
        api.call("stationPressure", buildJsonObject {
            put("qnhHpa", qnhHpa)
            put("altitudeM", altitudeM)
        }).jsonPrimitive.double
}
