package org.vetalguru.balcalc.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableDoubleStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import kotlin.math.abs
import kotlin.math.min
import kotlin.math.roundToInt
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.decodeToImageBitmap
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.LocalPlatform
import org.vetalguru.balcalc.PhotoPoint
import org.vetalguru.balcalc.analyzeGroup
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

private enum class Marking { Scale, Aim, Holes }

private val ScaleColor = Color(0xFF1E88E5)
private val AimColor = Color(0xFF43A047)
private val HoleColor = Color(0xFFE53935)

/**
 * A group shot at a target, measured on its photo: the spread, where its
 * centre is, and what to do with it (this cartridge's zero shift, the
 * rifle's precision for the hit chance).
 */
@Composable
fun GroupScreen(model: AppModel, onBack: () -> Unit) {
    val files = LocalPlatform.current.files
    val scope = rememberCoroutineScope()
    val st = model.state
    var photo by remember { mutableStateOf<ImageBitmap?>(null) }
    var mode by remember { mutableStateOf(Marking.Scale) }
    val scale = remember { mutableStateListOf<PhotoPoint>() }
    var aim by remember { mutableStateOf<PhotoPoint?>(null) }
    val holes = remember { mutableStateListOf<PhotoPoint>() }
    var scaleCm by remember { mutableDoubleStateOf(10.0) }
    var rangeM by remember { mutableDoubleStateOf(st.currentPair?.zeroRangeM ?: st.conditions.targetRangeM) }
    var saved by remember { mutableStateOf(false) }

    val stats = aim?.let { a ->
        if (scale.size == 2) analyzeGroup(holes.toList(), a, scale[0], scale[1], scaleCm, rangeM) else null
    }
    val unit = stringResource(if (st.moa) Res.string.unit_moa else Res.string.unit_mrad)

    Column(Modifier.fillMaxSize()) {
        Row(Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
            TextButton(onClick = onBack) { Text(stringResource(Res.string.back)) }
            Text(stringResource(Res.string.group_title), style = MaterialTheme.typography.titleLarge, maxLines = 1,
                overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f))
        }
        Column(
            Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(horizontal = 12.dp, vertical = 8.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Button(onClick = {
                scope.launch {
                    val bytes = files.openImage() ?: return@launch
                    photo = runCatching { bytes.decodeToImageBitmap() }.getOrNull()
                    scale.clear(); holes.clear(); aim = null; mode = Marking.Scale; saved = false
                }
            }, modifier = Modifier.testTag("groupPhoto")) { Text(stringResource(Res.string.group_photo)) }
            Text(stringResource(Res.string.group_hint), color = MaterialTheme.colorScheme.onSurfaceVariant)

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                FilterChip(mode == Marking.Scale, { mode = Marking.Scale }, { Text(stringResource(Res.string.mode_scale)) },
                    modifier = Modifier.testTag("markScale"))
                FilterChip(mode == Marking.Aim, { mode = Marking.Aim }, { Text(stringResource(Res.string.mode_aim)) },
                    modifier = Modifier.testTag("markAim"))
                FilterChip(mode == Marking.Holes, { mode = Marking.Holes }, { Text(stringResource(Res.string.mode_holes)) },
                    modifier = Modifier.testTag("markHoles"))
                TextButton(onClick = {
                    when (mode) {
                        Marking.Scale -> scale.removeLastOrNull()
                        Marking.Aim -> aim = null
                        Marking.Holes -> holes.removeLastOrNull()
                    }
                    saved = false
                }) { Text(stringResource(Res.string.group_undo)) }
            }

            photo?.let { img ->
                TargetPhoto(img, scale, aim, holes, stats?.let { s -> s.centreRightCm to s.centreUpCm }, scaleCm) { p ->
                    saved = false
                    when (mode) {
                        Marking.Scale -> {
                            if (scale.size == 2) scale.clear()
                            scale.add(p)
                            if (scale.size == 2) mode = Marking.Aim
                        }
                        Marking.Aim -> { aim = p; mode = Marking.Holes }
                        Marking.Holes -> holes.add(p)
                    }
                }
            }

            Fields(
                false,
                { m -> NumberField(stringResource(Res.string.group_scale_cm), scaleCm, { scaleCm = it }, m,
                    stringResource(Res.string.unit_cm), decimals = 1, from = 0.1, to = 500.0, tag = "groupScaleCm") },
                { m -> NumberField(stringResource(Res.string.distance), rangeM, { rangeM = it }, m,
                    stringResource(Res.string.unit_m), decimals = 0, from = 5.0, to = 3000.0, tag = "groupRange") },
            )

            if (stats == null) {
                Text(stringResource(Res.string.group_need), color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                Card(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                        Text(stringResource(Res.string.group_shots, stats.shots, rangeM.roundToInt()), fontWeight = FontWeight.Bold)
                        Text(stringResource(Res.string.group_es, stats.extremeSpreadCm.fixed(1),
                            stats.moa(stats.extremeSpreadCm).fixed(2), stats.mrad(stats.extremeSpreadCm).fixed(2)),
                            modifier = Modifier.testTag("groupEs"))
                        Text(stringResource(Res.string.group_mr, stats.meanRadiusCm.fixed(1)))
                        Text(stringResource(Res.string.group_centre, stats.centreUpCm.fixed(1), stats.centreRightCm.fixed(1)),
                            modifier = Modifier.testTag("groupCentre"))
                        // Dialling the centre onto the aim point: the opposite way.
                        val up = -stats.centreUpCm
                        val right = -stats.centreRightCm
                        val angle = { cm: Double -> if (st.moa) stats.moa(cm) else stats.mrad(cm) }
                        Text(stringResource(Res.string.group_dial, angle(up).fixed(2), angle(right).fixed(2), unit))
                    }
                }
                val pair = st.currentPair
                if (pair != null && st.hasPair && abs(rangeM - pair.zeroRangeM) <= 0.05 * pair.zeroRangeM) {
                    Text(stringResource(Res.string.group_zero_hint, pair.zeroRangeM.roundToInt()),
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                    OutlinedButton(onClick = {
                        scope.launch { saved = model.setZeroOffset(stats.centreUpCm, stats.centreRightCm) == null }
                    }, modifier = Modifier.testTag("groupZero")) { Text(stringResource(Res.string.group_zero)) }
                }
                OutlinedButton(onClick = {
                    scope.launch { saved = runCatching { model.setRiflePrecision(stats.equivalentFiveShotMoa) }.isSuccess }
                }, modifier = Modifier.testTag("groupWez")) {
                    Text(stringResource(Res.string.group_wez, stats.equivalentFiveShotMoa.fixed(2)))
                }
                if (saved) Text(stringResource(Res.string.group_saved), color = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.testTag("groupSaved"))
            }
        }
    }
}

/** The photo fitted to the width, with the marks; a tap gives its pixel. */
@Composable
private fun TargetPhoto(
    img: ImageBitmap,
    scale: List<PhotoPoint>,
    aim: PhotoPoint?,
    holes: List<PhotoPoint>,
    centreCm: Pair<Double, Double>?,
    scaleCm: Double,
    onTap: (PhotoPoint) -> Unit,
) {
    val ratio = img.width.toFloat() / img.height
    val centreColor = Transonic
    Canvas(
        Modifier.fillMaxWidth().aspectRatio(ratio).background(Color.Black).testTag("groupCanvas")
            .pointerInput(img) {
                detectTapGestures { o ->
                    val k = img.width.toFloat() / size.width
                    onTap(PhotoPoint(o.x * k.toDouble(), o.y * k.toDouble()))
                }
            },
    ) {
        val k = size.width / img.width
        drawImage(img, srcOffset = IntOffset.Zero, srcSize = IntSize(img.width, img.height), dstSize = IntSize(size.width.toInt(), size.height.toInt()))
        fun at(p: PhotoPoint) = Offset((p.x * k).toFloat(), (p.y * k).toFloat())
        val r = 6.dp.toPx()
        val stroke = Stroke(width = 2.dp.toPx())
        scale.forEach { drawCircle(ScaleColor, r, at(it), style = stroke) }
        if (scale.size == 2) drawLine(ScaleColor, at(scale[0]), at(scale[1]), strokeWidth = 2.dp.toPx())
        aim?.let {
            val c = at(it)
            drawLine(AimColor, c - Offset(3 * r, 0f), c + Offset(3 * r, 0f), strokeWidth = 2.dp.toPx())
            drawLine(AimColor, c - Offset(0f, 3 * r), c + Offset(0f, 3 * r), strokeWidth = 2.dp.toPx())
        }
        holes.forEach { drawCircle(HoleColor, r, at(it), style = stroke) }
        // The centre of the group, from the aim point and the scale.
        if (aim != null && centreCm != null && scale.size == 2) {
            val px = kotlin.math.hypot(scale[1].x - scale[0].x, scale[1].y - scale[0].y) / scaleCm
            val c = at(PhotoPoint(aim.x + centreCm.first * px, aim.y - centreCm.second * px))
            drawCircle(centreColor, r / 2, c)
            drawCircle(centreColor, min(size.width, size.height) / 40, c, style = stroke)
        }
    }
}
