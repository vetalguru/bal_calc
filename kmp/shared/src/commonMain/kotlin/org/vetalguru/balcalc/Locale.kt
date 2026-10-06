package org.vetalguru.balcalc

import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.ProvidedValue
import androidx.compose.runtime.key

/**
 * The app language chosen in Settings ("uk", "ru", "en"; null = the
 * system's). Compose resources follow the platform locale, so this sets it.
 */
expect object LocalAppLocale {
    val current: String @Composable get

    @Composable
    infix fun provides(value: String?): ProvidedValue<*>
}

/** Strings of [content] in [language]; the subtree restarts when it changes. */
@Composable
fun AppLanguage(language: String?, content: @Composable () -> Unit) {
    CompositionLocalProvider(LocalAppLocale provides language) {
        key(language) { content() }
    }
}
