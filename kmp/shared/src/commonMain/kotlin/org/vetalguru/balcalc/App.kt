package org.vetalguru.balcalc

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationRail
import androidx.compose.material3.NavigationRailItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*
import org.vetalguru.balcalc.ui.ArmoryNav
import org.vetalguru.balcalc.ui.ArmoryScreen
import org.vetalguru.balcalc.ui.BalCalcTheme
import org.vetalguru.balcalc.ui.ConditionsScreen
import org.vetalguru.balcalc.ui.SolutionScreen
import org.vetalguru.balcalc.ui.TableScreen

private class Page(val title: StringResource, val short: StringResource, val tag: String)

private val pages = listOf(
    Page(Res.string.nav_solution, Res.string.nav_solution_short, "navSolution"),
    Page(Res.string.nav_table, Res.string.nav_table_short, "navTable"),
    Page(Res.string.nav_conditions, Res.string.nav_conditions_short, "navConditions"),
    Page(Res.string.nav_armory, Res.string.nav_armory_short, "navArmory"),
    Page(Res.string.nav_settings, Res.string.nav_settings_short, "navSettings"),
)

/**
 * The app: five pages, a side rail on desktops and tablets, a bottom bar
 * on phones. [startup] opens the platform's database (once per core);
 * [dark] is the system's dark mode, for the "system" theme.
 */
@Composable
fun BalCalcApp(api: Api, startup: suspend Api.() -> Unit, platform: Platform, dark: Boolean = false) {
    // Above the language switch: changing the language keeps the data and the page.
    val scope = rememberCoroutineScope()
    val model = remember(api) { AppModel(api, scope) }
    LaunchedEffect(model) { model.start(startup) }
    var page by rememberSaveable { mutableIntStateOf(0) }
    val armory = remember { ArmoryNav() }
    AppLanguage(model.state.language.ifEmpty { null }) {
    BalCalcTheme(model.state.prefs.theme, systemDark = dark) {
        KeepScreenOn(model.state.prefs.keepScreenOn)
        WidgetFeed(model, platform)
        // Back: an inner page first (handled by it), then to the solution, then out.
        BackHandler(enabled = page != 0) { page = 0 }
        val snackbar = remember { SnackbarHostState() }
        val message = model.message?.let { coreText(it) }
        LaunchedEffect(message) {
            message?.let {
                snackbar.showSnackbar(it)
                model.message = null
            }
        }

        androidx.compose.runtime.CompositionLocalProvider(LocalPlatform provides platform) {
        BoxWithConstraints(Modifier.fillMaxSize()) {
            val wide = maxWidth >= 840.dp
            Scaffold(
                snackbarHost = { SnackbarHost(snackbar) },
                bottomBar = {
                    if (!wide) {
                        NavigationBar {
                            pages.forEachIndexed { i, p ->
                                NavigationBarItem(
                                    selected = page == i,
                                    onClick = { page = i },
                                    icon = {},
                                    label = { Text(stringResource(p.short), fontSize = 12.sp, maxLines = 1) },
                                    modifier = Modifier.testTag(p.tag),
                                )
                            }
                        }
                    }
                },
            ) { padding ->
                Row(Modifier.fillMaxSize().padding(padding)) {
                    if (wide) {
                        NavigationRail(Modifier.fillMaxHeight()) {
                            Text(
                                stringResource(Res.string.app_name),
                                style = MaterialTheme.typography.titleLarge,
                                modifier = Modifier.padding(16.dp),
                            )
                            pages.forEachIndexed { i, p ->
                                NavigationRailItem(
                                    selected = page == i,
                                    onClick = { page = i },
                                    icon = {},
                                    label = { Text(stringResource(p.title), textAlign = TextAlign.Center) },
                                    modifier = Modifier.testTag(p.tag),
                                )
                            }
                        }
                    }
                    Surface(Modifier.weight(1f).fillMaxHeight()) {
                        when (page) {
                            0 -> SolutionScreen(model, onEditArmory = { page = 3 })
                            1 -> TableScreen(model, onRangeChosen = { page = 0 })
                            2 -> ConditionsScreen(model)
                            3 -> ArmoryScreen(model, armory, onChosen = { page = 0 })
                            else -> org.vetalguru.balcalc.ui.SettingsScreen(model)
                        }
                    }
                }
            }
        }
        }
    }
}

}

/** Keeps a home-screen widget (where the platform has one) showing the current solution. */
@Composable
private fun WidgetFeed(model: AppModel, platform: Platform) {
    val st = model.state
    val sol = model.solution
    val unit = stringResource(if (st.moa) Res.string.unit_moa else Res.string.unit_mrad)
    val other = stringResource(if (st.moa) Res.string.unit_mrad else Res.string.unit_moa)
    val metres = stringResource(Res.string.unit_m)
    val cm = stringResource(Res.string.unit_cm)
    val words = listOf(Res.string.up, Res.string.down, Res.string.right, Res.string.left).map { stringResource(it) }
    val lines = if (!sol.ok || !model.ready) null else {
        val click = { v: Double -> v.takeIf { sol.hasScope } }
        val e = formatCorrection(sol.elevation, click(sol.clickElevation), st.moa, sol.rangeM, st.prefs.roundToClicks, words[0], words[1], other, cm)
        val w = formatCorrection(sol.windage, click(sol.clickWindage), st.moa, sol.rangeM, st.prefs.roundToClicks, words[2], words[3], other, cm)
        val eClicks = e.clicks?.let { " · " + stringResource(Res.string.clicks, it) }.orEmpty()
        val wClicks = w.clicks?.let { " · " + stringResource(Res.string.clicks, it) }.orEmpty()
        SolutionLines(
            st.currentPair?.let { "${it.rifleName} · ${it.cartridgeName}" }.orEmpty(),
            "${sol.rangeM.roundToInt()} $metres",
            "${e.direction} ${e.value} $unit".trim() + eClicks,
            "${w.direction} ${w.value} $unit".trim() + wClicks,
        )
    }
    LaunchedEffect(lines) { platform.widget?.publish(lines) }
}
