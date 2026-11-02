package org.vetalguru.balcalc

import android.os.Looper
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.platform.LocalConfiguration
import java.util.Locale
import kotlin.test.assertEquals
import org.junit.After
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf

@RunWith(RobolectricTestRunner::class)
class LocaleAndroidTest {
    private val system = Locale.getDefault()

    @After
    fun restore() = Locale.setDefault(system)

    /** What the composition sees under LocalAppLocale provides [language]. */
    private class Seen(var current: String = "", var configuration: Locale? = null)

    /** The chosen language becomes the locale of everything Compose reads; null goes back to the system's. */
    @Test
    fun languageFollowsTheChoice() {
        Locale.setDefault(Locale.US)
        val activity = Robolectric.buildActivity(ComponentActivity::class.java).setup().get()
        val language = mutableStateOf<String?>("uk")
        val seen = Seen()
        activity.setContent {
            CompositionLocalProvider(LocalAppLocale provides language.value) {
                seen.current = LocalAppLocale.current
                seen.configuration = LocalConfiguration.current.locales[0]
            }
        }
        shadowOf(Looper.getMainLooper()).idle()
        assertEquals("uk", seen.current)
        assertEquals(Locale.forLanguageTag("uk"), seen.configuration)
        // The activity's resources too: Compose resources and getString read them.
        assertEquals(Locale.forLanguageTag("uk"), activity.resources.configuration.locales[0])
        assertEquals(Locale.forLanguageTag("uk"), Locale.getDefault())

        language.value = "ru"
        shadowOf(Looper.getMainLooper()).idle()
        assertEquals("ru", seen.current)
        assertEquals(Locale.forLanguageTag("ru"), activity.resources.configuration.locales[0])

        language.value = null // the system's, as it was before the first choice
        shadowOf(Looper.getMainLooper()).idle()
        assertEquals(Locale.US, Locale.getDefault())
        assertEquals("en_US", seen.current)
        assertEquals(Locale.US, seen.configuration)
    }
}
