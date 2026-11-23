package org.vetalguru.balcalc

import androidx.compose.ui.graphics.toAwtImage
import androidx.compose.ui.semantics.getOrNull
import androidx.compose.ui.test.ComposeUiTest
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.captureToImage
import androidx.compose.ui.test.click
import androidx.compose.ui.test.isRoot
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.onLast
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performTextReplacement
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.runDesktopComposeUiTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.double
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.long
import kotlinx.serialization.json.put
import org.vetalguru.balcalc.ui.CartridgeStep
import org.vetalguru.balcalc.ui.cartridgePartAt

class CartridgeDrawingTest {
    @Test
    fun eachPartOfTheDrawingIsAStep() {
        assertEquals(CartridgeStep.Cartridge, cartridgePartAt(0.04f, 0.50f)) // the base and primer
        assertEquals(CartridgeStep.Cartridge, cartridgePartAt(0.30f, 0.31f)) // the case wall
        assertEquals(CartridgeStep.Cartridge, cartridgePartAt(0.10f, 0.05f)) // its label
        assertEquals(CartridgeStep.Load, cartridgePartAt(0.30f, 0.50f)) // the powder inside
        assertEquals(CartridgeStep.Load, cartridgePartAt(0.20f, 0.85f)) // its label
        assertEquals(CartridgeStep.Bullet, cartridgePartAt(0.70f, 0.50f))
        assertEquals(CartridgeStep.Bullet, cartridgePartAt(0.69f, 0.17f)) // its label
        assertEquals(CartridgeStep.Velocity, cartridgePartAt(0.90f, 0.50f)) // the arrow
        assertEquals(CartridgeStep.Velocity, cartridgePartAt(0.90f, 0.65f)) // its label
    }
}

@OptIn(ExperimentalTestApi::class)
class CartridgeWizardUiTest {
    private fun TestApp.cartridges() = call("state").jsonObject.getValue("cartridges").jsonArray.map { it.jsonObject }
    private fun TestApp.cartridgeForm(id: Long): JsonObject = call("cartridgeForm", buildJsonObject { put("id", id) }).jsonObject
    private fun JsonObject.num(key: String) = getValue(key).jsonPrimitive.double
    private fun JsonObject.str(key: String) = getValue(key).jsonPrimitive.content

    private fun ComposeUiTest.shot(name: String) {
        val dir = System.getProperty("balcalc.screenshots") ?: return
        java.io.File(dir).mkdirs()
        javax.imageio.ImageIO.write(
            onAllNodes(isRoot()).onLast().captureToImage().toAwtImage(), "png", java.io.File(dir, "cartridge-$name.png"),
        )
    }

    private fun ComposeUiTest.newCustomCartridge() {
        onNodeWithTag("navArmory").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("new") }
        onNodeWithTag("cartridgesTab").performClick()
        onNodeWithTag("new").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("newCustomCartridge") }
        onNodeWithTag("newCustomCartridge").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridgeDiagram") }
    }

    private fun ComposeUiTest.next(expectTag: String) {
        onNodeWithTag("wizardNext").performClick()
        waitUntil(timeoutMillis = 10_000) { exists(expectTag) }
    }

    /** A custom cartridge part by part, with its handload, to the summary and saved. */
    @Test
    fun customCartridgeEveryPart() = runDesktopComposeUiTest(412, 915) {
        val app = startApp(sample = false)
        newCustomCartridge()
        assertTrue(shown("wizardStep").startsWith("Step 1 of 5"))
        type("cartridgeName", "308 N150 handload")
        type("cartridgeCaliber", ".308 Win")
        shot("1-cartridge")

        next("bulletName")
        type("bulletName", "SMK 175")
        type("bc", "0.243")
        type("mass", "175")
        type("diameter", "0.308")
        shot("2-bullet")

        next("powder")
        type("powder", "Vihtavuori N150")
        type("charge", "44.5")
        type("primer", "CCI BR2")
        type("brass", "Lapua")
        type("coal", "2.81")
        type("cbto", "2.235")
        shot("3-load")

        next("muzzleVelocity")
        type("muzzleVelocity", "790")
        type("velocitySd", "3.2")
        type("velocityEs", "11")
        shot("4-velocity")

        next("summaryChange:Load")
        assertTrue(shown("wizardStep").startsWith("Step 5 of 5"))
        assertTrue(hasText("Vihtavuori N150, 44.5 gr"))
        assertTrue(hasText("SD / ES"))
        assertTrue(onNodeWithTag("cartridgeDiagram").fetchSemanticsNode()
            .config[androidx.compose.ui.semantics.SemanticsProperties.ContentDescription].first().contains("790 m/s"))
        shot("5-summary")

        onNodeWithTag("wizardFinish").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridge:308 N150 handload") }
        val id = app.cartridges().first { it.str("name") == "308 N150 handload" }.getValue("id").jsonPrimitive.long
        val f = app.cartridgeForm(id)
        assertEquals("SMK 175", f.str("bulletName"))
        assertEquals(0.243, f.num("bc"), 1e-9)
        assertEquals(790.0, f.num("muzzleVelocity"), 1e-9)
        assertEquals("Vihtavuori N150", f.str("powder"))
        assertEquals(44.5, f.num("chargeGr"), 1e-9)
        assertEquals("CCI BR2", f.str("primer"))
        assertEquals("Lapua", f.str("brass"))
        assertEquals(2.81, f.num("coalIn"), 1e-9)
        assertEquals(2.235, f.num("cbtoIn"), 1e-9)
        assertEquals(3.2, f.num("velocitySd"), 1e-9)
        assertEquals(11.0, f.num("velocityEs"), 1e-9)
    }

    /** A tap on the drawing opens that part; a nameless cartridge is refused back on the first part. */
    @Test
    fun drawingOpensPartsAndTheNameIsRequired() = runDesktopComposeUiTest(412, 915) {
        startApp(sample = false)
        newCustomCartridge()
        val drawing = onNodeWithTag("cartridgeDiagram")
        drawing.performTouchInput { click(androidx.compose.ui.geometry.Offset(width * 0.30f, height * 0.50f)) }
        waitUntil(timeoutMillis = 10_000) { exists("powder") }
        drawing.performTouchInput { click(androidx.compose.ui.geometry.Offset(width * 0.70f, height * 0.50f)) }
        waitUntil(timeoutMillis = 10_000) { exists("bulletName") }
        drawing.performTouchInput { click(androidx.compose.ui.geometry.Offset(width * 0.90f, height * 0.50f)) }
        waitUntil(timeoutMillis = 10_000) { exists("muzzleVelocity") }
        type("muzzleVelocity", "800")
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("formError") }
        assertEquals("Enter a cartridge name.", shown("formError"))
        assertTrue(shown("wizardStep").startsWith("Step 1 of 5"))
    }

    /** A factory cartridge copied opens on the same drawing, filled in. */
    @Test
    fun factoryCopyOpensFilledIn() = runDesktopComposeUiTest(412, 915) {
        startApp(sample = false)
        onNodeWithTag("navArmory").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("new") }
        onNodeWithTag("cartridgesTab").performClick()
        onNodeWithTag("new").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("newFactoryCartridge") }
        onNodeWithTag("newFactoryCartridge").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("search") }
        onNodeWithTag("search").performTextReplacement("GP11")
        waitUntil(timeoutMillis = 10_000) { count("factory:") >= 1 }
        onAllNodes(androidx.compose.ui.test.SemanticsMatcher("factory") {
            it.config.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.TestTag)?.startsWith("factory:") == true
        }).onFirst().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridgeDiagram") && exists("cartridgeName") }
        assertTrue(shown("cartridgeName").contains("GP11"))
        onNodeWithTag("part:Velocity").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("muzzleVelocity") }
        assertTrue(shown("muzzleVelocity").toDouble() > 700)
    }
}
