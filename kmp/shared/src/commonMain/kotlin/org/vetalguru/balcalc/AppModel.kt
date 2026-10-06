package org.vetalguru.balcalc

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.decodeFromJsonElement
import kotlinx.serialization.json.double
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import org.vetalguru.balcalc.core.Api
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

    private var solutionJob: Job? = null

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
        solutionJob?.cancel()
        solutionJob = act {
            solution = api.get("solution")
            revision++
        }
    }

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

    suspend fun rangeTable(): RangeTable = api.get("rangeTable")

    suspend fun trajectory(maxRangeM: Double, points: Int): RangeTable =
        api.get("trajectoryCurve", buildJsonObject {
            put("maxRangeM", maxRangeM)
            put("points", points)
        })

    suspend fun stationPressure(qnhHpa: Double, altitudeM: Double): Double =
        api.call("stationPressure", buildJsonObject {
            put("qnhHpa", qnhHpa)
            put("altitudeM", altitudeM)
        }).jsonPrimitive.double
}
