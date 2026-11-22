package org.vetalguru.balcalc

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
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
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.clipPath
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.PI
import kotlin.math.roundToInt
import kotlin.math.sin
import org.jetbrains.compose.resources.DrawableResource
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.vectorResource
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
import org.vetalguru.balcalc.ui.clicksText
import org.vetalguru.balcalc.ui.correctionTexts

private class Page(val title: StringResource, val short: StringResource, val icon: DrawableResource, val tag: String)

private val pages = listOf(
    Page(Res.string.nav_solution, Res.string.nav_solution_short, Res.drawable.ic_my_location, "navSolution"),
    Page(Res.string.nav_table, Res.string.nav_table_short, Res.drawable.ic_table_chart, "navTable"),
    Page(Res.string.nav_conditions, Res.string.nav_conditions_short, Res.drawable.ic_air, "navConditions"),
    Page(Res.string.nav_armory, Res.string.nav_armory_short, Res.drawable.ic_inventory_2, "navArmory"),
    Page(Res.string.nav_settings, Res.string.nav_settings_short, Res.drawable.ic_settings, "navSettings"),
)

/**
 * The app: five pages, a side rail on desktops and tablets, a bottom bar
 * on phones. [startup] opens the platform's database (once per core);
 * [dark] is the system's dark mode, for the "system" theme; [greeting]
 * shows "Слава Україні!" on every start, closed by its answer or by its
 * cross, which calls [onQuit] (the platform closes the app).
 */
@Composable
fun BalCalcApp(
    api: Api,
    startup: suspend Api.() -> Unit,
    platform: Platform,
    dark: Boolean = false,
    greeting: Boolean = true,
    onQuit: () -> Unit = {},
) {
    // Above the language switch: changing the language keeps the data and the page.
    val scope = rememberCoroutineScope()
    val model = remember(api) { AppModel(api, scope) }
    LaunchedEffect(model) { model.start(startup) }
    var page by rememberSaveable { mutableIntStateOf(0) }
    var greeted by rememberSaveable { mutableStateOf(!greeting) } // once per start, not per rotation
    val armory = remember { ArmoryNav() }
    AppLanguage(model.state.language.ifEmpty { null }) {
    BalCalcTheme(model.state.prefs.theme, systemDark = dark) {
        KeepScreenOn(model.state.prefs.keepScreenOn)
        if (!greeted) Greeting(onAnswer = { greeted = true }, onQuit = onQuit)
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
                                    icon = { Icon(vectorResource(p.icon), contentDescription = null) }, // the label names it
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
                                    icon = { Icon(vectorResource(p.icon), contentDescription = null) }, // the label names it
                                    label = { Text(stringResource(p.title), textAlign = TextAlign.Center) },
                                    modifier = Modifier.testTag(p.tag),
                                )
                            }
                        }
                    }
                    Surface(Modifier.weight(1f).fillMaxHeight()) {
                        when (page) {
                            0 -> SolutionScreen(model, onEditArmory = { page = 3 }, onEditConditions = { page = 2 })
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

/**
 * "Слава Україні!" under the flag. "Героям слава!" goes on to the app, the
 * cross closes it; Back and a tap outside do nothing.
 */
@Composable
private fun Greeting(onAnswer: () -> Unit, onQuit: () -> Unit) {
    AlertDialog(
        onDismissRequest = {},
        title = {
            Box(Modifier.fillMaxWidth()) {
                UkrainianFlag(Modifier.align(Alignment.Center).padding(top = 8.dp).size(width = 132.dp, height = 88.dp))
                IconButton(onClick = onQuit, modifier = Modifier.align(Alignment.TopEnd).testTag("greetingQuit")) {
                    Icon(vectorResource(Res.drawable.ic_close), stringResource(Res.string.quit_app))
                }
            }
        },
        text = {
            Text(
                stringResource(Res.string.greeting_call),
                style = MaterialTheme.typography.headlineMedium,
                fontWeight = FontWeight.Bold,
                textAlign = TextAlign.Center,
                modifier = Modifier.fillMaxWidth().testTag("greeting"),
            )
        },
        confirmButton = {
            Button(onClick = onAnswer, modifier = Modifier.fillMaxWidth().testTag("greetingAnswer")) {
                Text(stringResource(Res.string.greeting_answer), style = MaterialTheme.typography.titleMedium)
            }
        },
    )
}

/** The flag waving: blue over yellow, both edges one wave, rounded corners. */
@Composable
private fun UkrainianFlag(modifier: Modifier = Modifier) {
    Canvas(modifier.testTag("greetingFlag")) {
        val amp = size.height * 0.06f
        fun wave(y: Float, x: Float) = y + amp * sin(2 * PI * x / size.width).toFloat()
        val steps = 48
        val flag = Path().apply {
            moveTo(0f, wave(amp, 0f))
            for (i in 1..steps) size.width.times(i / steps.toFloat()).let { x -> lineTo(x, wave(amp, x)) }
            for (i in steps downTo 0) size.width.times(i / steps.toFloat()).let { x -> lineTo(x, wave(size.height - amp, x)) }
            close()
        }
        val middle = Path().apply {
            moveTo(0f, 0f)
            lineTo(size.width, 0f)
            for (i in steps downTo 0) size.width.times(i / steps.toFloat()).let { x -> lineTo(x, wave(size.height / 2, x)) }
            close()
        }
        clipPath(flag) {
            drawRoundRect(Color(0xFFFFD700), cornerRadius = CornerRadius(12.dp.toPx())) // yellow: the field below
            clipPath(middle) { drawRect(Color(0xFF0057B7)) } // blue: above the wave through the middle
        }
    }
}

/** Keeps a home-screen widget (where the platform has one) showing the current solution. */
@Composable
private fun WidgetFeed(model: AppModel, platform: Platform) {
    val st = model.state
    val sol = model.solution
    val metres = stringResource(Res.string.unit_m)
    val lines = if (!sol.ok || !model.ready) null else {
        // In words whatever the screen's style: the widget stands alone.
        val c = correctionTexts(sol, st, style = "words")
        val upClicks = c.elevation.clicksText()
        val sideClicks = c.windage.clicksText()
        fun line(t: CorrectionText, clicks: String?) = "${t.direction} ${t.value} ${c.unit}".trim() + (clicks?.let { " · $it" }.orEmpty())
        SolutionLines(
            st.currentPair?.let { "${it.rifleName} · ${it.cartridgeName}" }.orEmpty(),
            "${sol.rangeM.roundToInt()} $metres",
            line(c.elevation, upClicks),
            line(c.windage, sideClicks),
        )
    }
    LaunchedEffect(lines) { platform.widget?.publish(lines) }
}
