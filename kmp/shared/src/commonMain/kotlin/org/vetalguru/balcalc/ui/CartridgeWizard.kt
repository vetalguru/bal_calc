package org.vetalguru.balcalc.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import kotlin.math.roundToInt
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.DrawableResource
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.loadOr
import org.vetalguru.balcalc.core.CartridgeForm
import org.vetalguru.balcalc.core.RifleForm
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** The cartridge's parts, in order; the last one is the summary. */
internal enum class CartridgeStep(
    override val title: StringResource,
    override val icon: DrawableResource?,
    override val note: StringResource?,
) : PagePart {
    Cartridge(Res.string.cartridge, Res.drawable.ic_cartridge, Res.string.wizard_cartridge_note),
    Bullet(Res.string.bullet, Res.drawable.ic_bullet, Res.string.wizard_bullet_note),
    Load(Res.string.load_section, null, Res.string.wizard_load_note),
    Velocity(Res.string.velocity, null, Res.string.wizard_velocity_note),
    Summary(Res.string.summary, null, null);

    override val isSummary get() = this == Summary
}

/**
 * A cartridge on its drawing (a custom one, a factory one copied, or one
 * being edited): the case, the bullet, the powder inside (the handload's
 * recipe) and the velocity; then a summary. Save works on any part.
 */
@Composable
internal fun CartridgeWizard(model: AppModel, route: Route.CartridgeWizard, nav: ArmoryNav) {
    var f by route::form
    var step by route::step
    LaunchedEffect(route) { route.load(model, "cartridge", route.form.cartridgeId) }
    val photo = route.photo
    var error by remember { mutableStateOf<String?>(null) }
    val scope = rememberCoroutineScope()
    // Stability and the velocity in the current rifle, as the cartridge is edited.
    val currentRifle = model.state.currentRifleId
    var partner by remember { mutableStateOf<RifleForm?>(null) }
    LaunchedEffect(currentRifle) {
        partner = if (currentRifle > 0) loadOr(null) { model.armory.rifleForm(currentRifle) } else null
    }
    var sg by remember { mutableStateOf<Double?>(null) }
    LaunchedEffect(f.massGr, f.diameterIn, f.lengthIn, f.muzzleVelocity, partner) {
        sg = partner?.let { r -> loadOr(null) { model.armory.stability(r.twistIn, f.massGr, f.diameterIn, f.lengthIn, f.muzzleVelocity) } }
    }
    var inBarrel by remember { mutableStateOf<Double?>(null) }
    LaunchedEffect(f.muzzleVelocity, f.barrelIn, f.caliber, partner) {
        inBarrel = partner?.takeIf { it.barrelIn > 0 && f.barrelIn > 0 && it.barrelIn != f.barrelIn }?.let { r ->
            loadOr(null) { model.armory.barrelVelocity(f.muzzleVelocity, f.barrelIn, r.barrelIn, f.caliber.ifBlank { r.caliber }) }
        }
    }
    val stability = sg
    val partnerName = partner?.name.orEmpty()
    val partnerBarrel = partner?.barrelIn ?: 0.0
    val velocityInBarrel = inBarrel
    val current = CartridgeStep.entries[step]
    LaunchedEffect(current) { route.visited = route.visited + current }
    val done = route.visited
    val set: (CartridgeForm) -> Unit = { f = it }
    PartsPage(
        title = stringResource(if (f.cartridgeId > 0) Res.string.edit_cartridge else Res.string.new_cartridge),
        parts = CartridgeStep.entries,
        step = step,
        onStep = { step = it },
        done = done,
        error = error,
        onCancel = nav::back,
        onSave = {
            scope.launch {
                error = model.armory.saveCartridge(f, route.photoChange)
                if (error == null) nav.back() else if (f.name.isBlank()) step = 0
            }
        },
        drawing = { mod -> CartridgeDiagram(f, current, done, { step = it.ordinal }, mod) },
        summary = { CartridgeSummary(f) { step = it.ordinal } },
    ) { part, wide ->
        when (part) {
            CartridgeStep.Cartridge -> {
                TextInput(stringResource(Res.string.cartridge_name_hint), f.name, { set(f.copy(name = it)) }, tag = "cartridgeName")
                TextInput(stringResource(Res.string.caliber_hint), f.caliber, { set(f.copy(caliber = it)) }, tag = "cartridgeCaliber")
                PhotoRow(photo, route::change)
            }
            CartridgeStep.Bullet -> BulletPartFields(f, set, wide, stability, partnerName) {
                nav.push(Route.Bullets { id -> model.act { f = model.armory.cartridgeFormWithBullet(f, id) } })
            }
            CartridgeStep.Load -> LoadFields(f, set, wide)
            CartridgeStep.Velocity -> VelocityFields(f, set, wide, velocityInBarrel, partnerBarrel, partnerName)
            CartridgeStep.Summary -> Unit
        }
    }
}

/** The bullet: from the library (shown, not edited) or the cartridge's own; Sg in the current rifle. */
@Composable
private fun BulletPartFields(
    f: CartridgeForm,
    set: (CartridgeForm) -> Unit,
    wide: Boolean,
    stability: Double?,
    partnerName: String,
    onLibrary: () -> Unit,
) {
    val fromLibrary = f.libraryBulletId > 0
    val inch = stringResource(Res.string.unit_in)
    // The bullet's name on its own line: beside the buttons a long one was
    // squeezed to a letter column on phones.
    Text(
        if (fromLibrary) stringResource(Res.string.from_library_named, f.bulletName) else stringResource(Res.string.own_bullet),
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        modifier = Modifier.testTag("bulletSource"),
    )
    FlowRow {
        TextButton(onClick = onLibrary, modifier = Modifier.testTag("chooseBullet")) {
            Text(stringResource(if (fromLibrary) Res.string.change else Res.string.from_library))
        }
        if (fromLibrary) TextButton(onClick = { set(f.copy(libraryBulletId = 0)) }) { Text(stringResource(Res.string.edit_as_own)) }
    }
    TextInput(stringResource(Res.string.bullet_hint), f.bulletName, { set(f.copy(bulletName = it)) }, enabled = !fromLibrary, tag = "bulletName")
    Column(Modifier.alpha(if (fromLibrary) 0.5f else 1f), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Fields(
            wide,
            { mod ->
                ChoiceField(
                    stringResource(Res.string.drag_model),
                    listOf("G7", "G1", "G2", "G5", "G6", "G8", "GI", "GS", "RA4").map { it to it },
                    f.dragTable, { if (!fromLibrary) set(f.copy(dragTable = it)) }, mod,
                )
            },
            { mod -> NumberField(stringResource(Res.string.bc), f.bc, { if (!fromLibrary) set(f.copy(bc = it)) }, mod, decimals = 3, from = 0.0, to = 2.0, tag = "bc") },
            { mod -> NumberField(stringResource(Res.string.weight), f.massGr, { if (!fromLibrary) set(f.copy(massGr = it)) }, mod, stringResource(Res.string.unit_gr), from = 0.0, to = 2000.0, tag = "mass") },
            { mod -> NumberField(stringResource(Res.string.diameter), f.diameterIn, { if (!fromLibrary) set(f.copy(diameterIn = it)) }, mod, inch, 3, 0.0, 1.0, tag = "diameter") },
            { mod -> NumberField(stringResource(Res.string.length_spin), f.lengthIn, { if (!fromLibrary) set(f.copy(lengthIn = it)) }, mod, inch, 3, 0.0, 4.0) },
        )
    }
    StabilityLine(stability, partnerName)
}

/** A handload's recipe: powder and charge, primer, brass, the cartridge's lengths. */
@Composable
private fun LoadFields(f: CartridgeForm, set: (CartridgeForm) -> Unit, wide: Boolean) {
    val inch = stringResource(Res.string.unit_in)
    Fields(
        wide,
        { mod -> TextInput(stringResource(Res.string.powder_name), f.powder, { set(f.copy(powder = it)) }, mod, tag = "powder") },
        { mod -> NumberField(stringResource(Res.string.charge), f.chargeGr, { set(f.copy(chargeGr = it)) }, mod, stringResource(Res.string.unit_gr), 2, 0.0, 300.0, tag = "charge") },
        { mod -> TextInput(stringResource(Res.string.primer), f.primer, { set(f.copy(primer = it)) }, mod, tag = "primer") },
        { mod -> TextInput(stringResource(Res.string.brass), f.brass, { set(f.copy(brass = it)) }, mod, tag = "brass") },
        { mod -> NumberField(stringResource(Res.string.coal), f.coalIn, { set(f.copy(coalIn = it)) }, mod, inch, 3, 0.0, 7.0, tag = "coal") },
        { mod -> NumberField(stringResource(Res.string.cbto), f.cbtoIn, { set(f.copy(cbtoIn = it)) }, mod, inch, 3, 0.0, 7.0, tag = "cbto") },
    )
}

/** The velocity: as measured (in which barrel), in the current rifle's barrel, with the powder's temperature; the spread. */
@Composable
private fun VelocityFields(
    f: CartridgeForm,
    set: (CartridgeForm) -> Unit,
    wide: Boolean,
    velocityInBarrel: Double?,
    partnerBarrel: Double,
    partnerName: String,
) {
    val inch = stringResource(Res.string.unit_in)
    val mps = stringResource(Res.string.unit_mps)
    Fields(
        wide,
        { mod -> NumberField(stringResource(Res.string.muzzle_velocity), f.muzzleVelocity, { set(f.copy(muzzleVelocity = it)) }, mod, mps, from = 0.0, to = 2000.0, tag = "muzzleVelocity") },
        { mod ->
            NumberField(stringResource(Res.string.test_barrel), f.barrelIn, { set(f.copy(barrelIn = it)) }, mod, inch, 1, 0.0, 40.0,
                tag = "testBarrel", hint = stringResource(Res.string.test_barrel_hint))
        },
    )
    if (velocityInBarrel != null) {
        Text(
            stringResource(Res.string.barrel_velocity_line, partnerBarrel.barrelInches(), partnerName, velocityInBarrel.roundToInt().toString()),
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.testTag("velocityInBarrel"),
        )
    }
    Fields(
        wide,
        { mod -> NumberField(stringResource(Res.string.measured_at_powder), f.powderReferenceC, { set(f.copy(powderReferenceC = it)) }, mod, stringResource(Res.string.unit_c), from = -60.0, to = 80.0) },
        { mod -> NumberField(stringResource(Res.string.powder_sensitivity), f.powderSensitivity, { set(f.copy(powderSensitivity = it)) }, mod, stringResource(Res.string.unit_pct_per_c), 3, -2.0, 2.0) },
        { mod ->
            NumberField(stringResource(Res.string.velocity_sd), f.velocitySd, { set(f.copy(velocitySd = it)) }, mod, mps, 1, 0.0, 200.0,
                tag = "velocitySd", hint = stringResource(Res.string.spread_hint))
        },
        { mod ->
            NumberField(stringResource(Res.string.velocity_es), f.velocityEs, { set(f.copy(velocityEs = it)) }, mod, mps, 1, 0.0, 400.0,
                tag = "velocityEs", hint = stringResource(Res.string.spread_hint))
        },
    )
}

@Composable
private fun CartridgeSummary(f: CartridgeForm, onChange: (CartridgeStep) -> Unit) {
    val inch = stringResource(Res.string.unit_in)
    val gr = stringResource(Res.string.unit_gr)
    val mps = stringResource(Res.string.unit_mps)
    fun n(v: Double, d: Int = 1) = formatNumber(v, d)
    PartsSummary(
        listOf(
            CartridgeStep.Cartridge to listOfNotNull(
                stringResource(Res.string.label_name) to f.name,
                (stringResource(Res.string.label_caliber) to f.caliber).takeIf { f.caliber.isNotBlank() },
            ),
            CartridgeStep.Bullet to listOfNotNull(
                (stringResource(Res.string.bullet) to f.bulletName).takeIf { f.bulletName.isNotBlank() },
                (stringResource(Res.string.weight) to "${n(f.massGr)} $gr").takeIf { f.massGr > 0 },
                (stringResource(Res.string.bc) to "${f.dragTable} ${n(f.bc, 3)}").takeIf { f.bc > 0 },
                (stringResource(Res.string.diameter) to "${n(f.diameterIn, 3)} $inch" + if (f.lengthIn > 0) " × ${n(f.lengthIn, 3)} $inch" else "")
                    .takeIf { f.diameterIn > 0 },
            ),
            CartridgeStep.Load to listOfNotNull(
                (stringResource(Res.string.powder_name) to listOfNotNull(f.powder.ifBlank { null }, f.chargeGr.takeIf { it > 0 }?.let { "${n(it, 2)} $gr" })
                    .joinToString(", ")).takeIf { it.second.isNotEmpty() },
                (stringResource(Res.string.primer) to f.primer).takeIf { f.primer.isNotBlank() },
                (stringResource(Res.string.brass) to f.brass).takeIf { f.brass.isNotBlank() },
                (stringResource(Res.string.coal) to "${n(f.coalIn, 3)} $inch").takeIf { f.coalIn > 0 },
                (stringResource(Res.string.cbto) to "${n(f.cbtoIn, 3)} $inch").takeIf { f.cbtoIn > 0 },
            ),
            CartridgeStep.Velocity to listOfNotNull(
                (stringResource(Res.string.muzzle_velocity) to "${n(f.muzzleVelocity, 0)} $mps" + if (f.barrelIn > 0) ", ${n(f.barrelIn)} $inch" else "")
                    .takeIf { f.muzzleVelocity > 0 },
                ("SD / ES" to "${n(f.velocitySd)} / ${n(f.velocityEs)} $mps").takeIf { f.velocitySd > 0 || f.velocityEs > 0 },
            ),
        ),
        onChange,
    )
}

/** The part of the cartridge's drawing at ([x], [y]) (fractions), or null. */
internal fun cartridgePartAt(x: Float, y: Float): CartridgeStep? = when {
    x >= 0.82f -> CartridgeStep.Velocity
    x >= 0.72f && y in 0.60f..0.73f -> CartridgeStep.Velocity // its label
    x in 0.56f..0.82f && y in 0.22f..0.76f -> CartridgeStep.Bullet
    y < 0.22f && x >= 0.45f -> CartridgeStep.Bullet // its label
    x in 0.08f..0.52f && y in 0.38f..0.62f -> CartridgeStep.Load // the powder inside
    y >= 0.76f -> CartridgeStep.Load // its label
    x < 0.56f -> CartridgeStep.Cartridge // the case, its base, its label
    else -> null
}

/**
 * The cartridge in section: the case (rim, body, shoulder, neck) with the
 * primer at its base, the powder inside, the bullet seated in the neck, and
 * an arrow for the velocity; [f]'s values on the parts. [selected] in the
 * accent colour, [done] solid, the rest faint; a tap selects a part.
 */
@Composable
internal fun CartridgeDiagram(
    f: CartridgeForm,
    selected: CartridgeStep,
    done: Set<CartridgeStep>,
    onSelect: (CartridgeStep) -> Unit,
    modifier: Modifier = Modifier,
) {
    val select by rememberUpdatedState(onSelect)
    val accent = MaterialTheme.colorScheme.primary
    val solid = MaterialTheme.colorScheme.onSurfaceVariant
    val faint = solid.copy(alpha = 0.3f)
    val text = MaterialTheme.colorScheme.onSurface
    fun color(step: CartridgeStep) = when (step) {
        selected -> accent
        in done -> solid
        else -> faint
    }
    val measurer = rememberTextMeasurer()
    val style = MaterialTheme.typography.labelMedium
    val inch = stringResource(Res.string.unit_in)
    val gr = stringResource(Res.string.unit_gr)
    val mps = stringResource(Res.string.unit_mps)
    val caseText = listOf(f.name.ifBlank { stringResource(Res.string.cartridge) }, f.caliber).filter { it.isNotBlank() }.joinToString(" · ")
    val bulletText = listOfNotNull(
        f.bulletName.ifBlank { null },
        f.massGr.takeIf { it > 0 }?.let { "${formatNumber(it, 1)} $gr" },
        f.bc.takeIf { it > 0 }?.let { "${f.dragTable} ${formatNumber(it, 3)}" },
    ).joinToString(" · ").ifBlank { stringResource(Res.string.bullet) }
    val loadText = listOfNotNull(
        f.powder.ifBlank { null },
        f.chargeGr.takeIf { it > 0 }?.let { "${formatNumber(it, 2)} $gr" },
        f.coalIn.takeIf { it > 0 }?.let { "COAL ${formatNumber(it, 3)} $inch" },
    ).joinToString(" · ").ifBlank { stringResource(Res.string.load_section) }
    val velocityText = listOfNotNull(
        f.muzzleVelocity.takeIf { it > 0 }?.let { "${formatNumber(it, 0)} $mps" },
        f.velocitySd.takeIf { it > 0 }?.let { "SD ${formatNumber(it, 1)}" },
    ).joinToString(" · ").ifBlank { stringResource(Res.string.velocity) }
    val description = listOf(caseText, bulletText, loadText, velocityText).joinToString("; ")

    Canvas(
        modifier
            .fillMaxWidth()
            .aspectRatio(2.4f)
            .testTag("cartridgeDiagram")
            .semantics { contentDescription = description }
            .pointerInput(Unit) {
                detectTapGestures { o -> cartridgePartAt(o.x / size.width, o.y / size.height)?.let(select) }
            },
    ) {
        val w = size.width
        val h = size.height
        fun p(x: Float, y: Float) = Offset(x * w, y * h)
        fun poly(vararg pts: Pair<Float, Float>) = Path().apply {
            pts.forEachIndexed { i, (x, y) -> if (i == 0) moveTo(x * w, y * h) else lineTo(x * w, y * h) }
            close()
        }

        // The case: rim, extractor groove, body, shoulder, neck - as an outline,
        // the inside left for the powder; the primer in its base.
        val case = color(CartridgeStep.Cartridge)
        val wall = 0.012f * w
        drawRoundRect(case, p(0.03f, 0.29f), Size(0.025f * w, 0.42f * h), CornerRadius(0.004f * w))
        drawPath(
            poly(0.055f to 0.33f, 0.07f to 0.33f, 0.08f to 0.30f, 0.50f to 0.32f, 0.55f to 0.40f, 0.62f to 0.40f,
                0.62f to 0.60f, 0.55f to 0.60f, 0.50f to 0.68f, 0.08f to 0.70f, 0.07f to 0.67f, 0.055f to 0.67f),
            case,
            style = androidx.compose.ui.graphics.drawscope.Stroke(wall / 1.5f),
        )
        drawCircle(case, 0.018f * w, p(0.045f, 0.50f))

        // The powder: grains filling the case.
        val load = color(CartridgeStep.Load)
        val grains = if (f.powder.isNotBlank() || f.chargeGr > 0) 1f else 0.6f
        var gx = 0.10f
        while (gx < 0.49f) {
            var gy = 0.37f
            while (gy < 0.64f) {
                drawCircle(load.copy(alpha = load.alpha * grains), 0.006f * w, p(gx, gy))
                gy += 0.07f
            }
            gx += 0.025f
        }

        // The bullet: boat tail inside the neck, bearing surface, ogive to the tip.
        val bullet = color(CartridgeStep.Bullet)
        drawPath(
            Path().apply {
                moveTo(0.57f * w, 0.44f * h)
                lineTo(0.59f * w, 0.415f * h)
                lineTo(0.68f * w, 0.415f * h)
                quadraticTo(0.77f * w, 0.42f * h, 0.81f * w, 0.50f * h)
                quadraticTo(0.77f * w, 0.58f * h, 0.68f * w, 0.585f * h)
                lineTo(0.59f * w, 0.585f * h)
                lineTo(0.57f * w, 0.56f * h)
                close()
            },
            bullet,
        )

        // The overall length, base to tip.
        drawLine(load, p(0.03f, 0.74f), p(0.81f, 0.74f), strokeWidth = 1.5f)
        drawLine(load, p(0.03f, 0.715f), p(0.03f, 0.765f), strokeWidth = 1.5f)
        drawLine(load, p(0.81f, 0.715f), p(0.81f, 0.765f), strokeWidth = 1.5f)

        // The velocity: an arrow out of the muzzle end.
        val velocity = color(CartridgeStep.Velocity)
        drawLine(velocity, p(0.84f, 0.50f), p(0.97f, 0.50f), strokeWidth = 0.012f * w, cap = StrokeCap.Round)
        drawPath(poly(0.97f to 0.42f, 0.995f to 0.50f, 0.97f to 0.58f), velocity)

        fun label(s: String, x: Float, y: Float, c: Color, center: Boolean = false) {
            val t = measurer.measure(s, TextStyle(color = c, fontSize = style.fontSize, fontWeight = style.fontWeight))
            val left = (x * w - if (center) t.size.width / 2f else 0f).coerceIn(0f, (w - t.size.width).coerceAtLeast(0f))
            drawText(t, topLeft = Offset(left, y * h))
        }
        fun ink(step: CartridgeStep) = if (step == selected) accent else text
        label(caseText, 0.02f, 0.03f, ink(CartridgeStep.Cartridge))
        label(bulletText, 0.69f, 0.15f, ink(CartridgeStep.Bullet), center = true)
        label(loadText, 0.02f, 0.80f, ink(CartridgeStep.Load))
        label(velocityText, 0.99f, 0.61f, ink(CartridgeStep.Velocity), center = true)
    }
}
