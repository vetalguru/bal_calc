package org.vetalguru.balcalc

import kotlin.io.encoding.Base64
import kotlin.io.encoding.ExperimentalEncodingApi

/** The black and white modules of a QR code, [size] by [size], no margin. */
class QrMatrix(val size: Int, private val dark: BooleanArray) {
    operator fun get(x: Int, y: Int): Boolean = dark[y * size + x]
}

/** A QR code of [text] (byte mode, error correction M). */
expect fun qrMatrix(text: String): QrMatrix

/** The text of the QR code in a picture (PNG, JPEG...), or null when none is found. */
expect fun decodeQrImage(image: ByteArray): String?

expect fun deflate(data: ByteArray): ByteArray

expect fun inflate(data: ByteArray): ByteArray

/**
 * A shared rifle or cartridge (the JSON of a file) as QR codes: compressed
 * and cut in parts small enough for a phone camera, each one saying which
 * part it is - "BALCALC1:<part>:<parts>:<base64 of the deflated JSON>".
 */
@OptIn(ExperimentalEncodingApi::class)
object QrShare {
    private const val TAG = "BALCALC1"
    /** Base64 characters per code: QR version 17 at level M, easy to scan off a screen. */
    const val CHUNK = 500

    fun parts(json: String): List<String> {
        val data = Base64.UrlSafe.encode(deflate(json.encodeToByteArray()))
        val chunks = data.chunked(CHUNK)
        return chunks.mapIndexed { i, c -> "$TAG:${i + 1}:${chunks.size}:$c" }
    }

    /** Collects scanned parts in any order; a plain JSON code is complete at once. */
    class Collector {
        private val parts = mutableMapOf<Int, String>()
        private var plain: String? = null
        var total = 0
            private set
        val have get() = if (plain != null) 1 else parts.size
        val complete get() = plain != null || (total > 0 && parts.size == total)

        /** False when [text] is not a BalCalc code (or belongs to another set of parts). */
        fun add(text: String): Boolean {
            val t = text.trim()
            if (t.startsWith("{")) {
                plain = t
                return true
            }
            val fields = t.split(":", limit = 4)
            if (fields.size != 4 || fields[0] != TAG) return false
            val part = fields[1].toIntOrNull() ?: return false
            val count = fields[2].toIntOrNull() ?: return false
            if (count < 1 || part !in 1..count) return false
            if (total != 0 && count != total) return false
            total = count
            parts[part] = fields[3]
            return true
        }

        /** The JSON once [complete]; throws on a damaged set. */
        fun json(): String = plain ?: inflate(
            Base64.UrlSafe.decode((1..total).joinToString("") { parts.getValue(it) }),
        ).decodeToString()
    }
}
