package org.vetalguru.balcalc

import org.vetalguru.balcalc.core.NamedText

/** For tests: no dialogs, an in-memory clipboard and canned files. */
class FakePlatform(var files: List<NamedText> = emptyList(), override val sensors: PhoneSensors? = null) : Platform {
    var clipboard: String? = null
    val saved = mutableListOf<NamedText>()
    override suspend fun saveText(suggestedName: String, text: String): Boolean {
        saved += NamedText(suggestedName, text)
        return true
    }
    override suspend fun openTexts(extensions: List<String>, multiple: Boolean) = files
    override suspend fun copyText(text: String) { clipboard = text }
    override suspend fun pasteText() = clipboard
    var image: ByteArray? = null
    override suspend fun openImage() = image
}
