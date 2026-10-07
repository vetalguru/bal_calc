package org.vetalguru.balcalc

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.setValue
import kotlin.io.encoding.Base64
import kotlin.io.encoding.ExperimentalEncodingApi
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put

/** Pictures of rifles and cartridges ([kind] "rifle" | "cartridge"). */
@OptIn(ExperimentalEncodingApi::class)
class PhotoModel internal constructor(private val app: AppModel) {
    /** Bumps when a picture changed (the lists reload theirs). */
    var revision by mutableIntStateOf(0)
        private set

    /** The pictures of all rifles or cartridges, by id. */
    suspend fun all(kind: String): Map<Long, ByteArray> {
        val all = app.api.call("photos", buildJsonObject { put("kind", kind) }).jsonObject
        return all.entries.associate { (id, data) -> id.toLong() to Base64.decode(data.jsonPrimitive.content) }
    }

    suspend fun one(kind: String, id: Long): ByteArray? {
        val data = app.api.call("photo", buildJsonObject { put("kind", kind); put("id", id) }).jsonPrimitive.content
        return if (data.isEmpty()) null else Base64.decode(data)
    }

    /** Stores [image] as the record's picture (null removes it). */
    internal suspend fun set(kind: String, id: Long, image: ByteArray?) {
        app.api.call("setPhoto", buildJsonObject {
            put("kind", kind)
            put("id", id)
            put("image", image?.let { Base64.encode(it) } ?: "")
        })
        revision++
    }
}
