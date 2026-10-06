package org.vetalguru.balcalc

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.launch
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.double
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import org.vetalguru.balcalc.core.Api

/**
 * Phase 2 skeleton: proves the C++ core runs under Kotlin on every
 * platform. Shows the current rifle + cartridge and the elevation at the
 * target range; replaced by the real screens in phase 3.
 */
@Composable
fun BalCalcApp(api: Api, startup: suspend Api.() -> Unit, dark: Boolean = false) {
    MaterialTheme(colorScheme = if (dark) darkColorScheme() else lightColorScheme()) {
        Surface(Modifier.fillMaxSize()) {
            var state by remember { mutableStateOf<JsonObject?>(null) }
            var solution by remember { mutableStateOf<JsonObject?>(null) }
            var error by remember { mutableStateOf<String?>(null) }
            val scope = rememberCoroutineScope()

            suspend fun reload() {
                state = api.call("state").jsonObject
                solution = api.call("solution").jsonObject
            }
            LaunchedEffect(Unit) {
                runCatching { api.startup(); reload() }.onFailure { error = it.message }
            }

            Column(
                Modifier.padding(24.dp),
                verticalArrangement = Arrangement.spacedBy(16.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Text("BalCalc", style = MaterialTheme.typography.headlineMedium)
                error?.let { Text(it, color = MaterialTheme.colorScheme.error) }
                val st = state ?: return@Column
                val pair = st["currentPair"]?.jsonObject
                if (st["rifles"]!!.jsonArray.isEmpty()) {
                    Button(
                        onClick = { scope.launch { api.call("addSample"); reload() } },
                        modifier = Modifier.testTag("sample"),
                    ) { Text("Try a sample") }
                    return@Column
                }
                if (pair != null && pair.isNotEmpty()) {
                    Text("${pair.str("rifleName")} / ${pair.str("cartridgeName")}")
                }
                val sol = solution ?: return@Column
                if (sol.bool("ok")) {
                    Text("${sol.num("rangeM").toInt()} m")
                    Text(
                        sol.num("elevation").fixed(2),
                        fontSize = 56.sp,
                        modifier = Modifier.testTag("elevation"),
                    )
                    Text("${st.str("angleUnit").uppercase()} · ${sol.num("elevationClicks").toInt()} clicks")
                } else {
                    Text(sol.str("error"), color = MaterialTheme.colorScheme.error)
                }
            }
        }
    }
}

private fun JsonObject.str(key: String) = this[key]?.jsonPrimitive?.content ?: ""
private fun JsonObject.num(key: String) = this[key]?.jsonPrimitive?.double ?: 0.0
private fun JsonObject.bool(key: String) = this[key]?.jsonPrimitive?.content == "true"
