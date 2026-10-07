package org.vetalguru.balcalc

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.hardware.GeomagneticField
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Build
import android.os.CancellationSignal
import android.os.Looper
import androidx.activity.ComponentActivity
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import kotlin.coroutines.resume
import kotlin.math.PI
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull

/**
 * The phone's barometer, location, compass and gravity. Create it in the
 * activity's onCreate (it registers the permission request). Nothing runs
 * in the background: each sensor is on only while a reading is taken or a
 * flow is collected.
 */
class AndroidSensors(private val activity: ComponentActivity) : PhoneSensors {
    private val sensors = activity.getSystemService(Context.SENSOR_SERVICE) as SensorManager
    private val locations = activity.getSystemService(Context.LOCATION_SERVICE) as LocationManager

    private var onPermission: ((Boolean) -> Unit)? = null
    private val permission: ActivityResultLauncher<Array<String>> =
        activity.registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { granted ->
            onPermission?.invoke(granted.values.any { it })
        }

    private val barometer: Sensor? = sensors.getDefaultSensor(Sensor.TYPE_PRESSURE)
    private val gravity: Sensor? =
        sensors.getDefaultSensor(Sensor.TYPE_GRAVITY) ?: sensors.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
    private val rotation: Sensor? = sensors.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR)

    override val hasBarometer get() = barometer != null
    override val hasLocation get() = activity.packageManager.hasSystemFeature(PackageManager.FEATURE_LOCATION)
    override val hasCompass get() = rotation != null
    override val hasTilt get() = gravity != null

    /** Readings of one sensor for about `millis`, then off. */
    private suspend fun sample(sensor: Sensor, millis: Long): List<FloatArray> {
        val out = mutableListOf<FloatArray>()
        withTimeoutOrNull(millis) {
            suspendCancellableCoroutine<Unit> { c ->
                val listener = object : SensorEventListener {
                    override fun onSensorChanged(e: SensorEvent) { out += e.values.clone() }
                    override fun onAccuracyChanged(s: Sensor, accuracy: Int) {}
                }
                sensors.registerListener(listener, sensor, SensorManager.SENSOR_DELAY_GAME)
                c.invokeOnCancellation { sensors.unregisterListener(listener) }
            }
        }
        return out
    }

    override suspend fun pressureHpa(): Double? {
        val s = barometer ?: return null
        val readings = sample(s, 1500)
        return readings.map { it[0].toDouble() }.takeIf { it.isNotEmpty() }?.average()
    }

    private fun granted() = listOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION)
        .any { activity.checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }

    private suspend fun askPermission(): Boolean = granted() || suspendCancellableCoroutine { c ->
        onPermission = { c.resume(it) }
        permission.launch(arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION))
    }

    @Suppress("MissingPermission", "DEPRECATION")
    private suspend fun currentLocation(provider: String): Location? = withTimeoutOrNull(30_000) {
        suspendCancellableCoroutine { c ->
            if (Build.VERSION.SDK_INT >= 30) {
                val cancel = CancellationSignal()
                c.invokeOnCancellation { cancel.cancel() }
                locations.getCurrentLocation(provider, cancel, activity.mainExecutor) { c.resume(it) }
            } else {
                val listener = LocationListener { c.resume(it) }
                c.invokeOnCancellation { locations.removeUpdates(listener) }
                locations.requestSingleUpdate(provider, listener, Looper.getMainLooper())
            }
        }
    }

    override suspend fun location(): GeoFix? {
        if (!askPermission()) return null
        val provider = when {
            locations.isProviderEnabled(LocationManager.GPS_PROVIDER) -> LocationManager.GPS_PROVIDER
            locations.isProviderEnabled(LocationManager.NETWORK_PROVIDER) -> LocationManager.NETWORK_PROVIDER
            else -> return null
        }
        val loc = currentLocation(provider) ?: return null
        // Height above the sea, not above the ellipsoid GPS works in (tens of
        // metres apart); Android 14+ converts it with its geoid model.
        if (Build.VERSION.SDK_INT >= 34 && loc.hasAltitude() && !loc.hasMslAltitude()) {
            runCatching {
                withContext(Dispatchers.IO) {
                    android.location.altitude.AltitudeConverter().addMslAltitudeToLocation(activity, loc)
                }
            }
        }
        val msl = if (Build.VERSION.SDK_INT >= 34 && loc.hasMslAltitude()) loc.mslAltitudeMeters else null
        return GeoFix(loc.latitude, loc.longitude, msl ?: loc.altitude.takeIf { loc.hasAltitude() }, loc.accuracy.toDouble())
    }

    override fun tilt(): Flow<Tilt> = callbackFlow {
        val s = gravity ?: run { close(); return@callbackFlow }
        val window = ArrayDeque<Pair<Long, Pair<Double, Double>>>()
        var lastSent = 0L
        val listener = object : SensorEventListener {
            override fun onSensorChanged(e: SensorEvent) {
                val now = e.timestamp / 1_000_000 // ms
                window.addLast(now to tiltFromGravity(e.values[0].toDouble(), e.values[1].toDouble(), e.values[2].toDouble()))
                while (window.isNotEmpty() && now - window.first().first > 1000) window.removeFirst()
                if (now - lastSent >= 200) {
                    lastSent = now
                    val (look, lookSpread) = steady(window.map { it.second.first })
                    val (cant, cantSpread) = steady(window.map { it.second.second })
                    trySend(Tilt(look, cant, maxOf(lookSpread, cantSpread)))
                }
            }
            override fun onAccuracyChanged(s: Sensor, accuracy: Int) {}
        }
        sensors.registerListener(listener, s, SensorManager.SENSOR_DELAY_GAME)
        awaitClose { sensors.unregisterListener(listener) }
    }

    override fun heading(at: GeoFix?): Flow<Heading> = callbackFlow {
        val s = rotation ?: run { close(); return@callbackFlow }
        val declination = at?.let {
            GeomagneticField(it.latitudeDeg.toFloat(), it.longitudeDeg.toFloat(), (it.altitudeM ?: 0.0).toFloat(),
                System.currentTimeMillis()).declination.toDouble()
        }
        var poor = false
        val m = FloatArray(9)
        val angles = FloatArray(3)
        var lastSent = 0L
        val listener = object : SensorEventListener {
            override fun onSensorChanged(e: SensorEvent) {
                val now = e.timestamp / 1_000_000
                if (now - lastSent < 200) return
                lastSent = now
                SensorManager.getRotationMatrixFromVector(m, e.values)
                SensorManager.getOrientation(m, angles)
                val magnetic = angles[0] * 180.0 / PI
                trySend(Heading(normalizeDeg(magnetic + (declination ?: 0.0)), declination != null, poor))
            }
            override fun onAccuracyChanged(s: Sensor, accuracy: Int) {
                poor = accuracy <= SensorManager.SENSOR_STATUS_ACCURACY_LOW
            }
        }
        sensors.registerListener(listener, s, SensorManager.SENSOR_DELAY_GAME)
        awaitClose { sensors.unregisterListener(listener) }
    }
}
