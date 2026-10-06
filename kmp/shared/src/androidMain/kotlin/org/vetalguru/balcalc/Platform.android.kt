package org.vetalguru.balcalc

import android.Manifest
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.net.Uri
import android.provider.OpenableColumns
import androidx.activity.ComponentActivity
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.core.content.ContextCompat
import com.journeyapps.barcodescanner.ScanContract
import com.journeyapps.barcodescanner.ScanOptions
import java.io.ByteArrayOutputStream
import kotlin.coroutines.resume
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import org.vetalguru.balcalc.core.NamedText

@Composable
actual fun BackHandler(enabled: Boolean, onBack: () -> Unit) =
    androidx.activity.compose.BackHandler(enabled, onBack)

/**
 * Files through the system document picker (works with Google Drive and
 * other providers, no storage permission). Create it in the activity's
 * onCreate: launchers must be registered before the activity starts.
 */
class AndroidPlatform(private val activity: ComponentActivity) : Platform {
    private var onCreated: ((Uri?) -> Unit)? = null
    private var onOpened: ((List<Uri>) -> Unit)? = null

    private val create: ActivityResultLauncher<String> =
        activity.registerForActivityResult(ActivityResultContracts.CreateDocument("application/json")) {
            onCreated?.invoke(it)
        }
    private val openOne: ActivityResultLauncher<Array<String>> =
        activity.registerForActivityResult(ActivityResultContracts.OpenDocument()) {
            onOpened?.invoke(listOfNotNull(it))
        }
    private val openMany: ActivityResultLauncher<Array<String>> =
        activity.registerForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) {
            onOpened?.invoke(it)
        }

    override suspend fun saveText(suggestedName: String, text: String): Boolean {
        val uri = suspendCancellableCoroutine<Uri?> { c ->
            onCreated = { c.resume(it) }
            create.launch(suggestedName)
        } ?: return false
        withContext(Dispatchers.IO) {
            activity.contentResolver.openOutputStream(uri, "wt")!!.use { it.write(text.encodeToByteArray()) }
        }
        return true
    }

    override suspend fun openTexts(extensions: List<String>, multiple: Boolean): List<NamedText> {
        // Providers rarely know .ammo/.drg types: accept anything, the core checks.
        val uris = suspendCancellableCoroutine<List<Uri>> { c ->
            onOpened = { c.resume(it) }
            if (multiple) openMany.launch(arrayOf("*/*")) else openOne.launch(arrayOf("*/*"))
        }
        return withContext(Dispatchers.IO) {
            uris.map { uri ->
                val text = activity.contentResolver.openInputStream(uri)!!.use { it.readBytes().decodeToString() }
                NamedText(displayName(uri), text)
            }
        }
    }

    private var onScanned: ((String?) -> Unit)? = null
    private val scan: ActivityResultLauncher<ScanOptions> =
        activity.registerForActivityResult(ScanContract()) { onScanned?.invoke(it.contents) }

    override val canScanQr: Boolean
        get() = activity.packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_ANY)

    // The scanner asks for the camera itself, when it opens.
    override suspend fun scanQr(): String? = suspendCancellableCoroutine { c ->
        onScanned = { c.resume(it) }
        scan.launch(
            ScanOptions()
                .setDesiredBarcodeFormats(ScanOptions.QR_CODE)
                .setBeepEnabled(false)
                .setOrientationLocked(false)
                .setPrompt(""),
        )
    }

    private var onPermission: ((Boolean) -> Unit)? = null
    private val askCamera: ActivityResultLauncher<String> =
        activity.registerForActivityResult(ActivityResultContracts.RequestPermission()) { onPermission?.invoke(it) }
    private var onPicture: ((Bitmap?) -> Unit)? = null
    private val takePicture: ActivityResultLauncher<Void?> =
        activity.registerForActivityResult(ActivityResultContracts.TakePicturePreview()) { onPicture?.invoke(it) }

    override val canTakePhoto: Boolean get() = canScanQr

    // The app declares the camera (for the QR scanner), so the system camera
    // app may be started only once the permission is granted.
    override suspend fun takePhoto(): ByteArray? {
        if (ContextCompat.checkSelfPermission(activity, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            val granted = suspendCancellableCoroutine<Boolean> { c ->
                onPermission = { c.resume(it) }
                askCamera.launch(Manifest.permission.CAMERA)
            }
            if (!granted) return null
        }
        val picture = suspendCancellableCoroutine<Bitmap?> { c ->
            onPicture = { c.resume(it) }
            takePicture.launch(null)
        } ?: return null
        return ByteArrayOutputStream().also { picture.compress(Bitmap.CompressFormat.JPEG, 90, it) }.toByteArray()
    }

    override suspend fun openImage(): ByteArray? {
        val uri = suspendCancellableCoroutine<List<Uri>> { c ->
            onOpened = { c.resume(it) }
            openOne.launch(arrayOf("image/*"))
        }.firstOrNull() ?: return null
        return withContext(Dispatchers.IO) { activity.contentResolver.openInputStream(uri)!!.use { it.readBytes() } }
    }

    private fun displayName(uri: Uri): String =
        activity.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)
            ?.use { if (it.moveToFirst()) it.getString(0) else null }
            ?: uri.lastPathSegment.orEmpty()

    private val clipboard get() = activity.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager

    override suspend fun copyText(text: String) =
        clipboard.setPrimaryClip(ClipData.newPlainText("BalCalc", text))

    override suspend fun pasteText(): String? =
        clipboard.primaryClip?.takeIf { it.itemCount > 0 }?.getItemAt(0)?.coerceToText(activity)?.toString()
}
