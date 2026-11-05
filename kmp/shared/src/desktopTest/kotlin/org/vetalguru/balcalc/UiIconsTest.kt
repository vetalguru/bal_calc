package org.vetalguru.balcalc

import androidx.compose.material3.Icon
import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.semantics.getOrNull
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.hasContentDescription
import androidx.compose.ui.test.hasAnyAncestor
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runDesktopComposeUiTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import org.jetbrains.compose.resources.ExperimentalResourceApi
import org.jetbrains.compose.resources.vectorResource
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.allDrawableResources

/** The icons: every file reads, and a button that is only an icon says what it does. */
@OptIn(ExperimentalTestApi::class, ExperimentalResourceApi::class)
class UiIconsTest {
    /** A broken vector file only fails when it is drawn: draw them all, at 24 dp. */
    @Test
    fun everyIconReads() = runDesktopComposeUiTest(400, 400) {
        val icons = Res.allDrawableResources
        assertTrue(icons.size >= 39, "${icons.size} icons")
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
}
