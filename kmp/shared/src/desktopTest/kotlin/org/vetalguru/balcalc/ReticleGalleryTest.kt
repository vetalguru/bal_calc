package org.vetalguru.balcalc

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toAwtImage
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.captureToImage
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.runDesktopComposeUiTest
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import java.io.File
import javax.imageio.ImageIO
import kotlin.test.Test
import kotlin.test.assertTrue
import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.core.Solution
import org.vetalguru.balcalc.core.desktopEngine
import org.vetalguru.balcalc.core.desktopSeed
import org.vetalguru.balcalc.ui.ReticleView

/**
 * Every bundled reticle, drawn with a 600 m hold of the sample rifle:
 * screenshots reticle-<name>.png to look at.
 */
@OptIn(ExperimentalTestApi::class)
class ReticleGalleryTest {
    @Test
    fun everyReticleDraws() = runDesktopComposeUiTest(500, 500) {
        val api = Api(desktopEngine())
        val reticles = runBlocking {
            api.call("open", buildJsonObject { put("path", ":memory:") })
            api.call("seed", buildJsonObject {
                put("version", 1)
                put("files", kotlinx.serialization.json.buildJsonArray {
                    for (f in desktopSeed()) add(buildJsonObject { put("name", f.name); put("content", f.content) })
                })
            })
            api.call("addSample", buildJsonObject { put("rifleName", "R"); put("cartridgeName", "C") })
            api.call("setConditions", buildJsonObject { put("targetRangeM", 600); put("windSpeed", 4) })
            api.call("setSettings", buildJsonObject { put("holdMode", "hold") })
            api.call("reticles").jsonArray.map { it.jsonObject }
        }
        assertTrue(reticles.size >= 14, "${reticles.size} reticles")

        var definition by mutableStateOf("")
        var target by mutableStateOf(0.0 to 0.0)
        setContent {
            ReticleView(definition, target.first, target.second, Modifier.fillMaxSize().background(Color.White).testTag("reticle"))
        }
        val dir = System.getProperty("balcalc.screenshots")?.let { File(it).apply { mkdirs() } }
        for (r in reticles) {
            val sol = runBlocking {
                val rifleId = api.call("state").jsonObject.getValue("currentRifleId")
                val form = api.call("rifleForm", buildJsonObject { put("id", rifleId) }).jsonObject.toMutableMap()
                form["reticleId"] = r.getValue("id")
                api.call("saveRifle", buildJsonObject { put("form", kotlinx.serialization.json.JsonObject(form)) })
                Api.json.decodeFromJsonElement(Solution.serializer(), api.call("solution"))
            }
            assertTrue(sol.ok && sol.hasReticle, r.getValue("name").jsonPrimitive.content)
            definition = sol.reticleDefinition
            target = sol.targetX to sol.targetY
            waitForIdle()
            if (dir != null) {
                val name = r.getValue("name").jsonPrimitive.content.replace(Regex("[^A-Za-z0-9.]+"), "-")
                ImageIO.write(onNodeWithTag("reticle").captureToImage().toAwtImage(), "png", File(dir, "reticle-$name.png"))
            }
        }
    }
}
