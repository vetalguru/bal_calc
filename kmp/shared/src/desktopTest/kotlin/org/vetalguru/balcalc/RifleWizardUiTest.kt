package org.vetalguru.balcalc

import androidx.compose.ui.test.ComposeUiTest
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.graphics.toAwtImage
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.captureToImage
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.onLast
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.click
import androidx.compose.ui.test.runDesktopComposeUiTest
import kotlin.math.PI
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.double
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.long
import kotlinx.serialization.json.put
import org.vetalguru.balcalc.core.RifleForm
import org.vetalguru.balcalc.ui.WizardStep
import org.vetalguru.balcalc.ui.convertAngle
import org.vetalguru.balcalc.ui.muzzleShiftNow
import org.vetalguru.balcalc.ui.shiftText
import org.vetalguru.balcalc.ui.suppressorShiftNow
import org.vetalguru.balcalc.ui.partAt
import org.vetalguru.balcalc.ui.reachUp
import org.vetalguru.balcalc.ui.sightHeightCm
import org.vetalguru.balcalc.ui.withClickUnits

/** The pure parts of the rifle's fields: the sight height calculator, angles, the turret's reach. */
class RifleMathTest {
    @Test
    fun sightHeightFromACaliper() {
        // An 18 mm bolt, 45 mm from its top to the top of a 30 mm tube: 9 + 45 - 15 = 39 mm.
        assertEquals(3.9, sightHeightCm(18.0, 45.0, 30.0)!!, 1e-9)
        assertNull(sightHeightCm(0.0, 45.0, 30.0)) // a measure missing
        assertNull(sightHeightCm(18.0, 0.0, 30.0))
        assertNull(sightHeightCm(18.0, 45.0, 0.0))
        assertNull(sightHeightCm(18.0, 5.0, 34.0)) // the tube's centre below the bore: a wrong measure
    }

    @Test
    fun anglesBetweenUnits() {
        assertEquals(20 * PI / 10800 / 1e-3, convertAngle(20.0, "moa", "mrad"), 1e-9)
        assertEquals(29.0, convertAngle(convertAngle(29.0, "mrad", "moa"), "moa", "mrad"), 1e-9)
        assertEquals(1.0, convertAngle(0.1, "mrad", "cm100m"), 1e-9)
        assertEquals(5.0, convertAngle(5.0, "furlong", "mrad")) // unknown units: as it is
    }

    @Test
    fun travelKeepsItsAngleWhenTheUnitsChange() {
        val f = RifleForm(clickUnits = "mrad", clickValue = 0.1, elevationTravel = 29.0, windageTravel = 14.5).withClickUnits("moa")
        assertEquals("moa", f.clickUnits)
        assertEquals(99.7, f.elevationTravel, 0.05)
        assertEquals(49.8, f.windageTravel, 0.05)
        assertEquals(0.1, f.clickValue) // the clicks are the new scope's to enter
    }

    @Test
    fun eachPartOfTheDrawingIsAStep() {
        assertEquals(WizardStep.Scope, partAt(0.40f, 0.36f)) // the tube
        assertEquals(WizardStep.Scope, partAt(0.40f, 0.05f)) // its label
        assertEquals(WizardStep.Mount, partAt(0.33f, 0.42f)) // a ring
        assertEquals(WizardStep.Mount, partAt(0.74f, 0.45f)) // the sight height
        assertEquals(WizardStep.Barrel, partAt(0.70f, 0.54f))
        assertEquals(WizardStep.Barrel, partAt(0.89f, 0.53f)) // the muzzle device
        assertEquals(WizardStep.Suppressor, partAt(0.94f, 0.53f))
        assertEquals(WizardStep.Rifle, partAt(0.10f, 0.60f)) // the stock
        assertEquals(WizardStep.Rifle, partAt(0.40f, 0.55f)) // the action
        assertEquals(WizardStep.Zero, partAt(0.95f, 0.365f)) // the target
        assertNull(partAt(0.80f, 0.95f))
    }

    @Test
    fun muzzleDeviceShiftsOnlyWhenNotAsAtTheZero() {
        val f = RifleForm(muzzleDevice = "brake", muzzlePoiUp = -1.5, muzzlePoiRight = 0.5)
        assertEquals(-1.5 to 0.5, f.muzzleShiftNow()) // on, zeroed without it
        assertEquals(0.0 to 0.0, f.copy(zeroedWithDevice = true).muzzleShiftNow())
        assertEquals(1.5 to -0.5, f.copy(zeroedWithDevice = true, muzzleDeviceOn = false).muzzleShiftNow())
        assertEquals(0.0 to 0.0, f.copy(muzzleDevice = "none").muzzleShiftNow())
        assertEquals(0.0 to 0.0, f.copy(muzzleDevice = "").muzzleShiftNow())
        // The suppressor on its own.
        val s = RifleForm(suppressorPoiUp = -1.0)
        assertEquals(0.0 to 0.0, s.suppressorShiftNow()) // no suppressor
        assertEquals(-1.0 to 0.0, s.copy(hasSuppressor = true).suppressorShiftNow())
        assertEquals(1.0 to 0.0, s.copy(hasSuppressor = true, suppressorOn = false, zeroedWithSuppressor = true).suppressorShiftNow())
        assertEquals("↓1.5 →0.5 MOA", shiftText(-1.5, 0.5, "MOA"))
        assertEquals("↑0.2 MRAD", shiftText(0.2, 0.0, "MRAD"))
        assertEquals("", shiftText(0.0, 0.0, "MOA"))
    }

    @Test
    fun reachIsHalfTheTravelAndTheRail() {
        assertNull(RifleForm(railSlope = 20.0).reachUp()) // travel not stated
        val f = RifleForm(clickUnits = "mrad", elevationTravel = 30.0, railSlope = 20.0, railSlopeUnits = "moa")
        assertEquals(15.0 + 20 * PI / 10800 / 1e-3, f.reachUp()!!, 1e-9)
        assertEquals(18.0, f.copy(railSlope = 3.0, railSlopeUnits = "mrad").reachUp()!!, 1e-9)
    }
}

@OptIn(ExperimentalTestApi::class)
class RifleWizardUiTest {
    private fun TestApp.rifles() = call("state").jsonObject.getValue("rifles").jsonArray.map { it.jsonObject }
    private fun TestApp.rifleForm(id: Long): JsonObject = call("rifleForm", buildJsonObject { put("id", id) }).jsonObject
    private fun JsonObject.num(key: String) = getValue(key).jsonPrimitive.double
    private fun JsonObject.str(key: String) = getValue(key).jsonPrimitive.content

    private fun ComposeUiTest.newRifleWizard() {
        onNodeWithTag("navArmory").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("new") }
        onNodeWithTag("new").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("wizardStep") }
    }

    private fun ComposeUiTest.next(expectTag: String) {
        onNodeWithTag("wizardNext").performClick()
        waitUntil(timeoutMillis = 10_000) { exists(expectTag) }
    }

    /** Picks [option] in the drop-down labelled [label]. */
    private fun ComposeUiTest.choose(label: String, option: String) {
        onNodeWithText(label).performScrollTo()
        onNodeWithText(label).performClick()
        waitUntil(timeoutMillis = 10_000) { hasText(option) }
        onAllNodesWithText(option).onLast().performClick()
        waitForIdle()
    }

    /** The window, or an open dialog, as screenshots/wizard-<name>.png (with -Dbalcalc.screenshots). */
    private fun ComposeUiTest.shot(name: String) {
        val dir = System.getProperty("balcalc.screenshots") ?: return
        java.io.File(dir).mkdirs()
        javax.imageio.ImageIO.write(
            onAllNodes(androidx.compose.ui.test.isRoot()).onLast().captureToImage().toAwtImage(), "png", java.io.File(dir, "wizard-$name.png"),
        )
    }

    /** Every step in turn, the sight height from the calculator, the summary, then Save. */
    @Test
    fun everyStepToTheSummaryAndSave() = runDesktopComposeUiTest(412, 915) {
        val app = startApp(sample = false)
        newRifleWizard()
        assertTrue(shown("wizardStep").startsWith("Step 1 of 7"))
        onNodeWithTag("wizardBack").assertIsNotEnabled() // nothing before the first step

        type("rifleName", "Long range")
        type("rifleMaker", "Tikka")
        type("rifleModel", "T3x TAC A1")
        type("rifleCaliber", ".308 Win")
        shot("1-rifle")

        next("twist")
        assertTrue(shown("wizardStep").startsWith("Step 2 of 7"))
        type("barrel", "24")
        type("twist", "10")
        type("grooves", "4")
        choose("Rifling", "Polygonal")
        choose("Muzzle device", "Muzzle brake")
        waitUntil(timeoutMillis = 10_000) { exists("muzzlePoiUp") }
        shot("2-barrel")

        next("hasSuppressor")
        onNodeWithTag("hasSuppressor").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("suppressorName") }
        type("suppressorName", "Ase Utra SL7")
        type("suppressorPoiUp", "-1.5")
        shot("3-suppressor")

        next("elevationTravel")
        type("scopeMaker", "Vortex")
        type("scopeModel", "Razor HD Gen III")
        type("clickHorizontal", "0.05")
        type("elevationTravel", "29")
        type("windageTravel", "14.5")
        type("tube", "34")
        type("objective", "56")
        shot("4-scope")

        next("railSlope")
        type("railSlope", "20")
        type("ringHeight", "30")
        assertTrue(exists("reachUp"))
        assertTrue(shown("reachUp").contains("20.3")) // 14.5 + 20 MOA (5.8 MRAD)
        onNodeWithTag("sightCalc").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("calcBolt") }
        assertEquals("34", shown("calcTube")) // the scope's tube
        type("calcBolt", "18")
        type("calcTopToTop", "48")
        waitUntil(timeoutMillis = 10_000) { exists("calcResult") }
        assertEquals("Sight height: 4.00 cm", shown("calcResult")) // 9 + 48 - 17 = 40 mm
        shot("5b-sight-calculator")
        onNodeWithTag("calcUse").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("calcBolt") }
        assertEquals("4", shown("sightHeight"))
        shot("5-mount")

        next("zeroRange")
        type("zeroRange", "200")
        shot("6-zero")
        next("summaryChange:Barrel")
        assertTrue(shown("wizardStep").startsWith("Step 7 of 7"))
        assertTrue(hasText("Tikka T3x TAC A1"))
        assertTrue(hasText("Polygonal, 4"))
        assertTrue(hasText("Muzzle brake"))
        assertTrue(hasText("Ase Utra SL7"))
        assertTrue(hasText("Vortex Razor HD Gen III"))
        assertTrue(hasText("20 MOA"))
        assertTrue(hasText("1:10 in"))
        shot("7-summary")

        // Back to a step from the summary, and on again.
        onNodeWithTag("summaryChange:Barrel").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("twist") }
        assertEquals("24", shown("barrel"))
        repeat(5) { onNodeWithTag("wizardNext").performClick() } // suppressor, scope, mount, zero, summary
        waitUntil(timeoutMillis = 10_000) { exists("summaryChange:Zero") }

        onNodeWithTag("wizardFinish").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifle:Long range") }
        val id = app.rifles().first { it.str("name") == "Long range" }.getValue("id").jsonPrimitive.long
        val f = app.rifleForm(id)
        assertEquals("Tikka", f.str("maker"))
        assertEquals("T3x TAC A1", f.str("model"))
        assertEquals(4, f.getValue("riflingGrooves").jsonPrimitive.int)
        assertEquals("polygonal", f.str("riflingType"))
        assertEquals("brake", f.str("muzzleDevice"))
        assertEquals("true", f.str("hasSuppressor"))
        assertEquals("Ase Utra SL7", f.str("suppressorName"))
        assertEquals(-1.5, f.num("suppressorPoiUp"), 1e-9)
        assertEquals(24.0, f.num("barrelIn"), 1e-9)
        assertEquals(20.0, f.num("railSlope"), 1e-9)
        assertEquals("moa", f.str("railSlopeUnits"))
        assertEquals(30.0, f.num("ringHeightMm"), 1e-9)
        assertEquals(4.0, f.num("sightHeightCm"), 1e-9)
        assertEquals(0.05, f.num("clickValueHorizontal"), 1e-9)
        assertEquals(29.0, f.num("elevationTravel"), 1e-9)
        assertEquals(14.5, f.num("windageTravel"), 1e-9)
        assertEquals("Vortex", f.str("scopeMaker"))
        assertEquals(34.0, f.num("tubeDiameterMm"), 1e-9)
        assertEquals(56.0, f.num("objectiveMm"), 1e-9)
        assertEquals(200.0, f.num("zeroRangeM"), 1e-9)
    }

    /** Save on a later step without a name: the error, back on the first step; Back keeps the draft; Cancel drops it. */
    @Test
    fun nameRequiredBackKeepsCancelDrops() = runDesktopComposeUiTest(412, 915) {
        val app = startApp(sample = false)
        newRifleWizard()
        next("twist")
        type("twist", "8")
        next("hasSuppressor")
        next("elevationTravel")
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("formError") }
        assertEquals("Enter a rifle name.", shown("formError"))
        assertTrue(shown("wizardStep").startsWith("Step 1 of 7"))

        next("twist")
        assertEquals("8", shown("twist")) // the draft kept
        onNodeWithTag("wizardBack").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifleName") }
        onNodeWithText("Cancel").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("new") }
        assertTrue(app.rifles().isEmpty())

        // A new wizard starts empty, at the first step.
        onNodeWithTag("new").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifleName") }
        assertEquals("", shown("rifleName"))
        assertTrue(shown("wizardStep").startsWith("Step 1 of 7"))
    }

    /** A tap on the drawing opens that part; the chips go there too; seen parts get their ✓. */
    @Test
    fun drawingAndChipsOpenParts() = runDesktopComposeUiTest(412, 915) {
        startApp(sample = false)
        newRifleWizard()
        val diagram = onNodeWithTag("rifleDiagram")
        diagram.performTouchInput { click(androidx.compose.ui.geometry.Offset(width * 0.40f, height * 0.36f)) }
        waitUntil(timeoutMillis = 10_000) { exists("elevationTravel") }
        assertTrue(shown("wizardStep").endsWith("Scope"))
        diagram.performTouchInput { click(androidx.compose.ui.geometry.Offset(width * 0.70f, height * 0.54f)) }
        waitUntil(timeoutMillis = 10_000) { exists("twist") }
        diagram.performTouchInput { click(androidx.compose.ui.geometry.Offset(width * 0.95f, height * 0.365f)) }
        waitUntil(timeoutMillis = 10_000) { exists("zeroRange") }
        onNodeWithTag("part:Mount").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("sightHeight") }
        onNodeWithTag("part:Summary").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("wizardFinish") }
        // The drawing tells its values to a screen reader.
        assertTrue(diagram.fetchSemanticsNode().config[androidx.compose.ui.semantics.SemanticsProperties.ContentDescription]
            .first().contains("100 m"))
    }

    /** A suppressor that hits 1.5 MOA low, its own part over a brake: the shift, whether it counts now, saved. */
    @Test
    fun suppressorShift() = runDesktopComposeUiTest(412, 915) {
        val app = startApp(sample = false)
        newRifleWizard()
        type("rifleName", "Quiet")
        next("twist")
        assertFalse(exists("muzzlePoiUp")) // no device yet
        choose("Muzzle device", "Muzzle brake")
        waitUntil(timeoutMillis = 10_000) { exists("muzzlePoiUp") }

        // The suppressor's part: a tap on its place on the drawing.
        onNodeWithTag("rifleDiagram").performTouchInput { click(androidx.compose.ui.geometry.Offset(width * 0.94f, height * 0.53f)) }
        waitUntil(timeoutMillis = 10_000) { exists("hasSuppressor") }
        assertTrue(shown("wizardStep").endsWith("Suppressor"))
        assertFalse(exists("suppressorPoiUp"))
        onNodeWithTag("hasSuppressor").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("suppressorPoiUp") }
        type("suppressorPoiUp", "-1.5")
        onNodeWithTag("suppressorShiftNow").performScrollTo()
        assertEquals("Now the solution shifts by ↓1.5 MOA.", shown("suppressorShiftNow"))
        assertTrue(onNodeWithTag("rifleDiagram").fetchSemanticsNode()
            .config[androidx.compose.ui.semantics.SemanticsProperties.ContentDescription].first().contains("↓1.5 MOA"))
        shot("3b-suppressor-shift")
        onNodeWithTag("suppressorAtZero").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { shown("suppressorShiftNow") == "Now as at the zero: no shift." }
        onNodeWithTag("suppressorAtZero").performClick() // back: zeroed without it

        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifle:Quiet") }
        val id = app.rifles().first { it.str("name") == "Quiet" }.getValue("id").jsonPrimitive.long
        val f = app.rifleForm(id)
        assertEquals("brake", f.str("muzzleDevice"))
        assertEquals("true", f.str("hasSuppressor"))
        assertEquals(-1.5, f.num("suppressorPoiUp"), 1e-9)
        assertEquals("moa", f.str("suppressorPoiUnits"))
        assertEquals("true", f.str("suppressorOn"))
        assertEquals("false", f.str("zeroedWithSuppressor"))
        assertEquals(0.0, f.num("muzzlePoiUp"), 1e-9) // the brake's own, untouched
    }

    /** An existing rifle opens on the same drawing, every part done, with the new fields. */
    @Test
    fun editorShowsTheDescription() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        val id = app.rifles().first().getValue("id").jsonPrimitive.long
        val form = app.rifleForm(id).toMutableMap().apply {
            put("maker", kotlinx.serialization.json.JsonPrimitive("Sako"))
            put("elevationTravel", kotlinx.serialization.json.JsonPrimitive(30.0))
            put("railSlope", kotlinx.serialization.json.JsonPrimitive(20.0))
        }
        app.call("saveRifle", buildJsonObject { put("form", JsonObject(form)) })
        onNodeWithTag("navArmory").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("new") }
        onAllNodesWithText("Edit").onFirst().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifleName") }
        assertTrue(hasText("Edit rifle"))
        assertEquals("Sako", shown("rifleMaker"))
        onNodeWithTag("part:Scope").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevationTravel") }
        onNodeWithTag("elevationTravel").performScrollTo()
        assertEquals("30", shown("elevationTravel"))
        onNodeWithTag("part:Mount").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("reachUp") }
        assertTrue(shown("reachUp").contains("20.8")) // 15 + 20 MOA
    }
}
