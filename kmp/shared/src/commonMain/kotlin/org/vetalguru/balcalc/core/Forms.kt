package org.vetalguru.balcalc.core

import kotlinx.serialization.Serializable

// Edit forms, as bridge::Api's rifleForm / cartridgeForm / bulletForm
// return them and save* take them back (units shooters type).

@Serializable
data class RifleForm(
    val rifleId: Long = 0,
    val name: String = "",
    val caliber: String = "",
    val sightHeightCm: Double = 5.0,
    val twistIn: Double = 10.0,
    val twistLeft: Boolean = false,
    val clickUnits: String = "mrad",
    val clickValue: Double = 0.1,
    val reticleId: Long = 0,
    val focalPlane: String = "ffp",
    val sfpReferenceMagnification: Double = 0.0,
    val minMagnification: Double = 0.0,
    val maxMagnification: Double = 0.0,
    val zeroRangeM: Double = 100.0,
    val zeroTemperatureC: Double = 15.0,
    val zeroPressureHpa: Double = 1013.25,
    val zeroAltitudeM: Double = 0.0,
    val zeroHumidityPct: Double = 50.0,
    val zeroPowderC: Double = 15.0,
)

@Serializable
data class CartridgeForm(
    val cartridgeId: Long = 0,
    val copyOf: Long = 0,
    val libraryBulletId: Long = 0,
    val name: String = "",
    val caliber: String = "",
    val bulletName: String = "",
    val dragTable: String = "G7",
    val bc: Double = 0.0,
    val massGr: Double = 0.0,
    val diameterIn: Double = 0.0,
    val lengthIn: Double = 0.0,
    val muzzleVelocity: Double = 0.0,
    val powderReferenceC: Double = 15.0,
    val powderSensitivity: Double = 0.0,
)

@Serializable
data class BcBand(val velocity: Double = 0.0, val bc: Double = 0.0)

@Serializable
data class BulletForm(
    val id: Long = 0,
    val name: String = "",
    val manufacturer: String = "",
    val caliber: String = "",
    val massGr: Double = 0.0,
    val diameterIn: Double = 0.0,
    val lengthIn: Double = 0.0,
    val dragTable: String = "G7",
    val bc: Double = 0.0,
    val bands: List<BcBand> = emptyList(),
    val notes: String = "",
    val source: String = "library",
    val hasCustomCurve: Boolean = false,
)

@Serializable
data class BulletItem(
    val id: Long = 0,
    val name: String = "",
    val manufacturer: String = "",
    val caliber: String = "",
    val massGr: Double = 0.0,
    val diameterIn: Double = 0.0,
    val dragKind: String = "bc",
    val dragTable: String = "G7",
    val bc: Double = 0.0,
    val bcBands: Int = 0,
    val source: String = "",
)

@Serializable
data class ReticleItem(val id: Long = 0, val name: String = "", val units: String = "mrad")

@Serializable
data class ExportedJson(val json: String = "", val fileName: String = "")

/** A file the user picked: its name and text. */
class NamedText(val name: String, val content: String)
