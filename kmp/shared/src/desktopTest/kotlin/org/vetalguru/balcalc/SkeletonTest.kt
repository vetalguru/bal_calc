package org.vetalguru.balcalc

import androidx.compose.ui.graphics.toAwtImage
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertTextEquals
import androidx.compose.ui.test.captureToImage
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onRoot
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.v2.runComposeUiTest
import java.io.File
import javax.imageio.ImageIO
import kotlin.test.Test
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.core.desktopEngine
import org.vetalguru.balcalc.core.desktopSeed

/** The C++ core under Compose on the desktop JVM, end to end. */
@OptIn(ExperimentalTestApi::class)
class SkeletonTest {
    @Test
    fun sampleGivesTheKnownSolution() = runComposeUiTest {
        val db = File.createTempFile("balcalc-test", ".db").apply { delete() }
        val api = Api(desktopEngine())
        setContent { BalCalcApp(api, startup = { start(db.path) { desktopSeed() } }) }

        waitUntil(timeoutMillis = 30_000) { exists("sample") }
        onNodeWithTag("sample").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") }
        // The library SMK 175 (seeded), 300 m: the number the Qt app showed.
        onNodeWithTag("elevation").assertTextEquals("1.61")

        System.getProperty("balcalc.screenshots")?.let { dir ->
            File(dir).mkdirs()
            ImageIO.write(onRoot().captureToImage().toAwtImage(), "png", File(dir, "skeleton.png"))
        }
        db.delete()
    }

    private fun androidx.compose.ui.test.ComposeUiTest.exists(tag: String) =
        onAllNodes(androidx.compose.ui.test.hasTestTag(tag)).fetchSemanticsNodes().isNotEmpty()
}
