package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.CorrectionText
import org.vetalguru.balcalc.core.Solution
import org.vetalguru.balcalc.core.AppState
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

@Composable
internal fun Corrections(sol: Solution, state: AppState) {
    val c = correctionTexts(sol, state)
    val gust = c.gust?.let { g ->
        val clicks = g.clicksText()?.let { ", $it" }.orEmpty()
        stringResource(Res.string.gust_windage, state.conditions.windGustMps.fixed(1), "${g.direction} ${g.value} ${c.unit}$clicks".trim())
    }.orEmpty()
    val second = state.prefs.showSecondUnit
    // Side by side: read together. Tagged with the range it was solved for,
    // so tests can wait for it.
    Row(Modifier.padding(horizontal = 12.dp).testTag("solvedFor:${sol.rangeM.roundToInt()}"), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        CorrectionTile(stringResource(Res.string.elevation), c.elevation, c.unit, second, "elevation", Modifier.weight(1f))
        CorrectionTile(stringResource(Res.string.windage), c.windage, c.unit, second, "windage", Modifier.weight(1f), gust)
    }
}

@Composable
private fun CorrectionTile(
    title: String,
    c: CorrectionText,
    unit: String,
    showSecond: Boolean,
    tag: String,
    modifier: Modifier,
    extra: String = "",
) {
    Card(modifier, elevation = CardDefaults.cardElevation(defaultElevation = 3.dp)) {
        Column(Modifier.padding(12.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(title, fontSize = 15.sp, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1, modifier = Modifier.weight(1f))
                Text(
                    c.direction, fontSize = 20.sp, fontWeight = FontWeight.Bold, color = MaterialTheme.colorScheme.primary,
                    maxLines = 1, modifier = Modifier.testTag("${tag}Direction"),
                )
            }
            Text(c.value, fontSize = 48.sp, fontWeight = FontWeight.Bold, maxLines = 1, modifier = Modifier.testTag(tag))
            // The unit with the clicks: the number keeps the whole width.
            Text(
                c.clicksText()?.let { "$unit · $it" } ?: unit,
                fontSize = 16.sp, maxLines = 1,
            )
            if (showSecond) {
                Text(
                    c.second, fontSize = 13.sp, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1,
                    modifier = Modifier.testTag("${tag}Second"),
                )
            }
            if (extra.isNotEmpty()) {
                Text(extra, fontSize = 16.sp, color = MaterialTheme.colorScheme.primary, modifier = Modifier.testTag("${tag}Gust"))
            }
        }
    }
}
