package org.vetalguru.balcalc

import kotlinx.serialization.json.addJsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonArray
import org.vetalguru.balcalc.core.ApiException
import org.vetalguru.balcalc.core.SituationItem
import org.vetalguru.balcalc.core.TargetItem

/** What is shot at: the target card and the saved situations. */
class TargetsModel internal constructor(private val app: AppModel) {
    private val api get() = app.api
    private fun nameArgs(name: String) = buildJsonObject { put("name", name) }

    suspend fun list(): List<TargetItem> = api.get("targets")

    /** Replaces the target list; returns the core's error or null. */
    suspend fun save(list: List<TargetItem>): String? = app.detached {
        try {
            api.call("saveTargets", buildJsonObject {
                putJsonArray("targets") {
                    list.forEach { t ->
                        addJsonObject {
                            put("name", t.name)
                            put("rangeM", t.rangeM)
                            put("lookAngleDeg", t.lookAngleDeg)
                            put("windSpeed", t.windSpeed)
                            put("windFromDeg", t.windFromDeg)
                        }
                    }
                }
            })
            app.recompute() // the card reloads on the revision
            null
        } catch (e: ApiException) {
            e.message
        }
    }

    /** Makes a target current: its range, angle and wind. */
    fun select(index: Int) = app.act {
        app.state = api.get("selectTarget", buildJsonObject { put("index", index) })
        app.recompute()
    }

    suspend fun situations(): List<SituationItem> = api.get("situations")

    /** Saves the current rifle, cartridge and conditions; returns the list, or the core's error. */
    suspend fun saveSituation(name: String): Result<List<SituationItem>> = app.detached {
        try {
            Result.success(api.get<List<SituationItem>>("saveSituation", nameArgs(name)))
        } catch (e: ApiException) {
            Result.failure(e)
        }
    }

    /** Makes a situation current; returns the core's error or null. */
    suspend fun applySituation(name: String): String? = app.detached {
        try {
            app.state = api.get("applySituation", nameArgs(name))
            app.recompute()
            null
        } catch (e: ApiException) {
            e.message
        }
    }

    suspend fun deleteSituation(name: String): List<SituationItem> =
        app.detached { api.get("deleteSituation", nameArgs(name)) }
}
