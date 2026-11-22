package com.nexora.optimizer.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.os.Build

/**
 * إدارة قناة وإشعارات NEXORA.
 */
object Notifs {
    const val CHANNEL_ID = "nexora_session"
    const val SESSION_NOTIFICATION_ID = 4201
    const val RESULT_NOTIFICATION_ID = 4202

    fun ensureChannel(context: Context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val mgr = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            val existing = mgr.getNotificationChannel(CHANNEL_ID)
            if (existing == null) {
                val channel = NotificationChannel(
                    CHANNEL_ID,
                    "NEXORA — جلسة اللعب",
                    NotificationManager.IMPORTANCE_LOW
                ).apply {
                    description = "إشعار جلسة وضع الأداء واستعادة الدقة تلقائياً"
                    setShowBadge(false)
                }
                mgr.createNotificationChannel(channel)
            }
        }
    }

    fun notify(context: Context, id: Int, notification: Notification) {
        ensureChannel(context)
        val mgr = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        mgr.notify(id, notification)
    }
}
