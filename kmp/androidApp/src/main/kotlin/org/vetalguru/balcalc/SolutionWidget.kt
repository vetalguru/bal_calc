package org.vetalguru.balcalc

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.Context
import android.content.Intent
import android.widget.RemoteViews

/**
 * The home-screen widget: the last solution the app computed (range,
 * elevation, windage), as AndroidPlatform.publishSolution left it; a tap
 * opens the app.
 */
class SolutionWidget : AppWidgetProvider() {
    override fun onUpdate(context: Context, manager: AppWidgetManager, ids: IntArray) {
        val saved = context.getSharedPreferences(AndroidPlatform.WIDGET_PREFS, Context.MODE_PRIVATE)
        val open = PendingIntent.getActivity(
            context, 0, Intent(context, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
        for (id in ids) {
            val views = RemoteViews(context.packageName, R.layout.widget_solution)
            val elevation = saved.getString("elevation", null)
            views.setTextViewText(R.id.widget_title, saved.getString("title", "BalCalc"))
            views.setTextViewText(R.id.widget_range, saved.getString("range", ""))
            views.setTextViewText(R.id.widget_elevation, elevation ?: context.getString(R.string.widget_empty))
            views.setTextViewText(R.id.widget_windage, saved.getString("windage", ""))
            views.setOnClickPendingIntent(R.id.widget_root, open)
            manager.updateAppWidget(id, views)
        }
    }
}
