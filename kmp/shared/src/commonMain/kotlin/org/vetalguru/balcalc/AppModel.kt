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
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.decodeFromJsonElement
import kotlinx.serialization.json.double
import kotlinx.serialization.json.encodeToJsonElement
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonArray
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.core.ApiException
import org.vetalguru.balcalc.core.AppState
import org.vetalguru.balcalc.core.Conditions
import org.vetalguru.balcalc.core.Info
import org.vetalguru.balcalc.core.PairOption
import org.vetalguru.balcalc.core.RangeTable
import org.vetalguru.balcalc.core.SeedReport
import org.vetalguru.balcalc.core.Solution
import org.vetalguru.balcalc.core.UiPrefs

/** A typed call: the result decoded as [T]. */
suspend inline fun <reified T> Api.get(method: String, args: JsonObject = JsonObject(emptyMap())): T =
    Api.json.decodeFromJsonElement(call(method, args))

internal fun idArgs(value: Long) = buildJsonObject { put("id", value) }

internal inline fun <reified T> formArgs(form: T) =
    buildJsonObject { put("form", Api.json.encodeToJsonElement(form)) }

/**
 * What the screens show and change: the core's state (selection, settings,
 * conditions) and the solution for it. Edits update the screen at once and
 * reach the core in order; the solution follows the latest edit only.
 *
 * The topics are models of their own on top of this one ([armory],
 * [library], [photos], [truing], [targets]): one state and one solution
 * queue for all of them, so they cannot race each other.
 */
class AppModel(val api: Api, private val scope: CoroutineScope) {
    var ready by mutableStateOf(false)
        private set
    var state by mutableStateOf(AppState())
        internal set
    var solution by mutableStateOf(Solution())
        private set
    /** Bumps whenever results may have changed (tables reload on it). */
    var revision by mutableIntStateOf(0)
        private set
    /** The last location taken with the sensors (true north for the compass); not kept. */
    var lastFix by mutableStateOf<GeoFix?>(null)

    /** The last failure of an action, for a snackbar; the screen clears it. */
    var message by mutableStateOf<String?>(null)

    val photos = PhotoModel(this)
    val armory = ArmoryModel(this)
    val library = LibraryModel(this)
    val truing = TruingModel(this)
    val targets = TargetsModel(this)

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

    internal fun recompute() {
        solutionRequests.trySend(Unit)
    }

    /**
     * Runs a change in the model's scope: it completes (and refreshes the
     * solution) even if the screen that asked is gone meanwhile, e.g. a tab
     * switched right after Save.
     */
    internal suspend fun <T> detached(block: suspend () -> T): T = scope.async { block() }.await()

    /**
     * Saves a form; the saved record becomes current. [then] gets the core's
     * answer before the state reloads. Returns the core's error (a
     * validation sentence) or null.
     */
    internal suspend fun saveForm(method: String, args: JsonObject, then: suspend (JsonElement) -> Unit = {}): String? =
        detached {
            try {
                then(api.call(method, args))
                state = api.get("state")
                recompute()
                null
            } catch (e: ApiException) {
                e.message
            }
        }

    // ---- Conditions, selection, settings -------------------------------------

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

    /** Changes interface preferences: shown at once, kept by the core. */
    fun setPrefs(change: (UiPrefs) -> UiPrefs) {
        val prefs = change(state.prefs)
        state = state.copy(prefs = prefs)
        act {
            api.call("setSettings", buildJsonObject { put("prefs", Api.json.encodeToJsonElement(UiPrefs.serializer(), prefs)) })
        }
    }

    fun setAngleUnit(unit: String) = setSettings(buildJsonObject { put("angleUnit", unit) })
    fun setHoldMode(mode: String) = setSettings(buildJsonObject { put("holdMode", mode) })
    fun setLanguage(language: String) = setSettings(buildJsonObject { put("language", language) })

    suspend fun info(): Info = api.get("info")

    val seedReport: SeedReport?
        get() = api.seedResult?.let { Api.json.decodeFromJsonElement(SeedReport.serializer(), it) }

    suspend fun stationPressure(qnhHpa: Double, altitudeM: Double): Double =
        api.call("stationPressure", buildJsonObject {
            put("qnhHpa", qnhHpa)
            put("altitudeM", altitudeM)
        }).jsonPrimitive.double

    // ---- Results beside the solution: tables and curves ---------------------

    /** The range card; with [windSpeeds] a windage column for each (computed in full). */
    suspend fun rangeTable(windSpeeds: List<Double> = emptyList()): RangeTable =
        api.get("rangeTable", buildJsonObject { putJsonArray("windSpeeds") { windSpeeds.forEach { add(JsonPrimitive(it)) } } })

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
}
