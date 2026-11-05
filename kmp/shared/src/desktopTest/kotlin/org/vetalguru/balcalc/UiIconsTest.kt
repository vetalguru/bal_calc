package org.vetalguru.balcalc

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.toAwtImage
import androidx.compose.ui.test.captureToImage
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.onLast
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.onRoot
import androidx.compose.ui.test.performImeAction
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performTextReplacement
import androidx.compose.ui.unit.dp
import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.semantics.getOrNull
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.hasContentDescription
import androidx.compose.ui.test.hasSetTextAction
import androidx.compose.ui.test.hasAnyAncestor
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runDesktopComposeUiTest
import java.io.File
import javax.imageio.ImageIO
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import org.jetbrains.compose.resources.ExperimentalResourceApi
import org.jetbrains.compose.resources.vectorResource
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.allDrawableResources
import org.vetalguru.balcalc.res.ic_bullet
import org.vetalguru.balcalc.res.ic_cartridge
import org.vetalguru.balcalc.res.ic_rifle
import org.vetalguru.balcalc.res.ic_scope
import org.vetalguru.balcalc.ui.AzimuthPicture
import org.vetalguru.balcalc.ui.CantPicture
import org.vetalguru.balcalc.ui.GroupMarkingPicture
import org.vetalguru.balcalc.ui.HeadingPicture
import org.vetalguru.balcalc.ui.LatitudePicture
import org.vetalguru.balcalc.ui.LookAnglePicture
import org.vetalguru.balcalc.ui.WindClockPicture
import org.vetalguru.balcalc.ui.ZeroOffsetPicture

/** The icons: every file reads, a button that is only an icon says what it does; the pictures and empty pages. */
@OptIn(ExperimentalTestApi::class, ExperimentalResourceApi::class)
class UiIconsTest {
    /** A broken vector file only fails when it is drawn: draw them all, at 24 dp. */
    @Test
    fun everyIconReads() = runDesktopComposeUiTest(400, 400) {
        val icons = Res.allDrawableResources
        assertTrue(icons.size >= 47, "${icons.size} icons")
        setContent { icons.values.forEach { Icon(vectorResource(it), contentDescription = null) } }
        waitForIdle()
    }

    /** Icon-only buttons are named for screen readers: a list row's menu, a field's arrows, its hint. */
    @Test
    fun iconButtonsSayWhatTheyDo() = runDesktopComposeUiTest(412, 915) {
        startApp()
        onNodeWithTag("navArmory").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("new") }
        waitUntil(timeoutMillis = 10_000) { count("more:") > 0 }
        val menu = onAllNodes(androidx.compose.ui.test.SemanticsMatcher("more") {
            it.config.getOrNull(SemanticsProperties.TestTag)?.startsWith("more:") == true
        }).fetchSemanticsNodes().first().config
        assertEquals(listOf("More actions"), menu.getOrNull(SemanticsProperties.ContentDescription))

        onNodeWithTag("navConditions").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("conditionsWindSpeed") }
        onNode(hasContentDescription("More information") and hasAnyAncestor(hasTestTag("cantAngleInfo")), useUnmergedTree = true)
            .assertExists()
    }

    private fun androidx.compose.ui.test.ComposeUiTest.sheet(name: String) {
        val dir = System.getProperty("balcalc.screenshots") ?: return
        ImageIO.write(onRoot().captureToImage().toAwtImage(), "png", File(dir, "$name.png"))
    }

    /** Every picture on one sheet, on both themes' surfaces: for the eye, and that they draw at all. */
    @Test
    fun picturesDraw() {
        for ((name, dark) in listOf("light" to false, "dark" to true)) runDesktopComposeUiTest(420, 900) {
            setContent {
                MaterialTheme(if (dark) darkColorScheme() else lightColorScheme()) {
                    Surface(color = MaterialTheme.colorScheme.inverseSurface, contentColor = MaterialTheme.colorScheme.inverseOnSurface) {
                        Column(Modifier.fillMaxSize().padding(8.dp)) {
                            Row { LookAnglePicture(); WindClockPicture() }
                            Row { LatitudePicture(); AzimuthPicture() }
                            Row { ZeroOffsetPicture(); GroupMarkingPicture() }
                            Row { CantPicture(); HeadingPicture() }
                            Row {
                                for (i in listOf(Res.drawable.ic_rifle, Res.drawable.ic_scope, Res.drawable.ic_cartridge, Res.drawable.ic_bullet)) {
                                    Icon(vectorResource(i), null, Modifier.size(64.dp))
                                }
                            }
                        }
                    }
                }
            }
            for (tag in listOf("lookAnglePicture", "windClockPicture", "latitudePicture", "azimuthPicture", "zeroOffsetPicture", "groupMarkingPicture")) {
                onNodeWithTag(tag).assertExists()
            }
            sheet("pictures-$name")
        }
    }

    /** A hint's picture shows with its sentence, when its mark is tapped. */
    @Test
    fun hintsShowTheirPictures() = runDesktopComposeUiTest(412, 915) {
        startApp()
        onNodeWithTag("navConditions").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("conditionsWindSpeed") }
        fun opens(info: String, picture: String) {
            onNodeWithTag(info).performScrollTo().performClick()
            waitUntil(timeoutMillis = 5_000) { exists(picture) }
            onNodeWithTag("navConditions").performClick() // a tap elsewhere closes it
            waitUntil(timeoutMillis = 5_000) { !exists(picture) }
        }
        opens("windFromInfo", "windClockPicture")
        opens("lookAngleInfo", "lookAnglePicture")
        onNodeWithText("Account for Earth rotation").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("latitudeInfo") }
        opens("latitudeInfo", "latitudePicture")
        onNodeWithText("Known shot direction").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("azimuthInfo") }
        opens("azimuthInfo", "azimuthPicture")
    }

    /**
     * The narrow fields with their longest values, on a phone and on the smallest: the values
     * reach the fields, and the sheets show whether they fit (a clipped field still holds its text).
     */
    @Test
    fun longValuesFit() {
        for ((w, h) in listOf(412 to 915, 320 to 700)) runDesktopComposeUiTest(w, h) {
            startApp()
            onNodeWithTag("navConditions").performClick()
            waitUntil(timeoutMillis = 10_000) { exists("conditionsWindSpeed") }
            onNodeWithTag("windFrom").performScrollTo()
            type("windFrom", "345")
            onNodeWithTag("altitude").performScrollTo()
            type("altitude", "2500")
            val humidity = onNode(hasSetTextAction() and androidx.compose.ui.test.hasText("Humidity"))
            humidity.performTextReplacement("100")
            humidity.performImeAction()
            type("temperature", "-10.5")
            waitUntil(timeoutMillis = 5_000) { shown("altitude") == "2500" }
            sheet("long-values-$w")
        }
    }

    /** With nothing there yet, the pages show what would be there. */
    @Test
    fun emptyPagesShowAnIcon() = runDesktopComposeUiTest(412, 915) {
        startApp(sample = false)
        waitUntil(timeoutMillis = 30_000) { exists("sample") }
        assertTrue(exists("emptyState")) // the solution: no rifle yet
        sheet("empty-solution")
        onNodeWithTag("navArmory").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("riflesTab") }
        assertTrue(hasText("No rifles yet", substring = true) && exists("emptyState"))
        sheet("empty-armory")
        onNodeWithTag("cartridgesTab").performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("New", substring = true) && exists("emptyState") }
    }

    /** The shot log with no shots; the point of impact's picture; the group's picture until there is a photo. */
    @Test
    fun shotLogAndGroupPictures() = runDesktopComposeUiTest(412, 915) {
        startApp(platform = FakePlatform().apply { image = java.io.ByteArrayOutputStream().also { out ->
            ImageIO.write(java.awt.image.BufferedImage(40, 40, java.awt.image.BufferedImage.TYPE_INT_RGB), "png", out)
        }.toByteArray() })
        onNodeWithTag("navArmory").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridgesTab") }
        onNodeWithTag("cartridgesTab").performClick()
        waitUntil(timeoutMillis = 10_000) { count("more:") > 0 }
        onAllNodes(androidx.compose.ui.test.SemanticsMatcher("more") {
            it.config.getOrNull(SemanticsProperties.TestTag)?.startsWith("more:") == true
        }).onFirst().performClick()
        waitUntil(timeoutMillis = 5_000) { exists("shotLog") }
        onNodeWithTag("shotLog").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("logHit") }
        assertTrue(exists("emptyState"))
        onNodeWithTag("offsetUpInfo").performScrollTo().performClick()
        waitUntil(timeoutMillis = 5_000) { exists("zeroOffsetPicture") }
        onNodeWithTag("logHit").performClick() // closes the hint, then its dialog
        waitUntil(timeoutMillis = 5_000) { !exists("zeroOffsetPicture") }
        if (hasText("Cancel")) onAllNodesWithText("Cancel").onLast().performClick()
        waitForIdle()
        onNodeWithTag("openGroup").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("groupMarkingPicture") }
        onNodeWithTag("groupPhoto").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("groupCanvas") && !exists("groupMarkingPicture") }
    }
}
