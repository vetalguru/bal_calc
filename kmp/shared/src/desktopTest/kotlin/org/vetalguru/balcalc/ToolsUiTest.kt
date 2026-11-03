package org.vetalguru.balcalc

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Text
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableDoubleStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.semantics.getOrNull
import androidx.compose.ui.test.ComposeUiTest
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.SemanticsMatcher
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.click
import androidx.compose.ui.test.hasAnyAncestor
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.hasText as withText
import androidx.compose.ui.test.isToggleable
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.onLast
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performScrollToNode
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.runDesktopComposeUiTest
import androidx.compose.ui.unit.dp
import java.awt.Color
import java.awt.image.BufferedImage
import java.io.ByteArrayOutputStream
import javax.imageio.ImageIO
import kotlin.math.abs
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.double
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.long
import kotlinx.serialization.json.put
import org.vetalguru.balcalc.ui.ChoiceField
import org.vetalguru.balcalc.ui.NumberField
import org.vetalguru.balcalc.ui.StepperField

/**
 * The tools around the solution, control by control: the shot log and its
 * truing, the DSF table, the BC calculator, the group from a photo, the
 * range card and the chart, and the shared input widgets.
 */
@OptIn(ExperimentalTestApi::class)
class ToolsUiTest {
    // ---- Reading the core ---------------------------------------------------

    private fun TestApp.obj(method: String, args: JsonObject = JsonObject(emptyMap())) = call(method, args).jsonObject
    private fun JsonObject.num(key: String) = getValue(key).jsonPrimitive.double
    private fun TestApp.shots(): List<JsonObject> = call("shots").jsonArray.map { it.jsonObject }
    private fun TestApp.pair() = obj("state").getValue("currentPair").jsonObject

    /** A hit logged straight into the core (angles in MRAD), as the dialog would. */
    private fun TestApp.logShot(rangeM: Double, elevation: Double, notes: String = "") = call("logShot", buildJsonObject {
        put("rangeM", rangeM)
        put("elevation", elevation)
        put("hasWindage", false)
        put("windage", 0.0)
        put("notes", notes)
    })

    /** What the core predicts at [rangeM] in the current conditions: a hit logged and taken back. */
    private fun TestApp.predictedAt(rangeM: Double): Double {
        val id = logShot(rangeM, 0.0).jsonObject.getValue("id").jsonPrimitive.long
        val p = shots().first { it.getValue("id").jsonPrimitive.long == id }.num("predicted")
        call("deleteShot", buildJsonObject { put("id", id) })
        return p
    }

    // ---- Getting there --------------------------------------------------------

    private fun tagStartsWith(prefix: String) = SemanticsMatcher("tag $prefix*") {
        it.config.getOrNull(SemanticsProperties.TestTag)?.startsWith(prefix) == true
    }

    /** Rifles tab → cartridges → ⋮ of the first → Shot log. */
    private fun ComposeUiTest.openShotLog() {
        onNodeWithTag("navArmory").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridgesTab") }
        onNodeWithTag("cartridgesTab").performClick()
        waitUntil(timeoutMillis = 10_000) { count("more:") > 0 }
        onAllNodes(tagStartsWith("more:")).onFirst().performClick()
        onNodeWithTag("shotLog").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("logHit") }
    }

    /** The checkbox and the ✕ of the shot rows, in the order shown. */
    private fun ComposeUiTest.shotChecks() = onAllNodes(isToggleable() and hasAnyAncestor(hasTestTag("shotRow")))
    private fun ComposeUiTest.shotDeletes() = onAllNodes(withText("✕") and hasAnyAncestor(hasTestTag("shotRow")))

    /** A blank 400 x 400 picture, the photo of the target. */
    private fun blankPng(): ByteArray = ByteArrayOutputStream().also { out ->
        val img = BufferedImage(400, 400, BufferedImage.TYPE_INT_RGB)
        val g = img.createGraphics()
        g.color = Color.WHITE
        g.fillRect(0, 0, 400, 400)
        g.dispose()
        ImageIO.write(img, "png", out)
    }.toByteArray()

    /** Image pixels -> a tap on the group canvas (the photo fills its width). */
    private fun ComposeUiTest.tapPhoto(x: Double, y: Double) = onNodeWithTag("groupCanvas").performTouchInput {
        click(Offset((x / 400 * width).toFloat(), (y / 400 * width).toFloat()))
    }

    // ---- Shot log ---------------------------------------------------------------

    /** Log a hit from the shot log itself: range, elevation, the windage switch and its field, notes; Cancel logs nothing. */
    @Test
    fun logAHitFromTheShotLog() = runDesktopComposeUiTest(1100, 1000) {
        val app = startApp()
        openShotLog()
        assertTrue(hasText("No shots logged", substring = true))

        onNodeWithTag("logHit").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("hitRange") }
        assertEquals("300", shown("hitRange")) // the target range of the solution
        type("hitRange", "400")
        type("hitElevation", "2.5")
        assertFalse(exists("hitWindage")) // only with the switch on
        onNodeWithText("Also log the windage that hit").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("hitWindage") }
        type("hitWindage", "0.3")
        type("hitNotes", "from the log")
        onNodeWithTag("saveHit").performClick()
        waitUntil(timeoutMillis = 10_000) { count("shotRow") == 1 }
        assertTrue(hasText("400 m: hit at 2.50 MRAD"))

        val s = app.shots().single()
        assertEquals(400.0, s.num("rangeM"), 1e-9)
        assertEquals(2.5, s.num("observed"), 1e-6)
        assertTrue(s.getValue("hasWindage").jsonPrimitive.boolean)
        assertEquals(0.3, s.num("observedWindage"), 1e-6)
        assertEquals("from the log", s.getValue("notes").jsonPrimitive.content)

        // Opened and cancelled: still one shot.
        onNodeWithTag("logHit").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("hitRange") }
        onNodeWithText("Cancel").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("hitRange") }
        assertEquals(1, app.shots().size)

        // Back: the lists again.
        onNodeWithText("Back").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("logHit") && count("more:") > 0 }
    }

    /** A shot's checkbox leaves it out of the truing (none left: the core says so); its ✕ deletes it. */
    @Test
    fun shotsLeftOutAndDeleted() = runDesktopComposeUiTest(1100, 1000) {
        val app = startApp()
        app.logShot(300.0, 1.70, "one")
        app.logShot(500.0, 3.80, "two")
        openShotLog()
        waitUntil(timeoutMillis = 10_000) { count("shotRow") == 2 }

        shotChecks().onFirst().performClick()
        waitUntil(timeoutMillis = 10_000) { app.shots().count { !it.getValue("used").jsonPrimitive.boolean } == 1 }
        shotChecks()[1].performClick()
        waitUntil(timeoutMillis = 10_000) { app.shots().none { it.getValue("used").jsonPrimitive.boolean } }

        // Nothing to true with: the result card is the core's sentence, no Apply.
        onNodeWithTag("computeTruing").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("Log at least one hit", substring = true) }
        assertFalse(exists("applyTruing"))

        // One back in, the other deleted.
        shotChecks().onFirst().performClick()
        waitUntil(timeoutMillis = 10_000) { app.shots().count { it.getValue("used").jsonPrimitive.boolean } == 1 }
        val kept = app.shots().first { it.getValue("used").jsonPrimitive.boolean }.getValue("id")
        shotDeletes()[1].performClick()
        waitUntil(timeoutMillis = 10_000) { count("shotRow") == 1 }
        assertEquals(listOf(kept), app.shots().map { it.getValue("id") })
    }

    /** Truing applied shows how the rifle is trued; Reset takes it back to the plain velocity and drag. */
    @Test
    fun truingAppliedAndReset() = runDesktopComposeUiTest(1100, 1000) {
        val app = startApp()
        app.logShot(300.0, 1.75) // more than the 1.61 predicted: a slower bullet
        openShotLog()
        waitUntil(timeoutMillis = 10_000) { count("shotRow") == 1 }
        onNodeWithTag("resetTruing").assertIsNotEnabled() // nothing trued yet

        onNodeWithTag("computeTruing").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("applyTruing") }
        onNodeWithTag("applyTruing").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("Trued: velocity", substring = true) }
        assertTrue(app.obj("solution").num("velocityScale") < 1.0)

        onNodeWithTag("resetTruing").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { !hasText("Trued: velocity", substring = true) }
        assertEquals(1.0, app.obj("solution").num("velocityScale"), 1e-12)
        assertEquals(1.0, app.obj("solution").num("dragScale"), 1e-12)
        onNodeWithTag("resetTruing").assertIsNotEnabled()
    }

    /** A truing result already applied elsewhere: Apply shows the core's refusal instead of failing silently. */
    @Test
    fun staleTruingResultIsRefused() = runDesktopComposeUiTest(1100, 1000) {
        val app = startApp()
        app.logShot(300.0, 1.75)
        openShotLog()
        waitUntil(timeoutMillis = 10_000) { count("shotRow") == 1 }
        onNodeWithTag("computeTruing").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("applyTruing") }
        app.call("applyTruing") // the core's result is used up
        onNodeWithTag("applyTruing").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("applyTruing") && hasText("Nothing to apply.") }
    }

    /** The point-of-impact shift to the right reaches the core; the one up keeps its value. */
    @Test
    fun zeroShiftToTheRight() = runDesktopComposeUiTest(1100, 1000) {
        val app = startApp()
        openShotLog()
        type("offsetUp", "1.5")
        waitUntil(timeoutMillis = 10_000) { app.pair().num("offsetUpCm") == 1.5 }
        type("offsetRight", "-2")
        waitUntil(timeoutMillis = 10_000) { app.pair().num("offsetRightCm") == -2.0 }
        assertEquals(1.5, app.pair().num("offsetUpCm"))
        // Beyond the field's range: clamped to 100 cm, which the core takes.
        type("offsetRight", "250")
        waitUntil(timeoutMillis = 10_000) { app.pair().num("offsetRightCm") == 100.0 }
        assertEquals("100", shown("offsetRight"))
    }

    /** No rifle: the shot log asks for a pair and offers nothing to log, shift, fit or calculate. */
    @Test
    fun shotLogWithoutARifle() = runDesktopComposeUiTest(1100, 1000) {
        val app = startApp()
        val rifle = app.obj("state").getValue("currentRifleId")
        app.call("deleteRifle", buildJsonObject { put("id", rifle) })
        openShotLog() // the cartridge is chosen again: the state is the core's
        waitUntil(timeoutMillis = 10_000) { hasText("Choose a rifle and a cartridge.") }
        onNodeWithTag("logHit").assertIsNotEnabled()
        onNodeWithTag("computeTruing").assertIsNotEnabled()
        assertFalse(exists("offsetUp") || exists("openGroup") || exists("dsfAdd") || exists("bcCalc"))
    }

    // ---- DSF ------------------------------------------------------------------

    /** Points added, edited, removed by ✕; two at the same Mach are refused with the core's sentence. */
    @Test
    fun dsfRowsEditedAndRefused() = runDesktopComposeUiTest(1100, 1000) {
        val app = startApp()
        fun stored() = app.obj("solution").getValue("dsf").jsonArray.map { p ->
            p.jsonObject.num("mach").fixed(3) to p.jsonObject.num("factor").fixed(3)
        }
        openShotLog()
        onNodeWithTag("dsfAdd").performScrollTo().performClick()
        onNodeWithTag("dsfAdd").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("dsfMach1") }
        assertEquals("1.1", shown("dsfMach0")) // below the lowest, from 1.2
        assertEquals("1", shown("dsfMach1"))

        type("dsfMach1", "1.1")
        onNodeWithTag("dsfSave").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("Two DSF points have the same Mach.") }
        assertEquals(emptyList(), stored())

        // The second row away: one point, saved.
        onAllNodes(withText("✕")).onLast().performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("dsfMach1") }
        type("dsfFactor0", "1.05")
        onNodeWithTag("dsfSave").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("dsfSave") }
        assertEquals(listOf("1.100" to "1.050"), stored())

        // Its Mach edited.
        type("dsfMach0", "0.9")
        onNodeWithTag("dsfSave").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { stored() == listOf("0.900" to "1.050") }
    }

    /**
     * A fit needs hits slower than Mach 1.3 (the core's sentence otherwise);
     * then each hit says what the fit made of it: supersonic, fitted, or beyond
     * what a DSF can explain.
     */
    @Test
    fun dsfFitExplainsEachHit() = runDesktopComposeUiTest(1100, 1000) {
        val app = startApp()
        app.logShot(300.0, app.predictedAt(300.0))
        openShotLog()
        waitUntil(timeoutMillis = 10_000) { count("shotRow") == 1 }
        onNodeWithTag("dsfFit").performScrollTo().performClick()
        waitUntil(timeoutMillis = 20_000) { exists("dsfFitError") }
        assertTrue(shown("dsfFitError").contains("slower than Mach 1.3"), shown("dsfFitError"))
        assertFalse(exists("dsfApply"))

        // Two transonic hits: at 1100 m where a factor of 2 (the most a DSF point
        // takes) at its Mach puts it; at 1300 m far above anything a DSF gives,
        // which cannot pull that point any higher.
        val rows = app.obj("trajectoryCurve", buildJsonObject { put("maxRangeM", 1300.0); put("points", 261) })
            .getValue("rows").jsonArray.map { it.jsonObject }
        val mach1100 = rows.minBy { abs(it.num("rangeM") - 1100.0) }.num("mach")
        app.call("setDsf", buildJsonObject {
            put("points", JsonArray(listOf(1.4 to 1.0, mach1100 to 2.0).map { (mach, factor) ->
                buildJsonObject { put("mach", mach); put("factor", factor) }
            }))
        })
        val at1100 = app.predictedAt(1100.0)
        app.call("resetDsf")
        app.logShot(1100.0, at1100)
        app.logShot(1300.0, app.predictedAt(1300.0) * 3)
        onNodeWithTag("dsfFit").performScrollTo().performClick()
        waitUntil(timeoutMillis = 20_000) { exists("dsfFitPoints") || exists("dsfFitError") }
        assertTrue(exists("dsfFitPoints"), if (exists("dsfFitError")) shown("dsfFitError") else "")
        assertTrue(hasText("supersonic, left to the velocity and drag truing", substring = true))
        assertTrue(hasText("cannot explain this hit", substring = true))
        assertTrue(hasText("1100 m, Mach", substring = true) && hasText(": hit ", substring = true))
    }

    // ---- BC calculator --------------------------------------------------------

    /** Two chronographs with the far one faster: the core's error; the table and distance change the BC; Cancel writes nothing. */
    @Test
    fun bcCalculatorInputs() = runDesktopComposeUiTest(1100, 1000) {
        val app = startApp()
        val cartridge = app.obj("state").getValue("currentCartridgeId")
        val before = app.obj("cartridgeForm", buildJsonObject { put("id", cartridge) })
        fun bc() = Regex("""BC (G\d): ([\d.]+)""").find(shown("bcResult"))!!.groupValues.let { it[1] to it[2].toDouble() }
        openShotLog()
        onNodeWithTag("bcCalc").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bcCalculate") }

        type("bcVNear", "700")
        type("bcVFar", "750")
        onNodeWithTag("bcCalculate").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bcError") }
        assertTrue(shown("bcError").contains("the far one lower"), shown("bcError"))
        assertFalse(exists("bcResult"))

        type("bcVNear", "790")
        type("bcVFar", "720")
        onNodeWithTag("bcCalculate").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bcResult") }
        val (table, g1) = bc()
        assertEquals("G1", table) // the sample bullet's table

        // G7: another, smaller number for the same bullet.
        onNodeWithTag("bcTable").performClick()
        onAllNodesWithText("G7").onLast().performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("bcResult") } // a new table: calculate again
        onNodeWithTag("bcCalculate").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bcResult") }
        val (t7, g7) = bc()
        assertEquals("G7", t7)
        assertTrue(g7 < g1, "$g7 < $g1")

        // The same loss over half the distance: a worse BC.
        type("bcDistance", "50")
        onNodeWithTag("bcCalculate").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bcResult") && bc().second < g7 / 1.5 }

        // A hit: more elevation at the range than predicted, a lower BC than the cartridge's.
        onNodeWithTag("bcModeHit").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bcRange") }
        type("bcRange", "800")
        val predicted = app.predictedAt(800.0)
        type("bcElevation", (predicted * 1.1).fixed(2))
        onNodeWithTag("bcCalculate").performClick()
        waitUntil(timeoutMillis = 20_000) { exists("bcResult") }
        assertEquals("G7", bc().first)

        onNodeWithText("Cancel").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("bcCalculate") }
        assertEquals(before, app.obj("cartridgeForm", buildJsonObject { put("id", cartridge) }))
    }

    // ---- Group from a photo -----------------------------------------------------

    /** The marking chips and Undo: each takes back the last mark of its kind; the range field re-scales the group. */
    @Test
    fun groupMarksUndoneAndRanged() = runDesktopComposeUiTest(412, 1000) {
        startApp(platform = FakePlatform().apply { image = blankPng() })
        openShotLog()
        onNodeWithTag("openGroup").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("groupPhoto") }
        onNodeWithTag("groupPhoto").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("groupCanvas") }
        type("groupScaleCm", "20")

        tapPhoto(100.0, 300.0); tapPhoto(300.0, 300.0) // scale, then Aim by itself
        tapPhoto(200.0, 200.0)                         // aim, then Holes
        tapPhoto(210.0, 180.0); tapPhoto(250.0, 180.0); tapPhoto(230.0, 160.0)
        waitUntil(timeoutMillis = 10_000) { hasText("3 shots at 100 m") }

        val undo = { onNodeWithText("Undo").performClick() }
        undo() // the last hole
        waitUntil(timeoutMillis = 10_000) { hasText("2 shots at 100 m") }
        // Aim: undone, the group needs it back; a tap sets it.
        onNodeWithTag("markAim").performClick()
        undo()
        waitUntil(timeoutMillis = 10_000) { hasText("Mark the scale", substring = true) }
        tapPhoto(200.0, 200.0)
        waitUntil(timeoutMillis = 10_000) { hasText("2 shots at 100 m") }
        // Scale: one point undone, one tap puts it back.
        onNodeWithTag("markScale").performClick()
        undo()
        waitUntil(timeoutMillis = 10_000) { hasText("Mark the scale", substring = true) }
        tapPhoto(300.0, 300.0)
        waitUntil(timeoutMillis = 10_000) { hasText("2 shots at 100 m") }
        // Holes: back to holes, a third one.
        onNodeWithTag("markHoles").performClick()
        tapPhoto(230.0, 160.0)
        waitUntil(timeoutMillis = 10_000) { hasText("3 shots at 100 m") }

        // Shot at 200 m: the same group is twice as tight in angle; not at the zero.
        val esAt100 = shown("groupEs")
        type("groupRange", "200")
        waitUntil(timeoutMillis = 10_000) { hasText("3 shots at 200 m") }
        assertTrue(shown("groupEs") != esAt100)
        assertFalse(exists("groupZero"))

        onNodeWithText("Back").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("logHit") }
    }

    /** The zero shift and the precision reach the core; a centre more than 1 m off is refused, and says so. */
    @Test
    fun groupSavedToTheCore() = runDesktopComposeUiTest(412, 1000) {
        val app = startApp(platform = FakePlatform().apply { image = blankPng() })
        openShotLog()
        onNodeWithTag("openGroup").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("groupPhoto") }
        onNodeWithTag("groupPhoto").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("groupCanvas") }
        type("groupScaleCm", "20")
        tapPhoto(100.0, 300.0); tapPhoto(300.0, 300.0)
        tapPhoto(200.0, 200.0)
        tapPhoto(250.0, 150.0); tapPhoto(290.0, 150.0); tapPhoto(270.0, 130.0) // 7 cm right, 5.7 cm up
        waitUntil(timeoutMillis = 10_000) { exists("groupCentre") }
        val (up, right) = Regex("""([-\d.]+) cm up, ([-\d.]+) cm right""").find(shown("groupCentre"))!!.destructured

        onNodeWithTag("groupZero").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("groupSaved") }
        assertEquals(up.toDouble(), app.pair().num("offsetUpCm"), 0.05)
        assertEquals(right.toDouble(), app.pair().num("offsetRightCm"), 0.05)

        val moa = Regex("""([\d.]+) MOA""").find(shown("groupWez"))!!.groupValues[1].toDouble()
        onNodeWithTag("groupWez").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) {
            val settings = app.obj("wez", buildJsonObject { put("toM", 100.0); put("stepM", 100.0) }).getValue("settings").jsonObject
            abs(settings.num("groupMoa") - moa) < 0.006
        }

        // A changed scale is not saved yet.
        type("groupScaleCm", "500")
        assertTrue(!exists("groupSaved"))
        // 500 cm apart: the centre is metres off, the core refuses it and the screen says so.
        onNodeWithTag("groupZero").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("groupError") }
        assertTrue(!exists("groupSaved"))
        assertEquals(up.toDouble(), app.pair().num("offsetUpCm"), 0.05)
        // Back to the right scale: the refusal goes with the change.
        type("groupScaleCm", "20")
        assertTrue(!exists("groupError"))
    }

    // ---- Range card -------------------------------------------------------------

    /** From, to and step reach the core and the rows follow; the note closes the card; a tapped row aims at it. */
    @Test
    fun rangeCardSpanAndRowTap() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        onNodeWithTag("navTable").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rangeTable") && hasText("150") }

        type("tableFrom", "200")
        type("tableTo", "600")
        type("tableStep", "100")
        waitUntil(timeoutMillis = 10_000) { !hasText("150") && !hasText("700") && hasText("600") }
        val st = app.obj("state")
        assertEquals(listOf(200.0, 600.0, 100.0), listOf(st.num("tableFromM"), st.num("tableToM"), st.num("tableStepM")))

        onNodeWithTag("rangeTable").performScrollToNode(withText("Tap a row to aim at it", substring = true))
        assertTrue(hasText("Tap a row to aim at it", substring = true))

        onAllNodes(withText("500") and hasAnyAncestor(hasTestTag("rangeTable"))).onFirst().performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("solvedFor:500") }
        assertEquals(500.0, app.obj("state").getValue("conditions").jsonObject.num("targetRangeM"))
    }

    /** The columns dialog's Cancel keeps the columns; the Table tab comes back from the chart. */
    @Test
    fun columnsCancelledAndTabsSwitched() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        onNodeWithTag("navTable").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("tableColumns") }
        onNodeWithTag("tableColumns").performClick()
        onNodeWithTag("columnsAuto").performClick()
        onNodeWithTag("column:mach").performClick()
        onNodeWithText("Cancel").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("columnsAuto") }
        val prefs = app.obj("state").getValue("prefs").jsonObject
        assertEquals(JsonArray(emptyList()), prefs["tableColumns"] ?: JsonArray(emptyList()))
        assertFalse(exists("col:mach")) // a phone shows no Mach column by itself

        onNodeWithTag("chartTab").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("chart") && !exists("rangeTable") }
        onNode(withText("Table") and !hasTestTag("navTable")).performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rangeTable") && !exists("chart") }
    }

    /** The card as a picture marks the target's row only when it is asked to; by default no row is marked. */
    @Test
    fun pngMarksOnlyTheTargetRow() {
        fun marked(png: ByteArray): Boolean {
            val img = ImageIO.read(png.inputStream())
            return (0 until img.height).any { y -> (0 until img.width).any { x -> img.getRGB(x, y) and 0xFFFFFF == 0xFFE0B2 } }
        }
        val rows = List(3) { listOf("${(it + 1) * 100}", "1.0") }
        assertFalse(marked(tablePng("Card", listOf("Range", "Elev"), rows)))
        assertTrue(marked(tablePng("Card", listOf("Range", "Elev"), rows, highlight = 1)))
    }

    // ---- Chart --------------------------------------------------------------------

    /** Earth rotation on: the chart offers the Coriolis drift, and with a known direction the Eötvös lift. */
    @Test
    fun coriolisInTheChart() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        fun openQuantities() {
            onNodeWithTag("navTable").performClick()
            onNodeWithTag("chartTab").performClick()
            waitUntil(timeoutMillis = 10_000) { exists("chartQuantity") }
            onNodeWithTag("chartQuantity").performClick()
        }
        openQuantities()
        waitUntil(timeoutMillis = 10_000) { hasText("Spin drift") }
        assertFalse(hasText("Coriolis drift"))
        onAllNodesWithText("Spin drift").onLast().performClick() // closes the menu

        onNodeWithTag("navConditions").performClick()
        onNodeWithText("Account for Earth rotation").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("Known shot direction") }
        onNodeWithText("Known shot direction").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { app.obj("state").getValue("conditions").jsonObject["useAzimuth"]?.jsonPrimitive?.boolean == true }

        openQuantities()
        waitUntil(timeoutMillis = 10_000) { hasText("Coriolis drift") && hasText("Eötvös lift") }
        onAllNodesWithText("Eötvös lift").onLast().performClick()
        waitUntil(timeoutMillis = 10_000) { onAllNodesWithText("Eötvös lift").fetchSemanticsNodes().size == 1 && exists("chart") }
    }

    /** Compare: Cancel adds nothing; two loads compared hide the button; removing one brings it back. */
    @Test
    fun compareUpToTwo() = runDesktopComposeUiTest(1100, 760) {
        val app = startApp()
        // Two more loads of the sample cartridge (through the core, as the editor would).
        val st = app.obj("state")
        val first = st.getValue("currentCartridgeId")
        val form = app.obj("cartridgeForm", buildJsonObject { put("id", first) })
        for ((name, v) in listOf("Hot load" to 850.0, "Cold load" to 760.0)) {
            val f = JsonObject(form + ("cartridgeId" to JsonPrimitive(0)) + ("name" to JsonPrimitive(name)) + ("muzzleVelocity" to JsonPrimitive(v)))
            app.call("saveCartridge", buildJsonObject { put("form", f) })
        }
        app.call("select", buildJsonObject { put("rifleId", st.getValue("currentRifleId")); put("cartridgeId", first) })

        onNodeWithTag("navTable").performClick()
        onNodeWithTag("chartTab").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("compare") }

        onNodeWithTag("compare").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("compareAdd") }
        onNodeWithText("Cancel").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("compareAdd") }
        assertFalse(exists("compared0"))

        // The cold load, picked from the list.
        onNodeWithTag("compare").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("compareCartridge") }
        onNodeWithTag("compareCartridge").performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("Cold load") }
        onAllNodesWithText("Cold load").onLast().performClick()
        onNodeWithTag("compareAdd").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("compared0") && shown("compared0").contains("Cold load") }

        // The hot one: two compared, no more.
        onNodeWithTag("compare").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("compareCartridge") }
        onNodeWithTag("compareCartridge").performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("Hot load") }
        onAllNodesWithText("Hot load").onLast().performClick()
        onNodeWithTag("compareAdd").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("compared1") }
        assertFalse(exists("compare"))

        onNodeWithTag("compared0").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("compared1") && exists("compare") }
    }

    // ---- Shared widgets -----------------------------------------------------------

    /** A number field: garbage keeps the old value, too much is clamped, a good value is passed on. */
    @Test
    fun numberFieldCommits() = runDesktopComposeUiTest(400, 300) {
        val value = mutableDoubleStateOf(5.0)
        val edits = mutableListOf<Double>()
        setContent {
            NumberField("Value", value.doubleValue, { edits += it; value.doubleValue = it }, unit = "m", decimals = 1,
                from = 0.0, to = 10.0, tag = "n")
        }
        type("n", "1..2")
        assertEquals("5", shown("n"))
        assertEquals(emptyList(), edits)
        type("n", "42")
        assertEquals("10", shown("n"))
        type("n", "2,5") // a decimal comma reads too
        assertEquals("2.5", shown("n"))
        assertEquals(listOf(10.0, 2.5), edits)
    }

    /**
     * A stepper on a narrow screen with the field taking the rest of the row:
     * the steps stop at the ends; a direction goes round.
     */
    @Test
    fun stepperFieldSteps() = runDesktopComposeUiTest(400, 400) {
        val speed = mutableDoubleStateOf(9.0)
        val direction = mutableDoubleStateOf(345.0)
        setContent {
            Column {
                StepperField("Speed", speed.doubleValue, { speed.doubleValue = it }, Modifier.width(280.dp),
                    step = 1.0, from = 0.0, to = 10.0, tag = "speed")
                StepperField("From", direction.doubleValue, { direction.doubleValue = it }, Modifier.width(280.dp),
                    step = 15.0, from = 0.0, to = 360.0, tag = "dir", wrap = true, decimals = 0)
            }
        }
        onNodeWithTag("speedPlus", useUnmergedTree = true).performClick()
        waitUntil(timeoutMillis = 5_000) { shown("speed") == "10" }
        onNodeWithTag("speedPlus", useUnmergedTree = true).performClick() // at the end: stays
        announce()
        assertEquals(10.0, speed.doubleValue)
        onNodeWithTag("speedMinus", useUnmergedTree = true).performClick()
        waitUntil(timeoutMillis = 5_000) { shown("speed") == "9" }

        onNodeWithTag("dirPlus", useUnmergedTree = true).performClick()
        waitUntil(timeoutMillis = 5_000) { shown("dir") == "0" }
        onNodeWithTag("dirMinus", useUnmergedTree = true).performClick()
        waitUntil(timeoutMillis = 5_000) { shown("dir") == "345" }
    }

    /** A choice field: the menu lists the options, the one picked is shown and passed on. */
    @Test
    fun choiceFieldPicks() = runDesktopComposeUiTest(400, 400) {
        val picked = mutableStateOf("a")
        setContent {
            val p by picked
            Column {
                ChoiceField("Letter", listOf("a" to "Alpha", "b" to "Bravo"), p, { picked.value = it }, Modifier.testTag("choice"))
                Text(p, Modifier.testTag("picked"))
            }
        }
        assertFalse(hasText("Bravo"))
        onNodeWithTag("choice").performClick()
        waitUntil(timeoutMillis = 5_000) { hasText("Bravo") }
        onAllNodesWithText("Bravo").onLast().performClick()
        waitUntil(timeoutMillis = 5_000) { shown("picked") == "b" }
        assertEquals(1, onAllNodesWithText("Bravo").fetchSemanticsNodes().size) // the menu closed
    }
}
