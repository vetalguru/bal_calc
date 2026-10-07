package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.core.Info
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

@Composable
fun SettingsScreen(model: AppModel) {
    val st = model.state
    var info by remember { mutableStateOf(Info()) }
    LaunchedEffect(Unit) { info = runCatching { model.info() }.getOrDefault(Info()) }
    Column(
        Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(vertical = 12.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Section(stringResource(Res.string.angle_units)) {
            for ((unit, label) in listOf("mrad" to Res.string.mrad_long, "moa" to Res.string.moa_long)) {
                Row(
                    Modifier.fillMaxWidth()
                        .selectable(st.angleUnit == unit, onClick = { model.setAngleUnit(unit) })
                        .testTag("unit:$unit"),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    RadioButton(st.angleUnit == unit, onClick = null)
                    Text(stringResource(label), Modifier.padding(start = 8.dp))
                }
            }
            ChoiceField(
                stringResource(Res.string.language),
                listOf(
                    "" to stringResource(Res.string.system_language),
                    "uk" to "Українська",
                    "ru" to "Русский",
                    "en" to "English",
                ),
                st.language, model::setLanguage, Modifier.testTag("language"),
            )
        }
        Section(stringResource(Res.string.corrections_format)) {
            ChoiceField(
                stringResource(Res.string.correction_style),
                listOf(
                    "words" to stringResource(Res.string.style_words),
                    "arrows" to stringResource(Res.string.style_arrows),
                    "signs" to stringResource(Res.string.style_signs),
                ),
                st.prefs.correctionStyle, { v -> model.setPrefs { it.copy(correctionStyle = v) } }, Modifier.testTag("correctionStyle"),
            )
            SwitchRow(
                stringResource(Res.string.round_to_clicks), st.prefs.roundToClicks,
                { on -> model.setPrefs { it.copy(roundToClicks = on) } }, Modifier.testTag("roundToClicks"),
            )
            SwitchRow(
                stringResource(Res.string.show_second_unit), st.prefs.showSecondUnit,
                { on -> model.setPrefs { it.copy(showSecondUnit = on) } }, Modifier.testTag("showSecondUnit"),
            )
        }
        Section(stringResource(Res.string.display)) {
            ChoiceField(
                stringResource(Res.string.theme),
                listOf(
                    Themes.SYSTEM to stringResource(Res.string.theme_system),
                    Themes.LIGHT to stringResource(Res.string.theme_light),
                    Themes.DARK to stringResource(Res.string.theme_dark),
                    Themes.NIGHT to stringResource(Res.string.theme_night),
                ),
                st.prefs.theme, { t -> model.setPrefs { it.copy(theme = t) } }, Modifier.testTag("theme"),
            )
            SwitchRow(
                stringResource(Res.string.keep_screen_on), st.prefs.keepScreenOn,
                { on -> model.setPrefs { it.copy(keepScreenOn = on) } }, Modifier.testTag("keepScreenOn"),
            )
        }
        Section(stringResource(Res.string.about)) {
            Text(stringResource(Res.string.engine_versions, info.engineVersion, info.sqliteVersion))
            Text(stringResource(Res.string.about_model), color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(stringResource(Res.string.about_data), color = MaterialTheme.colorScheme.onSurfaceVariant)
            model.seedReport?.takeIf { it.imported > 0 }?.let {
                Text(stringResource(Res.string.seed_report, it.imported), color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Text(info.databasePath, fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}
