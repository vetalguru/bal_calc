package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.core.Solution
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/**
 * The minimal view: the corrections as big as the screen allows and the
 * range with its steps; nothing else to read past.
 */
@Composable
internal fun MinimalSolution(model: AppModel, sol: Solution) {
    val st = model.state
    val c = st.conditions
    val texts = correctionTexts(sol, st)
    val unit = texts.unit
    val e = texts.elevation
    val w = texts.windage
    Column(
        Modifier.fillMaxSize().padding(16.dp).testTag("minimal"),
        verticalArrangement = Arrangement.SpaceEvenly,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        for ((title, corr, tag) in listOf(
            Triple(stringResource(Res.string.elevation), e, "elevation"),
            Triple(stringResource(Res.string.windage), w, "windage"),
        )) {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text("$title  ${corr.direction}", fontSize = 22.sp, fontWeight = FontWeight.Bold, color = MaterialTheme.colorScheme.primary)
                Text(corr.value, fontSize = 96.sp, fontWeight = FontWeight.Bold, maxLines = 1, modifier = Modifier.testTag(tag))
                Text(
                    corr.clicksText()?.let { "$unit · $it" } ?: unit,
                    fontSize = 22.sp,
                )
            }
        }
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Step("−10") { model.setTargetRange(c.targetRangeM - 10) }
            RangeField(c.targetRangeM, model::setTargetRange, Modifier.width(160.dp))
            Step("+10") { model.setTargetRange(c.targetRangeM + 10) }
        }
        OutlinedButton(onClick = { model.setPrefs { it.copy(minimal = false) } }, modifier = Modifier.testTag("minimalOff")) {
            Text(stringResource(Res.string.full_view))
        }
    }
}
