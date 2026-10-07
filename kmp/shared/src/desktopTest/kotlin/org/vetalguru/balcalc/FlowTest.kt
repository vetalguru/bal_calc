package org.vetalguru.balcalc

import androidx.compose.ui.graphics.toAwtImage
import androidx.compose.ui.test.ComposeUiTest
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertTextEquals
import androidx.compose.ui.test.captureToImage
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.semantics.getOrNull
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.onLast
import androidx.compose.ui.test.click
import androidx.compose.ui.test.performTouchInput
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import androidx.compose.ui.test.onRoot
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performImeAction
import androidx.compose.ui.test.performTextReplacement
import androidx.compose.ui.test.runDesktopComposeUiTest
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.Density
import java.io.File
import javax.imageio.ImageIO
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotEquals
import kotlin.test.assertTrue
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.core.desktopEngine
import org.vetalguru.balcalc.core.desktopSeed

/** The app on the desktop JVM with the real C++ core, end to end. */
@OptIn(ExperimentalTestApi::class)
class FlowTest {
    // State written from the core's worker thread reaches the test once its
    // snapshot changes are announced; announce them before every check.
    private fun ComposeUiTest.announce() {
        androidx.compose.runtime.snapshots.Snapshot.sendApplyNotifications()
        waitForIdle() // let the recomposer run the frame those changes need
    }

    private fun ComposeUiTest.exists(tag: String): Boolean {
        announce()
        return onAllNodes(hasTestTag(tag)).fetchSemanticsNodes().isNotEmpty()
    }

    /** What a node shows: an input's content, or its text. */
    private fun ComposeUiTest.shown(tag: String): String {
        announce()
        val config = onNodeWithTag(tag).fetchSemanticsNode().config
        return config.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.EditableText)?.text
            ?: config.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.Text).orEmpty()
                .joinToString("") { it.text }
    }

    /** One element as an image (e.g. a card below the fold). */
    private fun ComposeUiTest.shotOf(tag: String, name: String) {
        val dir = System.getProperty("balcalc.screenshots") ?: return
        File(dir).mkdirs()
        ImageIO.write(onNodeWithTag(tag).captureToImage().toAwtImage(), "png", File(dir, "$name.png"))
    }

    private fun ComposeUiTest.shot(name: String) {
        val dir = System.getProperty("balcalc.screenshots") ?: return
        File(dir).mkdirs()
        // The topmost layer: a dialog when one is open.
        ImageIO.write(onAllNodes(androidx.compose.ui.test.isRoot()).onLast().captureToImage().toAwtImage(), "png", File(dir, "$name.png"))
    }

    /** The main flow at one screen size; [fontScale] is the phone's font size setting. */
    private fun run(width: Int, height: Int, name: String, fontScale: Float = 1f) = runDesktopComposeUiTest(width, height) {
        val db = File.createTempFile("balcalc-test", ".db").apply { delete() }
        val api = Api(desktopEngine())
        setContent {
            CompositionLocalProvider(LocalDensity provides Density(LocalDensity.current.density, fontScale)) {
                BalCalcApp(api, startup = { start(db.path) { desktopSeed() } }, platform = FakePlatform())
            }
        }

        // Seeded library, sample rifle + cartridge: the Qt app's 1.61 at 300 m.
        waitUntil(timeoutMillis = 30_000) { exists("sample") }
        onNodeWithTag("sample").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") }
        onNodeWithTag("elevation").assertTextEquals("1.61")
        shot("$name-solution")
        onNodeWithTag("quickWind").performScrollTo()
        shotOf("quickWind", "$name-wind")

        // Colder air: more elevation.
        onNodeWithTag("navConditions").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("temperature") }
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
        waitUntil(timeoutMillis = 10_000) { exists("chart") }
        shot("$name-chart")
        assertTrue(exists("chart"))
        db.delete()
    }

    private fun ComposeUiTest.type(tag: String, value: String) {
        onNodeWithTag(tag).performTextReplacement(value)
        onNodeWithTag(tag).performImeAction()
    }

    private fun ComposeUiTest.count(prefix: String): Int {
        announce()
        return onAllNodes(androidx.compose.ui.test.SemanticsMatcher("tag $prefix*") {
            it.config.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.TestTag)?.startsWith(prefix) == true
        }).fetchSemanticsNodes().size
    }

    @Test
    fun armory() = runDesktopComposeUiTest(400, 820) {
        val db = File.createTempFile("balcalc-test", ".db").apply { delete() }
        val platform = FakePlatform()
        setContent { BalCalcApp(Api(desktopEngine()), startup = { start(db.path) { desktopSeed() } }, platform = platform) }
        waitUntil(timeoutMillis = 30_000) { exists("sample") }

        // A rifle through its editor.
        onNodeWithTag("navArmory").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("new") }
        onNodeWithTag("new").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifleName") }
        type("rifleName", "Tikka")
        type("rifleCaliber", ".308 Win")
        shot("armory-rifle-editor")
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifle:Tikka") }

        // A cartridge with its own bullet.
        onNodeWithTag("cartridgesTab").performClick()
        onNodeWithTag("new").performClick()
        onNodeWithTag("newEmptyCartridge").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridgeName") }
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
        waitUntil(timeoutMillis = 10_000) { exists("more:Load") }
        onNodeWithTag("more:Load").performClick()
        onNodeWithTag("copy").performClick()
        waitUntil(timeoutMillis = 10_000) { platform.clipboard?.contains("balcalc-cartridge") == true }
        onNodeWithTag("cartridgesTab").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridge:Load") }
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
        waitUntil(timeoutMillis = 10_000) { exists("search") }
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

    /** Seeded library + sample rifle and cartridge, on the given screen size. */
    private var testApi: Api? = null

    private fun ComposeUiTest.startWithSample(): File {
        val db = File.createTempFile("balcalc-test", ".db").apply { delete() }
        val api = Api(desktopEngine()).also { testApi = it }
        setContent { BalCalcApp(api, startup = { start(db.path) { desktopSeed() } }, platform = FakePlatform()) }
        waitUntil(timeoutMillis = 30_000) { exists("sample") }
        onNodeWithTag("sample").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") }
        return db
    }

    private fun ComposeUiTest.elevation(): Double = shown("elevation").toDouble()

    private fun ComposeUiTest.setRange(m: Int) {
        type("range", m.toString())
        // The corrections must be the ones for this range, not the last.
        waitUntil(timeoutMillis = 10_000) { exists("solvedFor:$m") && shown("range") == m.toString() }
        waitForIdle()
    }

    @Test
    fun rifleAndScopeFromTheLibrary() = runDesktopComposeUiTest(412, 915) {
        val db = File.createTempFile("balcalc-test", ".db").apply { delete() }
        setContent { BalCalcApp(Api(desktopEngine()), startup = { start(db.path) { desktopSeed() } }, platform = FakePlatform()) }
        waitUntil(timeoutMillis = 30_000) { exists("sample") }
        onNodeWithTag("navArmory").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("new") }
        onNodeWithTag("new").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifleFromLibrary") }

        onNodeWithTag("rifleFromLibrary").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("search") }
        onNodeWithTag("search").performTextReplacement("tikka ctr 6.5 creedmoor")
        waitUntil(timeoutMillis = 10_000) { count("libraryRifle:") == 1 }
        shot("library-rifles")
        onNodeWithTag("libraryRifle:Tikka T3x CTR 6.5 Creedmoor 8").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifleName") }
        assertEquals("Tikka T3x CTR 6.5 Creedmoor", shown("rifleName"))
        assertEquals("6.5 Creedmoor", shown("rifleCaliber"))
        assertEquals("8", shown("twist"))

        onNodeWithTag("scopeFromLibrary").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("search") }
        onNodeWithTag("search").performTextReplacement("atacr 7-35 f1")
        waitUntil(timeoutMillis = 10_000) { count("libraryScope:") == 2 } // MOA and MRAD turrets
        shot("library-scopes")
        onNodeWithTag("libraryScope:Nightforce ATACR 7-35x56 F1 mrad0.1").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("clickValue") }
        assertEquals("0.1", shown("clickValue"))
        assertEquals("35", shown("magTo"))
        assertEquals("Tikka T3x CTR 6.5 Creedmoor", shown("rifleName")) // the rifle part kept

        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifle:Tikka T3x CTR 6.5 Creedmoor") }
        db.delete()
    }

    @Test
    fun rifleThroughAQrCode() = runDesktopComposeUiTest(412, 915) {
        val db = File.createTempFile("balcalc-test", ".db").apply { delete() }
        val platform = FakePlatform()
        setContent { BalCalcApp(Api(desktopEngine()), startup = { start(db.path) { desktopSeed() } }, platform = platform) }
        waitUntil(timeoutMillis = 30_000) { exists("sample") }
        onNodeWithTag("sample").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") }
        onNodeWithTag("navArmory").performClick()
        waitUntil(timeoutMillis = 10_000) { count("more:") > 0 }
        val name = "Sample .308 Win"

        onNodeWithTag("more:$name").performClick()
        onNodeWithTag("showQr").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("qrCode") }
        shot("qr-show")
        onAllNodesWithText("Close").onFirst().performClick()

        // The same file as the code carries, read back from pictures of its parts.
        onNodeWithTag("more:$name").performClick()
        onNodeWithTag("copy").performClick()
        waitUntil(timeoutMillis = 10_000) { platform.clipboard != null }
        val parts = QrShare.parts(platform.clipboard!!)
        onAllNodesWithText("Import").onFirst().performClick()
        onNodeWithTag("importQr").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("qrPicture") }
        platform.image = qrPng("https://example.com")
        onNodeWithTag("qrPicture").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("qrError") }
        for (p in parts) {
            platform.image = qrPng(p)
            onNodeWithTag("qrPicture").performClick()
            waitForIdle()
        }
        waitUntil(timeoutMillis = 10_000) { exists("rifle:$name (2)") }
        db.delete()
    }

    @Test
    fun photoOfARifle() = runDesktopComposeUiTest(412, 915) {
        val db = File.createTempFile("balcalc-test", ".db").apply { delete() }
        val platform = FakePlatform().apply { image = pngOf(1600, 1200) }
        setContent { BalCalcApp(Api(desktopEngine()), startup = { start(db.path) { desktopSeed() } }, platform = platform) }
        waitUntil("sample button", 30_000) { exists("sample") }
        onNodeWithTag("sample").performClick()
        waitUntil("first solution", 10_000) { exists("elevation") }
        onNodeWithTag("navArmory").performClick()
        val name = "Sample .308 Win"
        waitUntil("rifle in the list", 10_000) { exists("rifle:$name") }
        // The picture sits inside the clickable row: in the unmerged tree.
        fun thumb(): Boolean {
            announce()
            return onAllNodes(hasTestTag("thumb:$name"), useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty()
        }
        assertTrue(!thumb())

        onAllNodesWithText("Edit").onFirst().performClick()
        waitUntil("rifle editor", 10_000) { exists("photoChoose") }
        onNodeWithTag("photoChoose").performClick()
        waitUntil("picture in the editor", 10_000) { exists("photo") }
        shot("photo-editor")
        onNodeWithTag("save").performClick()
        waitUntil("thumbnail after saving", 10_000) { thumb() }
        shot("photo-list")

        // Opened again: the picture is there; removed: gone from the list too.
        onAllNodesWithText("Edit").onFirst().performClick()
        waitUntil("editor with the picture again", 10_000) { exists("photoRemove") }
        onNodeWithTag("photoRemove").performClick()
        onNodeWithTag("save").performClick()
        waitUntil("thumbnail gone", 10_000) { exists("rifle:$name") && !thumb() }
        db.delete()
    }

    @Test
    fun themes() = runDesktopComposeUiTest(412, 915) {
        val db = startWithSample()
        setRange(600)
        for ((label, name) in listOf("Dark" to "dark", "Night (red)" to "night", "Light" to "light")) {
            onNodeWithTag("navSettings").performClick()
            waitUntil(timeoutMillis = 10_000) { exists("theme") }
            onNodeWithTag("theme").performClick()
            onAllNodesWithText(label).onLast().performClick()
            waitUntil(timeoutMillis = 10_000) { testApi!!.let { api -> kotlinx.coroutines.runBlocking { api.call("state") } }.toString().contains("\"theme\":\"$name\"") }
            onNodeWithTag("navSolution").performClick()
            waitUntil(timeoutMillis = 10_000) { exists("elevation") }
            shot("theme-$name")
        }
        onNodeWithTag("navSettings").performClick()
        onNodeWithTag("keepScreenOn").performClick()
        waitUntil(timeoutMillis = 10_000) { testApi!!.let { api -> kotlinx.coroutines.runBlocking { api.call("state") } }.toString().contains("\"keepScreenOn\":true") }
        db.delete()
    }

    @Test
    fun situationsSwitchConditions() = runDesktopComposeUiTest(412, 915) {
        val db = startWithSample()
        setRange(650)
        val far = elevation()

        onNodeWithTag("situations").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("saveSituation") }
        onNodeWithTag("saveSituation").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("situationError") }
        assertEquals("Enter a name for the situation.", shown("situationError"))
        onNodeWithTag("situationName").performTextReplacement("Match")
        onNodeWithTag("saveSituation").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("applySituation:Match") }
        shot("situations")
        onAllNodesWithText("Close").onFirst().performClick()

        setRange(200)
        onNodeWithTag("situations").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("applySituation:Match") }
        onNodeWithTag("applySituation:Match").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("solvedFor:650") && !exists("saveSituation") }
        waitForIdle()
        assertEquals(far, elevation())
        db.delete()
    }

    @Test
    fun logHitsTrueAndShift() = runDesktopComposeUiTest(1100, 900) {
        val db = startWithSample()
        // The reticle card is there (plain crosshair: no reticle chosen).
        assertTrue(exists("reticle"))

        // Pretend the rifle needs 4 % more elevation than predicted at two ranges.
        for (r in listOf(500, 900)) {
            setRange(r)
            val predicted = elevation()
            onNodeWithTag("logHitSolution").performClick()
            waitUntil(timeoutMillis = 10_000) { exists("hitElevation") }
            type("hitElevation", (predicted * 1.04).fixed(2))
            type("hitNotes", "test")
            onNodeWithTag("saveHit").performClick()
            waitUntil(timeoutMillis = 10_000) { !exists("hitElevation") }
        }
        val before = elevation()

        onNodeWithTag("navArmory").performClick()
        onNodeWithTag("cartridgesTab").performClick()
        waitUntil(timeoutMillis = 10_000) { count("more:") > 0 }
        onAllNodes(androidx.compose.ui.test.SemanticsMatcher("more") {
            it.config.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.TestTag)?.startsWith("more:") == true
        }).onFirst().performClick()
        onNodeWithTag("shotLog").performClick()
        waitUntil(timeoutMillis = 10_000) { count("shotRow") == 2 }
        onNodeWithTag("computeTruing").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("truingResult") }
        shot("truing")
        onNodeWithTag("applyTruing").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("truingResult") }

        onNodeWithTag("navSolution").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") && elevation() > before }
        assertTrue(elevation() > before) // slower bullet: more elevation

        // Point-of-impact shift of this cartridge: 3 cm high at the 100 m zero.
        setRange(100)
        val atZero = elevation()
        onNodeWithTag("navArmory").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("offsetUp") } // the shot log is still open in the Rifles tab
        type("offsetUp", "3")
        onNodeWithTag("navSolution").performClick()
        // The tile shows the size of the correction (its direction is a word): 0.30 DOWN.
        waitUntil(timeoutMillis = 10_000) { exists("elevation") && kotlin.math.abs(elevation() - kotlin.math.abs(atZero - 0.3)) < 0.02 }
        db.delete()
    }

    @Test
    fun warningsAndDensityAltitude() = runDesktopComposeUiTest(412, 915) {
        val db = startWithSample()
        assertTrue(!exists("warnings")) // the sample at its zero air: nothing to warn about

        // Much warmer than the zero: a warning on the solution screen.
        onNodeWithTag("navConditions").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("temperature") }
        type("temperature", "35")
        onNodeWithTag("navSolution").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("warning_zeroTemperature") }
        shot("warnings")
        val warm = elevation()

        // Density altitude instead of pressure: thinner air, less elevation.
        onNodeWithTag("navConditions").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("useDensityAltitude") }
        onNodeWithTag("useDensityAltitude").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("densityAltitude") }
        type("densityAltitude", "3000")
        shot("conditions-density-altitude")
        onNodeWithTag("navSolution").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") && elevation() < warm }
        db.delete()
    }

    @Test
    fun windZonesAndGust() = runDesktopComposeUiTest(412, 915) {
        val db = startWithSample()
        setRange(800)
        val calm = shown("windage")
        assertTrue(!exists("windageGust"))

        // Gusts of 6 m/s: the windage tile shows the second correction.
        onNodeWithTag("navConditions").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("windGust") }
        onNodeWithTag("windGust").performScrollTo()
        type("windGust", "6")
        onNodeWithTag("navSolution").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("windageGust") }
        shot("wind-gust")

        // Calm near the shooter, 8 m/s further out, then a third zone.
        onNodeWithTag("navConditions").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("windZonesOn") }
        onNodeWithTag("windZonesOn").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("zoneSpeed1") }
        onNodeWithTag("zoneSpeed1").performScrollTo()
        type("zoneSpeed1", "8")
        onNodeWithTag("addZone").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("zoneSpeed2") }
        onNodeWithTag("zoneSpeed2").performScrollTo()
        shot("wind-zones")
        assertTrue(!exists("addZone")) // three zones in all
        onNodeWithTag("navSolution").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("windage") && shown("windage") != calm }
        onNodeWithTag("quickWind").performScrollTo()
        assertTrue(hasText("Zone 1 of 3", substring = true))
        db.delete()
    }

    @Test
    fun movingTarget() = runDesktopComposeUiTest(412, 915) {
        val db = startWithSample()
        setRange(500)
        assertTrue(!exists("lead"))

        // 15 km/h to the right (the default direction): lead to the right.
        onNodeWithTag("targetSpeed").performScrollTo()
        type("targetSpeed", "15")
        waitUntil(timeoutMillis = 10_000) { exists("lead") && shown("lead").contains("RIGHT") }
        onNodeWithTag("movesLeft").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { shown("lead").contains("LEFT") }
        onNodeWithTag("movingTarget").performScrollTo()
        shotOf("movingTarget", "moving-target")

        // The stopwatch: 5 m in about half a second is about 36 km/h.
        onNodeWithTag("stopwatch").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("stopwatchDistance") }
        type("stopwatchDistance", "5")
        onNodeWithTag("stopwatchToggle").performClick()
        Thread.sleep(500)
        onNodeWithTag("stopwatchToggle").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("stopwatchResult") }
        onNodeWithTag("stopwatchApply").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("stopwatchToggle") }
        val kmh = shown("targetSpeed").toDouble()
        assertTrue(kmh in 15.0..40.0, "speed $kmh")

        // The range card gets a lead column.
        onNodeWithTag("navTable").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rangeTable") }
        assertTrue(hasText("Lead", substring = true))
        shot("table-lead")
        db.delete()
    }

    @Test
    fun chartsAndCompare() = runDesktopComposeUiTest(1100, 760) {
        val db = startWithSample()
        // A second, faster load of the sample cartridge (through the core, as
        // the cartridge editor would); the sample stays chosen.
        val api = testApi!!
        kotlinx.coroutines.runBlocking {
            val st = api.call("state") as JsonObject
            val rifle = st.getValue("currentRifleId")
            val first = st.getValue("currentCartridgeId")
            val form = api.call("cartridgeForm", buildJsonObject { put("id", first) }) as JsonObject
            val hot = JsonObject(form + ("cartridgeId" to JsonPrimitive(0)) + ("name" to JsonPrimitive("Hot load")) + ("muzzleVelocity" to JsonPrimitive(850.0)))
            api.call("saveCartridge", buildJsonObject { put("form", hot) })
            api.call("select", buildJsonObject { put("rifleId", rifle); put("cartridgeId", first) })
        }
        onNodeWithTag("navTable").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("chartTab") }
        onNodeWithTag("chartTab").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("chart") }

        // Velocity instead of the trajectory.
        onNodeWithTag("chartQuantity").performClick()
        onAllNodesWithText("Velocity").onLast().performClick()
        waitUntil(timeoutMillis = 10_000) { onAllNodesWithText("Velocity").fetchSemanticsNodes().size == 1 } // the menu closed, the field shows it

        // Compared with the faster load.
        onNodeWithTag("compare").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("compareCartridge") && hasText("Hot load") }
        onNodeWithTag("compareAdd").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("compared0") }
        assertTrue(hasText("Hot load", substring = true))
        shot("chart-compare")

        // Removing it leaves one line.
        onNodeWithTag("compared0").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("compared0") }
        db.delete()
    }

    @Test
    fun dsfByHandAndFromTheLog() = runDesktopComposeUiTest(1100, 1000) {
        val db = startWithSample()
        setRange(1300)
        val plain = elevation()
        fun openShotLog() {
            onNodeWithTag("navArmory").performClick()
            if (exists("dsfAdd")) return // still open from before
            onNodeWithTag("cartridgesTab").performClick()
            waitUntil(timeoutMillis = 10_000) { count("more:") > 0 }
            onAllNodes(androidx.compose.ui.test.SemanticsMatcher("more") {
                it.config.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.TestTag)?.startsWith("more:") == true
            }).onFirst().performClick()
            onNodeWithTag("shotLog").performClick()
            waitUntil(timeoutMillis = 10_000) { exists("dsfAdd") }
        }

        // By hand: one point, 10 % more drag at every speed.
        openShotLog()
        onNodeWithTag("dsfAdd").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("dsfFactor0") }
        type("dsfFactor0", "1.1")
        onNodeWithTag("dsfSave").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("dsfSave") }
        onNodeWithTag("navSolution").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") && elevation() > plain + 0.3 }

        // The bullet "really" has more drag in the transonic part (set through
        // the core); log the hits it makes.
        val api = testApi!!
        kotlinx.coroutines.runBlocking {
            api.call("setDsf", buildJsonObject {
                put("points", kotlinx.serialization.json.buildJsonArray {
                    add(buildJsonObject { put("mach", 1.4); put("factor", 1.0) })
                    add(buildJsonObject { put("mach", 0.9); put("factor", 1.12) })
                })
            })
        }
        var truth = 0.0
        for (r in listOf(900, 1100, 1300)) {
            setRange(r)
            truth = elevation()
            onNodeWithTag("logHitSolution").performClick()
            waitUntil(timeoutMillis = 10_000) { exists("hitElevation") }
            type("hitElevation", truth.fixed(2))
            onNodeWithTag("saveHit").performClick()
            waitUntil(timeoutMillis = 10_000) { !exists("hitElevation") }
        }

        // Forget the table, fit it from the log.
        openShotLog()
        onNodeWithTag("dsfReset").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("dsfNone") }
        onNodeWithTag("dsfFit").performScrollTo().performClick()
        waitUntil(timeoutMillis = 20_000) { exists("dsfFitPoints") }
        onNodeWithTag("dsfFitPoints").performScrollTo()
        shot("dsf")
        onNodeWithTag("dsfApply").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("dsfFitPoints") }

        onNodeWithTag("navSolution").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") && kotlin.math.abs(elevation() - truth) <= 0.03 }
        db.delete()
    }

    @Test
    fun bcCalculator() = runDesktopComposeUiTest(1100, 1000) {
        val db = startWithSample()
        setRange(800)
        val before = elevation()
        onNodeWithTag("navArmory").performClick()
        onNodeWithTag("cartridgesTab").performClick()
        waitUntil(timeoutMillis = 10_000) { count("more:") > 0 }
        onAllNodes(androidx.compose.ui.test.SemanticsMatcher("more") {
            it.config.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.TestTag)?.startsWith("more:") == true
        }).onFirst().performClick()
        onNodeWithTag("shotLog").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bcCalc") }

        // A hit at the current range with the current correction: the BC it has.
        onNodeWithTag("bcCalc").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bcCalculate") }
        onNodeWithTag("bcModeHit").performClick()
        onNodeWithTag("bcCalculate").performClick()
        waitUntil(timeoutMillis = 20_000) { exists("bcResult") }
        // The sample bullet is a library SMK with Sierra's G1 BCs (about 0.50).
        assertTrue(Regex("""BC G1: 0\.(49|50)\d""").matches(shown("bcResult")), shown("bcResult"))

        // Two chronographs: 790 and 720 m/s 100 m apart, a lower BC (G1 ~0.42); written in.
        onNodeWithText("Two chronographs").performClick()
        type("bcVNear", "790")
        type("bcVFar", "720")
        onNodeWithTag("bcCalculate").performClick()
        waitUntil(timeoutMillis = 20_000) { exists("bcResult") }
        shot("bc-calculator")
        onNodeWithTag("bcSave").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("bcCalculate") }

        onNodeWithTag("navSolution").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") && elevation() != before }
        assertTrue(elevation() > before) // more drag, more elevation
        db.delete()
    }

    @Test
    fun hitChance() = runDesktopComposeUiTest(412, 915) {
        val db = startWithSample()
        setRange(600)
        onNodeWithTag("navTable").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("hitTab") }
        onNodeWithTag("hitTab").performClick()
        waitUntil(timeoutMillis = 20_000) { exists("hitChance") }
        fun chance() = shown("hitChance").removeSuffix(" %").toInt()
        val before = chance()
        assertTrue(before in 1..100, "$before")
        assertTrue(exists("shotsToHit"))
        shot("hit-chance")

        // A rifle that shoots 3 MOA groups hits less often.
        onNodeWithTag("errGroup").performScrollTo()
        type("errGroup", "3")
        onNodeWithTag("hitChance").performScrollTo()
        waitUntil(timeoutMillis = 20_000) { exists("hitChance") && chance() < before }
        onNodeWithTag("part:dispersion").performScrollTo()
        shot("hit-chance-sources")
        db.delete()
    }

    @Test
    fun groupFromAPhoto() = runDesktopComposeUiTest(412, 1000) {
        // A blank 400 x 400 target: 10 px per cm, as the scale below says.
        val png = java.io.ByteArrayOutputStream().also { out ->
            val img = java.awt.image.BufferedImage(400, 400, java.awt.image.BufferedImage.TYPE_INT_RGB)
            val g = img.createGraphics()
            g.color = java.awt.Color.WHITE
            g.fillRect(0, 0, 400, 400)
            g.dispose()
            ImageIO.write(img, "png", out)
        }.toByteArray()
        val db = File.createTempFile("balcalc-test", ".db").apply { delete() }
        val platform = FakePlatform().apply { image = png }
        setContent { BalCalcApp(Api(desktopEngine()), startup = { start(db.path) { desktopSeed() } }, platform = platform) }
        waitUntil(timeoutMillis = 30_000) { exists("sample") }
        onNodeWithTag("sample").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") }

        onNodeWithTag("navArmory").performClick()
        onNodeWithTag("cartridgesTab").performClick()
        waitUntil(timeoutMillis = 10_000) { count("more:") > 0 }
        onAllNodes(androidx.compose.ui.test.SemanticsMatcher("more") {
            it.config.getOrNull(androidx.compose.ui.semantics.SemanticsProperties.TestTag)?.startsWith("more:") == true
        }).onFirst().performClick()
        onNodeWithTag("shotLog").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("openGroup") }
        onNodeWithTag("openGroup").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("groupPhoto") }
        onNodeWithTag("groupPhoto").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("groupCanvas") }
        type("groupScaleCm", "20")

        // Image pixels -> taps on the canvas (the photo fills its width).
        fun tap(x: Double, y: Double) = onNodeWithTag("groupCanvas").performTouchInput {
            click(androidx.compose.ui.geometry.Offset((x / 400 * width).toFloat(), (y / 400 * width).toFloat()))
        }
        tap(100.0, 300.0); tap(300.0, 300.0) // scale: 200 px = 20 cm
        tap(200.0, 200.0)                   // aim
        tap(210.0, 180.0); tap(250.0, 180.0); tap(230.0, 160.0) // holes, 4 cm apart
        waitUntil(timeoutMillis = 10_000) { exists("groupEs") }
        shot("group")
        // Extreme spread 4 cm (+-a pixel of the taps) at 100 m: 0.40 MRAD.
        assertTrue(Regex("""Extreme spread: (3.9|4.0|4.1) cm""").containsMatchIn(shown("groupEs")), shown("groupEs"))
        assertTrue(shown("groupCentre").startsWith("Centre of the group: 2."), shown("groupCentre"))

        // Shot at the zero range: it can be this cartridge's zero shift; and the
        // precision goes to the hit chance.
        onNodeWithTag("groupZero").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("groupSaved") }
        onNodeWithTag("groupWez").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("groupSaved") }
        db.delete()
    }

    @Test
    fun phoneSensors() = runDesktopComposeUiTest(412, 915) {
        val db = File.createTempFile("balcalc-test", ".db").apply { delete() }
        val sensors = FakeSensors()
        setContent { BalCalcApp(Api(desktopEngine()), startup = { start(db.path) { desktopSeed() } }, platform = FakePlatform(sensors = sensors)) }
        waitUntil(timeoutMillis = 30_000) { exists("sample") }
        onNodeWithTag("sample").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") }
        onNodeWithTag("navConditions").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("fromBarometer") }

        // Barometer and location fill the air.
        onNodeWithTag("fromBarometer").performClick()
        waitUntil(timeoutMillis = 10_000) { shown("pressure") == "987.6" }
        onNodeWithTag("myLocation").performClick()
        waitUntil(timeoutMillis = 10_000) { shown("altitude") == "296" }
        assertTrue(shown("sensorNote").contains("296"), shown("sensorNote"))
        shot("sensors-air")

        // The rifle's angles from the phone lying on it.
        onNodeWithTag("measureAngles").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("tiltNow") }
        shot("sensors-tilt")
        onNodeWithTag("useAngles").performClick()
        waitUntil(timeoutMillis = 10_000) { shown("lookAngle") == "4.3" && shown("cantAngle") == "-1.3" }

        // Coriolis on: latitude from the fix, the azimuth from the compass
        // against true north (the location was taken).
        onNodeWithText("Account for Earth rotation", substring = true).performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("fromCompass") }
        assertEquals("49.8", shown("latitude"))
        onNodeWithTag("fromCompass").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("headingNow") }
        assertEquals("124°", shown("headingNow")) // 117.4 magnetic + 6.3 declination
        onNodeWithTag("useHeading").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("azimuth") && shown("azimuth") == "124" }
        db.delete()
    }

    @Test
    fun libraryBulletAndSettings() = runDesktopComposeUiTest(400, 820) {
        val db = startWithSample()
        val before = elevation()

        // A banded library bullet through its editor.
        onNodeWithTag("navArmory").performClick()
        onNodeWithText("Bullets").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("newBullet") }
        onNodeWithTag("newBullet").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bulletEditName") }
        type("bulletEditName", "Test bullet 175 HPBT")
        type("bulletMass", "175")
        type("bulletDiameter", "0.308")
        onNodeWithText("Different BCs by velocity (as published by Sierra)").performClick()
        type("bandVelocity0", "869")
        type("bandBc0", "0.505")
        onNodeWithTag("addBand").performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bandBc1") }
        type("bandBc1", "0.496")
        shot("bullet-editor")
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bullet:Test bullet 175 HPBT") }
        onNodeWithText("Back").performClick()

        // The sample cartridge switches to it.
        onNodeWithTag("cartridgesTab").performClick()
        waitUntil(timeoutMillis = 10_000) { count("cartridge:") == 1 }
        onAllNodesWithText("Edit").onFirst().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("chooseBullet") }
        onNodeWithTag("chooseBullet").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("search") }
        onNodeWithTag("search").performTextReplacement("Test bullet")
        waitUntil(timeoutMillis = 10_000) { exists("bullet:Test bullet 175 HPBT") }
        onNodeWithTag("bullet:Test bullet 175 HPBT").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bulletName") && shown("bulletName") == "Test bullet 175 HPBT" }
        onNodeWithTag("save").performClick()
        onNodeWithTag("navSolution").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") && elevation() != before }

        // MOA: the same correction, ×3.4377.
        val mrad = elevation()
        onNodeWithTag("navSettings").performClick()
        onNodeWithTag("unit:moa").performClick()
        onNodeWithTag("navSolution").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") && kotlin.math.abs(elevation() - mrad * 3.4377) < 0.02 }
        shot("settings-moa-solution")
        db.delete()
    }

    @Test
    fun reticleFromTheLibrary() = runDesktopComposeUiTest(1100, 1000) {
        val db = startWithSample()
        // Give the sample rifle each starter-library reticle in turn (through
        // the core, as the rifle editor would) and draw it with the hold.
        val api = testApi!!
        val reticles = kotlinx.coroutines.runBlocking { api.call("reticles") }.toString()
        val ids = Regex("\"id\":(\\d+)").findAll(reticles).map { it.groupValues[1] }.toList()
        assertTrue(ids.isNotEmpty())
        for (id in ids) {
            kotlinx.coroutines.runBlocking {
                val form = api.call("rifleForm", kotlinx.serialization.json.buildJsonObject { put("id", kotlinx.serialization.json.JsonPrimitive(1)) })
                val withReticle = kotlinx.serialization.json.JsonObject(form.let { it as kotlinx.serialization.json.JsonObject } + ("reticleId" to kotlinx.serialization.json.JsonPrimitive(id.toLong())))
                api.call("saveRifle", kotlinx.serialization.json.buildJsonObject { put("form", withReticle) })
            }
            // Any edit refreshes the screen's solution.
            setRange(600 + id.toInt())
            waitUntil(timeoutMillis = 10_000) { exists("reticle") }
            shot("reticle-$id")
        }
        db.delete()
    }

    private fun ComposeUiTest.hasText(text: String, substring: Boolean = false): Boolean {
        announce()
        return onAllNodesWithText(text, substring = substring).fetchSemanticsNodes().isNotEmpty()
    }

    private fun ComposeUiTest.chooseLanguage(name: String) {
        onNodeWithTag("navSettings").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("language") }
        onNodeWithTag("language").performClick()
        onAllNodesWithText(name).onLast().performClick()
    }

    @Test
    fun languages() = runDesktopComposeUiTest(400, 820) {
        val db = startWithSample()
        try {
            chooseLanguage("Українська")
            waitUntil(timeoutMillis = 10_000) { hasText("Рішення") } // the bottom bar
            shot("uk-settings")
            onNodeWithTag("navSolution").performClick()
            waitUntil(timeoutMillis = 10_000) { exists("elevation") }
            shot("uk-solution")

            // A message of the core, in Ukrainian: a cartridge without a name.
            onNodeWithTag("navArmory").performClick()
            onNodeWithTag("cartridgesTab").performClick()
            onNodeWithTag("new").performClick()
            onNodeWithTag("newEmptyCartridge").performClick()
            waitUntil(timeoutMillis = 10_000) { exists("save") }
            onNodeWithTag("save").performClick()
            waitUntil(timeoutMillis = 10_000) { exists("formError") }
            kotlin.test.assertEquals("Введіть назву набою.", shown("formError"))
            onNodeWithText("Скасувати").performClick()

            chooseLanguage("Русский")
            waitUntil(timeoutMillis = 10_000) { hasText("Решение") }
            shot("ru-settings")
        } finally {
            // The language is process-wide: back to the system's for the other tests.
            chooseLanguage("English")
            waitUntil(timeoutMillis = 10_000) { hasText("Solve") }
            db.delete()
        }
    }

    @Test
    fun phone() = run(412, 915, "phone") // Samsung S26 Ultra and most large phones, dp

    @Test
    fun commonPhone() = run(360, 780, "common") // the most common Android width

    @Test
    fun largeText() = run(412, 915, "large-text", fontScale = 1.3f) // Settings > Font size, large

    @Test
    fun landscape() = run(915, 412, "landscape")

    @Test
    fun smallPhone() = run(320, 640, "small")

    @Test
    fun desktop() = run(1100, 760, "desktop")
}
