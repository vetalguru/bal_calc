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
                            m, stringResource(Res.string.unit_m), decimals = 0, from = -500.0, to = 6000.0,
                        )
                    },
                    { m ->
                        NumberField(
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
                            m, stringResource(Res.string.unit_hpa), from = 300.0, to = 1200.0,
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
                SwitchRow(stringResource(Res.string.enter_qnh), qnhMode, { qnhMode = it })
                if (qnhMode) {
                    Text(
                        stringResource(Res.string.station_pressure_is, c.pressureHpa.fixed(1)),
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
                Fields(
                    wide,
                    { m ->
                        NumberField(
                            stringResource(Res.string.speed), c.windSpeed,
                            { v -> model.updateConditions { it.copy(windSpeed = v) } },
                            m, stringResource(Res.string.unit_mps), from = 0.0, to = 40.0,
                        )
                    },
                    { m ->
                        NumberField(
                            stringResource(Res.string.wind_from_degrees), c.windFromDeg,
                            { v -> model.updateConditions { it.copy(windFromDeg = v % 360) } },
                            m, deg, decimals = 0, from = 0.0, to = 360.0,
                        )
                    },
                )
            }

            Section(stringResource(Res.string.angles)) {
                Fields(
                    wide,
                    { m ->
                        NumberField(
                            stringResource(Res.string.shot_angle), c.lookAngleDeg,
                            { v -> model.updateConditions { it.copy(lookAngleDeg = v) } },
                            m, deg, from = -60.0, to = 60.0,
                        )
                    },
                    { m ->
                        NumberField(
                            stringResource(Res.string.cant), c.cantDeg,
                            { v -> model.updateConditions { it.copy(cantDeg = v) } },
                            m, deg, from = -45.0, to = 45.0,
                        )
                    },
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
                        unit = deg, from = -90.0, to = 90.0,
                    )
                    SwitchRow(
                        stringResource(Res.string.known_direction), c.useAzimuth,
                        { on -> model.updateConditions { it.copy(useAzimuth = on) } },
                    )
                    if (c.useAzimuth) {
                        NumberField(
                            stringResource(Res.string.azimuth), c.azimuthDeg,
                            { v -> model.updateConditions { it.copy(azimuthDeg = v % 360) } },
                            unit = deg, decimals = 0, from = 0.0, to = 360.0,
                        )
                    }
                }
            }
        }
    }
}
