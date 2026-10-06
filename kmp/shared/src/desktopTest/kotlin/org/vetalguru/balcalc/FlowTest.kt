package org.vetalguru.balcalc

import androidx.compose.ui.graphics.toAwtImage
import androidx.compose.ui.test.ComposeUiTest
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertTextEquals
import androidx.compose.ui.test.captureToImage
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.onNodeWithTag
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

    private fun ComposeUiTest.text(tag: String): String =
        onNodeWithTag(tag).fetchSemanticsNode().config
            .let { it[androidx.compose.ui.semantics.SemanticsProperties.Text] }
            .joinToString("") { it.text }

    private fun ComposeUiTest.shot(name: String) {
        val dir = System.getProperty("balcalc.screenshots") ?: return
        File(dir).mkdirs()
        ImageIO.write(onRoot().captureToImage().toAwtImage(), "png", File(dir, "$name.png"))
    }

    private fun run(width: Int, height: Int, name: String) = runDesktopComposeUiTest(width, height) {
        val db = File.createTempFile("balcalc-test", ".db").apply { delete() }
        val api = Api(desktopEngine())
        setContent { BalCalcApp(api, startup = { start(db.path) { desktopSeed() } }) }

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
        waitUntil(timeoutMillis = 10_000) { exists("elevation") && text("elevation") != "1.61" }
        assertNotEquals("1.61", text("elevation"))

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

    @Test
    fun phone() = run(400, 820, "phone")

    @Test
    fun desktop() = run(1100, 760, "desktop")
}
