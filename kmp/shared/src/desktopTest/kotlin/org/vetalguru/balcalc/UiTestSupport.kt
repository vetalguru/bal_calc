package org.vetalguru.balcalc

import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.semantics.getOrNull
import androidx.compose.ui.test.ComposeUiTest
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.SemanticsMatcher
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performImeAction
import androidx.compose.ui.test.performTextReplacement
import java.io.File
import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.core.desktopEngine
import org.vetalguru.balcalc.core.desktopSeed

/*
 * What the UI tests share: the app on a fresh database, and finding, reading
 * and typing into its nodes by test tag.
 */

/** The app under test: its core (to check what reached it) and its database file. */
class TestApp(val api: Api, val db: File, val platform: FakePlatform) {
    /** A call straight to the core, e.g. "state". */
    fun call(method: String, args: JsonObject = JsonObject(emptyMap())): JsonElement = runBlocking { api.call(method, args) }
}

/** The app on a fresh seeded database; with [sample] the sample rifle and cartridge are added. */
@OptIn(ExperimentalTestApi::class)
fun ComposeUiTest.startApp(sample: Boolean = true, platform: FakePlatform = FakePlatform()): TestApp {
    val db = File.createTempFile("balcalc-test", ".db").apply { delete(); deleteOnExit() }
    val api = Api(desktopEngine())
    setContent { BalCalcApp(api, startup = { start(db.path) { desktopSeed() } }, platform = platform) }
    waitUntil(timeoutMillis = 30_000) { exists("navSolution") }
    if (sample) {
        waitUntil(timeoutMillis = 30_000) { exists("sample") }
        onNodeWithTag("sample").performClick()
        waitUntil(timeoutMillis = 10_000) { exists("elevation") }
    }
    return TestApp(api, db, platform)
}

/**
 * State written from the core's worker thread reaches the test once its
 * snapshot changes are announced; announce them before every check.
 */
@OptIn(ExperimentalTestApi::class)
fun ComposeUiTest.announce() {
    androidx.compose.runtime.snapshots.Snapshot.sendApplyNotifications()
    waitForIdle()
}

@OptIn(ExperimentalTestApi::class)
fun ComposeUiTest.exists(tag: String): Boolean {
    announce()
    return onAllNodes(hasTestTag(tag)).fetchSemanticsNodes().isNotEmpty()
}

/** What a node shows: an input's content, or its text. */
@OptIn(ExperimentalTestApi::class)
fun ComposeUiTest.shown(tag: String): String {
    announce()
    val config = onNodeWithTag(tag).fetchSemanticsNode().config
    return config.getOrNull(SemanticsProperties.EditableText)?.text
        ?: config.getOrNull(SemanticsProperties.Text).orEmpty().joinToString("") { it.text }
}

@OptIn(ExperimentalTestApi::class)
fun ComposeUiTest.hasText(text: String, substring: Boolean = false): Boolean {
    announce()
    return onAllNodesWithText(text, substring = substring).fetchSemanticsNodes().isNotEmpty()
}

/** Types into a field and commits it; a wheel of the ring opens its own field ("<tag>Input"). */
@OptIn(ExperimentalTestApi::class)
fun ComposeUiTest.type(tag: String, value: String) {
    val editable = onNodeWithTag(tag).fetchSemanticsNode().config.getOrNull(SemanticsProperties.EditableText) != null
    val field = if (editable) tag else "${tag}Input"
    if (!editable) {
        onNodeWithTag(tag).performClick()
        waitUntil(timeoutMillis = 10_000) { exists(field) }
    }
    onNodeWithTag(field).performTextReplacement(value)
    onNodeWithTag(field).performImeAction()
    if (!editable) waitUntil(timeoutMillis = 10_000) { !exists(field) }
}

/** How many nodes have a tag starting with [prefix] (list rows: "rifle-1", "rifle-2"…). */
@OptIn(ExperimentalTestApi::class)
fun ComposeUiTest.count(prefix: String): Int {
    announce()
    return onAllNodes(SemanticsMatcher("tag $prefix*") {
        it.config.getOrNull(SemanticsProperties.TestTag)?.startsWith(prefix) == true
    }).fetchSemanticsNodes().size
}
