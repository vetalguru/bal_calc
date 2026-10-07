package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableDoubleStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.platform.testTag
import androidx.compose.foundation.layout.Row
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.TextButton
import androidx.compose.ui.Alignment
import androidx.compose.ui.text.font.FontWeight
import org.vetalguru.balcalc.core.WindZoneIn
import kotlin.math.roundToInt
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** Everything about the shot that is not the rifle: air, powder, wind, angles, Earth rotation. */
@Composable
fun ConditionsScreen(model: AppModel) {
    val c = model.state.conditions
    val scope = rememberCoroutineScope()
    // Pressure can be typed as station pressure or as sea-level QNH.
    var qnhMode by remember { mutableStateOf(false) }
    var qnh by remember { mutableDoubleStateOf(1013.25) }
    fun stationFrom(qnhHpa: Double, altitude: Double) = scope.launch {
        val p = model.stationPressure(qnhHpa, altitude)
        model.updateConditions { it.copy(pressureHpa = p) }
    }

    val deg = stringResource(Res.string.unit_deg)
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val wide = maxWidth >= 600.dp
        Column(
            Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(vertical = 12.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Section(stringResource(Res.string.atmosphere)) {
                Fields(
                    wide,
                    { m ->
                        NumberField(
                            stringResource(Res.string.temperature), c.temperatureC,
                            { v -> model.updateConditions { it.copy(temperatureC = v) } },
                            m, stringResource(Res.string.unit_c), from = -60.0, to = 60.0, tag = "temperature",
                        )
                    },
                    { m ->
                        NumberField(
                            stringResource(Res.string.altitude), c.altitudeM,
                            { v ->
                                model.updateConditions { it.copy(altitudeM = v) }
                                if (qnhMode) stationFrom(qnh, v)
                            },
                            m, stringResource(Res.string.unit_m), decimals = 0, from = -500.0, to = 6000.0, tag = "altitude",
                        )
                    },
                    { m ->
                        if (c.useDensityAltitude) {
                            NumberField(
                                stringResource(Res.string.density_altitude), c.densityAltitudeM,
                                { v -> model.updateConditions { it.copy(densityAltitudeM = v) } },
                                m, stringResource(Res.string.unit_m), decimals = 0, from = -2000.0, to = 8000.0,
                                tag = "densityAltitude",
                            )
                        } else NumberField(
                            stringResource(if (qnhMode) Res.string.pressure_qnh else Res.string.pressure_station),
                            if (qnhMode) qnh else c.pressureHpa,
                            { v ->
                                if (qnhMode) {
                                    qnh = v
                                    stationFrom(v, c.altitudeM)
                                } else {
                                    model.updateConditions { it.copy(pressureHpa = v) }
                                }
                            },
                            m, stringResource(Res.string.unit_hpa), from = 300.0, to = 1200.0, tag = "pressure",
                        )
                    },
                    { m ->
                        NumberField(
                            stringResource(Res.string.humidity), c.humidityPct,
                            { v -> model.updateConditions { it.copy(humidityPct = v) } },
                            m, stringResource(Res.string.unit_percent), decimals = 0, from = 0.0, to = 100.0,
                        )
                    },
                )
                SwitchRow(
                    stringResource(Res.string.enter_density_altitude), c.useDensityAltitude,
                    { on ->
                        // Start from the density altitude of the air as it is now.
                        val da = model.solution.densityAltitudeM
                        model.updateConditions {
                            if (on && model.solution.ok) it.copy(useDensityAltitude = true, densityAltitudeM = da.roundToInt().toDouble())
                            else it.copy(useDensityAltitude = on)
                        }
                    },
                    modifier = Modifier.testTag("useDensityAltitude"),
                )
                if (!c.useDensityAltitude) SwitchRow(stringResource(Res.string.enter_qnh), qnhMode, { qnhMode = it })
                AirSensorButtons(model) { qnhMode = false }
                val sol = model.solution
                if (sol.ok && (qnhMode || c.useDensityAltitude)) {
                    Text(
                        stringResource(Res.string.station_pressure_is, sol.pressureHpa.fixed(1)),
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                if (sol.ok && !c.useDensityAltitude) {
                    Text(
                        stringResource(Res.string.density_altitude) + ": " + sol.densityAltitudeM.roundToInt() + " " + stringResource(Res.string.unit_m),
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }

            Section(stringResource(Res.string.powder)) {
                SwitchRow(
                    stringResource(Res.string.powder_follows_air), c.powderFollowsAir,
                    { on -> model.updateConditions { it.copy(powderFollowsAir = on) } },
                )
                if (!c.powderFollowsAir) {
                    NumberField(
                        stringResource(Res.string.powder_temperature), c.powderC,
                        { v -> model.updateConditions { it.copy(powderC = v) } },
                        unit = stringResource(Res.string.unit_c), from = -60.0, to = 80.0,
                    )
                }
            }

            Section(stringResource(Res.string.wind)) {
                val windUnit = WindUnit.of(model.state.prefs.windUnit)
                val setWindUnit = { u: WindUnit -> model.setPrefs { it.copy(windUnit = u.key) } }
                val zoned = c.windZones.isNotEmpty()
                if (zoned) Text(stringResource(Res.string.zone_title, 1), fontWeight = FontWeight.Bold)
                Fields(
                    wide,
                    { m ->
                        WindSpeedField(
                            stringResource(Res.string.wind_speed), c.windSpeed,
                            { v -> model.updateConditions { it.copy(windSpeed = v) } },
                            windUnit, setWindUnit, m, tag = "conditionsWindSpeed", fieldMaxWidth = FIELD_MAX,
                        )
                    },
                    { m ->
                        DirectionField(c.windFromDeg, { v -> model.updateConditions { it.copy(windFromDeg = v) } }, m, "windFrom")
                    },
                )
                if (zoned) {
                    NumberField(
                        stringResource(Res.string.zone_until), c.windUntilM,
                        { v -> model.updateConditions { it.copy(windUntilM = v) } },
                        unit = stringResource(Res.string.unit_m), decimals = 0, from = 10.0, to = 3000.0,
                        tag = "zoneUntil0",
                    )
                }
                WindSpeedField(
                    stringResource(Res.string.wind_gust), c.windGustMps,
                    { v -> model.updateConditions { it.copy(windGustMps = v) } },
                    windUnit, setWindUnit, tag = "windGust", fieldMaxWidth = FIELD_MAX,
                    hint = stringResource(Res.string.wind_gust_hint),
                )
                SwitchRow(
                    stringResource(Res.string.wind_zones_on), zoned,
                    { on ->
                        model.updateConditions {
                            if (!on) {
                                it.copy(windZones = emptyList())
                            } else {
                                // The first third of the current range, then the same wind beyond.
                                val until = (it.targetRangeM / 3).roundToInt().coerceAtLeast(50).toDouble()
                                it.copy(
                                    windUntilM = until,
                                    windZones = listOf(WindZoneIn(it.windSpeed, it.windFromDeg)),
                                )
                            }
                        }
                    },
                    modifier = Modifier.testTag("windZonesOn"),
                )
                c.windZones.forEachIndexed { i, z ->
                    val last = i == c.windZones.lastIndex
                    fun change(f: (WindZoneIn) -> WindZoneIn) = model.updateConditions {
                        it.copy(windZones = it.windZones.mapIndexed { j, old -> if (j == i) f(old) else old })
                    }
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            stringResource(Res.string.zone_title, i + 2) +
                                if (last) " — " + stringResource(Res.string.zone_to_end) else "",
                            fontWeight = FontWeight.Bold,
                            modifier = Modifier.weight(1f),
                        )
                        TextButton(
                            onClick = {
                                model.updateConditions {
                                    it.copy(windZones = it.windZones.filterIndexed { j, _ -> j != i })
                                }
                            },
                            modifier = Modifier.testTag("removeZone$i"),
                        ) { Text(stringResource(Res.string.remove_zone)) }
                    }
                    Fields(
                        wide,
                        { m ->
                            WindSpeedField(
                                stringResource(Res.string.wind_speed), z.speedMps,
                                { v -> change { it.copy(speedMps = v) } },
                                windUnit, setWindUnit, m, tag = "zoneSpeed${i + 1}", fieldMaxWidth = FIELD_MAX,
                            )
                        },
                        { m ->
                            DirectionField(z.fromDeg, { v -> change { it.copy(fromDeg = v) } }, m, "zoneFrom${i + 1}")
                        },
                    )
                    if (!last) {
                        NumberField(
                            stringResource(Res.string.zone_until), z.untilM,
                            { v -> change { it.copy(untilM = v) } },
                            unit = stringResource(Res.string.unit_m), decimals = 0, from = 10.0, to = 3000.0,
                            tag = "zoneUntil${i + 1}",
                        )
                    }
                }
                if (zoned && c.windZones.size < 2) {
                    OutlinedButton(
                        onClick = {
                            model.updateConditions {
                                // The zone that went to the end now stops halfway to the target.
                                val start = maxOf(it.windUntilM, it.windZones.dropLast(1).maxOfOrNull { z -> z.untilM } ?: 0.0)
                                val until = ((start + it.targetRangeM.coerceAtLeast(start + 100)) / 2).roundToInt().toDouble()
                                val zones = it.windZones.toMutableList()
                                val lastZone = zones.removeAt(zones.lastIndex)
                                it.copy(windZones = zones + lastZone.copy(untilM = until) + lastZone.copy(untilM = 0.0))
                            }
                        },
                        modifier = Modifier.testTag("addZone"),
                    ) { Text(stringResource(Res.string.add_zone)) }
                }
            }

            Section(stringResource(Res.string.angles)) {
                Fields(
                    wide,
                    { m ->
                        StepperField(
                            stringResource(Res.string.look_angle_short), c.lookAngleDeg,
                            { v -> model.updateConditions { it.copy(lookAngleDeg = v) } },
                            m, deg, from = -60.0, to = 60.0, tag = "lookAngle", fieldMaxWidth = FIELD_MAX,
                            hint = stringResource(Res.string.shot_angle_hint),
                        )
                    },
                    { m ->
                        StepperField(
                            stringResource(Res.string.cant), c.cantDeg,
                            { v -> model.updateConditions { it.copy(cantDeg = v) } },
                            m, deg, from = -45.0, to = 45.0, tag = "cantAngle", fieldMaxWidth = FIELD_MAX,
                            hint = stringResource(Res.string.cant_hint),
                        )
                    },
                )
                TiltButton(model)
            }

            Section(stringResource(Res.string.target_section)) {
                NumberField(
                    stringResource(Res.string.target_height), c.targetHeightCm,
                    { v -> model.updateConditions { it.copy(targetHeightCm = v) } },
                    unit = stringResource(Res.string.unit_cm), decimals = 0, from = 1.0, to = 300.0,
                    tag = "targetHeight",
                )
            }

            Section(stringResource(Res.string.coriolis_section)) {
                SwitchRow(
                    stringResource(Res.string.coriolis_on), c.coriolis,
                    { on -> model.updateConditions { it.copy(coriolis = on) } },
                )
                if (c.coriolis) {
                    NumberField(
                        stringResource(Res.string.latitude), c.latitudeDeg,
                        { v -> model.updateConditions { it.copy(latitudeDeg = v) } },
                        unit = deg, from = -90.0, to = 90.0, tag = "latitude",
                    )
                    SwitchRow(
                        stringResource(Res.string.known_direction), c.useAzimuth,
                        { on -> model.updateConditions { it.copy(useAzimuth = on) } },
                    )
                    CompassButton(model)
                    if (c.useAzimuth) {
                        NumberField(
                            stringResource(Res.string.azimuth), c.azimuthDeg,
                            { v -> model.updateConditions { it.copy(azimuthDeg = v % 360) } },
                            unit = deg, decimals = 0, from = 0.0, to = 360.0, tag = "azimuth",
                        )
                    }
                }
            }
        }
    }
}

/** Fields with steps are no wider than a number needs, even on a wide card. */
private val FIELD_MAX = 200.dp

/** Where the wind blows from, in degrees with its clock hour; steps of 15° that go round. */
@Composable
private fun DirectionField(fromDeg: Double, onDeg: (Double) -> Unit, modifier: Modifier, tag: String) {
    StepperField(
        stringResource(Res.string.wind_from), fromDeg, { v -> onDeg(v % 360) }, modifier,
        "°·" + stringResource(Res.string.wind_clock_short, clockHour(fromDeg)),
        step = 15.0, from = 0.0, to = 360.0, tag = tag, fieldMaxWidth = FIELD_MAX,
        hint = stringResource(Res.string.wind_direction_hint), wrap = true, decimals = 0,
    )
}
