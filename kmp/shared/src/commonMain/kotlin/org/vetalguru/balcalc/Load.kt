package org.vetalguru.balcalc

import kotlin.coroutines.cancellation.CancellationException

/**
 * A screen's load with a fallback when it fails, like runCatching, except
 * that a cancelled load stays cancelled: an effect restarted by a newer
 * state must not have the old run write its fallback over the new result.
 */
suspend inline fun <T> loadOr(fallback: T, block: () -> T): T =
    try {
        block()
    } catch (e: CancellationException) {
        throw e
    } catch (e: Exception) {
        fallback
    }
