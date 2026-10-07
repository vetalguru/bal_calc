package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.coreText
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** One editor screen: bar, error line, scrolled sections, Save at the bottom. */
@Composable
internal fun EditorPage(
    title: String,
    error: String?,
    onCancel: () -> Unit,
    onSave: () -> Unit,
    content: @Composable (wide: Boolean) -> Unit,
) {
    val cancel = stringResource(Res.string.cancel)
    val save = stringResource(Res.string.save)
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val wide = maxWidth >= 640.dp
        Column(Modifier.fillMaxSize()) {
            EditorBar(title, cancel, save, onCancel, onSave)
            Column(
                Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(vertical = 12.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                if (!error.isNullOrEmpty()) {
                    Text(coreText(error), color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(horizontal = 16.dp).testTag("formError"))
                }
                content(wide)
                Button(onClick = onSave, modifier = Modifier.align(Alignment.CenterHorizontally).padding(bottom = 24.dp)) { Text(save) }
            }
        }
    }
}
