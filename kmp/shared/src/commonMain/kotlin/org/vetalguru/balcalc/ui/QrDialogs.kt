package org.vetalguru.balcalc.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.LocalPlatform
import org.vetalguru.balcalc.QrShare
import org.vetalguru.balcalc.coreText
import org.vetalguru.balcalc.decodeQrImage
import org.vetalguru.balcalc.qrMatrix
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** A rifle or cartridge as QR codes, one part at a time, black on white. */
@Composable
fun QrShowDialog(title: String, parts: List<String>, onClose: () -> Unit) {
    var part by remember { mutableIntStateOf(0) }
    val matrix = remember(parts, part) { qrMatrix(parts[part]) }
    AlertDialog(
        onDismissRequest = onClose,
        title = { Text(title) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                // White with a quiet zone of four modules, whatever the theme.
                Canvas(Modifier.fillMaxWidth().aspectRatio(1f).testTag("qrCode")) {
                    val cell = size.width / (matrix.size + 8)
                    drawRect(Color.White)
                    for (y in 0 until matrix.size) for (x in 0 until matrix.size) {
                        if (matrix[x, y]) {
                            drawRect(Color.Black, Offset((x + 4) * cell, (y + 4) * cell), Size(cell + 0.5f, cell + 0.5f))
                        }
                    }
                }
                if (parts.size > 1) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        TextButton(onClick = { part-- }, enabled = part > 0) { Text("‹") }
                        Text(stringResource(Res.string.qr_part, part + 1, parts.size), modifier = Modifier.testTag("qrPart"))
                        TextButton(onClick = { part++ }, enabled = part < parts.lastIndex, modifier = Modifier.testTag("qrNext")) { Text("›") }
                    }
                }
                Text(stringResource(Res.string.qr_hint), color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        },
        confirmButton = { TextButton(onClick = onClose) { Text(stringResource(Res.string.close)) } },
    )
}

/**
 * Reads QR codes (camera or pictures) until every part of a shared rifle or
 * cartridge is in, then imports it.
 */
@Composable
fun QrImportDialog(model: AppModel, onImported: () -> Unit, onClose: () -> Unit) {
    val files = LocalPlatform.current.files
    val camera = LocalPlatform.current.camera
    val scope = rememberCoroutineScope()
    val collector = remember { QrShare.Collector() }
    var have by remember { mutableIntStateOf(0) }
    var error by remember { mutableStateOf<String?>(null) }
    val notFound = stringResource(Res.string.qr_not_found)

    fun take(text: String?) {
        if (text == null) return
        if (!collector.add(text)) {
            error = notFound
            return
        }
        error = null
        have = collector.have
        if (collector.complete) {
            scope.launch {
                try {
                    model.importShared(collector.json())
                    onImported()
                } catch (e: Exception) {
                    error = e.message
                }
            }
        }
    }

    AlertDialog(
        onDismissRequest = onClose,
        title = { Text(stringResource(Res.string.qr_import_title)) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(stringResource(Res.string.qr_import_hint), color = MaterialTheme.colorScheme.onSurfaceVariant)
                if (collector.total > 1) {
                    Text(stringResource(Res.string.qr_progress, have, collector.total), modifier = Modifier.testTag("qrProgress"))
                }
                if (camera != null) {
                    Button(onClick = { scope.launch { take(camera.scanQr()) } }, modifier = Modifier.testTag("qrScan")) {
                        Text(stringResource(Res.string.qr_scan))
                    }
                }
                OutlinedButton(onClick = {
                    scope.launch {
                        val image = files.openImage() ?: return@launch
                        take(decodeQrImage(image) ?: "")
                    }
                }, modifier = Modifier.testTag("qrPicture")) { Text(stringResource(Res.string.qr_picture)) }
                error?.let { Text(coreText(it), color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("qrError")) }
            }
        },
        confirmButton = { TextButton(onClick = onClose) { Text(stringResource(Res.string.cancel)) } },
    )
}
