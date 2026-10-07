package org.vetalguru.balcalc.ui

import androidx.compose.foundation.Canvas
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Fill
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipPath
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.sp
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sqrt
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.double
import kotlinx.serialization.json.doubleOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import org.vetalguru.balcalc.core.Api

/**
 * A reticle (the core's JSON drawing in nominal mrad, y up; see
 * applogic/importers.h) with the target marked where it should go. Zooms
 * so the target and its surroundings fit. No drawing: a crosshair with
 * 1 mrad ticks.
 */
/**
 * Something drawn on the reticle at (x, y) in reticle mrad, y up: another
 * target's hold (a ring and its name), or with [ring] false a label only
 * (the range at a mark).
 */
data class ReticleMark(val x: Double, val y: Double, val label: String, val ring: Boolean = true)

@Composable
fun ReticleView(
    definition: String,
    targetX: Double,
    targetY: Double,
    modifier: Modifier = Modifier,
    marks: List<ReticleMark> = emptyList(),
    zoom: Float = 1f,
) {
    val drawing = remember(definition) {
        if (definition.isEmpty()) null
        else runCatching { Api.json.parseToJsonElement(definition).jsonObject }.getOrNull()
    }
    val measurer = rememberTextMeasurer()
    val colors = appColors
    Canvas(modifier.clipToBounds().testTag("reticle")) {
        val reach = (marks.filter { it.ring }.map { max(abs(it.x), abs(it.y)) } + max(abs(targetX), abs(targetY))).max()
        val sizeArr = drawing?.get("size")?.jsonArray
        val half = if (sizeArr != null && sizeArr.size >= 2) max(sizeArr[0].jsonPrimitive.double, sizeArr[1].jsonPrimitive.double) / 2 else 10.0
        // The reticle around the centre, widened when the target needs room.
        val span = maxOf(2.5, min(half, 6.0), reach * 1.25 + 1) / zoom.coerceIn(0.25f, 20f)

        val side = min(size.width, size.height)
        val cx = size.width / 2
        val cy = size.height / 2
        val k = side / 2 / span // px per mrad
        fun x(v: Double) = (cx + v * k).toFloat()
        fun y(v: Double) = (cy - v * k).toFloat()
        fun w(v: Double?) = max(1.0, (v ?: 0.0) * k).toFloat()

        val field = Path().apply { addOval(Rect(Offset(cx, cy), side / 2 - 1)) }
        drawPath(field, colors.reticleField)
        clipPath(field) {
            val elements = drawing?.get("elements")?.jsonArray?.map { it.jsonObject } ?: crosshair(span)
            for (e in elements) drawElement(e, ::x, ::y, ::w, k, measurer, colors.reticleInk)
        }
        drawCircle(colors.reticleEdge, radius = side / 2 - 1, center = Offset(cx, cy), style = Stroke(2f))

        // The other targets: a small ring and the name.
        val small = TextStyle(color = colors.target, fontSize = (11f).sp)
        for (m in marks) {
            val o = Offset(x(m.x), y(m.y))
            if ((o - Offset(cx, cy)).getDistance() > side / 2 - 8) continue // outside the field of view
            if (m.ring) drawCircle(colors.target.copy(alpha = 0.7f), 5f, o, style = Stroke(1.5f))
            val label = measurer.measure(m.label, small)
            drawText(label, topLeft = Offset(o.x + 7f, o.y - label.size.height / 2f))
        }

        // Target.
        val t = Offset(x(targetX), y(targetY))
        drawCircle(colors.target.copy(alpha = 0.25f), 9f, t)
        drawCircle(colors.target, 9f, t, style = Stroke(2.5f))
        for ((a, b) in listOf(-15f to -5f, 5f to 15f)) {
            drawLine(colors.target, Offset(t.x + a, t.y), Offset(t.x + b, t.y), 2.5f)
            drawLine(colors.target, Offset(t.x, t.y + a), Offset(t.x, t.y + b), 2.5f)
        }
    }
}

private fun crosshair(span: Double): List<JsonObject> = buildList {
    fun line(x1: Double, y1: Double, x2: Double, y2: Double) = add(JsonObject(mapOf(
        "t" to JsonPrimitive("line"), "x1" to JsonPrimitive(x1), "y1" to JsonPrimitive(y1),
        "x2" to JsonPrimitive(x2), "y2" to JsonPrimitive(y2), "w" to JsonPrimitive(0.02),
    )))
    line(-span, 0.0, span, 0.0)
    line(0.0, -span, 0.0, span)
    var m = -floor(span)
    while (m <= span) {
        if (m != 0.0) {
            line(m, -0.15, m, 0.15)
            line(-0.15, m, 0.15, m)
        }
        m += 1
    }
}

private fun JsonObject.num(key: String) = this[key]?.jsonPrimitive?.doubleOrNull ?: 0.0

private fun DrawScope.drawElement(
    e: JsonObject,
    x: (Double) -> Float,
    y: (Double) -> Float,
    w: (Double?) -> Float,
    k: Double,
    measurer: androidx.compose.ui.text.TextMeasurer,
    ink: Color,
) {
    val fill = e["fill"]?.jsonPrimitive?.booleanOrNull == true
    when (e["t"]?.jsonPrimitive?.content) {
        "line" -> drawLine(ink, Offset(x(e.num("x1")), y(e.num("y1"))), Offset(x(e.num("x2")), y(e.num("y2"))), w(e.num("w")))
        "circle" -> {
            val r = max(0.8, e.num("r") * k).toFloat()
            drawCircle(ink, r, Offset(x(e.num("x")), y(e.num("y"))), style = if (fill) Fill else Stroke(w(e.num("w"))))
        }
        "text" -> {
            val layout = measurer.measure(e["s"]?.jsonPrimitive?.content.orEmpty(), TextStyle(color = ink, fontSize = max(8.0, e.num("h") * k).toFloat().let { (it / density).sp }))
            drawText(layout, topLeft = Offset(x(e.num("x")), y(e.num("y")) - layout.firstBaseline))
        }
        "path" -> {
            val p = Path()
            var px = 0.0
            var py = 0.0
            for (cmd in e["d"]?.jsonArray.orEmpty().map { it.jsonArray }) {
                val c = cmd[0].jsonPrimitive.content
                val x1 = cmd[1].jsonPrimitive.double
                val y1 = cmd[2].jsonPrimitive.double
                when (c) {
                    "M" -> p.moveTo(x(x1), y(y1))
                    "L" -> p.lineTo(x(x1), y(y1))
                    "A" -> arcTo(p, px, py, x1, y1, cmd[3].jsonPrimitive.double, cmd.flag(4), cmd.flag(5), x, y, k)
                }
                px = x1
                py = y1
            }
            drawPath(p, ink, style = if (fill) Fill else Stroke(w(e.num("w"))))
        }
    }
}

private fun JsonArray.flag(i: Int): Boolean =
    getOrNull(i)?.jsonPrimitive?.let { it.booleanOrNull ?: (it.doubleOrNull?.let { d -> d != 0.0 }) } ?: false

/** SVG-style arc from (x0, y0) to (x1, y1) with radius r (drawing units). */
private fun arcTo(
    p: Path, x0: Double, y0: Double, x1: Double, y1: Double, r: Double,
    clockwise: Boolean, major: Boolean, x: (Double) -> Float, y: (Double) -> Float, k: Double,
) {
    val dx = (x1 - x0) / 2
    val dy = (y1 - y0) / 2
    val d2 = dx * dx + dy * dy
    if (d2 == 0.0) return
    val rr = max(r, sqrt(d2))
    val h = sqrt(max(0.0, rr * rr - d2))
    val len = sqrt(d2)
    // Two candidate centres either side of the chord.
    val sign = if (clockwise == major) 1 else -1
    val ccx = x0 + dx + sign * h * (-dy / len)
    val ccy = y0 + dy + sign * h * (dx / len)
    // Angles in screen space (y down), as the canvas measures them.
    val a0 = atan2(-(y0 - ccy), x0 - ccx)
    val a1 = atan2(-(y1 - ccy), x1 - ccx)
    var sweep = a1 - a0
    if (clockwise) {
        while (sweep <= 0) sweep += 2 * PI
    } else {
        while (sweep >= 0) sweep -= 2 * PI
    }
    val rpx = (rr * k).toFloat()
    val c = Offset(x(ccx), y(ccy))
    p.arcTo(Rect(c, rpx), (a0 * 180 / PI).toFloat(), (sweep * 180 / PI).toFloat(), false)
}

