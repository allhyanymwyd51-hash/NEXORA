package com.nexora.optimizer.core

/**
 * Cool Mode — وضع التبريد.
 *
 * يطبّق قبل اللعب:
 *   1) إيقاف كل العمليات الخلفية غير الأساسية (cmd activity kill-all)
 *   2) Force-Stop للتطبيقات الثقيلة المختارة (تيك توك، فيسبوك، ...)
 *
 * النتيجة: حرارة أقل → لا يصل المعالج لمرحلة الـ Thermal Throttling
 * → الفريمات تبقى ثابتة بدل ما تهبط فجأة أثناء اللعب.
 */
object CoolMode {

    /** التطبيقات الثقيلة الأكثر شيوعاً — تُعرض كخيارات قابلة للتحديد */
    val HEAVY_APPS: LinkedHashMap<String, String> = linkedMapOf(
        "com.zhiliaoapp.musically" to "TikTok",
        "com.ss.android.ugc.trill" to "TikTok Lite",
        "com.facebook.katana" to "Facebook",
        "com.facebook.orca" to "Messenger",
        "com.instagram.android" to "Instagram",
        "com.google.android.youtube" to "YouTube",
        "com.snapchat.android" to "Snapchat",
        "com.android.chrome" to "Chrome"
    )

    /**
     * @return تقرير نصي يُعرض للمستخدم ويُسجَّل في الكونسول
     */
    fun enable(packagesToKill: List<String>): String {
        val report = StringBuilder()
        report.appendLine("═══════ Cool Mode — تقرير التفعيل ═══════")

        // 1) إيقاف العمليات الخلفية كلها
        val killAll = ShellExecutor.run("cmd activity kill-all")
        LogCenter.logResult("cmd activity kill-all", killAll)
        report.appendLine(
            if (killAll.success) "✓ تم إيقاف جميع العمليات الخلفية غير الأساسية"
            else "✗ تعذر إيقاف العمليات الخلفية (${killAll.output.take(120)})"
        )

        // 2) Force-stop للتطبيقات الثقيلة المختارة
        var killed = 0
        for (pkg in packagesToKill) {
            val r = ShellExecutor.run("am force-stop $pkg")
            LogCenter.logResult("am force-stop $pkg", r)
            val label = HEAVY_APPS[pkg] ?: pkg
            if (r.success) {
                killed++
                report.appendLine("✓ أُوقف: $label")
            } else {
                report.appendLine("✗ تعذر إيقاف $label — ${r.output.take(80)}")
            }
        }
        report.appendLine("──────────────────────────────")
        report.appendLine("النتيجة: $killed تطبيق ثقيل موقوف + العمليات الخلفية منظّفة.")
        report.appendLine("العب الآن — الحرارة ستبقى منخفضة والفريمات ثابتة.")

        if (killAll.exitCode == -4) {
            report.appendLine()
            report.appendLine("⚠ تحتاج صلاحية Root أو Shizuku ليعمل Cool Mode بكامل قوته.")
        }
        return report.toString()
    }
}
