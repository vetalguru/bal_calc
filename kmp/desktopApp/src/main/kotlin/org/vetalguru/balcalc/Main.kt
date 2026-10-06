package org.vetalguru.balcalc

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.remember
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application
import androidx.compose.ui.window.rememberWindowState
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.core.desktopDatabasePath
import org.vetalguru.balcalc.core.desktopEngine
import org.vetalguru.balcalc.core.desktopSeed

fun main() {
    val core = Api(desktopEngine())
    application {
        Window(
            onCloseRequest = ::exitApplication,
            title = "BalCalc",
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
