package org.vetalguru.balcalc

import androidx.compose.ui.semantics.getOrNull
import androidx.compose.ui.semantics.SemanticsActions
import androidx.compose.ui.test.ComposeUiTest
import androidx.compose.ui.test.SemanticsNodeInteraction
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.assertIsEnabled
import androidx.compose.ui.test.assertIsOff
import androidx.compose.ui.test.assertIsOn
import androidx.compose.ui.test.assertIsSelected
import androidx.compose.ui.test.hasSetTextAction
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.hasText as withText
import androidx.compose.ui.test.onAllNodesWithContentDescription
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.onLast
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performImeAction
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performSemanticsAction
import androidx.compose.ui.test.performTextReplacement
import androidx.compose.ui.test.runDesktopComposeUiTest
import java.io.File
import kotlin.random.Random
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.double
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.long
import kotlinx.serialization.json.put
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.core.NamedText
import org.vetalguru.balcalc.core.desktopEngine
import org.vetalguru.balcalc.core.desktopSeed

/*
 * The Rifles tab and the library behind it, control by control: the two
 * lists and their menus, the rifle, cartridge and bullet editors, the
 * library pickers, pictures and QR codes. Each test checks what a control
 * did: the list, the core's records, a file saved, a dialog closed.
 */

/** A phone with a camera: QR codes and photos it "takes" are canned. */
private class CameraPlatform(val fake: FakePlatform) : Platform by fake, Camera {
    var qr: String? = null
    var photo: ByteArray? = null
    override val camera: Camera get() = this
    override suspend fun scanQr() = qr
    override suspend fun takePhoto() = photo
}

@OptIn(ExperimentalTestApi::class)
class ArmoryUiTest {
    private val sampleRifle = "Sample .308 Win"
    private val sampleCartridge = "Sample SMK 175 gr"

    // ---- Helpers ------------------------------------------------------------

    private fun TestApp.state(): JsonObject = call("state").jsonObject

    /** A form from the core: "rifleForm", "cartridgeForm", "bulletForm". */
    private fun TestApp.form(method: String, id: Long): JsonObject = call(method, buildJsonObject { put("id", id) }).jsonObject

    /** The library bullet called [name], or null. */
    private fun TestApp.bulletId(name: String): Long? =
        call("libraryBullets", buildJsonObject { put("filter", name) }).jsonArray
            .map { it.jsonObject }.firstOrNull { it.str("name") == name }?.long("id")

    private fun TestApp.rifleNames() = state().getValue("rifles").jsonArray.map { it.jsonObject.str("name") }
    private fun TestApp.cartridgeNames() = state().getValue("cartridges").jsonArray.map { it.jsonObject.str("name") }

    private fun JsonObject.num(key: String) = getValue(key).jsonPrimitive.double
    private fun JsonObject.long(key: String) = getValue(key).jsonPrimitive.long
    private fun JsonObject.str(key: String) = getValue(key).jsonPrimitive.content

    /** Types into the input labelled [label] (fields without a tag) and commits it. */
    private fun ComposeUiTest.typeInto(label: String, value: String, commit: Boolean = true) {
        val field = onNode(withText(label) and hasSetTextAction())
        field.performScrollTo()
        field.performTextReplacement(value)
        if (commit) field.performImeAction() // a text area has no Done
        waitForIdle()
    }

    /**
     * Presses a button by its click action, not at its coordinates: in an
     * editor that is still settling (a line appearing above it) a click at
     * the old place can miss.
     */
    private fun SemanticsNodeInteraction.press() {
        performScrollTo()
        performSemanticsAction(SemanticsActions.OnClick)
    }

    /** Picks [option] in the drop-down labelled [label]. */
    private fun ComposeUiTest.choose(label: String, option: String) {
        onNode(withText(label)).performScrollTo()
        waitForIdle()
        onNode(withText(label)).performClick()
        waitUntil(timeoutMillis = 10_000) { onAllNodesWithText(option).fetchSemanticsNodes().size > 1 || hasText(option) }
        onAllNodesWithText(option).onLast().performClick()
        waitForIdle()
    }

    /** The armory lists (rifles shown). */
    private fun ComposeUiTest.openArmory() {
        onNodeWithTag("navArmory").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("new") }
    }

    private fun ComposeUiTest.openMenu(title: String) {
        waitUntil(timeoutMillis = 10_000) { exists("more:$title") }
        onNodeWithTag("more:$title").performClick()
        waitForIdle()
    }

    /** The app with [platform] (one with a camera); startApp takes only the fake. */
    private fun ComposeUiTest.startWith(platform: CameraPlatform): TestApp {
        val db = File.createTempFile("balcalc-test", ".db").apply { delete(); deleteOnExit() }
        val api = Api(desktopEngine())
        setContent { BalCalcApp(api, startup = { start(db.path) { desktopSeed() } }, platform = platform) }
        waitUntil(timeoutMillis = 30_000) { exists("sample") }
        onNodeWithTag("sample").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") }
        return TestApp(api, db, platform.fake)
    }

    /** A new rifle named [name] through the editor; back on the lists. */
    private fun ComposeUiTest.newRifle(name: String, caliber: String = ".308 Win") {
        onNodeWithTag("riflesTab").performClick()
        onNodeWithTag("new").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifleName") }
        type("rifleName", name)
        type("rifleCaliber", caliber)
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifle:$name") }
    }

    /** The bullet library, from the lists. */
    private fun ComposeUiTest.openBullets() {
        onNodeWithText("Bullets").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("newBullet") }
    }

    /** A new library bullet with one BC; back on the bullet list. */
    private fun ComposeUiTest.newBullet(name: String) {
        onNodeWithTag("newBullet").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bulletEditName") }
        type("bulletEditName", name)
        type("bulletMass", "150")
        type("bulletDiameter", "0.308")
        type("bulletBc", "0.4")
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("newBullet") }
    }

    /** The bullet list filtered down to [name]. */
    private fun ComposeUiTest.findBullet(name: String) {
        onNodeWithTag("search").performTextReplacement(name)
        waitUntil(timeoutMillis = 10_000) { exists("bullet:$name") && count("bullet:") == 1 }
    }

    // ---- The lists ----------------------------------------------------------

    /** No rifle and no cartridge yet: each tab says what to do instead of a blank page; the tabs switch. */
    @Test
    fun emptyListsExplainAndTabsSwitch() = runDesktopComposeUiTest(412, 915) {
        startApp(sample = false)
        openArmory()
        waitUntil(timeoutMillis = 10_000) { hasText("No rifles yet", substring = true) }
        assertTrue(!exists("rifleList"))
        onNodeWithTag("cartridgesTab").performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("No cartridges yet", substring = true) }
        onNodeWithTag("cartridgesTab").assertIsSelected()
        onNodeWithTag("riflesTab").performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("No rifles yet", substring = true) }
        onNodeWithTag("riflesTab").assertIsSelected()
    }

    /** A tap on a rifle makes it current and turns to its cartridges; a tap on a cartridge goes to the solution. */
    @Test
    fun tappingRowsChoosesThePair() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        openArmory()
        newRifle("Second rifle")
        val second = app.state().long("currentRifleId") // a saved rifle becomes current
        assertTrue(second != 1L)

        onNodeWithTag("rifle:$sampleRifle").performClick()
        waitUntil(timeoutMillis = 10_000) { app.state().long("currentRifleId") == 1L }
        waitUntil(timeoutMillis = 10_000) { exists("cartridge:$sampleCartridge") }
        onNodeWithTag("cartridgesTab").assertIsSelected()

        // The cartridge: current, and the solution page opens.
        onNodeWithTag("cartridge:$sampleCartridge").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") && !exists("cartridgesTab") }
        val sample = app.state().getValue("cartridges").jsonArray.map { it.jsonObject }.single { it.str("name") == sampleCartridge }
        assertEquals(sample.long("id"), app.state().long("currentCartridgeId"))
    }

    /** "Export to file…" in a row's menu saves the record's shared file and says so. */
    @Test
    fun exportToFile() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        openArmory()
        openMenu(sampleRifle)
        onNodeWithText("Export to file…").performClick()
        waitUntil(timeoutMillis = 10_000) { app.platform.saved.isNotEmpty() }
        val file = app.platform.saved.single()
        assertTrue(file.name.endsWith(".json"), file.name)
        assertTrue(file.content.contains("balcalc-rifle") && file.content.contains(sampleRifle), file.content.take(200))
        waitUntil(timeoutMillis = 10_000) { hasText("Saved.") }

        // A cartridge too.
        onNodeWithTag("cartridgesTab").performClick()
        openMenu(sampleCartridge)
        onNodeWithText("Export to file…").performClick()
        waitUntil(timeoutMillis = 10_000) { app.platform.saved.size == 2 }
        assertTrue(app.platform.saved[1].content.contains("balcalc-cartridge"))
    }

    /** Import > From file…: the shared file becomes a second rifle; a cancelled pick changes nothing. */
    @Test
    fun importFromFile() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        val shared = app.call("exportJson", buildJsonObject { put("kind", "rifle"); put("id", 1) }).jsonObject.str("json")
        openArmory()

        // Nothing picked: no import.
        onAllNodesWithText("Import").onFirst().performClick()
        onNodeWithText("From file…").performClick()
        waitForIdle()
        assertEquals(listOf(sampleRifle), app.rifleNames())

        app.platform.openFiles = listOf(NamedText("rifle.json", shared))
        onAllNodesWithText("Import").onFirst().performClick()
        onNodeWithText("From file…").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifle:$sampleRifle (2)") }
        assertEquals(listOf(sampleRifle, "$sampleRifle (2)").sorted(), app.rifleNames().sorted())
        waitUntil(timeoutMillis = 10_000) { hasText("Imported.") }
    }

    /** Delete asks first: No keeps the rifle; Yes deletes it, and its list is empty again. */
    @Test
    fun deleteRifleAfterConfirming() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        openArmory()
        openMenu(sampleRifle)
        onNodeWithTag("delete").performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("Delete rifle?") }
        onNodeWithText("No").performClick()
        waitUntil(timeoutMillis = 10_000) { !hasText("Delete rifle?") }
        assertEquals(listOf(sampleRifle), app.rifleNames())
        assertTrue(exists("rifle:$sampleRifle"))

        openMenu(sampleRifle)
        onNodeWithTag("delete").performClick()
        onNodeWithTag("confirmDelete").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("rifle:$sampleRifle") }
        assertEquals(emptyList(), app.rifleNames())
        assertTrue(hasText("No rifles yet", substring = true))
    }

    // ---- QR codes -----------------------------------------------------------

    /** A rifle too big for one code: parts, stepped through with › and ‹, read back one picture each. */
    @Test
    fun qrCodeInParts() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        openArmory()
        // Random letters hardly compress: the shared file needs several codes.
        // In the caliber: the name is the dialog's title and must stay short.
        val random = Random(3)
        val name = "Long rifle"
        newRifle(name, caliber = (1..1200).map { 'a' + random.nextInt(26) }.joinToString(""))

        openMenu(name)
        onNodeWithTag("showQr").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("qrPart") }
        val total = shown("qrPart").substringAfter(" of ").toInt()
        assertTrue(total > 1, shown("qrPart"))
        assertEquals("Part 1 of $total", shown("qrPart"))
        onNodeWithTag("qrNext").performClick()
        waitUntil(timeoutMillis = 10_000) { shown("qrPart") == "Part 2 of $total" }
        onNodeWithContentDescription("Previous part").performClick()
        waitUntil(timeoutMillis = 10_000) { shown("qrPart") == "Part 1 of $total" }
        onNodeWithContentDescription("Previous part").assertIsNotEnabled() // the first part: nothing before it
        onNodeWithText("Close").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("qrCode") }

        // Back in, part by part: the dialog counts them.
        val shared = app.call("exportJson", buildJsonObject {
            put("kind", "rifle"); put("id", app.state().long("currentRifleId"))
        }).jsonObject.str("json")
        val parts = QrShare.parts(shared)
        assertEquals(total, parts.size)
        onAllNodesWithText("Import").onFirst().performClick()
        onNodeWithTag("importQr").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("qrPicture") }
        app.platform.image = qrPng(parts[0])
        onNodeWithTag("qrPicture").performClick()
        waitForIdle()
        for (p in parts.drop(1)) {
            app.platform.image = qrPng(p)
            onNodeWithTag("qrPicture").performClick()
            waitForIdle()
        }
        waitUntil(timeoutMillis = 10_000) { exists("rifle:$name (2)") && !exists("qrPicture") }
    }

    /** Reading a code in parts: after the first one the dialog says how many are in. */
    @Test
    fun qrImportShowsProgress() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        openArmory()
        val random = Random(3)
        newRifle("Long rifle", caliber = (1..1200).map { 'a' + random.nextInt(26) }.joinToString(""))
        val shared = app.call("exportJson", buildJsonObject {
            put("kind", "rifle"); put("id", app.state().long("currentRifleId"))
        }).jsonObject.str("json")
        val parts = QrShare.parts(shared)
        assertTrue(parts.size > 1)
        onAllNodesWithText("Import").onFirst().performClick()
        onNodeWithTag("importQr").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("qrPicture") }
        assertTrue(!exists("qrProgress"))
        app.platform.image = qrPng(parts[0])
        onNodeWithTag("qrPicture").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("qrProgress") }
        assertEquals("1 of ${parts.size} parts read: scan the next one.", shown("qrProgress"))
    }

    /** Reading QR codes: a cancelled pick and a picture without a code leave the dialog open; Cancel closes it. */
    @Test
    fun qrImportProblemsAndCancel() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        openArmory()
        onAllNodesWithText("Import").onFirst().performClick()
        onNodeWithTag("importQr").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("qrPicture") }
        assertTrue(!exists("qrScan")) // no camera on this desktop

        // Cancelled: nothing happens.
        app.platform.image = null
        onNodeWithTag("qrPicture").performClick()
        waitForIdle()
        assertTrue(!exists("qrError"))

        // A plain picture: no code in it.
        app.platform.image = pngOf(200, 200)
        onNodeWithTag("qrPicture").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("qrError") }
        assertEquals("No Holdmark QR code found.", shown("qrError"))

        onNodeWithText("Cancel").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("qrPicture") }
        assertEquals(listOf(sampleRifle), app.rifleNames())
    }

    /** A code that is JSON but no rifle or cartridge: the core's refusal shows in the dialog, the app goes on. */
    @Test
    fun qrCodeThatIsNoRifle() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        openArmory()
        onAllNodesWithText("Import").onFirst().performClick()
        onNodeWithTag("importQr").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("qrPicture") }
        app.platform.image = qrPng("""{"format":"balcalc-nothing","version":1}""")
        onNodeWithTag("qrPicture").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("qrError") }
        assertTrue(shown("qrError").startsWith("Not a valid Holdmark file"), shown("qrError"))
        assertEquals(listOf(sampleRifle), app.rifleNames())

        // Still alive: Cancel closes the dialog, the tabs switch.
        onNodeWithText("Cancel").performClick()
        waitUntil(timeoutMillis = 10_000) { !exists("qrPicture") }
        onNodeWithTag("cartridgesTab").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridge:$sampleCartridge") }
    }

    /** Import > From clipboard with text that is no shared file: the core's refusal as a message, the app goes on. */
    @Test
    fun clipboardThatIsNoRifle() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        openArmory()
        app.platform.clipboardText = "hello"
        onAllNodesWithText("Import").onFirst().performClick()
        onNodeWithTag("importClipboard").performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("Not a valid Holdmark file", substring = true) }
        assertEquals(listOf(sampleRifle), app.rifleNames())
        onNodeWithTag("cartridgesTab").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridge:$sampleCartridge") }
    }

    /** With a camera: Scan reads a code and imports it; Take a photo gives a cartridge its picture. */
    @Test
    fun cameraScansAndPhotographs() = runDesktopComposeUiTest(412, 915) {
        val platform = CameraPlatform(FakePlatform())
        val app = startWith(platform)
        openArmory()

        // A scanned rifle code: one more rifle.
        platform.qr = app.call("exportJson", buildJsonObject { put("kind", "rifle"); put("id", 1) }).jsonObject.str("json")
        onAllNodesWithText("Import").onFirst().performClick()
        onNodeWithTag("importQr").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("qrScan") }
        onNodeWithTag("qrScan").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifle:$sampleRifle (2)") && !exists("qrScan") }

        // A photo of the cartridge, stored with it.
        platform.photo = pngOf(800, 600)
        onNodeWithTag("cartridgesTab").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridge:$sampleCartridge") }
        onAllNodesWithText("Edit").onFirst().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridgeName") }
        onNodeWithText("Take a photo").press()
        waitUntil(timeoutMillis = 10_000) { exists("photo") }
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridge:$sampleCartridge") }
        val id = app.state().long("currentCartridgeId")
        val stored = app.call("photo", buildJsonObject { put("kind", "cartridge"); put("id", id) }).jsonPrimitive.content
        assertTrue(stored.isNotEmpty())
        // Its thumbnail in the list (inside the clickable row: the unmerged tree).
        waitUntil(timeoutMillis = 10_000) {
            announce()
            onAllNodes(hasTestTag("thumb:$sampleCartridge"), useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty()
        }
    }

    // ---- The rifle editor ---------------------------------------------------

    /** Every field of the rifle editor reaches the core when saved with the Save at the bottom. */
    @Test
    fun rifleEditorSavesEveryField() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        openArmory()
        onAllNodesWithText("Edit").onFirst().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifleName") }
        assertTrue(hasText("Edit rifle"))

        type("rifleName", "Edited rifle")
        type("rifleCaliber", "6.5 Creedmoor")
        typeInto("Sight height over bore", "4.5")
        type("twist", "8")
        onNodeWithText("Left-hand twist").press()
        onNodeWithText("Left-hand twist").assertIsOn()
        choose("Turret units", "MOA")
        type("clickValue", "0.25")
        val reticles = app.call("reticles").jsonArray.map { it.jsonObject }
        val reticle = reticles.first()
        choose("Reticle", reticle.str("name"))
        choose("Focal plane", "Second (SFP)")
        typeInto("Magnification from", "5")
        type("magTo", "25")
        waitUntil(timeoutMillis = 10_000) { hasText("SFP: marks are true at") } // only for SFP
        typeInto("SFP: marks are true at", "20")
        type("zeroRange", "200")
        typeInto("Temperature when zeroed", "5")
        typeInto("Station pressure when zeroed", "950")
        typeInto("Altitude when zeroed", "500")
        typeInto("Humidity when zeroed", "80")

        // The Save at the bottom of the page.
        onAllNodesWithText("Save").onLast().press()
        waitUntil(timeoutMillis = 10_000) { exists("rifle:Edited rifle") }
        val f = app.form("rifleForm", 1)
        assertEquals("Edited rifle", f.str("name"))
        assertEquals("6.5 Creedmoor", f.str("caliber"))
        assertEquals(4.5, f.num("sightHeightCm"), 1e-9)
        assertEquals(8.0, f.num("twistIn"), 1e-9)
        assertEquals("true", f.str("twistLeft"))
        assertEquals("moa", f.str("clickUnits"))
        assertEquals(0.25, f.num("clickValue"), 1e-9)
        assertEquals(reticle.long("id"), f.long("reticleId"))
        assertEquals("sfp", f.str("focalPlane"))
        assertEquals(5.0, f.num("minMagnification"), 1e-9)
        assertEquals(25.0, f.num("maxMagnification"), 1e-9)
        assertEquals(20.0, f.num("sfpReferenceMagnification"), 1e-9)
        assertEquals(200.0, f.num("zeroRangeM"), 1e-9)
        assertEquals(5.0, f.num("zeroTemperatureC"), 1e-9)
        assertEquals(5.0, f.num("zeroPowderC"), 1e-9) // the powder was as warm as the air
        assertEquals(950.0, f.num("zeroPressureHpa"), 1e-9)
        assertEquals(500.0, f.num("zeroAltitudeM"), 1e-9)
        assertEquals(80.0, f.num("zeroHumidityPct"), 1e-9)
    }

    /** Cancel leaves the rifle as it was; a nameless rifle is refused with the core's error. */
    @Test
    fun rifleEditorCancelAndError() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        openArmory()
        onAllNodesWithText("Edit").onFirst().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifleName") }
        type("rifleName", "Not kept")
        onNodeWithText("Cancel").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifle:$sampleRifle") }
        assertEquals(sampleRifle, app.form("rifleForm", 1).str("name"))

        onNodeWithTag("new").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifleName") }
        assertTrue(hasText("New rifle"))
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("formError") }
        assertTrue(shown("formError").isNotEmpty())
        assertTrue(exists("rifleName")) // still in the editor
        assertEquals(listOf(sampleRifle), app.rifleNames())
    }

    /**
     * Second focal plane scopes from the library: the stated reference
     * magnification, else the highest; Back from a catalog changes nothing,
     * and a search that finds nothing says so.
     */
    @Test
    fun libraryScopesAndRiflesPickers() = runDesktopComposeUiTest(412, 915) {
        startApp()
        openArmory()
        onNodeWithTag("new").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("scopeFromLibrary") }

        // A scope without a stated reference: true at its highest power.
        onNodeWithTag("scopeFromLibrary").press()
        waitUntil(timeoutMillis = 10_000) { exists("search") }
        onNodeWithTag("search").performTextReplacement("K18i-2")
        waitUntil(timeoutMillis = 10_000) { exists("libraryScope:Kahles K18i-2 1-8x24i mrad0.1") }
        onNodeWithTag("libraryScope:Kahles K18i-2 1-8x24i mrad0.1").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("magTo") }
        assertEquals("8", shown("magTo"))
        assertEquals("8", shown(onNode(withText("SFP: marks are true at") and hasSetTextAction())))

        // One with its own reference.
        onNodeWithTag("scopeFromLibrary").press()
        waitUntil(timeoutMillis = 10_000) { exists("search") }
        onNodeWithTag("search").performTextReplacement("NX8 4-32")
        waitUntil(timeoutMillis = 10_000) { exists("libraryScope:Nightforce NX8 4-32x50 moa0.25") }
        onNodeWithTag("libraryScope:Nightforce NX8 4-32x50 moa0.25").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("clickValue") }
        assertEquals("0.25", shown("clickValue"))
        assertEquals("32", shown(onNode(withText("SFP: marks are true at") and hasSetTextAction())))

        // Back from the scopes, a search that finds nothing.
        onNodeWithTag("scopeFromLibrary").press()
        waitUntil(timeoutMillis = 10_000) { exists("search") }
        onNodeWithTag("search").performTextReplacement("no such scope at all")
        waitUntil(timeoutMillis = 10_000) { hasText("Nothing found.") }
        onNodeWithText("Back").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("clickValue") }
        assertEquals("0.25", shown("clickValue"))

        // The same from the rifles: the form is untouched.
        onNodeWithTag("rifleFromLibrary").press()
        waitUntil(timeoutMillis = 10_000) { exists("search") }
        onNodeWithText("Back").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("rifleName") }
        assertEquals("", shown("rifleName"))
    }

    private fun ComposeUiTest.shown(node: SemanticsNodeInteraction): String {
        announce()
        return node.fetchSemanticsNode().config
            .getOrNull(androidx.compose.ui.semantics.SemanticsProperties.EditableText)?.text.orEmpty()
    }

    // ---- The cartridge editor -----------------------------------------------

    /** Every field of the cartridge editor reaches the core; Cancel keeps the old values. */
    @Test
    fun cartridgeEditorSavesEveryField() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        openArmory()
        onNodeWithTag("cartridgesTab").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridge:$sampleCartridge") }
        val id = app.state().long("currentCartridgeId")

        onAllNodesWithText("Edit").onFirst().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridgeName") }
        assertTrue(hasText("Edit cartridge"))
        type("cartridgeName", "Not kept")
        onNodeWithText("Cancel").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridge:$sampleCartridge") }
        assertEquals(sampleCartridge, app.form("cartridgeForm", id).str("name"))

        onAllNodesWithText("Edit").onFirst().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridgeName") }
        // The sample's bullet is the library's: "Edit as own" first.
        assertTrue(shown("bulletSource").startsWith("From the library: "), shown("bulletSource"))
        onNodeWithText("Edit as own").press()
        waitUntil(timeoutMillis = 10_000) { shown("bulletSource") == "Own bullet of this cartridge" }
        type("cartridgeName", "Edited load")
        type("cartridgeCaliber", ".308 Winchester")
        type("bulletName", "Own 168")
        choose("Drag model", "G1")
        type("bc", "0.462")
        type("mass", "168")
        type("diameter", "0.308")
        typeInto("Length (for spin drift)", "1.215")
        type("muzzleVelocity", "800")
        typeInto("Measured at powder temperature", "20")
        typeInto("Powder sensitivity", "0.1")
        onAllNodesWithText("Save").onLast().press()
        waitUntil(timeoutMillis = 10_000) { exists("cartridge:Edited load") }

        val f = app.form("cartridgeForm", id)
        assertEquals("Edited load", f.str("name"))
        assertEquals(".308 Winchester", f.str("caliber"))
        assertEquals("Own 168", f.str("bulletName"))
        assertEquals("G1", f.str("dragTable"))
        assertEquals(0.462, f.num("bc"), 1e-9)
        assertEquals(168.0, f.num("massGr"), 1e-6)
        assertEquals(0.308, f.num("diameterIn"), 1e-6)
        assertEquals(1.215, f.num("lengthIn"), 1e-6)
        assertEquals(800.0, f.num("muzzleVelocity"), 1e-6)
        assertEquals(20.0, f.num("powderReferenceC"), 1e-9)
        assertEquals(0.1, f.num("powderSensitivity"), 1e-9)
        assertEquals(0L, f.long("libraryBulletId"))
    }

    /**
     * A library bullet locks the bullet fields; "Edit as own" frees them and
     * the cartridge keeps its own copy. In the picker, Edit opens the bullet
     * instead of choosing it.
     */
    @Test
    fun libraryBulletLocksUntilEditedAsOwn() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        openArmory()
        openBullets()
        newBullet("Lock test 150")
        onNodeWithText("Back").performClick()
        onNodeWithTag("cartridgesTab").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridge:$sampleCartridge") }
        val id = app.state().long("currentCartridgeId")
        onAllNodesWithText("Edit").onFirst().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("chooseBullet") }

        // The picker: Edit edits, Cancel comes back to the picker, a tap chooses.
        onNodeWithTag("chooseBullet").press()
        waitUntil(timeoutMillis = 10_000) { exists("search") && hasText("Choose a bullet") }
        findBullet("Lock test 150")
        onAllNodesWithText("Edit").onLast().press()
        waitUntil(timeoutMillis = 10_000) { exists("bulletEditName") }
        assertEquals("Lock test 150", shown("bulletEditName"))
        onNodeWithText("Cancel").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bullet:Lock test 150") && hasText("Choose a bullet") }
        onNodeWithTag("bullet:Lock test 150").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bulletName") && shown("bulletName") == "Lock test 150" }
        assertEquals("From the library: Lock test 150", shown("bulletSource"))
        onNodeWithTag("bulletName").assertIsNotEnabled()
        assertEquals("0.4", shown("bc"))

        // Locked: a typed BC does not stick.
        type("bc", "0.6")
        waitForIdle()
        assertEquals("0.4", shown("bc"))

        // Own: editable, saved without the library link.
        onNodeWithText("Edit as own").press()
        waitUntil(timeoutMillis = 10_000) { shown("bulletSource") == "Own bullet of this cartridge" }
        onNodeWithTag("bulletName").assertIsEnabled()
        type("bc", "0.6")
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridge:$sampleCartridge") }
        val f = app.form("cartridgeForm", id)
        assertEquals(0L, f.long("libraryBulletId"))
        assertEquals(0.6, f.num("bc"), 1e-9)
        assertEquals("Lock test 150", f.str("bulletName"))
    }

    // ---- The bullet library -------------------------------------------------

    /** Import in the bullet library: a bullet list and a file of an unknown kind, reported one per line. */
    @Test
    fun bulletLibraryImportsFiles() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        openArmory()
        openBullets()

        // Nothing picked: no report.
        onNodeWithText("Import").performClick()
        waitForIdle()
        assertTrue(!hasText("file(s) imported", substring = true))

        val list = """{"format":"balcalc-bullets","version":1,"bullets":[
            {"name":"Imported test 140","manufacturer":"Test","caliber":"6.5mm","weight_gr":140,"diameter_in":0.264,"g7":0.31}
        ]}"""
        app.platform.openFiles = listOf(NamedText("bullets.json", list), NamedText("notes.txt", "hello"))
        onNodeWithText("Import").performClick()
        waitUntil(timeoutMillis = 10_000) { hasText("1 file(s) imported.", substring = true) }
        assertTrue(hasText("notes.txt: Unknown file type: notes.txt", substring = true))
        onNodeWithText("OK").performClick()
        waitUntil(timeoutMillis = 10_000) { !hasText("1 file(s) imported.", substring = true) }
        assertTrue(app.bulletId("Imported test 140") != null)
        findBullet("Imported test 140")
    }

    /** Search narrows the list, "Nothing found." when it is empty; a tap opens the bullet; Back returns to the lists. */
    @Test
    fun bulletLibrarySearchEditAndBack() = runDesktopComposeUiTest(412, 915) {
        startApp()
        openArmory()
        openBullets()
        assertTrue(hasText("Bullet library"))
        waitUntil(timeoutMillis = 10_000) { count("bullet:") > 0 } // the list loads in the background
        onNodeWithTag("search").performTextReplacement("no bullet has this name")
        waitUntil(timeoutMillis = 10_000) { hasText("Nothing found.") && count("bullet:") == 0 }

        onNodeWithTag("search").performTextReplacement("")
        waitUntil(timeoutMillis = 10_000) { count("bullet:") > 0 }
        newBullet("Search test 155")
        findBullet("Search test 155")
        onNodeWithTag("bullet:Search test 155").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bulletEditName") }
        assertTrue(hasText("Edit bullet"))
        assertEquals("Search test 155", shown("bulletEditName"))
        onNodeWithText("Cancel").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("newBullet") }

        onNodeWithText("Back").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("riflesTab") }
    }

    /** Every field of the bullet editor reaches the library, the bottom Save included. */
    @Test
    fun bulletEditorSavesEveryField() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        openArmory()
        openBullets()
        onNodeWithTag("newBullet").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bulletEditName") }
        assertTrue(hasText("New bullet"))
        type("bulletEditName", "Field test 180")
        typeInto("Manufacturer", "Testworks")
        typeInto("Caliber, e.g. .308 Win", ".308")
        type("bulletMass", "180")
        type("bulletDiameter", "0.308")
        typeInto("Length (for spin drift)", "1.3")
        choose("Drag model", "G1")
        type("bulletBc", "0.52")
        typeInto("Notes, source of the data", "From the box", commit = false)
        onAllNodesWithText("Save").onLast().press()
        waitUntil(timeoutMillis = 10_000) { exists("newBullet") }

        val id = assertNotNullId(app.bulletId("Field test 180"))
        val f = app.form("bulletForm", id)
        assertEquals("Testworks", f.str("manufacturer"))
        assertEquals(".308", f.str("caliber"))
        assertEquals(180.0, f.num("massGr"), 1e-6)
        assertEquals(0.308, f.num("diameterIn"), 1e-6)
        assertEquals(1.3, f.num("lengthIn"), 1e-6)
        assertEquals("G1", f.str("dragTable"))
        assertEquals(0.52, f.num("bc"), 1e-9)
        assertEquals("From the box", f.str("notes"))
    }

    private fun assertNotNullId(id: Long?): Long = kotlin.test.assertNotNull(id)

    /** BC bands: the switch adds one, its cross removes one, removing the last turns banding off. */
    @Test
    fun bulletBandsAddAndRemove() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        openArmory()
        openBullets()
        onNodeWithTag("newBullet").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bulletEditName") }
        type("bulletEditName", "Band test 175")
        type("bulletMass", "175")
        type("bulletDiameter", "0.308")
        type("bulletBc", "0.5")
        val banded = "Different BCs by velocity (as published by Sierra)"
        onNodeWithText(banded).press()
        waitUntil(timeoutMillis = 10_000) { exists("bandBc0") }
        onNodeWithText(banded).assertIsOn()
        assertTrue(!exists("bulletBc"))
        assertEquals("0.5", shown("bandBc0")) // the one BC as the first band
        onNodeWithTag("addBand").press()
        onNodeWithTag("addBand").press()
        waitUntil(timeoutMillis = 10_000) { exists("bandBc2") }
        type("bandBc1", "0.49")
        type("bandBc2", "0.48")

        // The middle band out: the last moves up.
        onAllNodesWithContentDescription("Remove")[1].press()
        waitUntil(timeoutMillis = 10_000) { !exists("bandBc2") }
        assertEquals("0.48", shown("bandBc1"))
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("newBullet") }
        val id = assertNotNullId(app.bulletId("Band test 175"))
        val bands = app.form("bulletForm", id).getValue("bands").jsonArray.map { it.jsonObject.num("bc") }
        assertEquals(listOf(0.5, 0.48), bands)

        // Every band out: one BC again.
        findBullet("Band test 175")
        onNodeWithTag("bullet:Band test 175").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bandBc1") }
        onAllNodesWithContentDescription("Remove").onLast().press()
        onAllNodesWithContentDescription("Remove").onLast().press()
        waitUntil(timeoutMillis = 10_000) { exists("bulletBc") }
        onNodeWithText(banded).assertIsOff()
        type("bulletBc", "0.47")
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("newBullet") }
        assertTrue(app.form("bulletForm", id).getValue("bands").jsonArray.isEmpty())
        assertEquals(0.47, app.form("bulletForm", id).num("bc"), 1e-9)
    }

    /** Delete in the bullet editor: gone from the library; a bullet a cartridge uses is kept, with the reason. */
    @Test
    fun deleteBullet() = runDesktopComposeUiTest(412, 915) {
        val app = startApp()
        openArmory()
        openBullets()
        newBullet("Delete test 150")
        findBullet("Delete test 150")
        onNodeWithTag("bullet:Delete test 150").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bulletEditName") }
        onNodeWithText("Delete").press()
        waitUntil(timeoutMillis = 10_000) { exists("newBullet") }
        waitUntil(timeoutMillis = 10_000) { !exists("bullet:Delete test 150") }
        assertEquals(null, app.bulletId("Delete test 150"))

        // A bullet in use: the sample cartridge takes it.
        newBullet("Used test 150")
        onNodeWithText("Back").performClick()
        onNodeWithTag("cartridgesTab").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridge:$sampleCartridge") }
        onAllNodesWithText("Edit").onFirst().performClick()
        waitUntil(timeoutMillis = 10_000) { exists("chooseBullet") }
        onNodeWithTag("chooseBullet").press()
        findBullet("Used test 150")
        onNodeWithTag("bullet:Used test 150").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bulletName") && shown("bulletName") == "Used test 150" }
        onNodeWithTag("save").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("cartridge:$sampleCartridge") }

        openBullets()
        findBullet("Used test 150")
        onNodeWithTag("bullet:Used test 150").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("bulletEditName") }
        onNodeWithText("Delete").press()
        waitUntil(timeoutMillis = 10_000) { exists("formError") }
        assertEquals("This bullet is used by a cartridge and cannot be deleted.", shown("formError"))
        assertTrue(exists("bulletEditName")) // still open
        assertTrue(app.bulletId("Used test 150") != null)
    }
}
