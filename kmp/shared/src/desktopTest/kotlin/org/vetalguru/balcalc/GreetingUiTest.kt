package org.vetalguru.balcalc

import androidx.compose.ui.graphics.toAwtImage
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.captureToImage
import androidx.compose.ui.test.isRoot
import androidx.compose.ui.test.onLast
import androidx.compose.ui.test.assertTextEquals
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runDesktopComposeUiTest
import java.io.File
import javax.imageio.ImageIO
import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.core.desktopEngine
import org.vetalguru.balcalc.core.desktopSeed

@OptIn(ExperimentalTestApi::class)
class GreetingUiTest {
    /** Every start says "Слава Україні!"; "Героям слава!" is the button that closes it. */
    @Test
    fun greetingAnsweredByItsButton() = runDesktopComposeUiTest(412, 915) {
        val db = File.createTempFile("balcalc-test", ".db").apply { delete(); deleteOnExit() }
        setContent { BalCalcApp(Api(desktopEngine()), startup = { start(db.path) { desktopSeed() } }, platform = FakePlatform()) }
        waitUntil(timeoutMillis = 30_000) { exists("greeting") }
        onNodeWithTag("greeting").assertTextEquals("Слава Україні!")
        System.getProperty("balcalc.screenshots")?.let { dir ->
            File(dir).mkdirs()
            // The topmost layer is the dialog.
            ImageIO.write(onAllNodes(isRoot()).onLast().captureToImage().toAwtImage(), "png", File(dir, "greeting.png"))
        }
        onNodeWithText("Героям слава!").performClick()
        waitForIdle()
        assertFalse(exists("greeting"))
        assertFalse(exists("greetingAnswer"))
        onNodeWithTag("navArmory").performClick() // the app behind it works
        waitForIdle()
    }

    /** The cross under the flag closes the app. */
    @Test
    fun greetingCrossQuits() = runDesktopComposeUiTest(412, 915) {
        val db = File.createTempFile("balcalc-test", ".db").apply { delete(); deleteOnExit() }
        var quit = false
        setContent {
            BalCalcApp(Api(desktopEngine()), startup = { start(db.path) { desktopSeed() } }, platform = FakePlatform(), onQuit = { quit = true })
        }
        waitUntil(timeoutMillis = 30_000) { exists("greeting") }
        assertTrue(exists("greetingFlag"))
        onNodeWithContentDescription("Close the app").performClick()
        waitForIdle()
        assertTrue(quit)
    }
}
