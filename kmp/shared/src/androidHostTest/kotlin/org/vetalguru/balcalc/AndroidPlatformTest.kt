package org.vetalguru.balcalc

import android.Manifest
import android.app.Activity
import android.app.Application
import android.appwidget.AppWidgetManager
import android.content.ClipData
import android.content.ComponentName
import android.content.ContentProvider
import android.content.ContentValues
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.database.Cursor
import android.database.MatrixCursor
import android.graphics.Bitmap
import android.graphics.Color
import android.net.Uri
import android.os.ParcelFileDescriptor
import android.provider.MediaStore
import android.provider.OpenableColumns
import androidx.activity.ComponentActivity
import androidx.test.core.app.ApplicationProvider
import java.io.File
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.async
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.runCurrent
import kotlinx.coroutines.test.runTest
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.GraphicsMode
import org.vetalguru.balcalc.core.NamedText

/** Documents with display names, as a cloud provider gives them: content://test.docs/<id>. */
class DocsProvider : ContentProvider() {
    val docs = mutableMapOf<String, Pair<String, File>>()

    fun add(id: String, name: String, file: File): Uri {
        docs[id] = name to file
        return Uri.parse("content://$AUTHORITY/$id")
    }

    override fun onCreate() = true
    override fun query(uri: Uri, projection: Array<String>?, selection: String?, args: Array<String>?, sort: String?): Cursor {
        val (name, _) = docs.getValue(uri.lastPathSegment!!)
        return MatrixCursor(arrayOf(OpenableColumns.DISPLAY_NAME)).apply { addRow(arrayOf(name)) }
    }
    override fun openFile(uri: Uri, mode: String): ParcelFileDescriptor =
        ParcelFileDescriptor.open(docs.getValue(uri.lastPathSegment!!).second, ParcelFileDescriptor.parseMode(mode))
    override fun getType(uri: Uri): String? = null
    override fun insert(uri: Uri, values: ContentValues?): Uri? = null
    override fun delete(uri: Uri, selection: String?, args: Array<String>?) = 0
    override fun update(uri: Uri, values: ContentValues?, selection: String?, args: Array<String>?) = 0

    companion object {
        const val AUTHORITY = "test.docs"
    }
}

@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE) // real JPEG encoding of camera pictures
class AndroidPlatformTest {
    @get:Rule
    val tmp = TemporaryFolder()

    private val app: Application = ApplicationProvider.getApplicationContext()

    private fun platform(): Pair<ComponentActivity, AndroidPlatform> {
        val (controller, platform) = startedActivity { AndroidPlatform(it) }
        return controller.get() to platform
    }

    /** The picker (or camera, or scanner) the code started last; fails when there is none. */
    private fun ComponentActivity.started(): Intent =
        assertNotNull(shadowOf(this).nextStartedActivityForResult, "nothing was started").intent

    /** The started picker answers: OK with [data], or cancelled when [data] is null. */
    private fun ComponentActivity.answer(request: Intent, data: Intent?) =
        shadowOf(this).receiveResult(request, if (data == null) Activity.RESULT_CANCELED else Activity.RESULT_OK, data)

    private fun List<NamedText>.pairs() = map { it.name to it.content }

    /** Starts [call] and lets it run up to the picker. */
    private fun <T> TestScope.begin(call: suspend () -> T): Deferred<T> = async { call() }.also { runCurrent() }

    // --- clipboard -----------------------------------------------------------

    /** What is copied is pasted back. */
    @Test
    fun clipboardRoundTrip() = runTest {
        val (_, p) = platform()
        p.copyText("BALCALC1:1:1:abc")
        assertEquals("BALCALC1:1:1:abc", p.pasteText())
    }

    /** An empty clipboard pastes nothing. */
    @Test
    fun emptyClipboardIsNull() = runTest {
        assertNull(platform().second.pasteText())
    }

    // --- widget --------------------------------------------------------------

    private val widgetComponent = ComponentName(ApplicationProvider.getApplicationContext<Context>().packageName,
        "org.vetalguru.balcalc.SolutionWidget")

    private fun widgetBroadcasts() = shadowOf(app).broadcastIntents.filter { it.action == AppWidgetManager.ACTION_APPWIDGET_UPDATE }

    /** A solution is kept in the widget's preferences; null clears them. */
    @Test
    fun publishKeepsTheLines() {
        val (activity, p) = platform()
        p.publish(SolutionLines("Tikka .308", "600 m", "↑ 3.4 mil", "→ 0.6 mil"))
        val prefs = activity.getSharedPreferences(AndroidPlatform.WIDGET_PREFS, Context.MODE_PRIVATE)
        assertEquals(
            mapOf("title" to "Tikka .308", "range" to "600 m", "elevation" to "↑ 3.4 mil", "windage" to "→ 0.6 mil"),
            prefs.all,
        )
        p.publish(null)
        assertTrue(prefs.all.isEmpty())
    }

    /** Without a widget on the home screen nothing is broadcast. */
    @Test
    fun noWidgetNoBroadcast() {
        platform().second.publish(SolutionLines("t", "r", "e", "w"))
        assertTrue(widgetBroadcasts().isEmpty())
    }

    /** With widgets placed, they are told to update: the widget's own component, all their ids. */
    @Test
    fun widgetsAreUpdated() {
        val widgets = shadowOf(AppWidgetManager.getInstance(app))
        widgets.bindAppWidgetId(7, widgetComponent)
        widgets.bindAppWidgetId(9, widgetComponent)
        widgets.bindAppWidgetId(11, ComponentName(app.packageName, "some.OtherWidget"))
        platform().second.publish(SolutionLines("t", "r", "e", "w"))
        val sent = widgetBroadcasts().single()
        assertEquals(widgetComponent, sent.component)
        assertEquals(setOf(7, 9), sent.getIntArrayExtra(AppWidgetManager.EXTRA_APPWIDGET_IDS)!!.toSet())
    }

    // --- files ---------------------------------------------------------------

    /** The save picker gets the name and type; the chosen file gets the text. */
    @Test
    fun saveTextWritesTheChosenFile() = runTest {
        val (activity, p) = platform()
        val target = tmp.newFile("chosen.json").apply { writeText("old, longer content to be truncated") }
        val saved = begin { p.saveText("rifle.json", """{"a":1}""") }
        val request = activity.started()
        assertEquals(Intent.ACTION_CREATE_DOCUMENT, request.action)
        assertEquals("application/json", request.type)
        assertEquals("rifle.json", request.getStringExtra(Intent.EXTRA_TITLE))
        activity.answer(request, Intent().setData(Uri.fromFile(target)))
        assertTrue(saved.await())
        assertEquals("""{"a":1}""", target.readText()) // "wt": the old content is gone
    }

    /** Cancelled save: false, nothing written. */
    @Test
    fun saveTextCancelledIsFalse() = runTest {
        val (activity, p) = platform()
        val saved = begin { p.saveText("rifle.json", "{}") }
        activity.answer(activity.started(), null)
        assertFalse(saved.await())
    }

    /** CSV and PNG each have their picker type; the bytes land in the chosen file. */
    @Test
    fun saveBytesUsesTheTypeOfTheFile() = runTest {
        val (activity, p) = platform()
        for ((mime, bytes) in listOf("text/csv" to "a,b\n1,2\n".encodeToByteArray(), "image/png" to byteArrayOf(1, 2, 3))) {
            val target = tmp.newFile()
            val saved = begin { p.saveBytes("card", mime, bytes) }
            val request = activity.started()
            assertEquals(mime, request.type)
            assertEquals("card", request.getStringExtra(Intent.EXTRA_TITLE))
            activity.answer(request, Intent().setData(Uri.fromFile(target)))
            assertTrue(saved.await())
            assertContentEquals(bytes, target.readBytes())
        }
    }

    /** A type without a picker is refused at once; a cancelled save is false. */
    @Test
    fun saveBytesRefusedOrCancelled() = runTest {
        val (activity, p) = platform()
        assertFalse(p.saveBytes("x.pdf", "application/pdf", byteArrayOf(1)))
        assertNull(shadowOf(activity).nextStartedActivityForResult)
        val saved = begin { p.saveBytes("card.csv", "text/csv", byteArrayOf(1)) }
        activity.answer(activity.started(), null)
        assertFalse(saved.await())
    }

    /** One file opened: any type is offered; the name comes from the provider's display name. */
    @Test
    fun openOneText() = runTest {
        val (activity, p) = platform()
        val docs = Robolectric.setupContentProvider(DocsProvider::class.java, DocsProvider.AUTHORITY)
        val uri = docs.add("42", "308 Win.ammo", tmp.newFile().apply { writeText("ammo text") })
        val opened = begin { p.openTexts(listOf("ammo"), multiple = false) }
        val request = activity.started()
        assertEquals(Intent.ACTION_OPEN_DOCUMENT, request.action)
        assertFalse(request.getBooleanExtra(Intent.EXTRA_ALLOW_MULTIPLE, false))
        assertEquals(listOf("*/*"), request.getStringArrayExtra(Intent.EXTRA_MIME_TYPES)?.toList() ?: listOf(request.type))
        activity.answer(request, Intent().setData(uri))
        assertEquals(listOf("308 Win.ammo" to "ammo text"), opened.await().pairs())
    }

    /** Several files at once; without a display name the file name of the URI is used. */
    @Test
    fun openManyTexts() = runTest {
        val (activity, p) = platform()
        val docs = Robolectric.setupContentProvider(DocsProvider::class.java, DocsProvider.AUTHORITY)
        val a = docs.add("1", "a.drg", tmp.newFile().apply { writeText("drag a") })
        val b = Uri.fromFile(tmp.newFile("b.reticle").apply { writeText("reticle b") })
        val opened = begin { p.openTexts(listOf("drg", "reticle"), multiple = true) }
        val request = activity.started()
        assertTrue(request.getBooleanExtra(Intent.EXTRA_ALLOW_MULTIPLE, false))
        val clip = ClipData.newRawUri(null, a).apply { addItem(ClipData.Item(b)) }
        activity.answer(request, Intent().apply { clipData = clip })
        assertEquals(listOf("a.drg" to "drag a", "b.reticle" to "reticle b"), opened.await().pairs())
    }

    /** Cancelled open: no files. */
    @Test
    fun openCancelledIsEmpty() = runTest {
        val (activity, p) = platform()
        val opened = begin { p.openTexts(listOf("ammo"), multiple = true) }
        activity.answer(activity.started(), null)
        assertTrue(opened.await().isEmpty())
    }

    /** A picture is asked for (any image type); its bytes come back unchanged, cancelled is null. */
    @Test
    fun openImage() = runTest {
        val (activity, p) = platform()
        val bytes = byteArrayOf(0xFF.toByte(), 0xD8.toByte(), 1, 2, 3)
        val opened = begin { p.openImage() }
        val request = activity.started()
        assertEquals(listOf("image/*"), request.getStringArrayExtra(Intent.EXTRA_MIME_TYPES)?.toList() ?: listOf(request.type))
        activity.answer(request, Intent().setData(Uri.fromFile(tmp.newFile().apply { writeBytes(bytes) })))
        assertContentEquals(bytes, opened.await())
        val cancelled = begin { p.openImage() }
        activity.answer(activity.started(), null)
        assertNull(cancelled.await())
    }

    // --- camera --------------------------------------------------------------

    /** No camera on the phone: no camera actions. */
    @Test
    fun cameraFollowsTheFeature() {
        val pm = shadowOf(app.packageManager)
        pm.setSystemFeature(PackageManager.FEATURE_CAMERA_ANY, false)
        assertNull(platform().second.camera)
        pm.setSystemFeature(PackageManager.FEATURE_CAMERA_ANY, true)
        assertNotNull(platform().second.camera)
    }

    private fun picture(): Bitmap = Bitmap.createBitmap(64, 48, Bitmap.Config.ARGB_8888).apply { eraseColor(Color.RED) }

    /** With the permission the camera app is started; its picture comes back as a JPEG. */
    @Test
    fun photoWithPermission() = runTest {
        shadowOf(app).grantPermissions(Manifest.permission.CAMERA)
        val (activity, p) = platform()
        val photo = begin { p.takePhoto() }
        val request = activity.started()
        assertEquals(MediaStore.ACTION_IMAGE_CAPTURE, request.action)
        activity.answer(request, Intent().putExtra("data", picture()))
        val jpeg = assertNotNull(photo.await())
        assertTrue(isJpeg(jpeg))
        assertEquals(64 to 48, decode(jpeg).let { it.width to it.height })
    }

    /** Without it the permission is asked first; refused, the camera is never started. */
    @Test
    fun photoRefused() = runTest {
        val (activity, p) = platform()
        val photo = begin { p.takePhoto() }
        val asked = assertNotNull(shadowOf(activity).lastRequestedPermission)
        assertEquals(listOf(Manifest.permission.CAMERA), asked.requestedPermissions.toList())
        @Suppress("DEPRECATION")
        activity.onRequestPermissionsResult(asked.requestCode, asked.requestedPermissions, intArrayOf(PackageManager.PERMISSION_DENIED))
        assertNull(photo.await())
        assertTrue(shadowOf(activity).nextStartedActivityForResult?.intent?.action != MediaStore.ACTION_IMAGE_CAPTURE)
    }

    /** Granted in the dialog: the camera follows; cancelled there, null. */
    @Test
    fun photoGrantedThenCancelled() = runTest {
        val (activity, p) = platform()
        val photo = begin { p.takePhoto() }
        val asked = shadowOf(activity).lastRequestedPermission
        shadowOf(app).grantPermissions(Manifest.permission.CAMERA)
        @Suppress("DEPRECATION")
        activity.onRequestPermissionsResult(asked.requestCode, asked.requestedPermissions, intArrayOf(PackageManager.PERMISSION_GRANTED))
        runCurrent()
        var request = activity.started()
        // The permission request itself is an activity for result too: skip to the camera.
        while (request.action != MediaStore.ACTION_IMAGE_CAPTURE) request = activity.started()
        activity.answer(request, null)
        assertNull(photo.await())
    }

    /** The scanner is asked for QR codes only; its text comes back, cancelled is null. */
    @Test
    fun scanQr() = runTest {
        val (activity, p) = platform()
        val scanned = begin { p.scanQr() }
        val request = activity.started()
        assertEquals("com.google.zxing.client.android.SCAN", request.action)
        assertEquals("QR_CODE", request.getStringExtra("SCAN_FORMATS"))
        activity.answer(request, Intent().putExtra("SCAN_RESULT", "BALCALC1:1:1:abc"))
        assertEquals("BALCALC1:1:1:abc", scanned.await())
        val cancelled = begin { p.scanQr() }
        activity.answer(activity.started(), null)
        assertNull(cancelled.await())
    }
}
