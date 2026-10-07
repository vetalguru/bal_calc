package org.vetalguru.balcalc

import org.vetalguru.balcalc.core.NamedText

/**
 * For tests: no dialogs, an in-memory clipboard, canned files and a picture,
 * a widget that records what it is shown; no camera.
 */
class FakePlatform(
    /** What "open files" returns. */
    var openFiles: List<NamedText> = emptyList(),
    override val sensors: PhoneSensors? = null,
) : Platform, Files, Clipboard, HomeWidget {
    override val files: Files get() = this
    override val clipboard: Clipboard get() = this
    override val widget: HomeWidget get() = this

    var clipboardText: String? = null
    val saved = mutableListOf<NamedText>()
    val savedBytes = mutableListOf<Pair<String, ByteArray>>()
    var image: ByteArray? = null
    var published: SolutionLines? = null

    override suspend fun saveText(suggestedName: String, text: String): Boolean {
        saved += NamedText(suggestedName, text)
        return true
    }
    override suspend fun saveBytes(suggestedName: String, mimeType: String, bytes: ByteArray): Boolean {
        savedBytes += suggestedName to bytes
        return true
    }
    override suspend fun openTexts(extensions: List<String>, multiple: Boolean) = openFiles
    override suspend fun openImage() = image
    override suspend fun copyText(text: String) { clipboardText = text }
    override suspend fun pasteText() = clipboardText
    override fun publish(summary: SolutionLines?) { published = summary }
}
