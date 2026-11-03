package org.vetalguru.balcalc

import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.semantics.SemanticsActions
import androidx.compose.ui.test.ComposeUiTest
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.SemanticsMatcher
import androidx.compose.ui.test.SemanticsNodeInteraction
import androidx.compose.ui.test.hasSetTextAction
import androidx.compose.ui.test.click
import androidx.compose.ui.test.isDisplayed
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.onLast
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performImeAction
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performSemanticsAction
import androidx.compose.ui.test.performTextReplacement
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.pinch
import androidx.compose.ui.test.runDesktopComposeUiTest
import androidx.compose.ui.unit.dp
import kotlin.math.cos
import kotlin.math.sin
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlinx.coroutines.awaitCancellation
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.double
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put

/*
 * The solution side of the app control by control: the pickers, the pages
 * of the viewer, the ring and its wheels, the reticle, targets and
 * situations, the moving target, the conditions page, the hit chance and
 * the settings. Each test checks what the control did: the core's state,
 * the solution shown, a dialog gone.
 */
@OptIn(ExperimentalTestApi::class)
class SolutionUiTest {
    private fun TestApp.state(): JsonObject = call("state").jsonObject
    private fun TestApp.conditions(): JsonObject = state().getValue("conditions").jsonObject
    private fun TestApp.condition(key: String): Double = conditions().getValue(key).jsonPrimitive.double
    private fun TestApp.text(key: String): String = state().getValue(key).jsonPrimitive.content

    private fun ComposeUiTest.elevation(): Double = shown("elevation").toDouble()

    private fun ComposeUiTest.setRange(m: Int) {
        type("range", m.toString())
        waitUntil(timeoutMillis = 10_000) { exists("solvedFor:$m") && shown("range") == m.toString() }
    }

    /** A number field without a tag, by its label. */
    private fun ComposeUiTest.field(label: String): SemanticsNodeInteraction =
        onNode(hasSetTextAction() and androidx.compose.ui.test.hasText(label))

    private fun ComposeUiTest.typeInto(label: String, value: String) {
        field(label).performScrollTo()
        field(label).performTextReplacement(value)
        field(label).performImeAction()
    }

    /** Opens a drop-down by what it shows now and picks [option]. */
    private fun ComposeUiTest.choose(current: String, option: String) {
        onNodeWithText(current).performScrollTo().performClick()
        waitUntil(timeoutMillis = 5_000) { onAllNodesWithText(option).fetchSemanticsNodes().isNotEmpty() }
        onAllNodesWithText(option).onLast().performClick()
    }

    private fun ComposeUiTest.displayed(tag: String): Boolean {
        announce()
        return onNodeWithTag(tag).isDisplayed()
    }

    /** A point on the ring's band at [deg] clockwise from 12 o'clock, in the ring's own coordinates. */
    private fun ringPoint(width: Int, height: Int, deg: Double): Offset {
        val r = minOf(width, height) / 2f * 0.8f
        val a = Math.toRadians(deg)
        return Offset(width / 2f + r * sin(a).toFloat(), height / 2f - r * cos(a).toFloat())
    }

    /** A rifle saved through the core with its form changed by [change]; the solution recomputed by a range. */
    private fun ComposeUiTest.changeRifle(app: TestApp, range: Int, change: (JsonObject) -> Map<String, JsonPrimitive>) {
        val id = app.state().getValue("currentRifleId")
        val form = app.call("rifleForm", buildJsonObject { put("id", id) }).jsonObject
        app.call("saveRifle", buildJsonObject { put("form", JsonObject(form + change(form))) })
        setRange(range)
    }

    /** The rifle's twist through its editor, saved. */
    private fun ComposeUiTest.editTwist(twist: String) {
        onNodeWithTag("navArmory").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifle:Sample .308 Win") }
        onAllNodesWithText("Edit").onFirst().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("twist") }
        type("twist", twist)
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifle:Sample .308 Win") && !exists("twist") }
        onNodeWithTag("navSolution").performClick()
    }

    // ---- The solution page ---------------------------------------------------

    /** The rifle picker: another rifle from its list becomes current, and its solution shows. */
    @Test
    fun rifleFromThePicker() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        val sampleId = app.state().getValue("currentRifleId").jsonPrimitive.content
        // A second rifle through the armory: current after saving.
        onNodeWithTag("navArmory").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("new") }
        onNodeWithTag("new").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifleName") }
        type("rifleName", "Second")
        type("rifleCaliber", ".308 Win")
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifle:Second") }
        onNodeWithTag("navSolution").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifleBox") && hasText("Second") && exists("elevation") }
        assertTrue(app.state().getValue("currentRifleId").jsonPrimitive.content != sampleId)

        // Back to the sample by the picker: its 1.61 at 300 m again.
        onNodeWithTag("rifleBox").performClick()
        waitUntil(timeoutMillis = 5_000) { hasText("Sample .308 Win") }
        onNodeWithText("Sample .308 Win").performClick()
        waitUntil(timeoutMillis = 10_000) { app.state().getValue("currentRifleId").jsonPrimitive.content == sampleId }
        waitUntil(timeoutMillis = 10_000) { shown("elevation") == "1.61" }
        assertTrue(!hasText("Second")) // the menu closed, the box shows the sample

        // The cartridge picker lists the one cartridge; choosing it again changes nothing.
        onNodeWithTag("cartridgeBox").performClick()
        waitUntil(timeoutMillis = 5_000) { onAllNodesWithText("Sample SMK 175 gr").fetchSemanticsNodes().size == 2 }
        onAllNodesWithText("Sample SMK 175 gr").onLast().performClick()
        waitUntil(timeoutMillis = 5_000) { onAllNodesWithText("Sample SMK 175 gr").fetchSemanticsNodes().size == 1 }
        assertEquals("1.61", shown("elevation"))
    }

    /** The viewer's tabs and the details line: each shows its page. */
    @Test
    fun viewerTabsAndDetailsLine() = runDesktopComposeUiTest(412, 915) {
        startApp()
        assertTrue(displayed("elevation") && !displayed("movingTarget"))
        // The line of velocity, time and energy leads to "More".
        onNodeWithTag("detailsLine").performClick()
        waitUntil(timeoutMillis = 10_000) { displayed("movingTarget") }
        assertTrue(!displayed("elevation"))
        onNodeWithTag("viewTargets").performClick()
        waitUntil(timeoutMillis = 10_000) { displayed("targetCard") }
        onNodeWithTag("viewReticle").performClick()
        waitUntil(timeoutMillis = 10_000) { displayed("reticle") }
        onNodeWithTag("viewCorrections").performClick()
        waitUntil(timeoutMillis = 10_000) { displayed("elevation") && !displayed("reticle") }
    }

    /** No rifle yet: "Add a rifle" opens the armory; the hit chance says what is missing. */
    @Test
    fun withoutARifle() = runDesktopComposeUiTest(412, 915) {
        startApp(sample = false)
        waitUntil(timeoutMillis = 10_000) { hasText("Add a rifle") }
        onNodeWithText("Add a rifle").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("new") } // the armory page
        onNodeWithTag("navTable").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("hitTab") }
        onNodeWithTag("hitTab").performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("Choose a rifle and a cartridge.") }
        assertTrue(!exists("hitChance"))
    }

    /** Low and too little stability warn on the solution screen. */
    @Test
    fun stabilityWarnings() = runDesktopComposeUiTest(412, 915) {
        startApp()
        assertTrue(!exists("warnings"))
        editTwist("14") // marginal
        waitUntil(timeoutMillis = 10_000) { exists("warning_lowStability") }
        assertTrue(shown("warning_lowStability").contains(Regex("""1\.[0-2]\d""")), shown("warning_lowStability"))
        editTwist("20") // unstable
        waitUntil(timeoutMillis = 10_000) { exists("warning_unstable") && !exists("warning_lowStability") }
    }

    /** A target removed elsewhere: a tap on its stale row tells the core's answer in a snackbar, which goes. */
    @Test
    fun staleTargetIsReported() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        onNodeWithTag("viewTargets").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("editTargets") }
        onNodeWithTag("editTargets").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("addTarget") }
        onNodeWithTag("addTarget").performClick()
        onNodeWithTag("saveTargets").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("target:Target 1") }

        // The list emptied behind the card's back (another screen, a sync).
        app.call("saveTargets", buildJsonObject { put("targets", JsonArray(emptyList())) })
        onNodeWithTag("target:Target 1").performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("No such target.") }
        waitUntil(timeoutMillis = 30_000) { !hasText("No such target.") } // shown, then cleared
    }

    // ---- The ring and its wheels ---------------------------------------------

    /** A drag round the ring's band: the wind follows to where it ends (9 o'clock). */
    @Test
    fun ringDragTurnsTheWind() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        type("windSpeed", "5")
        // Over the top, from 3 o'clock round to 9.
        val ring = onNodeWithTag("windDial").fetchSemanticsNode().size
        // A move a frame: moves sent at once can merge with the lift.
        onNodeWithTag("windDial").performTouchInput { down(ringPoint(ring.width, ring.height, 90.0)) }
        for (deg in listOf(75, 60, 30, 0, 330, 300, 285, 270)) {
            onNodeWithTag("windDial").performTouchInput { moveTo(ringPoint(ring.width, ring.height, deg.toDouble())) }
            waitForIdle()
        }
        onNodeWithTag("windDial").performTouchInput { up() }
        waitUntil(timeoutMillis = 10_000) { shown("windDirectionText").contains("9 o'clock") }
        assertEquals(270.0, app.condition("windFromDeg"))
        waitUntil(timeoutMillis = 10_000) { hasText("LEFT") } // from the left: drifts right, hold left
    }

    /** A wheel dragged up brings larger values, down smaller, on its step grid. */
    @Test
    fun wheelsFollowADrag() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        fun drag(tag: String, rows: Int) = onNodeWithTag(tag).performTouchInput {
            val row = 26.dp.toPx() * if (rows > 0) -1 else 1 // up: larger values
            down(center)
            repeat(kotlin.math.abs(rows) * 4) { moveBy(Offset(0f, row / 4)) }
            moveBy(Offset(0f, row)) // past the touch slop
            up()
        }
        drag("range", 3)
        waitUntil(timeoutMillis = 10_000) { app.condition("targetRangeM") > 300.0 }
        val far = app.condition("targetRangeM")
        assertEquals(0.0, far % 10, "$far")
        waitUntil(timeoutMillis = 10_000) { shown("range") == far.toInt().toString() }

        drag("range", -5)
        waitUntil(timeoutMillis = 10_000) { app.condition("targetRangeM") < far }
        assertEquals(0.0, app.condition("targetRangeM") % 10)

        drag("windSpeed", 2)
        waitUntil(timeoutMillis = 10_000) { app.condition("windSpeed") >= 1.0 }
        drag("lookAngle", -2)
        waitUntil(timeoutMillis = 10_000) { app.condition("lookAngleDeg") <= -1.0 && shown("lookAngle").startsWith("↓") }
    }

    /** The wheels' dialogs: the angle typed, OK sets a value, Cancel leaves it. */
    @Test
    fun wheelDialogs() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        type("lookAngle", "5")
        waitUntil(timeoutMillis = 10_000) { shown("lookAngle") == "↑5°" && app.condition("lookAngleDeg") == 5.0 }

        onNodeWithTag("range").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rangeInput") }
        onNodeWithTag("rangeInput").performTextReplacement("450")
        onNodeWithTag("wheelOk").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("rangeInput") && app.condition("targetRangeM") == 450.0 }
        assertEquals("450", shown("range"))
    }

    /** Cancel in a wheel's dialog throws the typed value away. */
    @Test
    fun wheelDialogCancelKeepsTheValue() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        onNodeWithTag("range").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rangeInput") }
        onNodeWithTag("rangeInput").performTextReplacement("900")
        onAllNodesWithText("Cancel").onLast().performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("rangeInput") }
        waitForIdle()
        assertEquals(300.0, app.condition("targetRangeM"))
        assertEquals("300", shown("range"))
    }

    // ---- The reticle ---------------------------------------------------------

    /** The hold mode: holding everything leaves the turrets alone, dialling everything turns both. */
    @Test
    fun holdMode() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        setRange(600)
        type("windSpeed", "5")
        val ring = onNodeWithTag("windDial").fetchSemanticsNode().size
        onNodeWithTag("windDial").performTouchInput { click(ringPoint(ring.width, ring.height, 90.0)) }
        onNodeWithTag("viewReticle").performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("Turrets:", substring = true) }
        assertTrue(hasText(", 0 clicks right", substring = true)) // wind held, not dialled

        choose("Dial elevation, hold wind", "Hold everything")
        waitUntil(timeoutMillis = 10_000) { app.text("holdMode") == "hold" && !hasText("Turrets:", substring = true) }
        choose("Hold everything", "Dial everything")
        waitUntil(timeoutMillis = 10_000) { app.text("holdMode") == "dial" && hasText("Turrets:", substring = true) }
        waitUntil(timeoutMillis = 10_000) { !hasText(", 0 clicks", substring = true) } // the wind dialled too
    }

    /** A tap on the reticle opens it on the whole screen; − and a pinch zoom it. */
    @Test
    fun reticleFullScreenZoom() = runDesktopComposeUiTest(412, 915) {
        startApp()
        onNodeWithTag("viewReticle").performClick()
        waitUntil(timeoutMillis = 10_000) { displayed("reticle") }
        onNodeWithTag("reticle").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("reticleScreen") }
        assertEquals("×1.0", shown("reticleZoom"))
        onNodeWithTag("reticleZoomOut").performClick()
        waitUntil(timeoutMillis = 5_000) { shown("reticleZoom") == "×0.7" }
        onNodeWithTag("reticleZoomOut").performClick()
        waitUntil(timeoutMillis = 5_000) { shown("reticleZoom") == "×0.5" } // no smaller
        onNodeWithTag("reticleZoomOut").performClick()
        waitForIdle()
        assertEquals("×0.5", shown("reticleZoom"))
        // Two fingers apart: larger.
        onNodeWithTag("reticleScreen").performTouchInput {
            pinch(center - Offset(20f, 0f), center - Offset(200f, 0f), center + Offset(20f, 0f), center + Offset(200f, 0f))
        }
        waitUntil(timeoutMillis = 5_000) { shown("reticleZoom").removePrefix("×").toDouble() > 1.0 }
        onNodeWithTag("reticleClose").performClick()
        waitUntil(timeoutMillis = 5_000) { !exists("reticleScreen") }
    }

    /** A second-focal-plane scope: the magnification slider reaches the core's conditions. */
    @Test
    fun sfpMagnification() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        changeRifle(app, 600) {
            mapOf(
                "focalPlane" to JsonPrimitive("sfp"),
                "minMagnification" to JsonPrimitive(5.0),
                "maxMagnification" to JsonPrimitive(25.0),
                "sfpReferenceMagnification" to JsonPrimitive(25.0),
            )
        }
        onNodeWithTag("viewReticle").performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("second focal plane", substring = true) }
        val slider = onNode(SemanticsMatcher.keyIsDefined(SemanticsActions.SetProgress))
        slider.performScrollTo()
        slider.performSemanticsAction(SemanticsActions.SetProgress) { it(10f) }
        waitUntil(timeoutMillis = 10_000) { app.condition("magnification") == 10.0 }
        waitUntil(timeoutMillis = 10_000) { hasText("Magnification 10.0×", substring = true) }
    }

    // ---- Targets and situations -------------------------------------------------

    /** The target list: every field of a row, a name required, rows removed, at most 20, Cancel. */
    @Test
    fun targetsDialog() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        onNodeWithTag("viewTargets").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("editTargets") }
        assertTrue(hasText("No targets yet", substring = true))

        // Cancel throws the new rows away.
        onNodeWithTag("editTargets").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("addTarget") }
        onNodeWithTag("addTarget").performClick()
        onAllNodesWithText("Cancel").onLast().performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("addTarget") }
        assertTrue(!exists("target:Target 1"))

        // At most 20: the button goes.
        onNodeWithTag("editTargets").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("addTarget") }
        repeat(20) { onNodeWithTag("addTarget").performScrollTo().performClick() }
        waitUntil(timeoutMillis = 10_000) { !exists("addTarget") }
        assertEquals(20, count("targetName"))
        // A row without a name: the core's sentence; removing rows clears it.
        onNodeWithTag("targetName1").performScrollTo().performTextReplacement("")
        onNodeWithTag("saveTargets").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("targetsError") }
        assertEquals("Enter a name for each target.", shown("targetsError"))
        while (count("targetRemove") > 1) onNodeWithTag("targetRemove1").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { count("targetName") == 1 }

        // The one left: far, uphill, wind from 3 o'clock.
        onNodeWithTag("targetName0").performTextReplacement("Far")
        type("targetRange0", "700")
        typeInto("Angle to target", "5")
        type("targetWind0", "4")
        choose("6 o'clock", "3 o'clock")
        onNodeWithTag("saveTargets").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("target:Far") && !exists("saveTargets") }
        assertTrue(hasText("700 m · 5° · 4 m/s 3 o'clock"))

        // A tap shoots it: range, angle and wind become the conditions.
        onNodeWithTag("target:Far").performClick()
        waitUntil(timeoutMillis = 10_000) { app.condition("targetRangeM") == 700.0 }
        assertEquals(5.0, app.condition("lookAngleDeg"))
        assertEquals(4.0, app.condition("windSpeed"))
        assertEquals(90.0, app.condition("windFromDeg"))
        waitUntil(timeoutMillis = 10_000) { exists("solvedFor:700") }
    }

    /** A situation whose cartridge is gone: Apply says so, the list marks it, Delete removes it. */
    @Test
    fun situationOfADeletedCartridge() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        onNodeWithTag("viewTargets").performClick()
        onNodeWithTag("situations").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("situationName") }
        assertTrue(hasText("No saved situations yet."))
        onNodeWithTag("situationName").performTextReplacement("Match")
        onNodeWithTag("saveSituation").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("applySituation:Match") }

        // The cartridge deleted meanwhile (in the armory).
        app.call("deleteCartridge", buildJsonObject { put("id", app.state().getValue("currentCartridgeId")) })
        onNodeWithTag("applySituation:Match").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("situationError") }
        assertEquals("The rifle or cartridge of this situation was deleted.", shown("situationError"))
        onAllNodesWithText("Close").onLast().performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("situationName") }

        // Opened again: marked, nothing to apply; deleted.
        onNodeWithTag("situations").performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("The rifle or cartridge was deleted") }
        assertTrue(!exists("applySituation:Match"))
        onNodeWithTag("deleteSituation:Match").performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("No saved situations yet.") }
        assertEquals(0, app.call("situations").jsonArray.size)
    }

    // ---- The moving target ---------------------------------------------------

    /** A target coming at an angle is met nearer than it is; the stopwatch's unit and Cancel. */
    @Test
    fun movingTargetMeetingPointAndStopwatch() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        setRange(600)
        onNodeWithTag("viewMore").performClick()
        onNodeWithTag("targetSpeed").performScrollTo()
        type("targetSpeed", "30")
        waitUntil(timeoutMillis = 10_000) { exists("lead") }
        assertTrue(!hasText("The bullet meets it at", substring = true)) // straight across: met where it is
        repeat(3) { onNodeWithTag("targetAngleMinus", useUnmergedTree = true).performScrollTo().performClick() } // 90 → 45°
        waitUntil(timeoutMillis = 10_000) { shown("targetAngle") == "45" }
        waitUntil(timeoutMillis = 10_000) { hasText("The bullet meets it at", substring = true) }
        assertEquals(45.0, app.condition("targetHeadingDeg"))

        // To the left, then back to the right.
        onNodeWithTag("movesLeft").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { app.condition("targetHeadingDeg") == 315.0 && shown("lead").contains("LEFT") }
        onNodeWithTag("movesRight").performClick()
        waitUntil(timeoutMillis = 10_000) { app.condition("targetHeadingDeg") == 45.0 && shown("lead").contains("RIGHT") }

        // The stopwatch in reticle units, then cancelled: nothing changes.
        onNodeWithTag("stopwatch").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("stopwatchDistance") }
        onAllNodesWithText("m").onLast().performClick() // the distance's unit, in the dialog
        onAllNodesWithText("MRAD").onLast().performClick()
        waitUntil(timeoutMillis = 5_000) { hasText("MRAD") }
        onAllNodesWithText("Cancel").onLast().performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("stopwatchToggle") }
        assertEquals(30 / 3.6, app.condition("targetSpeedMps"), 1e-6)
    }

    // ---- The conditions page -------------------------------------------------

    /** Station pressure typed; or QNH, turned into station pressure at the altitude, which follows it. */
    @Test
    fun pressureAndQnh() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        onNodeWithTag("navConditions").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("pressure") }
        type("pressure", "950")
        waitUntil(timeoutMillis = 10_000) { app.condition("pressureHpa") == 950.0 }
        type("altitude", "500")
        waitUntil(timeoutMillis = 10_000) { app.condition("altitudeM") == 500.0 }

        onNodeWithText("I enter sea-level pressure (QNH, weather report)").performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("Pressure QNH") }
        type("pressure", "1020")
        // 1020 hPa at sea level is about 960 hPa 500 m up.
        waitUntil(timeoutMillis = 10_000) { app.condition("pressureHpa") in 955.0..965.0 }
        waitUntil(timeoutMillis = 10_000) { hasText("Station pressure:", substring = true) }
        val at500 = app.condition("pressureHpa")
        // Higher up with the same QNH: less pressure.
        type("altitude", "1000")
        waitUntil(timeoutMillis = 10_000) { app.condition("altitudeM") == 1000.0 && app.condition("pressureHpa") < at500 - 40 }
        assertEquals("1020", shown("pressure")) // the field keeps showing the QNH
    }

    /** Humidity, powder temperature of its own, and the target height reach the core. */
    @Test
    fun humidityPowderAndTargetHeight() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        onNodeWithTag("navConditions").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("temperature") }
        typeInto("Humidity", "80")
        waitUntil(timeoutMillis = 10_000) { app.condition("humidityPct") == 80.0 }

        assertTrue(!hasText("Powder temperature"))
        onNodeWithText("Powder temperature = air temperature").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("Powder temperature") }
        assertEquals("false", app.conditions().getValue("powderFollowsAir").jsonPrimitive.content)
        typeInto("Powder temperature", "40")
        waitUntil(timeoutMillis = 10_000) { app.condition("powderC") == 40.0 }

        onNodeWithTag("targetHeight").performScrollTo()
        type("targetHeight", "50")
        waitUntil(timeoutMillis = 10_000) { app.condition("targetHeightCm") == 50.0 }
        // The point-blank range on "More" is for the new height.
        onNodeWithTag("navSolution").performClick()
        onNodeWithTag("viewMore").performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("Point-blank, 50 cm target") }
    }

    /** Wind zones: the "up to" of the first, a zone removed, all of them off. */
    @Test
    fun windZonesRemovedAndOff() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        onNodeWithTag("navConditions").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("windZonesOn") }
        onNodeWithTag("windZonesOn").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("zoneUntil0") }
        onNodeWithTag("zoneUntil0").performScrollTo()
        type("zoneUntil0", "150")
        waitUntil(timeoutMillis = 10_000) { app.condition("windUntilM") == 150.0 }
        onNodeWithTag("addZone").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("removeZone1") }
        assertEquals(2, app.conditions().getValue("windZones").jsonArray.size)

        onNodeWithTag("removeZone1").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("removeZone1") && app.conditions().getValue("windZones").jsonArray.size == 1 }
        assertTrue(exists("addZone")) // room for one more again

        onNodeWithTag("windZonesOn").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { app.conditions().getValue("windZones").jsonArray.isEmpty() }
        assertTrue(!exists("zoneUntil0") && !exists("removeZone0"))
        onNodeWithTag("navSolution").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("quickWind") }
        assertTrue(!hasText("Zone 1 of", substring = true))
    }

    /** Earth rotation: latitude and a known azimuth typed; the hit chance then asks for their errors. */
    @Test
    fun coriolisFieldsAndTheirErrors() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        onNodeWithTag("navConditions").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("temperature") }
        onNodeWithText("Account for Earth rotation").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("latitude") }
        onNodeWithTag("latitude").performScrollTo()
        type("latitude", "60")
        waitUntil(timeoutMillis = 10_000) { app.condition("latitudeDeg") == 60.0 }
        onNodeWithText("Known shot direction").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("azimuth") }
        onNodeWithTag("azimuth").performScrollTo()
        type("azimuth", "90")
        waitUntil(timeoutMillis = 10_000) { app.condition("azimuthDeg") == 90.0 }
        assertEquals("true", app.conditions().getValue("useAzimuth").jsonPrimitive.content)

        // Their errors in the hit chance (kept by the core).
        onNodeWithTag("navTable").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("hitTab") }
        onNodeWithTag("hitTab").performClick()
        waitUntil(timeoutMillis = 20_000) { exists("hitChance") }
        typeInto("Azimuth", "20")
        typeInto("Latitude", "2")
        fun stored() = app.call("wez", buildJsonObject { put("toM", 300.0); put("stepM", 50.0) }).jsonObject
            .getValue("settings").jsonObject
        waitUntil(timeoutMillis = 10_000) {
            stored().let { it.getValue("azimuthDeg").jsonPrimitive.double == 20.0 && it.getValue("latitudeDeg").jsonPrimitive.double == 2.0 }
        }
    }

    /** The hit chance's target: an ellipse and a narrower one are hit less often. */
    @Test
    fun hitChanceTarget() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        setRange(600)
        onNodeWithTag("navTable").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("hitTab") }
        onNodeWithTag("hitTab").performClick()
        waitUntil(timeoutMillis = 20_000) { exists("hitChance") }
        fun chance() = shown("hitChance").removeSuffix(" %").toInt()
        val square = chance()
        choose("Rectangle", "Ellipse or circle")
        waitUntil(timeoutMillis = 20_000) { exists("hitChance") && chance() < square }
        val ellipse = chance()
        onNodeWithTag("targetWidth").performScrollTo()
        type("targetWidth", "10")
        waitUntil(timeoutMillis = 20_000) { exists("hitChance") && chance() < ellipse }
        val stored = app.call("wez", buildJsonObject { put("toM", 600.0); put("stepM", 50.0) }).jsonObject.getValue("settings").jsonObject
        assertEquals("ellipse", stored.getValue("targetKind").jsonPrimitive.content)
        assertEquals(10.0, stored.getValue("targetWidthCm").jsonPrimitive.double)
    }

    /** Sensors that give nothing: no barometer button, a note for the location, "Reading…" in the dialogs. */
    @Test
    fun sensorsWithoutReadings() = runDesktopComposeUiTest(412, 915) {
        val app = startApp(platform = FakePlatform(sensors = SilentSensors(hasBarometer = false)))
        onNodeWithTag("navConditions").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("myLocation") }
        assertTrue(!exists("fromBarometer"))
        val altitude = app.condition("altitudeM")
        onNodeWithTag("myLocation").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("sensorNote") }
        assertTrue(shown("sensorNote").startsWith("No location"), shown("sensorNote"))
        assertEquals(altitude, app.condition("altitudeM"))

        // The tilt never comes: Use stays off, Cancel closes.
        onNodeWithTag("measureAngles").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("useAngles") }
        assertTrue(hasText("Reading…") && !exists("tiltNow"))
        onNodeWithTag("useAngles").performClick() // disabled: nothing
        waitForIdle()
        assertTrue(exists("useAngles"))
        onAllNodesWithText("Cancel").onLast().performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("useAngles") }

        // The compass too.
        onNodeWithText("Account for Earth rotation").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("fromCompass") }
        onNodeWithTag("fromCompass").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("useHeading") }
        assertTrue(hasText("Reading…") && !exists("headingNow"))
        onAllNodesWithText("Cancel").onLast().performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("useHeading") }
        assertEquals("false", app.conditions().getValue("useAzimuth").jsonPrimitive.content)
    }

    /** A barometer that gives no reading: a note, the pressure kept. */
    @Test
    fun barometerWithoutReading() = runDesktopComposeUiTest(412, 915) {
        val app = startApp(platform = FakePlatform(sensors = SilentSensors(hasBarometer = true)))
        onNodeWithTag("navConditions").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("fromBarometer") }
        onNodeWithTag("fromBarometer").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("sensorNote") }
        assertEquals("The barometer gave no reading.", shown("sensorNote"))
        assertEquals(1013.25, app.condition("pressureHpa"))
    }

    // ---- Settings ------------------------------------------------------------

    /** Signs for the corrections, MRAD back from MOA, the system theme: each reaches the core and the screen. */
    @Test
    fun settingsSignsUnitsAndTheme() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        setRange(600)
        val mrad = elevation()
        onNodeWithTag("navSettings").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("correctionStyle") }
        onNodeWithTag("correctionStyle").performClick()
        onAllNodesWithText("Signs: + −").onLast().performClick()
        onNodeWithTag("unit:moa").performClick()
        waitUntil(timeoutMillis = 10_000) { app.text("angleUnit") == "moa" }
        onNodeWithTag("unit:mrad").performClick()
        waitUntil(timeoutMillis = 10_000) { app.text("angleUnit") == "mrad" }
        onNodeWithTag("theme").performClick()
        onAllNodesWithText("Dark").onLast().performClick()
        waitUntil(timeoutMillis = 10_000) { app.state().getValue("prefs").jsonObject.getValue("theme").jsonPrimitive.content == "dark" }
        onNodeWithTag("theme").performClick()
        onAllNodesWithText("As the system").onLast().performClick()
        waitUntil(timeoutMillis = 10_000) {
            app.state().getValue("prefs").jsonObject.let { it.getValue("theme").jsonPrimitive.content == "system" && it.getValue("correctionStyle").jsonPrimitive.content == "signs" }
        }
        onNodeWithTag("navSolution").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevationDirection") && shown("elevationDirection") == "+" }
        assertEquals(mrad, elevation())
    }
}

/** Sensors that are there but give nothing: no fix, no pressure, and angles that never come. */
private class SilentSensors(override val hasBarometer: Boolean) : PhoneSensors {
    override val hasLocation = true
    override val hasCompass = true
    override val hasTilt = true
    override suspend fun pressureHpa(): Double? = null
    override suspend fun location(): GeoFix? = null
    override fun tilt(): Flow<Tilt> = flow { awaitCancellation() }
    override fun heading(at: GeoFix?): Flow<Heading> = flow { awaitCancellation() }
}
