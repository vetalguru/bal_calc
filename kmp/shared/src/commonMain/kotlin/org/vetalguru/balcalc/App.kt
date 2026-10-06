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
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.core.Api
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*
import org.vetalguru.balcalc.ui.ArmoryNav
import org.vetalguru.balcalc.ui.ArmoryScreen
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

private val Orange = Color(0xFFFF9800)
private val BlueGrey = Color(0xFF607D8B)

/**
 * The app: five pages, a side rail on desktops and tablets, a bottom bar
 * on phones. [startup] opens the platform's database (once per core).
 */
@Composable
fun BalCalcApp(api: Api, startup: suspend Api.() -> Unit, platform: Platform, dark: Boolean = false) {
    val scheme = if (dark) {
        darkColorScheme(primary = Orange, secondary = BlueGrey)
    } else {
        lightColorScheme(primary = Color(0xFFE65100), secondary = BlueGrey)
    }
    MaterialTheme(colorScheme = scheme) {
        val scope = rememberCoroutineScope()
        val model = remember(api) { AppModel(api, scope) }
        LaunchedEffect(model) { model.start(startup) }
        var page by rememberSaveable { mutableIntStateOf(0) }
        val armory = remember { ArmoryNav() }
        // Back: an inner page first (handled by it), then to the solution, then out.
        BackHandler(enabled = page != 0) { page = 0 }
        val snackbar = remember { SnackbarHostState() }
        LaunchedEffect(model.message) {
            model.message?.let {
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
                            else -> ComingSoon()
                        }
                    }
                }
            }
        }
        }
    }
}

/** Pages still being moved from the Qt app (phase 4). */
@Composable
private fun ComingSoon() {
    Column(Modifier.fillMaxSize().padding(24.dp), verticalArrangement = androidx.compose.foundation.layout.Arrangement.Center) {
        Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            Text(stringResource(Res.string.coming_soon), textAlign = TextAlign.Center)
        }
    }
}
