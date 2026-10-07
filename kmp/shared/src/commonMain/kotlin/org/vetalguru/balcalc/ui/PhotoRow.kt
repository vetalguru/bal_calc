package org.vetalguru.balcalc.ui

import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.decodeToImageBitmap
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.LocalPlatform
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*
import org.vetalguru.balcalc.shrinkImage

/** Picture bytes as an image, or null when they are not one. */
fun pictureOf(bytes: ByteArray?): ImageBitmap? = bytes?.let { runCatching { it.decodeToImageBitmap() }.getOrNull() }

/** A small square picture with rounded corners (lists, editors). */
@Composable
fun Thumbnail(image: ImageBitmap, size: Dp, modifier: Modifier = Modifier) = Image(
    image, null, contentScale = ContentScale.Crop,
    modifier = modifier.size(size).clip(RoundedCornerShape(8.dp)),
)

/**
 * The picture of a rifle or cartridge in its editor: choose one from the
 * files or gallery, take one with the camera, or remove it. Pictures are
 * stored shrunk (see [shrinkImage]).
 */
@Composable
fun PhotoRow(photo: ByteArray?, onPhoto: (ByteArray?) -> Unit) {
    val files = LocalPlatform.current.files
    val camera = LocalPlatform.current.camera
    val scope = rememberCoroutineScope()
    val image = remember(photo) { pictureOf(photo) }
    var bad by remember { mutableStateOf(false) }

    fun take(bytes: ByteArray?) {
        if (bytes == null) return
        val small = shrinkImage(bytes)
        bad = small == null
        if (small != null) onPhoto(small)
    }

    Row(horizontalArrangement = Arrangement.spacedBy(12.dp), verticalAlignment = Alignment.CenterVertically) {
        if (image != null) Thumbnail(image, 96.dp, Modifier.testTag("photo"))
        Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
            OutlinedButton(onClick = { scope.launch { take(files.openImage()) } }, modifier = Modifier.testTag("photoChoose")) {
                Text(stringResource(Res.string.photo_choose))
            }
            if (camera != null) {
                OutlinedButton(onClick = { scope.launch { take(camera.takePhoto()) } }) {
                    Text(stringResource(Res.string.photo_take))
                }
            }
            if (photo != null) {
                TextButton(onClick = { onPhoto(null) }, modifier = Modifier.testTag("photoRemove")) {
                    Text(stringResource(Res.string.photo_remove))
                }
            }
        }
    }
    if (bad) Text(stringResource(Res.string.photo_bad), color = MaterialTheme.colorScheme.error)
}
