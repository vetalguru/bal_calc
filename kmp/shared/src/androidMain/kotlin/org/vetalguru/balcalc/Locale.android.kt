package org.vetalguru.balcalc

import android.os.LocaleList
import androidx.compose.runtime.Composable
import androidx.compose.runtime.ProvidedValue
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import java.util.Locale

actual object LocalAppLocale {
    private var system: Locale? = null

    actual val current: String
        @Composable get() = Locale.getDefault().toString()

    @Suppress("DEPRECATION") // updateConfiguration: what Compose resources read
    @Composable
    actual infix fun provides(value: String?): ProvidedValue<*> {
        val configuration = LocalConfiguration.current
        if (system == null) system = Locale.getDefault()
        val locale = if (value.isNullOrEmpty()) system!! else Locale.forLanguageTag(value)
        Locale.setDefault(locale)
        configuration.setLocales(LocaleList(locale))
        val resources = LocalContext.current.resources
        resources.updateConfiguration(configuration, resources.displayMetrics)
        return LocalConfiguration.provides(configuration)
    }
}
