package org.vetalguru.balcalc.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt
import kotlinx.coroutines.launch
import org.jetbrains.compose.resources.stringResource
import org.vetalguru.balcalc.AppModel
import org.vetalguru.balcalc.LocalPlatform
import org.vetalguru.balcalc.fixed
import org.vetalguru.balcalc.res.Res
import org.vetalguru.balcalc.res.*

/** Barometer and location buttons of the air section; nothing without sensors. */
@Composable
fun AirSensorButtons(model: AppModel, onPressure: () -> Unit) {
    val sensors = LocalPlatform.current.sensors ?: return
    val scope = rememberCoroutineScope()
    var busy by remember { mutableStateOf(false) }
    // What the last reading gave: rendered below in the current language.
    var pressure by remember { mutableStateOf<Double?>(null) }
    var fixShown by remember { mutableStateOf<org.vetalguru.balcalc.GeoFix?>(null) }
    var failed by remember { mutableStateOf<String?>(null) } // "pressure" or "location"
    FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        if (sensors.hasBarometer) {
            OutlinedButton(
                enabled = !busy,
                onClick = {
                    scope.launch {
                        busy = true
                        val p = sensors.pressureHpa()
                        busy = false
                        fixShown = null
                        pressure = p
                        failed = if (p == null) "pressure" else null
                        if (p != null) {
                            onPressure()
                            model.updateConditions { it.copy(pressureHpa = (p * 10).roundToInt() / 10.0, useDensityAltitude = false) }
                        }
                    }
                },
                modifier = Modifier.testTag("fromBarometer"),
            ) { Text(stringResource(Res.string.sensor_barometer)) }
        }
        if (sensors.hasLocation) {
            OutlinedButton(
                enabled = !busy,
                onClick = {
                    scope.launch {
                        busy = true
                        val fix = sensors.location()
                        busy = false
                        pressure = null
                        fixShown = fix
                        failed = if (fix == null) "location" else null
                        if (fix == null) return@launch
                        model.lastFix = fix
                        model.updateConditions {
                            it.copy(
                                altitudeM = fix.altitudeM?.roundToInt()?.toDouble() ?: it.altitudeM,
                                latitudeDeg = (fix.latitudeDeg * 100).roundToInt() / 100.0,
                            )
                        }
                    }
                },
                modifier = Modifier.testTag("myLocation"),
            ) { Text(stringResource(Res.string.sensor_location)) }
        }
    }
    if (busy) Text(stringResource(Res.string.sensor_reading), color = MaterialTheme.colorScheme.onSurfaceVariant)
    val note = when {
        failed == "pressure" -> stringResource(Res.string.sensor_no_pressure)
        failed == "location" -> stringResource(Res.string.sensor_no_location)
        pressure != null -> stringResource(Res.string.sensor_pressure_set, pressure!!.fixed(1))
        fixShown != null -> fixShown!!.let { f ->
            val acc = f.accuracyM.roundToInt()
            val lat = f.latitudeDeg.fixed(2)
            f.altitudeM?.let { h -> stringResource(Res.string.sensor_location_set, acc, h.roundToInt(), lat) }
                ?: stringResource(Res.string.sensor_location_no_alt, acc, lat)
        }
        else -> null
    }
    note?.let { Text(it, color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.testTag("sensorNote")) }
}

/** "Measure with the phone" for the shot angle and cant; nothing without a tilt sensor. */
@Composable
fun TiltButton(model: AppModel) {
    val sensors = LocalPlatform.current.sensors
    if (sensors == null || !sensors.hasTilt) return
    var open by remember { mutableStateOf(false) }
    OutlinedButton(onClick = { open = true }, modifier = Modifier.testTag("measureAngles")) {
        Text(stringResource(Res.string.sensor_angles))
    }
    if (!open) return
    val tilt by remember(sensors) { sensors.tilt() }.collectAsState(null)
    AlertDialog(
        onDismissRequest = { open = false },
        title = { Text(stringResource(Res.string.tilt_title)) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(stringResource(Res.string.tilt_hint), color = MaterialTheme.colorScheme.onSurfaceVariant)
                val t = tilt
                if (t == null) {
                    Text(stringResource(Res.string.sensor_reading))
                } else {
                    Text(stringResource(Res.string.tilt_now, t.lookAngleDeg.fixed(1), t.cantDeg.fixed(1)),
                        fontSize = 22.sp, fontWeight = FontWeight.Bold, modifier = Modifier.testTag("tiltNow"))
                    Text(
                        if (t.unsteadyDeg > 0.3) stringResource(Res.string.tilt_unsteady, t.unsteadyDeg.fixed(1))
                        else stringResource(Res.string.tilt_steady),
                        color = if (t.unsteadyDeg > 0.3) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.primary,
                    )
                }
            }
        },
        confirmButton = {
            TextButton(
                enabled = tilt != null,
                onClick = {
                    val t = tilt ?: return@TextButton
                    model.updateConditions {
                        it.copy(lookAngleDeg = (t.lookAngleDeg * 10).roundToInt() / 10.0, cantDeg = (t.cantDeg * 10).roundToInt() / 10.0)
                    }
                    open = false
                },
                modifier = Modifier.testTag("useAngles"),
            ) { Text(stringResource(Res.string.sensor_use)) }
        },
        dismissButton = { TextButton(onClick = { open = false }) { Text(stringResource(Res.string.cancel)) } },
    )
}

/** "From the compass" for the shot azimuth; nothing without a compass. */
@Composable
fun CompassButton(model: AppModel) {
    val sensors = LocalPlatform.current.sensors
    if (sensors == null || !sensors.hasCompass) return
    var open by remember { mutableStateOf(false) }
    OutlinedButton(onClick = { open = true }, modifier = Modifier.testTag("fromCompass")) {
        Text(stringResource(Res.string.sensor_compass))
    }
    if (!open) return
    val fix = model.lastFix
    val heading by remember(sensors, fix) { sensors.heading(fix) }.collectAsState(null)
    AlertDialog(
        onDismissRequest = { open = false },
        title = { Text(stringResource(Res.string.compass_title)) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(stringResource(Res.string.compass_hint), color = MaterialTheme.colorScheme.onSurfaceVariant)
                val h = heading
                if (h == null) {
                    Text(stringResource(Res.string.sensor_reading))
                } else {
                    Text("${h.azimuthDeg.roundToInt()}°", fontSize = 40.sp, fontWeight = FontWeight.Bold, modifier = Modifier.testTag("headingNow"))
                    Text(stringResource(if (h.trueNorth) Res.string.compass_true else Res.string.compass_magnetic),
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                    if (h.needsCalibration) Text(stringResource(Res.string.compass_calibrate), color = MaterialTheme.colorScheme.error)
                }
            }
        },
        confirmButton = {
            TextButton(
                enabled = heading != null,
                onClick = {
                    val h = heading ?: return@TextButton
                    model.updateConditions { it.copy(useAzimuth = true, azimuthDeg = h.azimuthDeg.roundToInt().toDouble() % 360) }
                    open = false
                },
                modifier = Modifier.testTag("useHeading"),
            ) { Text(stringResource(Res.string.sensor_use)) }
        },
        dismissButton = { TextButton(onClick = { open = false }) { Text(stringResource(Res.string.cancel)) } },
    )
}
