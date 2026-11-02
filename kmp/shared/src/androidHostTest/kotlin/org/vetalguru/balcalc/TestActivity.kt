package org.vetalguru.balcalc

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorManager
import androidx.activity.ComponentActivity
import org.robolectric.Robolectric
import org.robolectric.Shadows.shadowOf
import org.robolectric.android.controller.ActivityController
import org.robolectric.shadows.ShadowSensor

/**
 * A created activity for code that registers result launchers in onCreate:
 * [setup] runs then (with the activity's sensors installed), the activity
 * is resumed afterwards, as MainActivity does it.
 */
fun <T> startedActivity(
    sensorTypes: List<Int> = emptyList(),
    setup: (ComponentActivity) -> T,
): Pair<ActivityController<ComponentActivity>, T> {
    val controller = Robolectric.buildActivity(ComponentActivity::class.java).create()
    val activity = controller.get()
    val manager = activity.getSystemService(Context.SENSOR_SERVICE) as SensorManager
    for (type in sensorTypes) shadowOf(manager).addSensor(ShadowSensor.newInstance(type))
    val made = setup(activity)
    controller.start().resume()
    return controller to made
}

val ComponentActivity.sensorManager get() = getSystemService(Context.SENSOR_SERVICE) as SensorManager

fun ComponentActivity.sensor(type: Int): Sensor = sensorManager.getDefaultSensor(type)!!
