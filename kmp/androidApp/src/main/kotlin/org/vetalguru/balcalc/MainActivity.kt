package org.vetalguru.balcalc

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.isSystemInDarkTheme
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.core.androidDatabasePath
import org.vetalguru.balcalc.core.androidEngine
import org.vetalguru.balcalc.core.androidSeed

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        val app = applicationContext
        setContent {
            BalCalcApp(
                api = core,
                startup = { start(androidDatabasePath(app)) { androidSeed(app) } },
                dark = isSystemInDarkTheme(),
            )
        }
    }

    companion object {
        // One core per process: it survives activity recreation (rotation).
        private val core: Api by lazy { Api(androidEngine()) }
    }
}
