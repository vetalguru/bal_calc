package org.vetalguru.balcalc.core

import kotlinx.serialization.Serializable

// Shapes of bridge::Api results (bridge/src/api.cpp). Unknown keys are
// ignored, missing ones take the defaults, so the core can grow first.

@Serializable
data class RifleItem(val id: Long = 0, val name: String = "", val caliber: String = "")

@Serializable
data class CartridgeItem(
    val id: Long = 0,
    val name: String = "",
    val caliber: String = "",
    val bulletName: String = "",
    val muzzleVelocity: Double = 0.0,
    val matches: Boolean = false,
)

@Serializable
data class CurrentPair(
    val rifleName: String = "",
    val cartridgeName: String = "",
    val zeroRangeM: Double = 0.0,
    val offsetUpCm: Double = 0.0,
    val offsetRightCm: Double = 0.0,
)

@Serializable
data class Conditions(
    val temperatureC: Double = 15.0,
    val pressureHpa: Double = 1013.25,
    val altitudeM: Double = 0.0,
    val humidityPct: Double = 50.0,
    val powderFollowsAir: Boolean = true,
    val powderC: Double = 15.0,
    val windSpeed: Double = 0.0,
    val windFromDeg: Double = 90.0,
    val lookAngleDeg: Double = 0.0,
    val cantDeg: Double = 0.0,
    val coriolis: Boolean = false,
    val latitudeDeg: Double = 50.0,
    val useAzimuth: Boolean = false,
    val azimuthDeg: Double = 0.0,
    val targetRangeM: Double = 300.0,
    val magnification: Double = 0.0,
    val useDensityAltitude: Boolean = false,
    val densityAltitudeM: Double = 0.0,
    val targetHeightCm: Double = 20.0,
    val windUntilM: Double = 0.0,
    val windZones: List<WindZoneIn> = emptyList(),
    val windGustMps: Double = 0.0,
    val targetSpeedMps: Double = 0.0,
    val targetHeadingDeg: Double = 90.0,
    val targetSpeedUnit: String = "kmh",
)

/** A wind zone after the first: up to `untilM` (the last one: to the end). */
@Serializable
data class WindZoneIn(val speedMps: Double = 0.0, val fromDeg: Double = 90.0, val untilM: Double = 0.0)

@Serializable
data class AppState(
    val rifles: List<RifleItem> = emptyList(),
    val cartridges: List<CartridgeItem> = emptyList(),
    val currentRifleId: Long = 0,
    val currentCartridgeId: Long = 0,
    val currentProfileId: Long = 0,
    val currentPair: CurrentPair? = null,
    val angleUnit: String = "mrad",
    val holdMode: String = "dial_elevation",
    val language: String = "",
    val tableFromM: Double = 100.0,
    val tableToM: Double = 1000.0,
    val tableStepM: Double = 50.0,
    val conditions: Conditions = Conditions(),
    val prefs: UiPrefs = UiPrefs(),
) {
    val moa: Boolean get() = angleUnit == "moa"
    val hasPair: Boolean get() = currentProfileId != 0L
}

@Serializable
data class Solution(
    val ok: Boolean = false,
    val error: String = "",
    val rangeM: Double = 0.0,
    val elevation: Double = 0.0,
    val windage: Double = 0.0,
    val elevationClicks: Double = 0.0,
    val windageClicks: Double = 0.0,
    val hasScope: Boolean = false,
    /** One click in the angle unit (0 without a scope). */
    val clickElevation: Double = 0.0,
    val clickWindage: Double = 0.0,
    val dropCm: Double = 0.0,
    val windageCm: Double = 0.0,
    val velocity: Double = 0.0,
    val energy: Double = 0.0,
    val time: Double = 0.0,
    val mach: Double = 0.0,
    val muzzleVelocity: Double = 0.0,
    val stability: Double = 0.0,
    val velocityScale: Double = 1.0,
    val dragScale: Double = 1.0,
    val spinDriftCm: Double = 0.0,
    val subsonic: Boolean = false,
    val transonicRangeM: Double = 0.0,
    val apexCm: Double = 0.0,
    val apexRangeM: Double = 0.0,
    val pointBlankNearM: Double = 0.0,
    val pointBlankFarM: Double = 0.0,
    val densityAltitudeM: Double = 0.0,
    val pressureHpa: Double = 0.0,
    val warnings: List<Warning> = emptyList(),
    val hasGust: Boolean = false,
    val gustWindage: Double = 0.0,
    val gustWindageClicks: Double = 0.0,
    val gustWindageCm: Double = 0.0,
    val dsf: List<DsfPointIn> = emptyList(),
    val hasLead: Boolean = false,
    val lead: Double = 0.0,
    val leadClicks: Double = 0.0,
    val leadCm: Double = 0.0,
    val leadTotalWindage: Double = 0.0,
    val leadTotalWindageClicks: Double = 0.0,
    val leadRangeM: Double = 0.0,
    val leadElevation: Double = 0.0,
    val leadElevationClicks: Double = 0.0,
    // Reticle hold (present with a scope).
    val hasReticle: Boolean = false,
    val holdMode: String = "",
    val dialElevationClicks: Double = 0.0,
    val dialWindageClicks: Double = 0.0,
    val targetX: Double = 0.0,
    val targetY: Double = 0.0,
    val subtensionScale: Double = 1.0,
    val focalPlane: String = "ffp",
    val minMagnification: Double = 0.0,
    val maxMagnification: Double = 0.0,
    val magnification: Double = 0.0,
    val reticleName: String = "",
    val reticleUnits: String = "mrad",
    val reticleDefinition: String = "",
)

/** Something to know about the solution; `code` as in applogic/session.h. */
@Serializable
data class Warning(val code: String = "", val value: Double = 0.0)

@Serializable
data class TableRow(
    val rangeM: Double = 0.0,
    val elevation: Double = 0.0,
    val windage: Double = 0.0,
    val elevationClicks: Double = 0.0,
    val windageClicks: Double = 0.0,
    val dropCm: Double = 0.0,
    val windageCm: Double = 0.0,
    val velocity: Double = 0.0,
    val mach: Double = 0.0,
    val energy: Double = 0.0,
    val time: Double = 0.0,
    val lead: Double = 0.0,
    val leadClicks: Double = 0.0,
    val leadCm: Double = 0.0,
    val spinDriftCm: Double = 0.0,
    /** Windage for each of the table's extra wind speeds (RangeTable.windSpeeds). */
    val windages: List<Double> = emptyList(),
    val windageClicksAt: List<Double> = emptyList(),
    val coriolisDriftCm: Double = 0.0,
    val coriolisLiftCm: Double = 0.0,
)

@Serializable
data class RangeTable(
    val ok: Boolean = false,
    val error: String = "",
    val hasScope: Boolean = false,
    val rows: List<TableRow> = emptyList(),
    val windSpeeds: List<Double> = emptyList(),
    // For a compared rifle + cartridge (compareCurves).
    val label: String = "",
    val rifleId: Long = 0,
    val cartridgeId: Long = 0,
)

/** A rifle and the cartridges of its calibre, for choosing what to compare. */
@Serializable
data class PairOption(val rifleId: Long = 0, val rifleName: String = "", val cartridges: List<NamedId> = emptyList())

@Serializable
data class NamedId(val id: Long = 0, val name: String = "")

/** A drag scale factor (DSF) point: the drag at `mach` times `factor`. */
@Serializable
data class DsfPointIn(val mach: Double = 0.0, val factor: Double = 1.0)

@Serializable
data class DsfShot(
    val shotId: Long = 0,
    val rangeM: Double = 0.0,
    val mach: Double = 0.0,
    val observed: Double = 0.0,
    val before: Double = 0.0,
    val after: Double = 0.0,
    val used: Boolean = false,
    val limited: Boolean = false,
)

@Serializable
data class DsfResult(
    val ok: Boolean = false,
    val error: String = "",
    val points: List<DsfPointIn> = emptyList(),
    val shots: List<DsfShot> = emptyList(),
    val rmsBefore: Double = 0.0,
    val rmsAfter: Double = 0.0,
)

/** What the BC calculator found (bcCalculator). */
@Serializable
data class BcCalc(val ok: Boolean = false, val error: String = "", val bc: Double = 0.0, val table: String = "")

/** Errors (1 sigma) and the target of the hit probability, as entered. */
@Serializable
data class WezSettings(
    val rangeM: Double = 5.0,
    val windSpeedMps: Double = 1.0,
    val windDirectionDeg: Double = 10.0,
    val muzzleVelocityMps: Double = 4.0,
    val bcPercent: Double = 1.0,
    val temperatureC: Double = 2.0,
    val pressureHpa: Double = 2.0,
    val humidityPct: Double = 10.0,
    val lookAngleDeg: Double = 0.5,
    val cantDeg: Double = 1.0,
    val azimuthDeg: Double = 5.0,
    val latitudeDeg: Double = 0.5,
    val groupMoa: Double = 1.0,
    val targetKind: String = "rectangle",
    val targetWidthCm: Double = 50.0,
    val targetHeightCm: Double = 50.0,
)

@Serializable
data class WezRow(val rangeM: Double = 0.0, val probability: Double = 0.0, val sigmaUpCm: Double = 0.0, val sigmaRightCm: Double = 0.0)

@Serializable
data class WezPart(val source: String = "", val upCm: Double = 0.0, val rightCm: Double = 0.0)

@Serializable
data class WezResult(
    val settings: WezSettings = WezSettings(),
    val ok: Boolean = false,
    val error: String = "",
    val rows: List<WezRow> = emptyList(),
    val atTarget: WezRow = WezRow(),
    val parts: List<WezPart> = emptyList(),
    val shots50: Int = 0,
    val shots80: Int = 0,
    val shots95: Int = 0,
)

/** A saved rifle, cartridge and conditions; [available] is false once one is deleted. */
@Serializable
data class SituationItem(
    val name: String = "",
    val rifleName: String = "",
    val cartridgeName: String = "",
    val rangeM: Double = 0.0,
    val available: Boolean = false,
)

/** A scope as its maker publishes it (published_scopes.json). */
@Serializable
data class LibraryScope(
    val maker: String = "",
    val model: String = "",
    val minMagnification: Double = 0.0,
    val maxMagnification: Double = 0.0,
    val focalPlane: String = "ffp",
    val sfpReferenceMagnification: Double = 0.0,
    val clicks: List<ScopeClick> = emptyList(),
    val reticles: List<String> = emptyList(),
    val source: String = "",
)

@Serializable
data class ScopeClick(val units: String = "mrad", val value: Double = 0.1)

/** A rifle variant as its maker publishes it (published_rifles.json). */
@Serializable
data class LibraryRifle(
    val maker: String = "",
    val model: String = "",
    val caliber: String = "",
    val twistIn: Double = 0.0,
    val barrelsIn: List<Double> = emptyList(),
    val source: String = "",
)

/** Interface preferences (the core keeps them in "ui.prefs"). */
@Serializable
data class UiPrefs(
    /** "system" | "light" | "dark" | "night" (red on black). */
    val theme: String = "system",
    val keepScreenOn: Boolean = false,
    /** How a correction shows its direction: "words" (UP, LEFT), "arrows" (↑ ←) or "signs" (+ −). */
    val correctionStyle: String = "words",
    /** Corrections rounded to whole clicks (what the turret can set) instead of exact. */
    val roundToClicks: Boolean = false,
    /** Also the other angle unit and the centimetres at the target, in small print. */
    val showSecondUnit: Boolean = true,
    /** Range card columns by key ("elev", "v", ...); empty: chosen for the screen width. */
    val tableColumns: List<String> = emptyList(),
    /** Extra wind speeds (m/s) with a windage column each in the range card. */
    val tableWinds: List<Double> = emptyList(),
)

/** A named target of the target card, with its corrections and its hold on the reticle. */
@Serializable
data class TargetItem(
    val name: String = "",
    val rangeM: Double = 300.0,
    val lookAngleDeg: Double = 0.0,
    val windSpeed: Double = 0.0,
    val windFromDeg: Double = 90.0,
    val ok: Boolean = false,
    val elevation: Double = 0.0,
    val windage: Double = 0.0,
    val elevationClicks: Double = 0.0,
    val windageClicks: Double = 0.0,
    /** Where it is held on the reticle with the turrets as set (reticle mrad, y up). */
    val holdX: Double = 0.0,
    val holdY: Double = 0.0,
)
