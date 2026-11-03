package org.vetalguru.balcalc

import androidx.compose.runtime.Composable
import java.awt.FileDialog
import java.awt.Frame
import java.awt.Toolkit
import java.awt.datatransfer.DataFlavor
import java.awt.datatransfer.StringSelection
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.vetalguru.balcalc.core.NamedText

@Composable
actual fun BackHandler(enabled: Boolean, onBack: () -> Unit) = Unit

/** Where to save or what to open: the native file dialog, or a stand-in in tests. */
interface FileChooser {
    /** The file to save to, or null when cancelled. */
    fun save(suggestedName: String): File?

    /** The files chosen (none when cancelled); [accept] filters by file name. */
    fun open(multiple: Boolean, accept: ((String) -> Boolean)?): List<File>
}

/** The native file dialog (GTK or Windows), on the UI thread. */
class AwtFileChooser(private val owner: () -> Frame?) : FileChooser {
    override fun save(suggestedName: String): File? =
        FileDialog(owner(), null, FileDialog.SAVE).run {
            file = suggestedName
            isVisible = true
            if (file == null) null else File(directory, file)
        }

    override fun open(multiple: Boolean, accept: ((String) -> Boolean)?): List<File> =
        FileDialog(owner(), null, FileDialog.LOAD).run {
            isMultipleMode = multiple
            if (accept != null) setFilenameFilter { _, name -> accept(name) }
            isVisible = true
            files.toList()
        }
}

/** Whether a file name ends in one of [extensions] (without the dot), any case. */
fun hasExtension(name: String, extensions: List<String>): Boolean =
    extensions.any { name.lowercase().endsWith(".${it.lowercase()}") }

/** The pictures the group screen reads. */
val imageExtensions = listOf("jpg", "jpeg", "png", "webp", "bmp")

/**
 * Native file dialogs and the system clipboard. [systemClipboard] is the
 * system's unless given (tests pass their own: a headless system has none).
 */
class DesktopPlatform(
    owner: () -> Frame?,
    private val chooser: FileChooser = AwtFileChooser(owner),
    private val systemClipboard: java.awt.datatransfer.Clipboard? = null,
) : Platform, Files, Clipboard {
    override val files: Files get() = this
    override val clipboard: Clipboard get() = this

    private suspend fun saveTo(suggestedName: String, write: (File) -> Unit): Boolean {
        val file = withContext(Dispatchers.Main) { chooser.save(suggestedName) } ?: return false
        withContext(Dispatchers.IO) { write(file) }
        return true
    }

    override suspend fun saveText(suggestedName: String, text: String): Boolean =
        saveTo(suggestedName) { it.writeText(text) }

    override suspend fun saveBytes(suggestedName: String, mimeType: String, bytes: ByteArray): Boolean =
        saveTo(suggestedName) { it.writeBytes(bytes) }

    override suspend fun openTexts(extensions: List<String>, multiple: Boolean): List<NamedText> {
        val files = withContext(Dispatchers.Main) {
            chooser.open(multiple, if (extensions.isEmpty()) null else { name -> hasExtension(name, extensions) })
        }
        return withContext(Dispatchers.IO) { files.map { NamedText(it.name, it.readText()) } }
    }

    override suspend fun openImage(): ByteArray? {
        val file = withContext(Dispatchers.Main) {
            chooser.open(false) { name -> hasExtension(name, imageExtensions) }.firstOrNull()
        } ?: return null
        return withContext(Dispatchers.IO) { file.readBytes() }
    }

    private val system get() = systemClipboard ?: Toolkit.getDefaultToolkit().systemClipboard

    override suspend fun copyText(text: String) = system.setContents(StringSelection(text), null)

    override suspend fun pasteText(): String? = runCatching {
        system.getData(DataFlavor.stringFlavor) as String
    }.getOrNull()
}


@Composable
actual fun KeepScreenOn(enabled: Boolean) = Unit
