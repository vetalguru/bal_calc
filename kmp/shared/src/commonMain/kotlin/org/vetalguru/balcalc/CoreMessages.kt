package org.vetalguru.balcalc

import androidx.compose.runtime.Composable
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/**
 * The C++ core answers in English sentences (bridge/src/api.cpp, applogic
 * validation); these are the ones the screens show, mapped to resources.
 */
private val coreMessages: Map<String, StringResource> = mapOf(
    "Choose a rifle and a cartridge." to Res.string.choose_pair,
    "Enter a rifle name." to Res.string.core_01,
    "Enter a cartridge name." to Res.string.core_02,
    "Sight height must be between 0 and 30 cm." to Res.string.core_03,
    "Point-of-impact shift must be within 100 cm." to Res.string.core_04,
    "Muzzle velocity must be between 50 and 2000 m/s." to Res.string.core_05,
    "Ballistic coefficient must be between 0 and 2." to Res.string.core_06,
    "Enter the bullet weight." to Res.string.core_07,
    "Bullet diameter must be between 0 and 1 inch." to Res.string.core_08,
    "Bullet length and twist cannot be negative." to Res.string.core_09,
    "Zero range must be between 10 and 1000 m." to Res.string.core_10,
    "Enter the scope click value." to Res.string.core_11,
    "Zero pressure must be between 300 and 1200 hPa." to Res.string.core_12,
    "Humidity must be between 0 and 100 %." to Res.string.core_13,
    "Enter a target range." to Res.string.core_14,
    "The bullet does not reach this range." to Res.string.core_15,
    "Check the table range and step." to Res.string.core_16,
    "Check the scope magnification range." to Res.string.core_17,
    "Enter the bullet name." to Res.string.core_18,
    "Log at least one hit to true the rifle and cartridge." to Res.string.core_19,
    "The bullet does not reach one of the logged ranges." to Res.string.core_20,
    "Nothing to apply." to Res.string.core_21,
    "Each BC band needs a velocity and a BC between 0 and 2." to Res.string.core_22,
    "This bullet is used by a cartridge and cannot be deleted." to Res.string.core_23,
    "Log hits where the bullet is slower than Mach 1.3 at the target." to Res.string.core_24,
    "Each DSF point needs a Mach between 0 and 5 and a factor between 0.5 and 2." to Res.string.core_25,
    "Two DSF points have the same Mach." to Res.string.core_26,
    "The DSF alone cannot explain these hits: true the velocity and drag first." to Res.string.core_27,
    "Unknown drag table." to Res.string.core_28,
    "Enter the distance and two velocities, the far one lower." to Res.string.core_29,
    "No BC between 0.02 and 2 gives these measurements." to Res.string.core_30,
    "No BC between 0.02 and 2 gives this correction." to Res.string.core_31,
    "Enter a name for the situation." to Res.string.core_32,
    "No situation of this name." to Res.string.core_33,
    "The rifle or cartridge of this situation was deleted." to Res.string.core_34,
    "The picture is too large." to Res.string.core_35,
    "The picture is damaged." to Res.string.core_36,
    "Enter a name for each target." to Res.string.core_37,
    "A target range must be between 10 and 3000 m." to Res.string.core_38,
    "At most 20 targets." to Res.string.core_39,
)

/** A message from the core in the app's language (unknown ones as they are). */
@Composable
fun coreText(message: String): String =
    coreMessages[message]?.let { stringResource(it) } ?: message
