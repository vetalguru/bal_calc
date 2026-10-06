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
        ImageIO.write(onRoot().captureToImage().toAwtImage(), "png", File(dir, "$name.png"))
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
        waitUntil(timeoutMillis = 10_000) { exists("elevation") && shown("range") == m.toString() }
        waitForIdle()
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
