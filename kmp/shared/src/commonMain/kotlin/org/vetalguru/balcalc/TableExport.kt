package org.vetalguru.balcalc

/**
 * A range card as text for a spreadsheet: a title line, then the header and
 * the rows, comma separated (fields with a comma, quote or line break quoted).
 */
fun tableCsv(title: String, header: List<String>, rows: List<List<String>>): String {
    fun field(s: String) = if (s.any { it == ',' || it == '"' || it == '\n' }) "\"" + s.replace("\"", "\"\"") + "\"" else s
    return buildString {
        appendLine(field(title))
        appendLine(header.joinToString(",") { field(it) })
        rows.forEach { appendLine(it.joinToString(",") { v -> field(v) }) }
    }
}

/**
 * A range card as a PNG picture, black on white, to print or keep on the
 * phone: the title, the header and the rows, the row [highlight] (or none)
 * shaded.
 */
expect fun tablePng(title: String, header: List<String>, rows: List<List<String>>, highlight: Int = -1): ByteArray
