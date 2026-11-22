package com.nexora.optimizer.core

import android.content.Context
import android.content.SharedPreferences

/**
 * المُخفّض الذكي للدقة (Smart Downscaler).
 *
 * يطبّق دقة عرض مؤقتة عبر: wm size WxH
 * ويحفظ الدقة الفيزيائية الأصلية ليستعيدها تلقائياً عبر: wm size reset
 *
 * هذا هو مبدأ "الـ DLSS الحقيقي": الرندر بدقة أقل ← حمل أقل على الـ GPU (Mali وغيره) ← فريمات أعلى وثبات أفضل.
 */
object ResolutionManager {

    private const val PREFS = "nexora_prefs"
    private const val KEY_ORIG_W = "orig_w"
    private const val KEY_ORIG_H = "orig_h"
    private const val KEY_ORIG_DENSITY = "orig_density"
    private const val KEY_OVERRIDE_ACTIVE = "override_active"
    private const val KEY_GAME_PKG = "watched_game"

    data class ScreenInfo(
        val physicalW: Int,
        val physicalH: Int,
        val overrideW: Int?,
        val overrideH: Int?,
        val physicalDensity: Int?,
        val overrideDensity: Int?
    ) {
        val effectiveW: Int get() = overrideW ?: physicalW
        val effectiveH: Int get() = overrideH ?: physicalH
        val effectiveDensity: Int? get() = overrideDensity ?: physicalDensity
    }

    private fun prefs(context: Context): SharedPreferences =
        context.applicationContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    /** قراءة الحالة الحقيقية من `wm size` و `wm density` (تحتاج صلاحية shell/root/Shizuku) */
    fun queryScreenInfo(): ScreenInfo? {
        val sizeRes = ShellExecutor.run("wm size")
        if (!sizeRes.success) return null

        var physicalW = 0
        var physicalH = 0
        var overrideW: Int? = null
        var overrideH: Int? = null

        Regex("Physical size:\\s*(\\d+)x(\\d+)").find(sizeRes.output)?.let {
            physicalW = it.groupValues[1].toInt()
            physicalH = it.groupValues[2].toInt()
        }
        Regex("Override size:\\s*(\\d+)x(\\d+)").find(sizeRes.output)?.let {
            overrideW = it.groupValues[1].toInt()
            overrideH = it.groupValues[2].toInt()
        }
        if (physicalW == 0 || physicalH == 0) return null

        var physicalDensity: Int? = null
        var overrideDensity: Int? = null
        val densityRes = ShellExecutor.run("wm density")
        if (densityRes.success) {
            Regex("Physical density:\\s*(\\d+)").find(densityRes.output)?.let {
                physicalDensity = it.groupValues[1].toInt()
            }
            Regex("Override density:\\s*(\\d+)").find(densityRes.output)?.let {
                overrideDensity = it.groupValues[1].toInt()
            }
        }
        return ScreenInfo(physicalW, physicalH, overrideW, overrideH, physicalDensity, overrideDensity)
    }

    fun isOverrideActive(context: Context): Boolean = prefs(context).getBoolean(KEY_OVERRIDE_ACTIVE, false)

    fun watchedGame(context: Context): String? = prefs(context).getString(KEY_GAME_PKG, null)

    /**
     * يطبّق الدقة المستهدفة. يحفظ الدقة الأصلية أول مرة فقط حتى لا تُفقد عند إعادة التفعيل.
     * @return true لو نجح التنفيذ عبر أي محرك shell
     */
    fun applyDownscale(context: Context, targetW: Int, targetH: Int, gamePkg: String?): Boolean {
        val p = prefs(context)

        // احفظ الأصل مرة واحدة فقط (إذا ما فيه override محفوظ من قبل)
        if (!p.getBoolean(KEY_OVERRIDE_ACTIVE, false)) {
            val info = queryScreenInfo()
            if (info != null) {
                p.edit()
                    .putInt(KEY_ORIG_W, info.physicalW)
                    .putInt(KEY_ORIG_H, info.physicalH)
                    .putInt(KEY_ORIG_DENSITY, info.physicalDensity ?: -1)
                    .apply()
                LogCenter.log("حفظ الدقة الأصلية: ${info.physicalW}x${info.physicalH} @${info.physicalDensity ?: "?"}dpi")
            } else {
                LogCenter.log("تعذرت قراءة الدقة الأصلية قبل التطبيق — سيتم الاعتماد على wm size reset عند الاستعادة")
            }
        }

        val evenW = targetW - (targetW % 2)
        val evenH = targetH - (targetH % 2)
        val result = ShellExecutor.run("wm size ${evenW}x${evenH}")
        LogCenter.logResult("wm size ${evenW}x${evenH}", result)
        if (result.success) {
            p.edit()
                .putBoolean(KEY_OVERRIDE_ACTIVE, true)
                .putString(KEY_GAME_PKG, gamePkg)
                .apply()
        }
        return result.success
    }

    /** يستعيد الدقة والكثافة الأصلية — يعمل حتى لو فُقدت الـ prefs (يعتمد على reset) */
    fun restoreOriginal(context: Context): Boolean {
        val sizeRes = ShellExecutor.run("wm size reset")
        LogCenter.logResult("wm size reset", sizeRes)
        val densityRes = ShellExecutor.run("wm density reset")
        LogCenter.logResult("wm density reset", densityRes)
        prefs(context).edit().remove(KEY_OVERRIDE_ACTIVE).remove(KEY_GAME_PKG).apply()
        return sizeRes.success
    }

    /**
     * يحسب أبعاد الهدف مع الحفاظ على نسبة الأبعاد، ويجبر الأرقام على قيم زوجية (شرط wm).
     */
    fun computeTarget(physicalW: Int, physicalH: Int, targetHeight: Int): Pair<Int, Int> {
        val portrait = physicalH >= physicalW
        val aspect = if (portrait) physicalW.toDouble() / physicalH else physicalH.toDouble() / physicalW
        val evenH = targetHeight - (targetHeight % 2)
        var evenOther = (aspect * evenH).toInt()
        evenOther -= evenOther % 2
        return if (portrait) evenOther to evenH else evenH to evenOther
    }

    /** رفع كثافة العرض قليلاً لتعويض نعومة الصورة بعد خفض الدقة (جزء من Sharpness Boost) */
    fun bumpDensity(delta: Int): ShellResult {
        val info = queryScreenInfo()
        val base = info?.physicalDensity
        val target = if (base != null) base + delta else 480 + delta
        val res = ShellExecutor.run("wm density $target")
        LogCenter.logResult("wm density $target", res)
        return res
    }
}
