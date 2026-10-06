package org.vetalguru.balcalc

import androidx.compose.ui.graphics.toAwtImage
import androidx.compose.ui.test.ComposeUiTest
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertTextEquals
import androidx.compose.ui.test.captureToImage
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.semantics.getOrNull
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.onRoot
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performImeAction
import androidx.compose.ui.test.performTextReplacement
import androidx.compose.ui.test.runDesktopComposeUiTest
import java.io.File
import javax.imageio.ImageIO
import kotlin.test.Test
import kotlin.test.assertNotEquals
import kotlin.test.assertTrue
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.core.desktopEngine
import org.vetalguru.balcalc.core.desktopSeed

/** The app on the desktop JVM with the real C++ core, end to end. */
@OptIn(ExperimentalTestApi::class)
class FlowTest {
    private fun ComposeUiTest.exists(tag: String) =
        onAllNodes(hasTestTag(tag)).fetchSemanticsNodes().isNotEmpty()

    /** What a node shows: an input's content, or its text. */
    private fun ComposeUiTest.shown(tag: String): String {
        val config = onNodeWithTag(tag).fetchSemanticsNode().config
        return config.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.EditableText)?.text
            ?: config.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.Text).orEmpty()
                .joinToString("") { it.text }
    }

    private fun ComposeUiTest.shot(name: String) {
        val dir = System.getProperty("balcalc.screenshots") ?: return
        File(dir).mkdirs()
        ImageIO.write(onRoot().captureToImage().toAwtImage(), "png", File(dir, "$name.png"))
    }

    private fun run(width: Int, height: Int, name: String) = runDesktopComposeUiTest(width, height) {
        val db = File.createTempFile("balcalc-test", ".db").apply { delete() }
        val api = Api(desktopEngine())
        setContent { BalCalcApp(api, startup = { start(db.path) { desktopSeed() } }, platform = FakePlatform()) }

        // Seeded library, sample rifle + cartridge: the Qt app's 1.61 at 300 m.
        waitUntil(timeoutMillis = 30_000) { exists("sample") }
        onNodeWithTag("sample").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") }
        onNodeWithTag("elevation").assertTextEquals("1.61")
        shot("$name-solution")

        // Colder air: more elevation.
        onNodeWithTag("navConditions").performClick()
        waitUntil { exists("temperature") }
        onNodeWithTag("temperature").performTextReplacement("-10")
        onNodeWithTag("temperature").performImeAction()
        shot("$name-conditions")
        onNodeWithTag("navSolution").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") && shown("elevation") != "1.61" }
        assertNotEquals("1.61", shown("elevation"))

        // Range table and chart.
        onNodeWithTag("navTable").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rangeTable") }
        shot("$name-table")
        onNodeWithTag("chartTab").performClick()
        waitUntil { exists("chart") }
        shot("$name-chart")
        assertTrue(exists("chart"))
        db.delete()
    }

    private fun ComposeUiTest.type(tag: String, value: String) {
        onNodeWithTag(tag).performTextReplacement(value)
        onNodeWithTag(tag).performImeAction()
    }

    private fun ComposeUiTest.count(prefix: String) =
        onAllNodes(androidx.compose.ui.test.SemanticsMatcher("tag $prefix*") {
            it.config.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.TestTag)?.startsWith(prefix) == true
        }).fetchSemanticsNodes().size

    @Test
    fun armory() = runDesktopComposeUiTest(400, 820) {
        val db = File.createTempFile("balcalc-test", ".db").apply { delete() }
        val platform = FakePlatform()
        setContent { BalCalcApp(Api(desktopEngine()), startup = { start(db.path) { desktopSeed() } }, platform = platform) }
        waitUntil(timeoutMillis = 30_000) { exists("sample") }

        // A rifle through its editor.
        onNodeWithTag("navArmory").performClick()
        waitUntil { exists("new") }
        onNodeWithTag("new").performClick()
        waitUntil { exists("rifleName") }
        type("rifleName", "Tikka")
        type("rifleCaliber", ".308 Win")
        shot("armory-rifle-editor")
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifle:Tikka") }

        // A cartridge with its own bullet.
        onNodeWithTag("cartridgesTab").performClick()
        onNodeWithTag("new").performClick()
        onNodeWithTag("newEmptyCartridge").performClick()
        waitUntil { exists("cartridgeName") }
        type("cartridgeName", "Load")
        type("cartridgeCaliber", ".308 Win")
        type("bulletName", "SMK 175")
        type("bc", "0,243") // a comma works too
        type("mass", "175")
        type("diameter", "0.308")
        type("muzzleVelocity", "790")
        shot("armory-cartridge-editor")
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridge:Load") }
        shot("armory-lists")

        // The pair is current: a solution.
        onNodeWithTag("navSolution").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") }

        // Share through the clipboard and back: a second cartridge.
        onNodeWithTag("navArmory").performClick()
        waitUntil { exists("more:Load") }
        onNodeWithTag("more:Load").performClick()
        onNodeWithTag("copy").performClick()
        waitUntil { platform.clipboard?.contains("balcalc-cartridge") == true }
        onNodeWithTag("cartridgesTab").performClick()
        waitUntil { exists("cartridge:Load") }
        onAllNodesWithText("Import").onFirst().performClick()
        onNodeWithTag("importClipboard").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridge:Load (2)") }

        // Delete it again, after confirming.
        onNodeWithTag("more:Load (2)").performClick()
        onNodeWithTag("delete").performClick()
        onNodeWithTag("confirmDelete").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("cartridge:Load (2)") }

        // A factory cartridge copied into the user's list.
        onNodeWithTag("new").performClick()
        onNodeWithTag("newFactoryCartridge").performClick()
        waitUntil { exists("search") }
        onNodeWithTag("search").performTextReplacement("GP11")
        waitUntil(timeoutMillis = 10_000) { count("factory:") == 1 }
        shot("armory-factory")
        onAllNodes(androidx.compose.ui.test.SemanticsMatcher("factory") {
            it.config.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.TestTag)?.startsWith("factory:") == true
        }).onFirst().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridgeName") && shown("cartridgeName").contains("GP11") }
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { count("cartridge:") == 2 }
        db.delete()
    }

    @Test
    fun phone() = run(400, 820, "phone")

    @Test
    fun desktop() = run(1100, 760, "desktop")
}
