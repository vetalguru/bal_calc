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
)

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
)

@Serializable
data class RangeTable(
    val ok: Boolean = false,
    val error: String = "",
    val hasScope: Boolean = false,
    val rows: List<TableRow> = emptyList(),
)
