package org.vetalguru.balcalc

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.setValue
import kotlin.math.roundToInt
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.encodeToJsonElement
import kotlinx.serialization.json.put
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.core.ApiException
import org.vetalguru.balcalc.core.BcCalc
import org.vetalguru.balcalc.core.DsfPointIn
import org.vetalguru.balcalc.core.DsfResult
import org.vetalguru.balcalc.core.Shot
import org.vetalguru.balcalc.core.TruingResult
import org.vetalguru.balcalc.core.WezResult
import org.vetalguru.balcalc.core.WezSettings

/**
 * Truing the current rifle + cartridge: the shot log, velocity and drag
 * scales, the DSF table, the zero shift, the BC calculator and the hit
 * chance with the rifle's precision.
 */
class TruingModel internal constructor(private val app: AppModel) {
    private val api get() = app.api

    /** Bumps when the shot log or the pair's truing changed. */
    var revision by mutableIntStateOf(0)
        private set

    suspend fun shots(): List<Shot> = api.get("shots")

    /** Angles in the current unit; returns the core's error or null. */
    suspend fun logShot(rangeM: Double, elevation: Double, windage: Double?, notes: String): String? = app.detached {
        try {
            api.call("logShot", buildJsonObject {
                put("rangeM", rangeM)
                put("elevation", elevation)
                put("hasWindage", windage != null)
                put("windage", windage ?: 0.0)
                put("notes", notes)
            })
            revision++
            null
        } catch (e: ApiException) {
            e.message
        }
    }

    fun deleteShot(id: Long) = app.act { api.call("deleteShot", idArgs(id)); revision++ }

    fun setShotUsed(id: Long, used: Boolean) = app.act {
        api.call("setShotUsed", buildJsonObject {
            put("id", id)
            put("used", used)
        })
        revision++
    }

    suspend fun computeTruing(): TruingResult = api.get("computeTruing")

    suspend fun applyTruing(): String? = app.detached {
        try {
            api.call("applyTruing")
            revision++
            app.recompute()
            null
        } catch (e: ApiException) {
            e.message
        }
    }

    fun resetTruing() = app.act { api.call("resetTruing"); revision++; app.recompute() }

    suspend fun computeDsf(): DsfResult = api.get("computeDsf")

    /** Applies the last fitted DSF table, or sets `points`; returns the core's error or null. */
    suspend fun applyDsf(points: List<DsfPointIn>? = null): String? = app.detached {
        try {
            if (points == null) {
                api.call("applyDsf")
            } else {
                api.call("setDsf", buildJsonObject {
                    put("points", kotlinx.serialization.json.buildJsonArray {
                        points.forEach { p -> add(buildJsonObject { put("mach", p.mach); put("factor", p.factor) }) }
                    })
                })
            }
            app.recompute()
            null
        } catch (e: ApiException) {
            e.message
        }
    }

    fun resetDsf() = app.act { api.call("resetDsf"); app.recompute() }

    /** Where this cartridge hits at the rifle's zero; returns the core's error or null. */
    suspend fun setZeroOffset(upCm: Double, rightCm: Double): String? = app.detached {
        try {
            api.call("setZeroOffset", buildJsonObject {
                put("upCm", upCm)
                put("rightCm", rightCm)
            })
            app.state = api.get("state")
            app.recompute()
            null
        } catch (e: ApiException) {
            e.message
        }
    }

    /** The rifle and shooter's precision for the hit chance, as a 5-shot group (MOA). */
    suspend fun setRiflePrecision(groupMoa: Double) = app.detached {
        val current = wez(null, 100.0, 100.0).settings
        wez(current.copy(groupMoa = (groupMoa * 100).roundToInt() / 100.0), 100.0, 100.0)
    }

    /** Hit probability over the range; `settings` (when given) are saved first. */
    suspend fun wez(settings: WezSettings?, toM: Double, stepM: Double): WezResult =
        api.get("wez", buildJsonObject {
            put("toM", toM)
            put("stepM", stepM)
            if (settings != null) put("settings", Api.json.encodeToJsonElement(WezSettings.serializer(), settings))
        })

    /** The BC from two chronograph readings, or from the elevation that hit (current unit). */
    suspend fun bcFromChronograph(table: String, vNear: Double, vFar: Double, distanceM: Double): BcCalc =
        api.get("bcCalculator", buildJsonObject {
            put("mode", "chronograph")
            put("table", table)
            put("vNearMps", vNear)
            put("vFarMps", vFar)
            put("distanceM", distanceM)
        })

    suspend fun bcFromHit(table: String, rangeM: Double, elevation: Double): BcCalc =
        api.get("bcCalculator", buildJsonObject {
            put("mode", "hit")
            put("table", table)
            put("rangeM", rangeM)
            put("elevation", elevation)
        })
}
