package org.vetalguru.balcalc.core

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.IO
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.addJsonObject
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonArray

/** One call into the C++ core (bridge::Api::Call): method and JSON in, JSON out. */
fun interface Engine {
    fun call(method: String, args: String): String
}

/** A failed call; [message] is the core's English sentence (a translation key). */
class ApiException(message: String) : Exception(message)

/** A file of the starter library (data/seed), passed to the core once. */
class SeedFile(val name: String, val content: String)

/**
 * The app's single way into the C++ core. Calls run one at a time off the
 * UI thread: the core keeps session state and is not thread-safe.
 */
class Api(private val engine: Engine) {
    private val mutex = Mutex()

    suspend fun call(method: String, args: JsonObject = JsonObject(emptyMap())): JsonElement =
        mutex.withLock {
            withContext(Dispatchers.IO) { parse(method, engine.call(method, args.toString())) }
        }

    private var started = false

    /**
     * Opens the database and imports the starter library on first run.
     * Once per core: later calls (a recreated activity) do nothing.
     */
    suspend fun start(databasePath: String, seed: () -> List<SeedFile>) {
        if (started) return
        started = true
        call("open", buildJsonObject { put("path", databasePath) })
        call("seed", buildJsonObject {
            put("version", SEED_VERSION)
            putJsonArray("files") {
                for (f in seed()) {
                    addJsonObject {
                        put("name", f.name)
                        put("content", f.content)
                    }
                }
            }
        })
    }

    companion object {
        /** Same as the Qt app: a database it seeded is not seeded again. */
        const val SEED_VERSION = 1

        val json = Json { ignoreUnknownKeys = true; encodeDefaults = true }

        fun parse(method: String, reply: String): JsonElement {
            val r = json.parseToJsonElement(reply).jsonObject
            if (!r.getValue("ok").jsonPrimitive.boolean) {
                throw ApiException(r["error"]?.jsonPrimitive?.content ?: "$method failed")
            }
            return r["result"] ?: JsonObject(emptyMap())
        }
    }
}

/** Starter-library files to send: the data formats, not docs or scripts. */
fun isSeedFile(name: String): Boolean =
    name.endsWith(".ammo") || name.endsWith(".drg") || name.endsWith(".reticle") ||
        name.endsWith(".json")

