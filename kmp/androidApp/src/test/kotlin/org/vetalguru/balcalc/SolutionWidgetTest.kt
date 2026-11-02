package org.vetalguru.balcalc

import android.app.Application
import android.appwidget.AppWidgetManager
import android.content.Context
import android.os.Looper
import android.widget.TextView
import androidx.activity.ComponentActivity
import androidx.test.core.app.ApplicationProvider
import kotlin.test.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf

@RunWith(RobolectricTestRunner::class)
class SolutionWidgetTest {
    private val app: Application = ApplicationProvider.getApplicationContext()
    private val widgets = shadowOf(AppWidgetManager.getInstance(app))

    /** The four lines of widget [id] as shown. */
    private fun lines(id: Int): List<String> {
        val view = widgets.getViewFor(id)
        return listOf(R.id.widget_title, R.id.widget_range, R.id.widget_elevation, R.id.widget_windage)
            .map { view.findViewById<TextView>(it).text.toString() }
    }

    private fun save(vararg lines: Pair<String, String>) =
        app.getSharedPreferences(AndroidPlatform.WIDGET_PREFS, Context.MODE_PRIVATE).edit().apply {
            for ((k, v) in lines) putString(k, v)
        }.commit()

    /** Nothing published yet: the app's name and a hint to open it. */
    @Test
    fun emptyWidgetInvitesToOpenTheApp() {
        val id = widgets.createWidget(SolutionWidget::class.java, R.layout.widget_solution)
        assertEquals(listOf("Holdmark", "", app.getString(R.string.widget_empty), ""), lines(id))
    }

    /** A published solution fills every placed widget. */
    @Test
    fun widgetShowsTheSavedSolution() {
        save("title" to "Tikka .308", "range" to "600 m", "elevation" to "↑ 3.4 mil", "windage" to "→ 0.6 mil")
        val ids = widgets.createWidgets(SolutionWidget::class.java, R.layout.widget_solution, 2)
        for (id in ids) assertEquals(listOf("Tikka .308", "600 m", "↑ 3.4 mil", "→ 0.6 mil"), lines(id))
    }

    /** A tap opens the app. */
    @Test
    fun tapOpensTheApp() {
        val id = widgets.createWidget(SolutionWidget::class.java, R.layout.widget_solution)
        widgets.getViewFor(id).findViewById<android.view.View>(R.id.widget_root).performClick()
        shadowOf(Looper.getMainLooper()).idle()
        assertEquals(MainActivity::class.java.name, shadowOf(app).nextStartedActivity?.component?.className)
    }

    /** The app's publish reaches a placed widget through the update broadcast. */
    @Test
    fun publishUpdatesThePlacedWidget() {
        val id = widgets.createWidget(SolutionWidget::class.java, R.layout.widget_solution)
        val controller = Robolectric.buildActivity(ComponentActivity::class.java).create()
        val platform = AndroidPlatform(controller.get())
        controller.start().resume()
        platform.publish(SolutionLines("Rem 700", "800 m", "↑ 5.1 mil", "← 1.2 mil"))
        shadowOf(Looper.getMainLooper()).idle()
        assertEquals(listOf("Rem 700", "800 m", "↑ 5.1 mil", "← 1.2 mil"), lines(id))
        platform.publish(null) // cleared: back to the hint
        shadowOf(Looper.getMainLooper()).idle()
        assertEquals(listOf("Holdmark", "", app.getString(R.string.widget_empty), ""), lines(id))
    }
}
