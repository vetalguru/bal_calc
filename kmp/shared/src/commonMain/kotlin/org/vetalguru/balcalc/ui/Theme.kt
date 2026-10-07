package org.vetalguru.balcalc.ui

import androidx.compose.material3.ColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp

/** The app's own colours beside Material's: drawings, charts, marks. */
@Immutable
data class AppColors(
    val reticleField: Color,
    val reticleInk: Color,
    val reticleEdge: Color,
    val target: Color,
    val transonic: Color,
    val drift: Color,
    val series: List<Color>,
    /** Stability: Sg 1.5 and over, 1 to 1.5, under 1. */
    val good: Color,
    val caution: Color,
    val bad: Color,
)

private val Orange = Color(0xFFFF9800)
private val DeepOrange = Color(0xFFE65100)
private val BlueGrey = Color(0xFF607D8B)

private val DayColors = AppColors(
    reticleField = Color(0xFFF4F1E8), reticleInk = Color(0xFF111111), reticleEdge = Color(0xFF555555),
    target = Color(0xFFE53935), transonic = Color(0xFFEF6C00), drift = Color(0xFF00897B),
    series = listOf(Color(0xFFEF6C00), Color(0xFF1E88E5), Color(0xFF8E24AA)),
    good = Color(0xFF2E7D32), caution = Color(0xFFF9A825), bad = Color(0xFFC62828),
)

private val DarkColors = DayColors.copy(
    reticleField = Color(0xFF1C1B19), reticleInk = Color(0xFFE8E4D8), reticleEdge = Color(0xFF8A8780),
    target = Color(0xFFFF5A52), transonic = Color(0xFFFFA040), drift = Color(0xFF4DB6AC),
    series = listOf(Color(0xFFFFA040), Color(0xFF64B5F6), Color(0xFFCE93D8)),
    good = Color(0xFF81C784), caution = Color(0xFFFFD54F), bad = Color(0xFFEF5350),
)

// Night: red on black only, dim, so the eyes keep their dark adaptation.
private val NightRed = Color(0xFFD32F2F)
private val NightDim = Color(0xFF8E1F1F)
private val NightColors = AppColors(
    reticleField = Color.Black, reticleInk = NightRed, reticleEdge = NightDim,
    target = Color(0xFFFF3B30), transonic = NightRed, drift = NightDim,
    series = listOf(NightRed, Color(0xFFB71C1C), Color(0xFF7F1010)),
    // Brightness instead of hue at night: dim is fine, bright is a problem.
    good = NightDim, caution = NightRed, bad = Color(0xFFFF3B30),
)

private val NightScheme = darkColorScheme(
    primary = NightRed, onPrimary = Color.Black,
    primaryContainer = Color(0xFF3A0A0A), onPrimaryContainer = NightRed,
    secondary = NightDim, onSecondary = Color.Black,
    secondaryContainer = Color(0xFF2A0808), onSecondaryContainer = NightRed,
    tertiary = NightDim, onTertiary = Color.Black,
    background = Color.Black, onBackground = NightRed,
    surface = Color.Black, onSurface = NightRed,
    surfaceVariant = Color(0xFF1A0505), onSurfaceVariant = NightDim,
    surfaceContainer = Color(0xFF120404), surfaceContainerHigh = Color(0xFF180505),
    surfaceContainerHighest = Color(0xFF1E0606), surfaceContainerLow = Color(0xFF0C0202),
    surfaceContainerLowest = Color.Black,
    outline = NightDim, outlineVariant = Color(0xFF4A1010),
    error = Color(0xFFFF5252), onError = Color.Black,
    inverseSurface = NightRed, inverseOnSurface = Color.Black,
)

val LocalAppColors = staticCompositionLocalOf { DayColors }

/** The app's colours in the current theme. */
val appColors: AppColors
    @Composable @ReadOnlyComposable get() = LocalAppColors.current

/** Below Mach 1.2: the colour that marks transonic rows and curves. */
val Transonic: Color @Composable @ReadOnlyComposable get() = LocalAppColors.current.transonic
val DriftColor: Color @Composable @ReadOnlyComposable get() = LocalAppColors.current.drift

/** Bigger, bolder numbers than Material's defaults: read at arm's length. */
private val AppTypography = Typography().let { t ->
    t.copy(
        displayLarge = t.displayLarge.copy(fontWeight = FontWeight.Bold),
        displayMedium = t.displayMedium.copy(fontWeight = FontWeight.Bold),
        headlineLarge = t.headlineLarge.copy(fontWeight = FontWeight.SemiBold),
        titleLarge = t.titleLarge.copy(fontWeight = FontWeight.SemiBold),
        bodyLarge = t.bodyLarge.copy(fontSize = 17.sp),
        labelLarge = t.labelLarge.copy(fontSize = 15.sp),
    )
}

/** Theme names the settings store: follow the system, or one fixed look. */
object Themes {
    const val SYSTEM = "system"
    const val LIGHT = "light"
    const val DARK = "dark"
    const val NIGHT = "night"
}

/**
 * The app's theme: [theme] as chosen in the settings ("system" follows
 * [systemDark]); "night" is red on black.
 */
@Composable
fun BalCalcTheme(theme: String, systemDark: Boolean, content: @Composable () -> Unit) {
    val dark = theme == Themes.DARK || (theme != Themes.LIGHT && theme != Themes.NIGHT && systemDark)
    val (scheme: ColorScheme, colors) = when {
        theme == Themes.NIGHT -> NightScheme to NightColors
        dark -> darkColorScheme(primary = Orange, secondary = BlueGrey) to DarkColors
        else -> lightColorScheme(primary = DeepOrange, secondary = BlueGrey) to DayColors
    }
    CompositionLocalProvider(LocalAppColors provides colors) {
        MaterialTheme(colorScheme = scheme, typography = AppTypography, content = content)
    }
}
