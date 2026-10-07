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

/** Native file dialogs and the system clipboard. */
class DesktopPlatform(private val owner: () -> Frame?) : Platform {
    override suspend fun saveText(suggestedName: String, text: String): Boolean {
        val file = withContext(Dispatchers.Main) {
            FileDialog(owner(), null, FileDialog.SAVE).run {
                file = suggestedName
                isVisible = true
                if (file == null) null else File(directory, file)
            }
        } ?: return false
        withContext(Dispatchers.IO) { file.writeText(text) }
        return true
    }

    override suspend fun openTexts(extensions: List<String>, multiple: Boolean): List<NamedText> {
        val files = withContext(Dispatchers.Main) {
            FileDialog(owner(), null, FileDialog.LOAD).run {
                isMultipleMode = multiple
                if (extensions.isNotEmpty()) {
                    setFilenameFilter { _, name -> extensions.any { name.lowercase().endsWith(".$it") } }
                }
                isVisible = true
                this.files.toList()
            }
        }
        return withContext(Dispatchers.IO) { files.map { NamedText(it.name, it.readText()) } }
    }

    override suspend fun openImage(): ByteArray? {
        val file = withContext(Dispatchers.Main) {
            FileDialog(owner(), null, FileDialog.LOAD).run {
                setFilenameFilter { _, name -> listOf(".jpg", ".jpeg", ".png", ".webp", ".bmp").any { name.lowercase().endsWith(it) } }
                isVisible = true
                this.files.firstOrNull()
            }
        } ?: return null
        return withContext(Dispatchers.IO) { file.readBytes() }
    }

    override suspend fun copyText(text: String) =
        Toolkit.getDefaultToolkit().systemClipboard.setContents(StringSelection(text), null)

    override suspend fun pasteText(): String? = runCatching {
        Toolkit.getDefaultToolkit().systemClipboard.getData(DataFlavor.stringFlavor) as String
    }.getOrNull()
}

