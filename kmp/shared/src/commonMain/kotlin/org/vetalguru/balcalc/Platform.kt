package org.vetalguru.balcalc

import androidx.compose.runtime.Composable
import org.vetalguru.balcalc.core.NamedText

/**
 * What the screens need from the operating system, as separate
 * capabilities: each screen takes only the one it uses. A capability a
 * platform lacks is null (no camera on a desktop, no widget in tests).
 */
interface Platform {
    val files: Files
    val clipboard: Clipboard
    val camera: Camera? get() = null
    val sensors: PhoneSensors? get() = null
    val widget: HomeWidget? get() = null
}

/** Files the user picks: our shared files, exports and pictures. */
interface Files {
    /** Asks where to save [text]; false when the user cancelled. */
    suspend fun saveText(suggestedName: String, text: String): Boolean

    /** Asks where to save a binary file ([mimeType], e.g. "image/png"); false when cancelled. */
    suspend fun saveBytes(suggestedName: String, mimeType: String, bytes: ByteArray): Boolean

    /** Asks for files to read; empty when cancelled. [extensions] without dots. */
    suspend fun openTexts(extensions: List<String>, multiple: Boolean): List<NamedText>

    /** Asks for a picture (a target photo, a rifle); its bytes, or null when cancelled. */
    suspend fun openImage(): ByteArray?
}

interface Clipboard {
    suspend fun copyText(text: String)

    suspend fun pasteText(): String?
}

/** The camera (asking for it first, each time it is needed). */
interface Camera {
    /** One QR code read by the camera; null when cancelled. */
    suspend fun scanQr(): String?

    /** A picture from the camera; null when cancelled. */
    suspend fun takePhoto(): ByteArray?
}

/** A home-screen widget showing the current solution. */
fun interface HomeWidget {
    /** The solution in four short lines (null: none). */
    fun publish(summary: SolutionLines?)
}

/** The system Back action (Android key or gesture); nothing on desktops. */
@Composable
expect fun BackHandler(enabled: Boolean, onBack: () -> Unit)

/** The platform for the screens below the app (set by [BalCalcApp]). */
val LocalPlatform = androidx.compose.runtime.staticCompositionLocalOf<Platform> {
    error("BalCalcApp provides the platform")
}

/** Keeps the screen from dimming and locking while [enabled] (phones; nothing on desktops). */
@Composable
expect fun KeepScreenOn(enabled: Boolean)

/** A solution as a widget shows it: who, how far, and the two corrections. */
data class SolutionLines(val title: String, val range: String, val elevation: String, val windage: String)
