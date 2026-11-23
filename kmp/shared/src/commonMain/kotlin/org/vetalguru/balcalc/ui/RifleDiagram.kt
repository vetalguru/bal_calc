package org.vetalguru.balcalc.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.layout.positionInParent
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import org.jetbrains.compose.resources.stringResource
import org.jetbrains.compose.resources.vectorResource
import org.vetalguru.balcalc.core.RifleForm
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

// A rifle drawn as a side view: stock and action, barrel and muzzle device,
// the suppressor, rail and rings, the scope, and the line of sight to a
// target at the zero.
// Each part is a step of the rifle's page: a tap on it opens its fields.
// Coordinates are fractions of the drawing's width (x) and height (y).

/** The part of the drawing at ([x], [y]) (fractions), or null. */
internal fun partAt(x: Float, y: Float): WizardStep? = when {
    x >= 0.89f && y in 0.16f..0.48f -> WizardStep.Zero
    x in 0.27f..0.53f && y in 0.40f..0.485f -> WizardStep.Mount
    x in 0.65f..0.84f && y in 0.30f..0.47f -> WizardStep.Mount // the sight height
    x in 0.17f..0.65f && y < 0.43f -> WizardStep.Scope
    x >= 0.895f && y in 0.48f..0.80f -> WizardStep.Suppressor
    x in 0.52f..0.895f && y in 0.45f..0.80f -> WizardStep.Barrel
    x < 0.53f && y >= 0.45f -> WizardStep.Rifle
    else -> null
}

/**
 * The rifle with [f]'s values on its parts; [selected] is drawn in the
 * accent colour, [done] parts solid, the rest faint. A tap on a part calls
 * [onSelect] with its step.
 */
@Composable
internal fun RifleDiagram(
    f: RifleForm,
    selected: WizardStep,
    done: Set<WizardStep>,
    onSelect: (WizardStep) -> Unit,
    modifier: Modifier = Modifier,
) {
    val select by rememberUpdatedState(onSelect)
    val accent = MaterialTheme.colorScheme.primary
    val solid = MaterialTheme.colorScheme.onSurfaceVariant
    val faint = solid.copy(alpha = 0.3f)
    val text = MaterialTheme.colorScheme.onSurface
    fun color(step: WizardStep) = when (step) {
        selected -> accent
        in done -> solid
        else -> faint
    }
    val measurer = rememberTextMeasurer()
    val style = MaterialTheme.typography.labelMedium
    val cm = stringResource(Res.string.unit_cm)
    val inch = stringResource(Res.string.unit_in)
    val m = stringResource(Res.string.unit_m)
    val rail = stringResource(if (f.railSlopeUnits == "mrad") Res.string.unit_mrad else Res.string.unit_moa)
    val clicks = stringResource(
        when (f.clickUnits) {
            "moa" -> Res.string.unit_moa
            "smoa" -> Res.string.click_smoa
            "cm100m" -> Res.string.click_cm100m
            else -> Res.string.unit_mrad
        },
    )
    val scopeText = listOf(
        listOf(f.scopeMaker, f.scopeModel).filter { it.isNotBlank() }.joinToString(" ").ifBlank { stringResource(Res.string.scope) },
        "${formatNumber(f.clickValue, 4)} $clicks",
    ).joinToString(" · ")
    val rifleText = listOf(f.name.ifBlank { stringResource(Res.string.rifle) }, f.caliber).filter { it.isNotBlank() }.joinToString(" · ")
    val barrelText = listOfNotNull(
        f.barrelIn.takeIf { it > 0 }?.let { "${formatNumber(it, 1)} $inch" },
        f.twistIn.takeIf { it > 0 }?.let { "1:${formatNumber(it, 2)}" + if (f.twistLeft) " ↺" else "" },
    ).joinToString(" · ").ifBlank { stringResource(Res.string.barrel_section) } + f.muzzleShiftNow().let { (up, right) ->
        shiftText(up, right, stringResource(if (f.muzzlePoiUnits == "mrad") Res.string.unit_mrad else Res.string.unit_moa))
            .let { if (it.isEmpty()) "" else " · $it" }
    }
    val suppressorText = if (!f.hasSuppressor) "" else listOf(
        f.suppressorName.ifBlank { stringResource(Res.string.suppressor_section) },
        f.suppressorShiftNow().let { (up, right) ->
            shiftText(up, right, stringResource(if (f.suppressorPoiUnits == "mrad") Res.string.unit_mrad else Res.string.unit_moa))
        },
    ).filter { it.isNotEmpty() }.joinToString(" · ")
    val mountText = "↕ ${formatNumber(f.sightHeightCm, 2)} $cm" + if (f.railSlope > 0) " · ∠${formatNumber(f.railSlope, 1)} $rail" else ""
    val zeroText = "${formatNumber(f.zeroRangeM, 0)} $m"
    val description = listOf(rifleText, barrelText, suppressorText, scopeText, mountText, zeroText).filter { it.isNotEmpty() }.joinToString("; ")

    Canvas(
        modifier
            .fillMaxWidth()
            .aspectRatio(2.4f)
            .testTag("rifleDiagram")
            .semantics { contentDescription = description }
            .pointerInput(Unit) {
                detectTapGestures { o -> partAt(o.x / size.width, o.y / size.height)?.let(select) }
            },
    ) {
        val w = size.width
        val h = size.height
        fun p(x: Float, y: Float) = Offset(x * w, y * h)
        fun poly(vararg pts: Pair<Float, Float>) = Path().apply {
            pts.forEachIndexed { i, (x, y) -> if (i == 0) moveTo(x * w, y * h) else lineTo(x * w, y * h) }
            close()
        }
        fun box(x0: Float, y0: Float, x1: Float, y1: Float, c: Color, r: Float = 0.01f) =
            drawRoundRect(c, p(x0, y0), Size((x1 - x0) * w, (y1 - y0) * h), CornerRadius(r * w))

        // The rifle: stock with its grip, the action, the bolt handle.
        val rifle = color(WizardStep.Rifle)
        drawPath(poly(0.02f to 0.50f, 0.27f to 0.48f, 0.27f to 0.62f, 0.23f to 0.63f, 0.21f to 0.82f, 0.16f to 0.82f,
            0.17f to 0.66f, 0.10f to 0.67f, 0.02f to 0.76f), rifle)
        box(0.26f, 0.485f, 0.53f, 0.60f, rifle)
        drawLine(rifle, p(0.31f, 0.58f), p(0.33f, 0.68f), strokeWidth = 0.012f * w)
        drawCircle(rifle, 0.016f * w, p(0.335f, 0.69f))

        // The barrel, thinner to the muzzle, and its muzzle device.
        val barrel = color(WizardStep.Barrel)
        drawPath(poly(0.53f to 0.505f, 0.86f to 0.515f, 0.86f to 0.555f, 0.53f to 0.575f), barrel)
        // A device taken off (or a suppressor not stated): its outline, dashed.
        fun dashed(x0: Float, y0: Float, x1: Float, y1: Float, c: Color) =
            drawRoundRect(c, p(x0, y0), Size((x1 - x0) * w, (y1 - y0) * h), CornerRadius(0.006f * w),
                style = Stroke(1.5f, pathEffect = PathEffect.dashPathEffect(floatArrayOf(4f, 3f))))
        when {
            !f.hasMuzzleDevice -> drawRect(barrel, p(0.86f, 0.505f), Size(0.02f * w, 0.06f * h), style = Stroke(1.5f))
            f.muzzleDeviceOn -> box(0.86f, 0.495f, 0.90f, 0.575f, barrel, 0.004f)
            else -> dashed(0.86f, 0.495f, 0.90f, 0.575f, barrel)
        }

        // The suppressor, beyond the muzzle device.
        val suppressor = color(WizardStep.Suppressor)
        if (f.hasSuppressor && f.suppressorOn) box(0.90f, 0.48f, 0.985f, 0.59f, suppressor, 0.008f)
        else dashed(0.90f, 0.48f, 0.985f, 0.59f, if (f.hasSuppressor) suppressor else faint)

        // The rail (tilted by its slope, if any) and the rings.
        val mount = color(WizardStep.Mount)
        val tilt = if (f.railSlope > 0) 0.012f else 0f
        drawPath(poly(0.28f to 0.445f - tilt, 0.52f to 0.445f + tilt, 0.52f to 0.485f, 0.28f to 0.485f), mount)
        box(0.31f, 0.40f, 0.345f, 0.45f, mount, 0.004f)
        box(0.45f, 0.40f, 0.485f, 0.45f, mount, 0.004f)

        // The scope: eyepiece, tube, turret, objective.
        val scope = color(WizardStep.Scope)
        drawPath(poly(0.19f to 0.30f, 0.25f to 0.33f, 0.25f to 0.39f, 0.19f to 0.42f), scope)
        box(0.25f, 0.33f, 0.56f, 0.40f, scope, 0.005f)
        box(0.38f, 0.23f, 0.43f, 0.33f, scope, 0.005f)
        drawPath(poly(0.56f to 0.33f, 0.62f to 0.28f, 0.645f to 0.28f, 0.645f to 0.45f, 0.62f to 0.45f, 0.56f to 0.40f), scope)

        // The line of sight to the target at the zero.
        val zero = color(WizardStep.Zero)
        drawLine(zero, p(0.645f, 0.365f), p(0.95f, 0.365f), strokeWidth = 2f,
            pathEffect = PathEffect.dashPathEffect(floatArrayOf(0.012f * w, 0.008f * w)))
        for (r in listOf(0.035f, 0.022f, 0.009f)) drawCircle(zero, r * w, p(0.95f, 0.365f), style = Stroke(2f))

        // The sight height: scope axis to bore axis.
        drawLine(mount, p(0.74f, 0.365f), p(0.74f, 0.535f), strokeWidth = 2f)
        drawLine(mount, p(0.725f, 0.365f), p(0.755f, 0.365f), strokeWidth = 2f)
        drawLine(mount, p(0.725f, 0.535f), p(0.755f, 0.535f), strokeWidth = 2f)

        // The values.
        fun label(s: String, x: Float, y: Float, c: Color, center: Boolean = false) {
            val t = measurer.measure(s, TextStyle(color = c, fontSize = style.fontSize, fontWeight = style.fontWeight))
            val left = (x * w - if (center) t.size.width / 2f else 0f).coerceIn(0f, (w - t.size.width).coerceAtLeast(0f))
            drawText(t, topLeft = Offset(left, y * h))
        }
        fun ink(step: WizardStep) = if (step == selected) accent else text
        label(scopeText, 0.41f, 0.04f, ink(WizardStep.Scope), center = true)
        label(mountText, 0.765f, 0.38f, ink(WizardStep.Mount))
        label(barrelText, 0.70f, 0.62f, ink(WizardStep.Barrel), center = true)
        if (suppressorText.isNotEmpty()) label(suppressorText, 0.94f, 0.73f, ink(WizardStep.Suppressor), center = true)
        label(rifleText, 0.02f, 0.86f, ink(WizardStep.Rifle))
        label(zeroText, 0.95f, 0.07f, ink(WizardStep.Zero), center = true)
    }
}
