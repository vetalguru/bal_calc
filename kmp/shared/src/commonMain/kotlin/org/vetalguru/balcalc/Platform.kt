package org.vetalguru.balcalc

import androidx.compose.runtime.Composable
import org.vetalguru.balcalc.core.NamedText

/** What the screens need from the operating system. */
interface Platform {
    /** Asks where to save [text]; false when the user cancelled. */
    suspend fun saveText(suggestedName: String, text: String): Boolean

    /** Asks for files to read; empty when cancelled. [extensions] without dots. */
    suspend fun openTexts(extensions: List<String>, multiple: Boolean): List<NamedText>

    suspend fun copyText(text: String)

    suspend fun pasteText(): String?

    /** Asks for a picture (a target photo); its bytes, or null when cancelled. */
    suspend fun openImage(): ByteArray? = null

    /** Whether [scanQr] has a camera to use. */
    val canScanQr: Boolean get() = false

    /** One QR code read by the camera (asking for it first); null when cancelled. */
    suspend fun scanQr(): String? = null

    /** The phone's sensors; null where there are none (desktops). */
    val sensors: PhoneSensors? get() = null
}

/** The system Back action (Android key or gesture); nothing on desktops. */
@Composable
expect fun BackHandler(enabled: Boolean, onBack: () -> Unit)

/** The platform for the screens below the app (set by [BalCalcApp]). */
val LocalPlatform = androidx.compose.runtime.staticCompositionLocalOf<Platform> {
    error("BalCalcApp provides the platform")
}
