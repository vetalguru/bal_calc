package org.vetalguru.balcalc.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.TextAutoSize
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.PI
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.roundToInt
import kotlin.math.sin
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** A small ± button of the controller. */
@Composable
internal fun Step(label: String, width: Dp = 50.dp, onClick: () -> Unit) {
    FilledTonalButton(
        onClick = onClick,
        contentPadding = PaddingValues(horizontal = 2.dp),
        modifier = Modifier.width(width).height(40.dp),
    ) { Text(label, fontSize = 13.sp, maxLines = 1) }
}

/** The big distance input: whole metres, committed on Done or focus loss. */
@Composable
internal fun RangeField(rangeM: Double, onRange: (Double) -> Unit, modifier: Modifier, showUnit: Boolean = true) {
    var text by remember { mutableStateOf(rangeM.roundToInt().toString()) }
    var focused by remember { mutableStateOf(false) }
    LaunchedEffect(rangeM, focused) { if (!focused) text = rangeM.roundToInt().toString() }
    val focus = LocalFocusManager.current
    OutlinedTextField(
        value = text,
        onValueChange = { t -> if (t.length <= 4 && t.all(Char::isDigit)) text = t },
        singleLine = true,
        textStyle = MaterialTheme.typography.headlineMedium.copy(fontWeight = FontWeight.Bold, textAlign = TextAlign.Center),
        suffix = if (showUnit) ({ Text(stringResource(Res.string.unit_m)) }) else null,
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number, imeAction = ImeAction.Done),
        keyboardActions = KeyboardActions(onDone = { focus.clearFocus() }),
        modifier = modifier.testTag("range").onFocusChanged {
            if (focused && !it.isFocused) text.toIntOrNull()?.let { m -> onRange(m.toDouble()) }
            focused = it.isFocused
        },
    )
}

/** Clock hour of a direction (0° = 12 o'clock = from the target). */
fun clockHour(deg: Double): Int {
    val h = (((deg % 360) + 360) % 360 / 30).roundToInt() % 12
    return if (h == 0) 12 else h
}
