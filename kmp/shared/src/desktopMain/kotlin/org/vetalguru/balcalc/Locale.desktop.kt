package org.vetalguru.balcalc

import androidx.compose.runtime.Composable
import androidx.compose.runtime.ProvidedValue
import androidx.compose.runtime.staticCompositionLocalOf
import java.util.Locale

actual object LocalAppLocale {
    private var system: Locale? = null
    private val local = staticCompositionLocalOf { Locale.getDefault().toString() }

    actual val current: String
        @Composable get() = local.current

    @Composable
    actual infix fun provides(value: String?): ProvidedValue<*> {
        if (system == null) system = Locale.getDefault()
        val locale = if (value.isNullOrEmpty()) system!! else Locale.forLanguageTag(value)
        Locale.setDefault(locale)
        return local.provides(locale.toString())
    }
}
