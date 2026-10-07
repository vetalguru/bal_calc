package org.vetalguru.balcalc.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.rotate
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

/** Cant, seen from behind the rifle: the scope and stock turned clockwise off the plumb line; clockwise is +. */
@Composable
fun CantPicture() {
    val line = LocalContentColor.current
    val accent = MaterialTheme.colorScheme.inversePrimary
    val tilt = 20f
    Box(Modifier.size(190.dp, 150.dp).padding(bottom = 6.dp).testTag("cantPicture")) {
        Canvas(Modifier.fillMaxSize()) {
            val c = Offset(size.width / 2, size.height * 0.42f)
            val r = size.height * 0.22f
            val thin = 2.dp.toPx()
            // The plumb line: what "upright" is.
            drawLine(
                line, Offset(c.x, 2f), Offset(c.x, size.height - 2f), thin,
                pathEffect = PathEffect.dashPathEffect(floatArrayOf(8f, 6f)),
            )
            rotate(tilt, pivot = c) {
                // The scope from behind with its crosshair, the stock under it.
                drawCircle(accent, r, c, style = Stroke(2.5.dp.toPx()))
                drawLine(accent, Offset(c.x - r, c.y), Offset(c.x + r, c.y), thin)
                drawLine(accent, Offset(c.x, c.y - r), Offset(c.x, c.y + r), thin)
                val top = c.y + r + 4.dp.toPx()
                val bottom = size.height - 4.dp.toPx()
                val stock = Path().apply {
                    moveTo(c.x - r * 0.35f, top)
                    lineTo(c.x + r * 0.35f, top)
                    lineTo(c.x + r * 0.6f, bottom)
                    lineTo(c.x - r * 0.6f, bottom)
                    close()
                }
                drawPath(stock, accent, style = Stroke(2.dp.toPx()))
            }
            // The arrow from the plumb line to the tilted crosshair: clockwise.
            val ar = r + 18.dp.toPx()
            drawArc(line, -90f, tilt, false, Offset(c.x - ar, c.y - ar), Size(ar * 2, ar * 2), style = Stroke(thin))
            val a = (-90.0 + tilt) * PI / 180
            val tip = Offset(c.x + (ar * cos(a)).toFloat(), c.y + (ar * sin(a)).toFloat())
            val back = Offset((sin(a)).toFloat(), (-cos(a)).toFloat()) // against the arrow's way
            val side = Offset((cos(a)).toFloat(), (sin(a)).toFloat())
            val h = 9.dp.toPx()
            drawLine(line, tip, tip + back * h + side * (h * 0.6f), thin)
            drawLine(line, tip, tip + back * h - side * (h * 0.6f), thin)
        }
        Text(
            "+",
            color = line,
            fontWeight = FontWeight.Bold,
            style = MaterialTheme.typography.titleMedium,
            modifier = Modifier.align(Alignment.TopCenter).offset(x = 36.dp, y = (-4).dp),
        )
    }
}
