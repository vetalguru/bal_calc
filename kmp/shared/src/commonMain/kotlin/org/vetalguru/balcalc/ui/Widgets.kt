package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.selection.toggleable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.vetalguru.balcalc.fixed

/** Text of a number with up to [decimals] digits, trailing zeros dropped. */
fun formatNumber(v: Double, decimals: Int): String {
    if (v.isNaN()) return ""
    val s = v.fixed(decimals)
    return if ('.' in s) s.trimEnd('0').trimEnd('.') else s
}

/** "1,5" and "1.5" both read as 1.5; anything else is null. */
fun parseNumber(text: String): Double? = text.trim().replace(',', '.').toDoubleOrNull()

/**
 * Labelled number input with a unit. The value is committed (clamped to
 * [from]..[to]) when the field loses focus or the keyboard's Done is
 * pressed; garbage keeps the old value.
 */
@Composable
fun NumberField(
    label: String,
    value: Double,
    onEdited: (Double) -> Unit,
    modifier: Modifier = Modifier,
    unit: String = "",
    decimals: Int = 1,
    from: Double = -1e9,
    to: Double = 1e9,
    tag: String? = null,
) {
    var text by remember { mutableStateOf(formatNumber(value, decimals)) }
    var focused by remember { mutableStateOf(false) }
    // Follow the model while not typing.
    LaunchedEffect(value, focused) {
        if (!focused) text = formatNumber(value, decimals)
    }
    fun commit() {
        val v = parseNumber(text)?.coerceIn(from, to)
        if (v == null) {
            text = formatNumber(value, decimals)
        } else {
            text = formatNumber(v, decimals)
            if (v != value) onEdited(v)
        }
    }
    val focus = LocalFocusManager.current
    OutlinedTextField(
        value = text,
        onValueChange = { t -> if (t.all { it.isDigit() || it == '.' || it == ',' || it == '-' }) text = t },
        label = { Text(label, maxLines = 1) },
        suffix = if (unit.isNotEmpty()) ({ Text(unit) }) else null,
        singleLine = true,
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal, imeAction = ImeAction.Done),
        keyboardActions = KeyboardActions(onDone = { focus.clearFocus() }),
        modifier = modifier
            .onFocusChanged {
                if (focused && !it.isFocused) commit()
                focused = it.isFocused
            }
            .let { m -> if (tag != null) m.testTag(tag) else m },
    )
}

@Composable
fun SwitchRow(text: String, checked: Boolean, onChange: (Boolean) -> Unit, modifier: Modifier = Modifier) {
    Row(
        modifier.fillMaxWidth().toggleable(checked, onValueChange = onChange),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Switch(checked = checked, onCheckedChange = null) // the whole row toggles
        Text(text, Modifier.weight(1f))
    }
}

/** A titled card of fields. */
@Composable
fun Section(title: String, modifier: Modifier = Modifier, content: @Composable ColumnScope.() -> Unit) {
    Card(modifier.fillMaxWidth().padding(horizontal = 12.dp)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            if (title.isNotEmpty()) Text(
                title,
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colorScheme.primary,
            )
            content()
        }
    }
}

/** A label over a value (solution details). */
@Composable
fun Detail(label: String, value: String, warn: Boolean = false, modifier: Modifier = Modifier) {
    Column(modifier) {
        Text(label, fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(value, fontSize = 17.sp, color = if (warn) MaterialTheme.colorScheme.error else Color.Unspecified)
    }
}

/** Fields side by side on wide screens, one per row on phones. */
@Composable
fun Fields(wide: Boolean, vararg fields: @Composable (Modifier) -> Unit) {
    if (wide) {
        fields.toList().chunked(2).forEach { pair ->
            Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                pair.forEach { it(Modifier.weight(1f)) }
                if (pair.size == 1) Row(Modifier.weight(1f)) {}
            }
        }
    } else {
        fields.forEach { it(Modifier.fillMaxWidth()) }
    }
}

/** A labelled drop-down of [options] (value to text). */
@OptIn(androidx.compose.material3.ExperimentalMaterial3Api::class)
@Composable
fun <T> ChoiceField(
    label: String,
    options: List<Pair<T, String>>,
    selected: T,
    onSelect: (T) -> Unit,
    modifier: Modifier = Modifier,
) {
    var open by remember { mutableStateOf(false) }
    androidx.compose.material3.ExposedDropdownMenuBox(expanded = open, onExpandedChange = { open = it }, modifier = modifier) {
        OutlinedTextField(
            value = options.firstOrNull { it.first == selected }?.second ?: "",
            onValueChange = {},
            readOnly = true,
            singleLine = true,
            label = { Text(label, maxLines = 1) },
            trailingIcon = { androidx.compose.material3.ExposedDropdownMenuDefaults.TrailingIcon(expanded = open) },
            modifier = Modifier
                .menuAnchor(androidx.compose.material3.ExposedDropdownMenuAnchorType.PrimaryNotEditable)
                .fillMaxWidth(),
        )
        ExposedDropdownMenu(expanded = open, onDismissRequest = { open = false }) {
            options.forEach { (value, text) ->
                androidx.compose.material3.DropdownMenuItem(text = { Text(text) }, onClick = {
                    open = false
                    onSelect(value)
                })
            }
        }
    }
}

/** A plain text input with a label. */
@Composable
fun TextInput(
    label: String,
    value: String,
    onChange: (String) -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    tag: String? = null,
) {
    val focus = LocalFocusManager.current
    OutlinedTextField(
        value = value,
        onValueChange = onChange,
        label = { Text(label, maxLines = 1) },
        singleLine = true,
        enabled = enabled,
        keyboardOptions = KeyboardOptions(imeAction = ImeAction.Done),
        keyboardActions = KeyboardActions(onDone = { focus.clearFocus() }),
        modifier = modifier.fillMaxWidth().let { m -> if (tag != null) m.testTag(tag) else m },
    )
}

/** Cancel - title - Save, the top of an editor. */
@Composable
fun EditorBar(title: String, cancel: String, save: String, onCancel: () -> Unit, onSave: () -> Unit) {
    Row(
        Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        androidx.compose.material3.TextButton(onClick = onCancel) { Text(cancel) }
        Text(
            title,
            style = MaterialTheme.typography.titleLarge,
            textAlign = androidx.compose.ui.text.style.TextAlign.Center,
            modifier = Modifier.weight(1f),
        )
        androidx.compose.material3.Button(onClick = onSave, modifier = Modifier.testTag("save")) { Text(save) }
    }
}
