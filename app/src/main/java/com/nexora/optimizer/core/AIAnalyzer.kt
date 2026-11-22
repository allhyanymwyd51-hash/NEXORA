package com.nexora.optimizer.core

import android.app.ActivityManager
import android.content.Context
import android.os.Build
import android.os.PowerManager
import android.util.DisplayMetrics
import android.view.WindowManager

/**
 * محرك NEXORA الذكي (On-device AI Heuristics).
 *
 * يحلل: دقة الشاشة الفيزيائية، كثافة البكسلات، شريحة المعالج (SoC/Mali/Adreno)،
 * الذاكرة العشوائية، معدل تحديث الشاشة، والحالة الحرارية الحالية —
 * ثم يقترح: درجة الأداء، عامل الخفض الأمثل، والدقة المستهدفة لطور "DLSS Performance".
 *
 * كل شيء يعمل محلياً على الجهاز — بدون إنترنت وبدون تسريب أي بيانات.
 */
object AIAnalyzer {

    data class AiReport(
        val score: Int,               // 0..100
        val verdict: String,          // وصف مختصر بالعربية
        val gpuTier: Int,             // 1 = أساسي، 2 = متوسط، 3 = رايات (flagship)
        val gpuName: String,
        val ramGb: Double,
        val nativeWidth: Int,
        val nativeHeight: Int,
        val refreshRate: Float,
        val thermalStatus: String,
        val recommendedHeight: Int,   // الارتفاع المستهدف بوضع الأداء
        val scalePercent: Int,        // نسبة الاحتفاظ بالبكسلات
        val expectedFpsGain: Int,     // الزيادة المتوقعة تقريباً %
        val notes: List<String>
    )

    fun analyze(context: Context): AiReport {
        val metrics = DisplayMetrics()
        @Suppress("DEPRECATION")
        (context.getSystemService(Context.WINDOW_SERVICE) as WindowManager)
            .defaultDisplay.getRealMetrics(metrics)

        val nativeW = metrics.widthPixels
        val nativeH = metrics.heightPixels
        val refresh = runCatching {
            @Suppress("DEPRECATION")
            (context.getSystemService(Context.WINDOW_SERVICE) as WindowManager)
                .defaultDisplay.refreshRate
        }.getOrElse { 60f }

        // الذاكرة العشوائية
        val am = context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        val memInfo = ActivityManager.MemoryInfo()
        am.getMemoryInfo(memInfo)
        val ramGb = memInfo.totalMem / (1024.0 * 1024.0 * 1024.0)

        // الحالة الحرارية (Android 10+)
        val thermalStatus = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val pm = context.getSystemService(Context.POWER_SERVICE) as PowerManager
            when (pm.currentThermalStatus) {
                PowerManager.THERMAL_STATUS_NONE -> "باردة ✓"
                PowerManager.THERMAL_STATUS_LIGHT -> "دافئة قليلاً"
                PowerManager.THERMAL_STATUS_MODERATE -> "دافئة"
                PowerManager.THERMAL_STATUS_SEVERE -> "ساخنة ⚠"
                PowerManager.THERMAL_STATUS_CRITICAL -> "حرجة ⚠⚠"
                else -> "غير معروفة"
            }
        } else {
            "غير متاحة (Android < 10)"
        }

        // شريحة المعالج
        val socRaw = listOfNotNull(
            Build.HARDWARE, Build.BOARD,
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) Build.SOC_MODEL else null,
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) Build.SOC_MANUFACTURER else null
        ).joinToString(" ").lowercase()

        val gpuName: String
        val tier: Int
        when {
            // رايات: Snapdragon 7/8 Gen، Adreno 650+، Mali-G71/G77/G78/G710/G715/G720/G925، Dimensity 9000+
            Regex("(adreno.?[78]|mali-g7[18]|mali-g71|mali-g77|mali-g78|mali-g710|mali-g715|mali-g720|mali-g925|dimensity.?9|snapdragon.?8|sd8|sm8|tensor.?g[345]|exynos.?2[12])").containsMatchIn(socRaw) -> {
                tier = 3
                gpuName = extractGpuName(socRaw) ?: "Flagship GPU (Adreno/Mali-G7x)"
            }
            // متوسط: Adreno 61x-64x، Mali-G52/G57/G68، Helio G، Dimensity 7/8، Exynos 9/2100
            Regex("(adreno.?6|adreno.?5|mali-g5|mali-g6|helio.?g|dimensity.?[78]|exynos.?2100|tensor.?g2|mt6)").containsMatchIn(socRaw) -> {
                tier = 2
                gpuName = extractGpuName(socRaw) ?: "Mid-range GPU (Mali/Adreno 6xx)"
            }
            // أساسي: أي Mali/Unisoc قديم
            else -> {
                tier = 1
                gpuName = extractGpuName(socRaw) ?: ("GPU أساسي (" + socRaw.take(40).uppercase() + ")")
            }
        }

        val mpx = (nativeW.toLong() * nativeH) / 1_000_000.0

        // ------- خوارزمية التقييم -------
        var score = 45
        score += (tier - 1) * 14                              // +0 / +14 / +28
        score += (ramGb.coerceAtMost(12.0) * 2.2).toInt()     // حتى +26
        score -= ((mpx - 2.0).coerceAtLeast(0.0) * 7).toInt() // عقوبة الشاشات العالية الدقة
        if (refresh >= 120f) score -= 4                       // 120Hz تحتاج بكسلات أكثر
        score = score.coerceIn(18, 97)

        // ------- التوصية -------
        val (recommendedHeight, scalePercent) = when {
            tier == 3 && mpx >= 3.5 -> 900 to 58   // 1.5K → 900p
            tier == 3 -> 720 to 66
            tier == 2 && mpx >= 3.5 -> 720 to 52   // 1.5K → 720p
            tier == 2 -> 720 to 62
            tier == 1 && mpx >= 2.5 -> 540 to 40
            tier == 1 -> 720 to 55
            else -> 720 to 60
        }

        val pixelsCut = (100 - scalePercent).coerceAtLeast(10)
        val expectedFpsGain = (pixelsCut * 1.6).toInt().coerceIn(25, 75)

        val verdict = when {
            score >= 80 -> "أداء ممتاز — ممتاز للألعاب الثقيلة بدقة كاملة مع DLSS Performance"
            score >= 60 -> "أداء جيد — وضع الأداء سيعطيك قفزة واضحة في الفريمات"
            score >= 40 -> "أداء متوسط — خفض الدقة إلزامي تقريباً لثبات الفريمات"
            else -> "أداء أساسي — استخدم 540p/720p مع Cool Mode وسدّة حرارة"
        }

        val notes = buildList {
            add("دقة الشاشة الأصلية: ${nativeW}×${nativeH} (${"%.1f".format(mpx)} ميجابكسل)")
            add("المعالج: $gpuName")
            add("الرام: ${"%.1f".format(ramGb)} جيجابايت  •  التحديث: ${refresh.toInt()}Hz")
            add("الحرارة الآن: $thermalStatus")
            add("التوصية: تفعيل خفض الدقة إلى ~${recommendedHeight}p (نسبة البكسلات ~$scalePercent%)")
            add("المتوقع: زيادة ~+$expectedFpsGain% في متوسط الفريمات مع حرارة أقل")
            add("لأفضل نتيجة اجمع بين: وضع الأداء + Sharpness Boost + Cool Mode")
        }

        return AiReport(
            score = score,
            verdict = verdict,
            gpuTier = tier,
            gpuName = gpuName,
            ramGb = ramGb,
            nativeWidth = nativeW,
            nativeHeight = nativeH,
            refreshRate = refresh,
            thermalStatus = thermalStatus,
            recommendedHeight = recommendedHeight,
            scalePercent = scalePercent,
            expectedFpsGain = expectedFpsGain,
            notes = notes
        )
    }

    private fun extractGpuName(soc: String): String? {
        Regex("(adreno[- _]?[a-z0-9]+)").find(soc)?.let { return it.value.replaceFirstChar { c -> c.uppercase() } }
        Regex("(mali[- _]?g[0-9]+[a-z]*)").find(soc)?.let { return it.value.uppercase() }
        Regex("(dimensity[- _]?[0-9]+)").find(soc)?.let { return "MediaTek " + it.value }
        Regex("(exynos[- _]?[a-z0-9]+)").find(soc)?.let { return it.value.replaceFirstChar { c -> c.uppercase() } }
        Regex("(tensor[- _]?g[0-9]+)").find(soc)?.let { return "Google " + it.value }
        Regex("(helio[- _]?g[0-9]+)").find(soc)?.let { return "MediaTek " + it.value }
        return null
    }
}
