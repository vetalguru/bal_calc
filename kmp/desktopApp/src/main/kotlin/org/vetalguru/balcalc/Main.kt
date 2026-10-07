package org.vetalguru.balcalc

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.remember
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.graphics.toComposeImageBitmap
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application
import androidx.compose.ui.window.rememberWindowState
import java.io.File
import org.jetbrains.skia.Image
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.core.desktopDatabasePath
import org.vetalguru.balcalc.core.desktopEngine
import org.vetalguru.balcalc.core.desktopSeed
import org.vetalguru.balcalc.core.resourcesDir

fun main() {
    val core = Api(desktopEngine())
    val icon = File(resourcesDir(), "balcalc.png").takeIf { it.isFile }
        ?.let { BitmapPainter(Image.makeFromEncoded(it.readBytes()).toComposeImageBitmap()) }
    application {
        Window(
            onCloseRequest = ::exitApplication,
            title = "Holdmark",
            icon = icon,
            state = rememberWindowState(width = 1100.dp, height = 760.dp),
        ) {
            BalCalcApp(
                api = core,
                startup = { start(desktopDatabasePath()) { desktopSeed() } },
                platform = remember { DesktopPlatform { window } },
                dark = isSystemInDarkTheme(),
            )
        }
    }
}
