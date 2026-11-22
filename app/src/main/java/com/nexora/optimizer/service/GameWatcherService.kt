package com.nexora.optimizer.service

import android.app.Notification
import android.app.PendingIntent
import android.app.Service
import android.app.usage.UsageStatsManager
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.Process
import android.app.AppOpsManager
import com.nexora.optimizer.core.LogCenter
import com.nexora.optimizer.core.ResolutionManager
import com.nexora.optimizer.MainActivity
import com.nexora.optimizer.R

/**
 * خدمة مراقبة جلسة اللعب (Foreground Service).
 *
 * بعد تفعيل وضع الأداء وإطلاق اللعبة:
 *   - تعرض إشعاراً دائماً فيه زر «استعادة الدقة الأصلية»
 *   - تفحص كل 2.5 ثانية الحزمة الموجودة في المقدمة (UsageStats)
 *   - أول ما تلاحظ أنك خرجت من اللعبة → تعيد الدقة الأصلية تلقائياً وتوقف نفسها
 *
 * تحتاج صلاحية "الوصول إلى بيانات الاستخدام" (Usage Access) — بدونها تبقى الخدمة
 * تعمل بالزر اليدوي فقط في الإشعار.
 */
class GameWatcherService : Service() {

    companion object {
        const val ACTION_RESTORE = "com.nexora.optimizer.action.RESTORE"
        const val EXTRA_GAME_PKG = "game_pkg"
        const val EXTRA_GAME_NAME = "game_name"
        private const val POLL_MS = 2500L

        fun hasUsageAccess(context: Context): Boolean {
            return try {
                val appOps = context.getSystemService(Context.APP_OPS_SERVICE) as AppOpsManager
                val mode = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                    appOps.unsafeCheckOpNoThrow(
                        AppOpsManager.OPSTR_GET_USAGE_STATS,
                        Process.myUid(),
                        context.packageName
                    )
                } else {
                    @Suppress("DEPRECATION")
                    appOps.checkOpNoThrow(
                        AppOpsManager.OPSTR_GET_USAGE_STATS,
                        Process.myUid(),
                        context.packageName
                    )
                }
                mode == AppOpsManager.MODE_ALLOWED
            } catch (_: Throwable) {
                false
            }
        }
    }

    private val handler = Handler(Looper.getMainLooper())
    private var targetPkg: String = ""
    private var gameName: String = "اللعبة"
    private var seenGameInForeground = false

    private val poll = object : Runnable {
        override fun run() {
            checkForeground()
            if (ResolutionManager.isOverrideActive(applicationContext)) handler.postDelayed(this, POLL_MS)
        }
    }

    override fun onCreate() {
        super.onCreate()
        Notifs.ensureChannel(this)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_RESTORE -> {
                LogCenter.log("زر الاستعادة من الإشعار — إعادة الدقة الأصلية")
                restoreAndStop("تم استعادة الدقة الأصلية ✋")
                return START_NOT_STICKY
            }
        }

        targetPkg = intent?.getStringExtra(EXTRA_GAME_PKG).orEmpty()
        gameName = intent?.getStringExtra(EXTRA_GAME_NAME) ?: targetPkg.ifEmpty { "اللعبة" }
        LogCenter.log("▶ بدأت مراقبة جلسة اللعب: $gameName ($targetPkg)")

        startInForeground()
        handler.removeCallbacks(poll)
        handler.postDelayed(poll, POLL_MS)
        return START_NOT_STICKY
    }

    private fun startInForeground() {
        val restoreIntent = Intent(this, GameWatcherService::class.java).apply {
            action = ACTION_RESTORE
        }
        val restorePending = PendingIntent.getService(
            this,
            99,
            restoreIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val openIntent = Intent(this, MainActivity::class.java)
        val openPending = PendingIntent.getActivity(
            this,
            100,
            openIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val notification: Notification = Notification.Builder(this, Notifs.CHANNEL_ID)
            .setContentTitle("NEXORA — وضع الأداء نشط ⚡")
            .setContentText("الدقة مخفّضة أثناء لعب $gameName — ستستعيد الأصلية تلقائياً عند خروجك")
            .setSmallIcon(R.drawable.ic_bolt)
            .setOngoing(true)
            .setContentIntent(openPending)
            .addAction(
                Notification.Action.Builder(null, "استعادة الدقة الأصلية", restorePending).build()
            )
            .build()

        if (Build.VERSION.SDK_INT >= 34) {
            startForeground(
                Notifs.SESSION_NOTIFICATION_ID,
                notification,
                android.content.pm.ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE
            )
        } else {
            @Suppress("DEPRECATION")
            startForeground(Notifs.SESSION_NOTIFICATION_ID, notification)
        }
    }

    private fun checkForeground() {
        if (targetPkg.isEmpty()) return
        if (!hasUsageAccess(this)) {
            // بدون صلاحية الاستخدام لا يمكن الكشف التلقائي — تبقى الاستعادة اليدوية من الإشعار
            return
        }
        val fg = currentForegroundPackage()
        when {
            fg == targetPkg -> {
                if (!seenGameInForeground) {
                    seenGameInForeground = true
                    LogCenter.log("✓ اللعبة في المقدمة الآن — المراقبة النشطة جارية")
                }
            }
            seenGameInForeground && fg != null -> {
                LogCenter.log("◼ خرجت من $gameName ← الواجهة الحالية: $fg — استعادة الدقة")
                restoreAndStop("عدت من اللعبة — رجّعنا الدقة الأصلية ✅")
            }
        }
    }

    private fun currentForegroundPackage(): String? {
        return try {
            val usm = getSystemService(Context.USAGE_STATS_SERVICE) as UsageStatsManager
            val end = System.currentTimeMillis()
            val begin = end - 12_000
            val stats = usm.queryUsageStats(UsageStatsManager.INTERVAL_BEST, begin, end)
            stats?.filter { it.lastTimeUsed > 0 }
                ?.maxByOrNull { it.lastTimeUsed }
                ?.packageName
        } catch (t: Throwable) {
            LogCenter.log("تعذرت قراءة حزمة المقدمة: ${t.message}")
            null
        }
    }

    private fun restoreAndStop(message: String) {
        val ok = ResolutionManager.restoreOriginal(applicationContext)
        val resultNotification: Notification = Notification.Builder(this, Notifs.CHANNEL_ID)
            .setContentTitle("NEXORA")
            .setContentText(if (ok) message else "تعذر تأكيد استعادة الدقة — راجع السجل داخل التطبيق")
            .setSmallIcon(R.drawable.ic_bolt)
            .setAutoCancel(true)
            .build()
        Notifs.notify(this, Notifs.RESULT_NOTIFICATION_ID, resultNotification)
        handler.removeCallbacks(poll)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            stopForeground(Service.STOP_FOREGROUND_REMOVE)
        } else {
            @Suppress("DEPRECATION")
            stopForeground(true)
        }
        stopSelf()
    }

    override fun onDestroy() {
        handler.removeCallbacks(poll)
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null
}
