package org.vetalguru.balcalc.ui

import androidx.compose.runtime.Composable
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.CorrectionText
import org.vetalguru.balcalc.core.AppState
import org.vetalguru.balcalc.core.Solution
import org.vetalguru.balcalc.formatCorrection
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** A solution's corrections in the user's format, as every view of them shows it. */
class CorrectionTexts(
    /** The main angle unit, e.g. "MRAD". */
    val unit: String,
    val elevation: CorrectionText,
    val windage: CorrectionText,
    /** The windage with the gust, when one is set. */
    val gust: CorrectionText?,
)

/**
 * Formats [sol]'s corrections with the user's settings in [state]: the unit,
 * rounding to whole clicks, and the direction as [style] says ("words",
 * "arrows" or "signs"; the setting by default).
 */
@Composable
fun correctionTexts(sol: Solution, state: AppState, style: String = state.prefs.correctionStyle): CorrectionTexts {
    val moa = state.moa
    val unit = stringResource(if (moa) Res.string.unit_moa else Res.string.unit_mrad)
    val other = stringResource(if (moa) Res.string.unit_mrad else Res.string.unit_moa)
    val cm = stringResource(Res.string.unit_cm)
    // The direction words, arrows or signs for up / down and right / left.
    val (up, down, right, left) = when (style) {
        "arrows" -> listOf("↑", "↓", "→", "←")
        "signs" -> listOf("+", "−", "+", "−")
        else -> listOf(
            stringResource(Res.string.up), stringResource(Res.string.down),
            stringResource(Res.string.right), stringResource(Res.string.left),
        )
    }
    val round = state.prefs.roundToClicks
    val clickUp = sol.clickElevation.takeIf { sol.hasScope }
    val clickSide = sol.clickWindage.takeIf { sol.hasScope }
    return CorrectionTexts(
        unit = unit,
        elevation = formatCorrection(sol.elevation, clickUp, moa, sol.rangeM, round, up, down, other, cm),
        windage = formatCorrection(sol.windage, clickSide, moa, sol.rangeM, round, right, left, other, cm),
        gust = if (sol.hasGust) formatCorrection(sol.gustWindage, clickSide, moa, sol.rangeM, round, right, left, other, cm) else null,
    )
}

/** "16 clicks", or null without a scope. */
@Composable
fun CorrectionText.clicksText(): String? = clicks?.let { stringResource(Res.string.clicks, it) }
