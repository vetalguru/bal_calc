package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import kotlin.math.roundToInt
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.loadOr
import org.vetalguru.balcalc.coreText
import org.vetalguru.balcalc.core.SituationItem
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/**
 * Saved situations: the rifle, the cartridge, the hold mode and the
 * conditions under a name. Applying one closes the dialog.
 */
@Composable
fun SituationsDialog(model: AppModel, onClose: () -> Unit) {
    val scope = rememberCoroutineScope()
    var list by remember { mutableStateOf<List<SituationItem>>(emptyList()) }
    var name by remember { mutableStateOf("") }
    var error by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(Unit) { list = loadOr(emptyList()) { model.targets.situations() } }

    AlertDialog(
        onDismissRequest = onClose,
        title = { Text(stringResource(Res.string.situations)) },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(stringResource(Res.string.situations_hint), color = MaterialTheme.colorScheme.onSurfaceVariant)
                if (list.isEmpty()) {
                    Text(stringResource(Res.string.situations_empty), color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                list.forEach { s ->
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                        Column(Modifier.weight(1f)) {
                            Text(s.name, fontWeight = FontWeight.Bold)
                            Text(
                                if (s.available) {
                                    stringResource(Res.string.situation_line, s.rifleName, s.cartridgeName, s.rangeM.roundToInt())
                                } else {
                                    stringResource(Res.string.situation_missing)
                                },
                                style = MaterialTheme.typography.bodySmall,
                                color = if (s.available) MaterialTheme.colorScheme.onSurfaceVariant else MaterialTheme.colorScheme.error,
                            )
                        }
                        if (s.available) {
                            TextButton(onClick = {
                                scope.launch {
                                    error = model.targets.applySituation(s.name)
                                    if (error == null) onClose()
                                }
                            }, modifier = Modifier.testTag("applySituation:${s.name}")) { Text(stringResource(Res.string.apply)) }
                        }
                        TextButton(onClick = {
                            scope.launch { list = runCatching { model.targets.deleteSituation(s.name) }.getOrDefault(list) }
                        }, modifier = Modifier.testTag("deleteSituation:${s.name}")) { Text(stringResource(Res.string.delete)) }
                    }
                }
                HorizontalDivider()
                OutlinedTextField(
                    name, { name = it; error = null },
                    label = { Text(stringResource(Res.string.situation_name)) },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth().testTag("situationName"),
                )
                Button(onClick = {
                    scope.launch {
                        model.targets.saveSituation(name).fold(
                            { list = it; name = ""; error = null },
                            { error = it.message },
                        )
                    }
                }, modifier = Modifier.testTag("saveSituation")) { Text(stringResource(Res.string.situation_save)) }
                error?.let { Text(coreText(it), color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("situationError")) }
            }
        },
        confirmButton = { TextButton(onClick = onClose) { Text(stringResource(Res.string.close)) } },
    )
}
