package org.vetalguru.balcalc.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
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
import androidx.compose.ui.graphics.graphicsLayer
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

/**
 * A moving target from above: you at the bottom, the line of fire up to the
 * target, the way it moves at an angle to that line (0° away, 90° across);
 * mirrored when it moves to the left.
 */
@Composable
fun HeadingPicture(toRight: Boolean = true) {
    val line = LocalContentColor.current
    val accent = MaterialTheme.colorScheme.inversePrimary
    val angle = 60.0
    Box(Modifier.size(190.dp, 150.dp).padding(bottom = 6.dp).testTag("headingPicture")) {
        Canvas(Modifier.fillMaxSize().graphicsLayer { scaleX = if (toRight) 1f else -1f }) {
            val thin = 2.dp.toPx()
            val you = Offset(size.width * 0.3f, size.height - 8.dp.toPx())
            val target = Offset(size.width * 0.3f, size.height * 0.42f)
            // The line of fire, on past the target (where "0°, away" points).
            drawLine(line, you, target, thin)
            drawLine(
                line.copy(alpha = 0.6f), target, Offset(target.x, 4.dp.toPx()), thin,
                pathEffect = PathEffect.dashPathEffect(floatArrayOf(8f, 6f)),
            )
            drawCircle(line, 5.dp.toPx(), you)
            drawCircle(accent, 7.dp.toPx(), target, style = Stroke(thin))
            // Where the target goes: [angle] clockwise from "away".
            val a = (angle - 90.0) * PI / 180
            val len = size.width * 0.5f
            val dir = Offset(cos(a).toFloat(), sin(a).toFloat())
            val tip = target + dir * len
            drawLine(accent, target + dir * 9.dp.toPx(), tip, 2.5.dp.toPx())
            val back = dir * -10.dp.toPx()
            val side = Offset(-dir.y, dir.x) * 6.dp.toPx()
            drawLine(accent, tip, tip + back + side, 2.5.dp.toPx())
            drawLine(accent, tip, tip + back - side, 2.5.dp.toPx())
            // The angle between them.
            val ar = 28.dp.toPx()
            drawArc(line, -90f, angle.toFloat(), false, Offset(target.x - ar, target.y - ar), Size(ar * 2, ar * 2), style = Stroke(thin))
        }
        Text(
            "α",
            color = line,
            fontWeight = FontWeight.Bold,
            style = MaterialTheme.typography.titleMedium,
            modifier = Modifier.align(Alignment.TopStart).offset(x = if (toRight) 70.dp else 110.dp, y = 14.dp),
        )
    }
}

/** A label on a picture: the same weight as the others. */
@Composable
private fun BoxScope.PictureLabel(text: String, align: Alignment, x: Int = 0, y: Int = 0) {
    Text(
        text,
        color = LocalContentColor.current,
        fontWeight = FontWeight.Bold,
        style = MaterialTheme.typography.labelLarge,
        modifier = Modifier.align(align).offset(x.dp, y.dp),
    )
}

/** The shot angle from the side: you, the horizontal, the line up to the target; uphill is +. */
@Composable
fun LookAnglePicture() {
    val line = LocalContentColor.current
    val accent = MaterialTheme.colorScheme.inversePrimary
    val angle = 28f
    Box(Modifier.size(190.dp, 120.dp).padding(bottom = 6.dp).testTag("lookAnglePicture")) {
        Canvas(Modifier.fillMaxSize()) {
            val thin = 2.dp.toPx()
            val you = Offset(14.dp.toPx(), size.height - 12.dp.toPx())
            // The horizontal: what 0° is.
            drawLine(
                line, you, Offset(size.width - 4.dp.toPx(), you.y), thin,
                pathEffect = PathEffect.dashPathEffect(floatArrayOf(8f, 6f)),
            )
            val a = -angle * PI.toFloat() / 180
            val len = size.width - 44.dp.toPx()
            val target = you + Offset(cos(a), sin(a)) * len
            drawLine(accent, you, target, 2.5.dp.toPx())
            drawCircle(line, 5.dp.toPx(), you)
            drawCircle(accent, 7.dp.toPx(), target, style = Stroke(thin))
            val ar = 62.dp.toPx()
            drawArc(line, -angle, angle, false, you - Offset(ar, ar), Size(ar * 2, ar * 2), style = Stroke(thin))
        }
        PictureLabel("α +", Alignment.BottomStart, x = 84, y = (-12))
    }
}

/** Where the wind blows from, from above: the target ahead, the degrees round you, a wind from 90°. */
@Composable
fun WindClockPicture() {
    val line = LocalContentColor.current
    val accent = MaterialTheme.colorScheme.inversePrimary
    Box(Modifier.size(190.dp, 150.dp).padding(bottom = 6.dp).testTag("windClockPicture")) {
        Canvas(Modifier.fillMaxSize()) {
            val thin = 2.dp.toPx()
            val c = center
            val r = size.height / 2 - 20.dp.toPx()
            drawCircle(line, r, c, style = Stroke(thin))
            drawCircle(line, 4.dp.toPx(), c) // you
            // The line of fire, up to the target.
            drawLine(line, c, c - Offset(0f, r - 8.dp.toPx()), thin, pathEffect = PathEffect.dashPathEffect(floatArrayOf(8f, 6f)))
            drawCircle(line, 6.dp.toPx(), c - Offset(0f, r), style = Stroke(thin))
            // The wind, from the right towards you.
            val from = c + Offset(r - 4.dp.toPx(), 0f)
            val to = c + Offset(14.dp.toPx(), 0f)
            drawLine(accent, from, to, 2.5.dp.toPx())
            val h = 9.dp.toPx()
            drawLine(accent, to, to + Offset(h, -h * 0.6f), 2.5.dp.toPx())
            drawLine(accent, to, to + Offset(h, h * 0.6f), 2.5.dp.toPx())
        }
        PictureLabel("0°", Alignment.TopCenter, x = 20, y = 2)
        PictureLabel("90°", Alignment.CenterEnd, x = (-2), y = (-14))
        PictureLabel("180°", Alignment.BottomCenter, y = 4)
        PictureLabel("270°", Alignment.CenterStart, x = 2, y = (-14))
    }
}

/** Latitude on the globe: the angle from the equator, north +. */
@Composable
fun LatitudePicture() {
    val line = LocalContentColor.current
    val accent = MaterialTheme.colorScheme.inversePrimary
    val angle = 40f
    Box(Modifier.size(190.dp, 130.dp).padding(bottom = 6.dp).testTag("latitudePicture")) {
        Canvas(Modifier.fillMaxSize()) {
            val thin = 2.dp.toPx()
            val c = center
            val r = size.height / 2 - 6.dp.toPx()
            drawCircle(line, r, c, style = Stroke(thin))
            // The equator and the axis.
            drawLine(line, c - Offset(r, 0f), c + Offset(r, 0f), thin, pathEffect = PathEffect.dashPathEffect(floatArrayOf(8f, 6f)))
            drawLine(line, c - Offset(0f, r), c + Offset(0f, r), thin, pathEffect = PathEffect.dashPathEffect(floatArrayOf(4f, 6f)))
            val a = -angle * PI.toFloat() / 180
            val place = c + Offset(cos(a), sin(a)) * r
            drawLine(accent, c, place, 2.5.dp.toPx())
            drawCircle(accent, 5.dp.toPx(), place)
            val ar = r * 0.45f
            drawArc(line, -angle, angle, false, c - Offset(ar, ar), Size(ar * 2, ar * 2), style = Stroke(thin))
        }
        PictureLabel("+", Alignment.TopCenter, x = 14, y = 4)
        PictureLabel("−", Alignment.BottomCenter, x = 14, y = (-6))
    }
}

/** The direction of fire from above: north up, the angle clockwise from it. */
@Composable
fun AzimuthPicture() {
    val line = LocalContentColor.current
    val accent = MaterialTheme.colorScheme.inversePrimary
    val angle = 60f
    Box(Modifier.size(190.dp, 130.dp).padding(bottom = 6.dp).testTag("azimuthPicture")) {
        Canvas(Modifier.fillMaxSize()) {
            val thin = 2.dp.toPx()
            val c = Offset(size.width / 2, size.height - 12.dp.toPx())
            val len = size.height - 34.dp.toPx()
            // North, 0°.
            val north = c - Offset(0f, len)
            drawLine(line, c, north, thin)
            drawLine(line, north, north + Offset(-5.dp.toPx(), 9.dp.toPx()), thin)
            drawLine(line, north, north + Offset(5.dp.toPx(), 9.dp.toPx()), thin)
            // The line of fire.
            val a = (angle - 90f) * PI.toFloat() / 180
            val dir = Offset(cos(a), sin(a))
            drawLine(accent, c, c + dir * len, 2.5.dp.toPx())
            drawCircle(accent, 6.dp.toPx(), c + dir * len, style = Stroke(thin))
            drawCircle(line, 4.dp.toPx(), c)
            val ar = len * 0.5f
            drawArc(line, -90f, angle, false, c - Offset(ar, ar), Size(ar * 2, ar * 2), style = Stroke(thin))
        }
        PictureLabel("0°", Alignment.TopCenter, y = 0)
        PictureLabel("α", Alignment.Center, x = 18, y = (-8))
    }
}

/** Point of impact: the aim point, the hit up and to the right of it; both are +. */
@Composable
fun ZeroOffsetPicture() {
    val line = LocalContentColor.current
    val accent = MaterialTheme.colorScheme.inversePrimary
    Box(Modifier.size(190.dp, 130.dp).padding(bottom = 6.dp).testTag("zeroOffsetPicture")) {
        Canvas(Modifier.fillMaxSize()) {
            val thin = 2.dp.toPx()
            val c = center
            val r = size.height / 2 - 6.dp.toPx()
            drawCircle(line, r, c, style = Stroke(thin))
            drawCircle(line, r * 0.55f, c, style = Stroke(thin))
            // The aim point.
            val k = 10.dp.toPx()
            drawLine(line, c - Offset(k, 0f), c + Offset(k, 0f), thin)
            drawLine(line, c - Offset(0f, k), c + Offset(0f, k), thin)
            // The hit, and how far up and right it is.
            val hit = c + Offset(r * 0.55f, -r * 0.5f)
            val dash = PathEffect.dashPathEffect(floatArrayOf(6f, 5f))
            drawLine(accent, c, Offset(hit.x, c.y), thin, pathEffect = dash)
            drawLine(accent, Offset(hit.x, c.y), hit, thin, pathEffect = dash)
            drawCircle(accent, 5.dp.toPx(), hit)
        }
        PictureLabel("+", Alignment.Center, x = 20, y = 10)
        PictureLabel("+", Alignment.Center, x = 48, y = (-16))
    }
}

/** What to tap on the photo, in the colours it marks them with: the two scale points, the aim point, the holes. */
@Composable
fun GroupMarkingPicture() {
    val line = LocalContentColor.current
    Box(Modifier.size(190.dp, 130.dp).padding(bottom = 6.dp).testTag("groupMarkingPicture")) {
        Canvas(Modifier.fillMaxSize()) {
            val thin = 2.dp.toPx()
            val paper = 4.dp.toPx()
            drawRect(line, Offset(paper, paper), Size(size.width - paper * 2, size.height - paper * 2), style = Stroke(thin))
            val r = 5.dp.toPx()
            // The scale.
            val a = Offset(size.width * 0.15f, size.height * 0.85f)
            val b = Offset(size.width * 0.85f, size.height * 0.85f)
            drawLine(GroupScaleColor, a, b, thin)
            drawCircle(GroupScaleColor, r, a, style = Stroke(thin))
            drawCircle(GroupScaleColor, r, b, style = Stroke(thin))
            // The aim point.
            val c = Offset(size.width * 0.42f, size.height * 0.45f)
            val k = 12.dp.toPx()
            drawLine(GroupAimColor, c - Offset(k, 0f), c + Offset(k, 0f), thin)
            drawLine(GroupAimColor, c - Offset(0f, k), c + Offset(0f, k), thin)
            // The holes.
            for ((x, y) in listOf(0.58f to 0.28f, 0.66f to 0.42f, 0.56f to 0.5f, 0.7f to 0.25f)) {
                drawCircle(GroupHoleColor, r, Offset(size.width * x, size.height * y), style = Stroke(thin))
            }
        }
    }
}
