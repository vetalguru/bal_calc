package org.vetalguru.balcalc

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.hardware.GeomagneticField
import android.hardware.Sensor
import android.hardware.SensorManager
import android.location.Location
import android.location.LocationManager
import android.os.Looper
import androidx.activity.ComponentActivity
import androidx.test.core.app.ApplicationProvider
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.async
import kotlinx.coroutines.flow.toList
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.advanceTimeBy
import kotlinx.coroutines.test.runCurrent
import kotlinx.coroutines.test.runTest
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config
import org.robolectric.shadows.SensorEventBuilder

@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(RobolectricTestRunner::class)
class AndroidSensorsTest {
    private val allSensors = listOf(Sensor.TYPE_PRESSURE, Sensor.TYPE_GRAVITY, Sensor.TYPE_ROTATION_VECTOR)

    private fun sensors(types: List<Int> = allSensors): Pair<ComponentActivity, AndroidSensors> {
        val (controller, sensors) = startedActivity(types) { AndroidSensors(it) }
        return controller.get() to sensors
    }

    /** One reading of [type] at [ms] milliseconds of uptime. */
    private fun ComponentActivity.send(type: Int, ms: Long, vararg values: Float) {
        val s = sensor(type)
        shadowOf(sensorManager).sendSensorEventToListeners(
            SensorEventBuilder.newBuilder().setSensor(s).setTimestamp(ms * 1_000_000).setValues(values).build(), s,
        )
    }

    private val ComponentActivity.listeners get() = shadowOf(sensorManager).listeners

    private val ComponentActivity.locations get() = getSystemService(Context.LOCATION_SERVICE) as LocationManager

    private fun grantLocation() = shadowOf(ApplicationProvider.getApplicationContext<android.app.Application>())
        .grantPermissions(Manifest.permission.ACCESS_FINE_LOCATION)

    private fun fix(provider: String, lat: Double, lon: Double, accuracy: Float = 5f) = Location(provider).apply {
        latitude = lat
        longitude = lon
        this.accuracy = accuracy
        time = System.currentTimeMillis()
        elapsedRealtimeNanos = android.os.SystemClock.elapsedRealtimeNanos()
    }

    // --- what the phone has --------------------------------------------------

    /** Each has* follows the sensor the phone reports. */
    @Test
    fun hasFollowsTheSensors() {
        val (_, all) = sensors()
        assertTrue(all.hasBarometer && all.hasTilt && all.hasCompass)
    }

    /** A phone without the sensors has none of them. */
    @Test
    fun noSensorsNoReadings() {
        val (_, none) = sensors(emptyList())
        assertFalse(none.hasBarometer || none.hasTilt || none.hasCompass)
    }

    /** Without a gravity sensor the accelerometer gives the tilt. */
    @Test
    fun accelerometerStandsInForGravity() {
        assertTrue(sensors(listOf(Sensor.TYPE_ACCELEROMETER)).second.hasTilt)
    }

    /** hasLocation is the location feature of the phone. */
    @Test
    fun hasLocationIsTheFeature() {
        val pm = shadowOf(ApplicationProvider.getApplicationContext<Context>().packageManager)
        pm.setSystemFeature(PackageManager.FEATURE_LOCATION, true)
        assertTrue(sensors().second.hasLocation)
        pm.setSystemFeature(PackageManager.FEATURE_LOCATION, false)
        assertFalse(sensors().second.hasLocation)
    }

    // --- barometer -----------------------------------------------------------

    /** The pressure is the mean of the readings within 1.5 s; the barometer is off afterwards. */
    @Test
    fun pressureIsAveraged() = runTest {
        val (activity, sensors) = sensors()
        val pressure = async { sensors.pressureHpa() }
        runCurrent()
        assertEquals(1, activity.listeners.size)
        for ((i, hpa) in listOf(1000f, 1001f, 1002.5f).withIndex()) activity.send(Sensor.TYPE_PRESSURE, 10_000L + i * 20, hpa)
        advanceTimeBy(1_501)
        assertEquals(1001.1666, pressure.await()!!, 1e-3)
        assertTrue(activity.listeners.isEmpty())
    }

    /** No readings in time: null, and the barometer is off. */
    @Test
    fun noPressureIsNull() = runTest {
        val (activity, sensors) = sensors()
        val pressure = async { sensors.pressureHpa() }
        advanceTimeBy(2_000)
        assertNull(pressure.await())
        assertTrue(activity.listeners.isEmpty())
    }

    /** No barometer: null at once, nothing registered. */
    @Test
    fun noBarometerIsNull() = runTest {
        val (activity, sensors) = sensors(emptyList())
        assertNull(sensors.pressureHpa())
        assertEquals(0L, testScheduler.currentTime)
        assertTrue(activity.listeners.isEmpty())
    }

    // --- tilt ----------------------------------------------------------------

    /** Gravity of a phone at this look angle and cant (as SensorsTest has it). */
    private fun gravity(lookDeg: Double, cantDeg: Double): FloatArray {
        val l = lookDeg * PI / 180
        val c = cantDeg * PI / 180
        val horizontal = 9.81 * cos(l)
        return floatArrayOf((-horizontal * sin(c)).toFloat(), (9.81 * sin(l)).toFloat(), (horizontal * cos(c)).toFloat())
    }

    private fun TestScope.collectTilt(sensors: AndroidSensors, into: MutableList<Tilt>) =
        launch(UnconfinedTestDispatcher(testScheduler)) { sensors.tilt().toList(into) }

    /** Readings become angles; at most one value per 200 ms; the sensor is off when collection stops. */
    @Test
    fun tiltIsSentFiveTimesASecond() = runTest {
        val (activity, sensors) = sensors()
        val got = mutableListOf<Tilt>()
        val job = collectTilt(sensors, got)
        assertEquals(1, activity.listeners.size)
        activity.send(Sensor.TYPE_GRAVITY, 10_000, *gravity(5.0, -2.0))
        activity.send(Sensor.TYPE_GRAVITY, 10_100, *gravity(5.0, -2.0)) // too soon: kept, not sent
        activity.send(Sensor.TYPE_GRAVITY, 10_200, *gravity(5.0, -2.0))
        assertEquals(2, got.size)
        assertEquals(5.0, got[0].lookAngleDeg, 1e-4)
        assertEquals(-2.0, got[0].cantDeg, 1e-4)
        assertEquals(0.0, got[1].unsteadyDeg, 1e-4)
        job.cancel()
        assertTrue(activity.listeners.isEmpty())
    }

    /** The angles are a mean over the last second, the spread is how far they moved; older readings drop out. */
    @Test
    fun tiltIsAveragedOverASecond() = runTest {
        val (activity, sensors) = sensors()
        val got = mutableListOf<Tilt>()
        val job = collectTilt(sensors, got)
        activity.send(Sensor.TYPE_GRAVITY, 10_000, *gravity(4.0, 0.0))
        activity.send(Sensor.TYPE_GRAVITY, 10_200, *gravity(6.0, 1.0))
        assertEquals(5.0, got.last().lookAngleDeg, 1e-4)
        assertEquals(0.5, got.last().cantDeg, 1e-4)
        assertEquals(1.0, got.last().unsteadyDeg, 1e-4) // look moved ±1, cant ±0.5
        // 1.1 s later the first two are out of the window.
        activity.send(Sensor.TYPE_GRAVITY, 11_300, *gravity(10.0, 3.0))
        assertEquals(10.0, got.last().lookAngleDeg, 1e-4)
        assertEquals(3.0, got.last().cantDeg, 1e-4)
        assertEquals(0.0, got.last().unsteadyDeg, 1e-4)
        job.cancel()
    }

    /** Without a tilt sensor the flow ends at once, empty. */
    @Test
    fun noTiltSensorIsAnEmptyFlow() = runTest {
        assertTrue(sensors(emptyList()).second.tilt().toList().isEmpty())
    }

    // --- compass -------------------------------------------------------------

    /** The rotation vector of a phone lying flat, its top [azimuthDeg] clockwise from north. */
    private fun flatTowards(azimuthDeg: Double): FloatArray {
        // A turn about z (out of the screen) by -azimuth: clockwise seen from above.
        val half = -azimuthDeg * PI / 180 / 2
        return floatArrayOf(0f, 0f, sin(half).toFloat(), cos(half).toFloat())
    }

    private fun TestScope.collectHeading(sensors: AndroidSensors, at: GeoFix?, into: MutableList<Heading>) =
        launch(UnconfinedTestDispatcher(testScheduler)) { sensors.heading(at).toList(into) }

    /** Without a position the heading is magnetic, as the rotation vector gives it. */
    @Test
    fun headingIsMagneticWithoutAPosition() = runTest {
        val (activity, sensors) = sensors()
        val got = mutableListOf<Heading>()
        val job = collectHeading(sensors, null, got)
        activity.send(Sensor.TYPE_ROTATION_VECTOR, 10_000, *flatTowards(90.0))
        activity.send(Sensor.TYPE_ROTATION_VECTOR, 10_050, *flatTowards(120.0)) // too soon: dropped
        activity.send(Sensor.TYPE_ROTATION_VECTOR, 10_300, *flatTowards(270.0))
        assertEquals(2, got.size)
        assertEquals(90.0, got[0].azimuthDeg, 1e-3)
        assertEquals(270.0, got[1].azimuthDeg, 1e-3) // west, in 0..360
        assertFalse(got[0].trueNorth)
        assertFalse(got[0].needsCalibration)
        job.cancel()
        assertTrue(activity.listeners.isEmpty())
    }

    /** With a position the local declination turns it to true north. */
    @Test
    fun headingIsTrueWithAPosition() = runTest {
        val (activity, sensors) = sensors()
        val lviv = GeoFix(49.84, 24.03, 296.0, 5.0)
        val declination = GeomagneticField(49.84f, 24.03f, 296f, System.currentTimeMillis()).declination.toDouble()
        assertTrue(declination > 1.0, "$declination") // eastern: the test means something
        val got = mutableListOf<Heading>()
        val job = collectHeading(sensors, lviv, got)
        activity.send(Sensor.TYPE_ROTATION_VECTOR, 10_000, *flatTowards(10.0))
        assertEquals(normalizeDeg(10.0 + declination), got.single().azimuthDeg, 1e-2)
        assertTrue(got.single().trueNorth)
        job.cancel()
    }

    /** A low (or unreliable) compass accuracy asks for calibration; a high one clears it. */
    @Test
    fun poorAccuracyAsksForCalibration() = runTest {
        val (activity, sensors) = sensors()
        val got = mutableListOf<Heading>()
        val job = collectHeading(sensors, null, got)
        val rotation = activity.sensor(Sensor.TYPE_ROTATION_VECTOR)
        val listener = activity.listeners.single()
        listener.onAccuracyChanged(rotation, SensorManager.SENSOR_STATUS_ACCURACY_LOW)
        activity.send(Sensor.TYPE_ROTATION_VECTOR, 10_000, *flatTowards(0.0))
        listener.onAccuracyChanged(rotation, SensorManager.SENSOR_STATUS_UNRELIABLE)
        activity.send(Sensor.TYPE_ROTATION_VECTOR, 10_200, *flatTowards(0.0))
        listener.onAccuracyChanged(rotation, SensorManager.SENSOR_STATUS_ACCURACY_HIGH)
        activity.send(Sensor.TYPE_ROTATION_VECTOR, 10_400, *flatTowards(0.0))
        assertEquals(listOf(true, true, false), got.map { it.needsCalibration })
        job.cancel()
    }

    /** Without a rotation vector sensor the flow ends at once, empty. */
    @Test
    fun noCompassIsAnEmptyFlow() = runTest {
        assertTrue(sensors(emptyList()).second.heading(null).toList().isEmpty())
    }

    // --- location ------------------------------------------------------------

    /** With the permission and GPS on, one GPS fix is returned (sea-level height as the phone gives it). */
    @Test
    fun locationFromGps() = runTest {
        grantLocation()
        val (activity, sensors) = sensors()
        val lm = shadowOf(activity.locations)
        lm.setProviderEnabled(LocationManager.GPS_PROVIDER, true)
        lm.setProviderEnabled(LocationManager.NETWORK_PROVIDER, true)
        val got = async { sensors.location() }
        runCurrent()
        lm.simulateLocation(fix(LocationManager.GPS_PROVIDER, 49.84, 24.03, 4f).apply {
            altitude = 340.0
            mslAltitudeMeters = 296.5
        })
        shadowOf(Looper.getMainLooper()).idle()
        assertEquals(GeoFix(49.84, 24.03, 296.5, 4.0), got.await())
    }

    /** GPS off: the network provider is asked instead; no altitude stays null. */
    @Test
    fun locationFromNetworkWhenGpsIsOff() = runTest {
        grantLocation()
        val (activity, sensors) = sensors()
        val lm = shadowOf(activity.locations)
        lm.setProviderEnabled(LocationManager.GPS_PROVIDER, false)
        lm.setProviderEnabled(LocationManager.NETWORK_PROVIDER, true)
        val got = async { sensors.location() }
        runCurrent()
        lm.simulateLocation(fix(LocationManager.NETWORK_PROVIDER, 50.45, 30.52, 120f))
        shadowOf(Looper.getMainLooper()).idle()
        val f = assertNotNull(got.await())
        assertEquals(50.45, f.latitudeDeg)
        assertNull(f.altitudeM)
        assertEquals(120.0, f.accuracyM)
    }

    /** Both providers off: null without waiting. */
    @Test
    fun noProviderIsNull() = runTest {
        grantLocation()
        val (activity, sensors) = sensors()
        val lm = shadowOf(activity.locations)
        lm.setProviderEnabled(LocationManager.GPS_PROVIDER, false)
        lm.setProviderEnabled(LocationManager.NETWORK_PROVIDER, false)
        assertNull(sensors.location())
    }

    /** No fix within 30 s: null, and the request is cancelled. */
    @Test
    fun noFixInTimeIsNull() = runTest {
        grantLocation()
        val (activity, sensors) = sensors()
        val lm = shadowOf(activity.locations)
        lm.setProviderEnabled(LocationManager.GPS_PROVIDER, true)
        val got = async { sensors.location() }
        runCurrent()
        assertTrue(lm.getLocationRequests(LocationManager.GPS_PROVIDER).isNotEmpty())
        advanceTimeBy(30_001)
        assertNull(got.await())
        shadowOf(Looper.getMainLooper()).idle()
        assertTrue(lm.getLocationRequests(LocationManager.GPS_PROVIDER).isEmpty())
    }

    /** Without the permission it is asked for; refused, the result is null and no provider is used. */
    @Test
    fun refusedPermissionIsNull() = runTest {
        val (activity, sensors) = sensors()
        shadowOf(activity.locations).setProviderEnabled(LocationManager.GPS_PROVIDER, true)
        val got = async { sensors.location() }
        runCurrent()
        val asked = assertNotNull(shadowOf(activity).lastRequestedPermission)
        assertEquals(
            setOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION),
            asked.requestedPermissions.toSet(),
        )
        @Suppress("DEPRECATION")
        activity.onRequestPermissionsResult(
            asked.requestCode, asked.requestedPermissions,
            IntArray(asked.requestedPermissions.size) { PackageManager.PERMISSION_DENIED },
        )
        assertNull(got.await())
        assertTrue(shadowOf(activity.locations).getLocationRequests(LocationManager.GPS_PROVIDER).isEmpty())
    }

    /** Coarse location granted in the dialog is enough: the fix follows. */
    @Test
    fun coarsePermissionIsEnough() = runTest {
        val (activity, sensors) = sensors()
        val lm = shadowOf(activity.locations)
        lm.setProviderEnabled(LocationManager.GPS_PROVIDER, false)
        lm.setProviderEnabled(LocationManager.NETWORK_PROVIDER, true)
        val got = async { sensors.location() }
        runCurrent()
        val asked = shadowOf(activity).lastRequestedPermission
        shadowOf(ApplicationProvider.getApplicationContext<android.app.Application>())
            .grantPermissions(Manifest.permission.ACCESS_COARSE_LOCATION)
        @Suppress("DEPRECATION")
        activity.onRequestPermissionsResult(
            asked.requestCode, asked.requestedPermissions,
            asked.requestedPermissions.map {
                if (it == Manifest.permission.ACCESS_COARSE_LOCATION) PackageManager.PERMISSION_GRANTED
                else PackageManager.PERMISSION_DENIED
            }.toIntArray(),
        )
        runCurrent()
        lm.simulateLocation(fix(LocationManager.NETWORK_PROVIDER, 48.0, 23.0))
        shadowOf(Looper.getMainLooper()).idle()
        assertEquals(48.0, got.await()!!.latitudeDeg)
    }

    /** Android 10 (no getCurrentLocation): a single update; the plain altitude is used there. */
    @Test
    @Config(sdk = [29])
    fun locationOnOlderAndroid() = runTest {
        grantLocation()
        val (activity, sensors) = sensors()
        val lm = shadowOf(activity.locations)
        lm.setProviderEnabled(LocationManager.GPS_PROVIDER, true)
        val got = async { sensors.location() }
        runCurrent()
        lm.simulateLocation(fix(LocationManager.GPS_PROVIDER, 49.84, 24.03).apply { altitude = 340.0 })
        shadowOf(Looper.getMainLooper()).idle()
        assertEquals(GeoFix(49.84, 24.03, 340.0, 5.0), got.await())
    }
}
