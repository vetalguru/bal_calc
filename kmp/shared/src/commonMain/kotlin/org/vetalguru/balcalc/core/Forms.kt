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
    val barrelIn: Double = 0.0,
    val maker: String = "",
    val model: String = "",
    val riflingGrooves: Int = 0,
    /** "" | "conventional" | "polygonal" | "5r" | "other" */
    val riflingType: String = "",
    /** "" | "none" | "brake" | "compensator" | "flash_hider" */
    val muzzleDevice: String = "",
    /** Where the device moves the point of impact, up and right positive, in [muzzlePoiUnits]. */
    val muzzlePoiUp: Double = 0.0,
    val muzzlePoiRight: Double = 0.0,
    val muzzlePoiUnits: String = "moa",
    /** It counts when the device is on now but was off at the zero, or the other way round. */
    val muzzleDeviceOn: Boolean = true,
    val zeroedWithDevice: Boolean = false,
    /** The suppressor, its own part (it may go over the muzzle device), with the same kind of shift. */
    val hasSuppressor: Boolean = false,
    val suppressorName: String = "",
    val suppressorPoiUp: Double = 0.0,
    val suppressorPoiRight: Double = 0.0,
    val suppressorPoiUnits: String = "moa",
    val suppressorOn: Boolean = true,
    val zeroedWithSuppressor: Boolean = false,
    val railSlope: Double = 0.0,
    val railSlopeUnits: String = "moa",
    val ringHeightMm: Double = 0.0,
    val clickUnits: String = "mrad",
    val clickValue: Double = 0.1,
    /** 0 = as the vertical click. */
    val clickValueHorizontal: Double = 0.0,
    /** Each turret's full travel, in the click units (0 = not stated). */
    val elevationTravel: Double = 0.0,
    val windageTravel: Double = 0.0,
    val scopeMaker: String = "",
    val scopeModel: String = "",
    val tubeDiameterMm: Double = 0.0,
    val objectiveMm: Double = 0.0,
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
    val barrelIn: Double = 0.0,
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

@Serializable
data class Shot(
    val id: Long = 0,
    val rangeM: Double = 0.0,
    val observed: Double = 0.0,
    val predicted: Double? = null,
    val hasWindage: Boolean = false,
    val observedWindage: Double = 0.0,
    val shotAt: String = "",
    val used: Boolean = true,
    val notes: String = "",
    val temperatureC: Double = 15.0,
)

@Serializable
data class TruingPoint(
    val rangeM: Double = 0.0,
    val observed: Double = 0.0,
    val before: Double = 0.0,
    val after: Double = 0.0,
)

@Serializable
data class TruingResult(
    val ok: Boolean = false,
    val error: String = "",
    val velocityScale: Double = 1.0,
    val dragScale: Double = 1.0,
    val dragFitted: Boolean = false,
    val rmsBefore: Double = 0.0,
    val rmsAfter: Double = 0.0,
    val velocityBefore: Double = 0.0,
    val velocityAfter: Double = 0.0,
    val points: List<TruingPoint> = emptyList(),
)

@Serializable
data class ImportProblem(val file: String = "", val message: String = "")

@Serializable
data class ImportReport(val imported: Int = 0, val problems: List<ImportProblem> = emptyList())

@Serializable
data class SeedReport(val imported: Int = 0, val skipped: Int = 0)

@Serializable
data class Info(val engineVersion: String = "", val sqliteVersion: String = "", val databasePath: String = "")
