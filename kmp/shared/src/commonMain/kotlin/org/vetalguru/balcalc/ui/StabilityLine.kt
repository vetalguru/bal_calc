package org.vetalguru.balcalc.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/**
 * The gyroscopic stability of the edited rifle or cartridge with its current
 * partner ([with], e.g. the current cartridge in the rifle editor), coloured:
 * stable from 1.5, marginal from 1, unstable below. Nothing without a partner;
 * a hint when an input (twist, bullet length) is missing.
 */
@Composable
fun StabilityLine(sg: Double?, with: String) {
    if (sg == null || with.isEmpty()) return
    if (sg <= 0.0) {
        Text(stringResource(Res.string.sg_missing, with), color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.testTag("stabilityLine"))
        return
    }
    val colors = appColors
    val (verdict, color) = when {
        sg >= 1.5 -> stringResource(Res.string.sg_stable) to colors.good
        sg >= 1.0 -> stringResource(Res.string.sg_marginal) to colors.caution
        else -> stringResource(Res.string.sg_unstable) to colors.bad
    }
    Text(
        stringResource(Res.string.sg_with, sg.fixed(2), with, verdict),
        color = color,
        fontWeight = FontWeight.Bold,
        modifier = Modifier.testTag("stabilityLine"),
    )
}
