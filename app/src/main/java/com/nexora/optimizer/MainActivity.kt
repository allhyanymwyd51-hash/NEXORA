package com.nexora.optimizer

import android.app.Activity
import android.app.AlertDialog
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.drawable.GradientDrawable
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.DisplayMetrics
import android.view.LayoutInflater
import android.view.View
import android.view.animation.ValueAnimator
import android.widget.Button
import android.widget.CheckBox
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import com.nexora.optimizer.core.AIAnalyzer
import com.nexora.optimizer.core.ConfigEngine
import com.nexora.optimizer.core.CoolMode
import com.nexora.optimizer.core.LogCenter
import com.nexora.optimizer.core.ResolutionManager
import com.nexora.optimizer.core.ShellExecutor
import com.nexora.optimizer.data.GameProfile
import com.nexora.optimizer.data.Profiles
import com.nexora.optimizer.service.GameWatcherService
import com.nexora.optimizer.service.Notifs
import java.util.concurrent.Executors

/**
 * NEXORA — AI Resolution & FPS Tuner
 * الشاشة الرئيسية: حالة النظام + المُخفّض الذكي + تحليل AI + مكتبة الألعاب + Cool Mode + السجل.
 */
class MainActivity : Activity() {

    private enum class Choice { H540, H720, H900, AI }

    private val executor = Executors.newSingleThreadExecutor()
    private val mainHandler = Handler(Looper.getMainLooper())

    private lateinit var tvStatusBackend: TextView
    private lateinit var tvInfoModel: TextView
    private lateinit var tvInfoSoc: TextView
    private lateinit var tvInfoRam: TextView
    private lateinit var tvInfoDisplay: TextView
    private lateinit var btnToggleDownscale: Button
    private lateinit var tvDownscaleStatus: TextView
    private lateinit var tvAiScore: TextView
    private lateinit var tvAiVerdict: TextView
    private lateinit var tvAiNotes: TextView
    private lateinit var pbAi: ProgressBar
    private lateinit var btnAnalyzeAi: Button
    private lateinit var btnApplyAi: Button
    private lateinit var gamesContainer: LinearLayout
    private lateinit var tvOutputTitle: TextView
    private lateinit var tvConfigOutput: TextView
    private lateinit var tvInstallNote: TextView
    private lateinit var gridCoolApps: LinearLayout
    private lateinit var tvCoolResult: TextView
    private lateinit var tvLog: TextView
    private lateinit var logScroll: ScrollView

    private var choice = Choice.H720
    private var aiReport: AIAnalyzer.AiReport? = null
    private var lastConfig: ConfigEngine.ConfigOutput? = null
    private val coolCheckboxes = LinkedHashMap<String, CheckBox>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        bindViews()
        setupChoiceButtons()
        setupCoolModeGrid()
        setupGamesLibrary()
        setupConsole()
        requestNotificationPermission()
        Notifs.ensureChannel(this)

        LogCenter.log("NEXORA v1.0.0 — تم الإقلاع")
        LogCenter.log("وضع التنفيذ التلقائي: Root ← ثم Shizuku ← ثم دليل ADB")
        LogCenter.log("نصيحة: ثبّت Shizuku وفعّله عبر ADB للحصول على صلاحية shell كاملة بدون روت")

        refreshDeviceInfo()
        refreshStatus()
        refreshDownscaleUI()
    }

    override fun onResume() {
        super.onResume()
        refreshStatus()
        refreshDownscaleUI()
    }

    override fun onDestroy() {
        LogCenter.setListener(null)
        executor.shutdown()
        super.onDestroy()
    }

    // ══════════════ ربط العناصر ══════════════

    private fun bindViews() {
        tvStatusBackend = findViewById(R.id.tvStatusBackend)
        tvInfoModel = findViewById(R.id.tvInfoModel)
        tvInfoSoc = findViewById(R.id.tvInfoSoc)
        tvInfoRam = findViewById(R.id.tvInfoRam)
        tvInfoDisplay = findViewById(R.id.tvInfoDisplay)
        btnToggleDownscale = findViewById(R.id.btnToggleDownscale)
        tvDownscaleStatus = findViewById(R.id.tvDownscaleStatus)
        tvAiScore = findViewById(R.id.tvAiScore)
        tvAiVerdict = findViewById(R.id.tvAiVerdict)
        tvAiNotes = findViewById(R.id.tvAiNotes)
        pbAi = findViewById(R.id.pbAi)
        btnAnalyzeAi = findViewById(R.id.btnAnalyzeAi)
        btnApplyAi = findViewById(R.id.btnApplyAi)
        gamesContainer = findViewById(R.id.gamesContainer)
        tvOutputTitle = findViewById(R.id.tvOutputTitle)
        tvConfigOutput = findViewById(R.id.tvConfigOutput)
        tvInstallNote = findViewById(R.id.tvInstallNote)
        gridCoolApps = findViewById(R.id.gridCoolApps)
        tvCoolResult = findViewById(R.id.tvCoolResult)
        tvLog = findViewById(R.id.tvLog)
        logScroll = findViewById(R.id.logScroll)

        findViewById<Button>(R.id.btnRefreshStatus).setOnClickListener { refreshStatus() }
        findViewById<Button>(R.id.btnGrantShizuku).setOnClickListener { onGrantShizuku() }
        findViewById<Button>(R.id.btnUsageAccess).setOnClickListener { onOpenUsageAccess() }

        btnToggleDownscale.setOnClickListener { onToggleDownscale() }
        findViewById<Button>(R.id.btnRestore).setOnClickListener { doRestore("استعادة يدوية للدقة الأصلية") }

        btnAnalyzeAi.setOnClickListener { onAnalyzeAi() }
        btnApplyAi.setOnClickListener { onApplyAiRecommendation() }

        findViewById<Button>(R.id.btnCopy).setOnClickListener { onCopyOutput() }
        findViewById<Button>(R.id.btnShare).setOnClickListener { onShareOutput() }
        findViewById<Button>(R.id.btnCoolMode).setOnClickListener { onCoolMode() }
        findViewById<Button>(R.id.btnClearLog).setOnClickListener { LogCenter.clear() }
    }

    private fun setupChoiceButtons() {
        val buttons = mapOf(
            Choice.H540 to findViewById<Button>(R.id.btnTarget540),
            Choice.H720 to findViewById<Button>(R.id.btnTarget720),
            Choice.H900 to findViewById<Button>(R.id.btnTarget900),
            Choice.AI to findViewById<Button>(R.id.btnTargetAi)
        )
        buttons.forEach { (c, v) ->
            v.setOnClickListener {
                choice = c
                updateChoiceSelection(buttons)
                LogCenter.log("تم اختيار الدقة المستهدفة: ${choiceLabel(c)}")
            }
        }
        updateChoiceSelection(buttons)
    }

    private fun updateChoiceSelection(buttons: Map<Choice, Button>) {
        buttons.forEach { (c, v) -> v.isSelected = (c == choice) }
    }

    private fun choiceLabel(c: Choice): String = when (c) {
        Choice.H540 -> "540p (أقصى أداء)"
        Choice.H720 -> "720p (متوازن)"
        Choice.H900 -> "900p (جودة أعلى)"
        Choice.AI -> "توصية AI (${aiReport?.recommendedHeight?.toString()?.plus("p") ?: "شغّل التحليل أولاً"})"
    }

    private fun selectedTargetHeight(): Int {
        return when (choice) {
            Choice.H540 -> 540
            Choice.H720 -> 720
            Choice.H900 -> 900
            Choice.AI -> {
                val h = aiReport?.recommendedHeight
                if (h == null) {
                    LogCenter.log("⚠ لم يتم تشغيل تحليل AI بعد — سيُستخدم 720p مؤقتاً")
                    720
                } else {
                    h
                }
            }
        }
    }

    // ══════════════ معلومات الجهاز والحالة ══════════════

    private fun refreshDeviceInfo() {
        tvInfoModel.text = "الموديل: ${Build.MANUFACTURER} ${Build.MODEL}"
        val soc = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            "${Build.SOC_MANUFACTURER} ${Build.SOC_MODEL}"
        } else {
            listOfNotNull(Build.HARDWARE, Build.BOARD).joinToString(" / ")
        }
        tvInfoSoc.text = "المعالج: $soc  •  Android ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})"

        val am = getSystemService(Context.ACTIVITY_SERVICE) as android.app.ActivityManager
        val mem = android.app.ActivityManager.MemoryInfo()
        am.getMemoryInfo(mem)
        tvInfoRam.text = "الرام: ${"%.1f".format(mem.totalMem / 1073741824.0)} جيجابايت  •  متوفرة: ${"%.1f".format(mem.availMem / 1073741824.0)}جيجابايت"

        val dm = DisplayMetrics()
        @Suppress("DEPRECATION")
        windowManager.defaultDisplay.getRealMetrics(dm)
        @Suppress("DEPRECATION")
        val refresh = windowManager.defaultDisplay.refreshRate.toInt()
        tvInfoDisplay.text = "العرض: ${dm.widthPixels}×${dm.heightPixels}px  •  ${(dm.density * 160).toInt()}dpi  •  ${refresh}Hz"
    }

    private fun refreshStatus() {
        executor.execute {
            val line = ShellExecutor.backendStatusLine()
            val shizukuInfo = if (ShellExecutor.shizukuBinderAlive()) {
                if (ShellExecutor.shizukuPermissionGranted()) "✓ Shizuku جاهز للعمل بصلاحية shell"
                else "Shizuku يعمل — الإذن غير ممنوح بعد"
            } else {
                "Shizuku غير مثبت أو غير مُشغّل"
            }
            mainHandler.post {
                tvStatusBackend.text = line
                LogCenter.log(shizukuInfo)
            }
        }
    }

    // ══════════════ Shizuku ══════════════

    private fun onGrantShizuku() {
        if (!ShellExecutor.shizukuBinderAlive()) {
            AlertDialog.Builder(this)
                .setTitle("تفعيل Shizuku")
                .setMessage(
                    "Shizuku غير مُشغّل على هذا الجهاز. الخطوات:\n\n" +
                        "1) ثبّت Shizuku من متجر Google Play أو GitHub (RikkaApps/Shizuku)\n" +
                        "2) فعّله — عبر ADB من الحاسوب:\n" +
                        "    adb shell sh /sdcard/Android/data/moe.shizuku.manager/start.sh\n" +
                        "   أو عبر خيارات المطوّر (Wireless debugging) مباشرة من الجهاز\n" +
                        "3) ارجع لـ NEXORA واضغط «منح إذن Shizuku» ووافق على الطلب\n\n" +
                        "البديل: جهاز مرووت — سيكتشف NEXORA وضع Root تلقائياً."
                )
                .setPositiveButton("حسناً", null)
                .show()
            return
        }
        if (ShellExecutor.shizukuPermissionGranted()) {
            toast("Shizuku جاهز ومأذون بالفعل ✓")
            return
        }
        val ok = ShellExecutor.shizukuRequestPermission(1001)
        if (ok) {
            toast("وافق على الطلب في نافذة Shizuku…")
            mainHandler.postDelayed({ refreshStatus() }, 3000)
        } else {
            toast("تعذر طلب الإذن — تأكد أن Shizuku يعمل")
        }
    }

    private fun onOpenUsageAccess() {
        try {
            startActivity(Intent(android.provider.Settings.ACTION_USAGE_ACCESS_SETTINGS))
            toast("ابحث عن NEXORA وفعّل «الوصول لبيانات الاستخدام»")
        } catch (e: Exception) {
            toast("تعذر فتح إعدادات Usage Access على هذا الجهاز")
        }
    }

    // ══════════════ المُخفّض الذكي للدقة ══════════════

    private fun currentPhysical(): Pair<Int, Int> {
        val dm = DisplayMetrics()
        @Suppress("DEPRECATION")
        windowManager.defaultDisplay.getRealMetrics(dm)
        return dm.widthPixels to dm.heightPixels
    }

    private fun onToggleDownscale() {
        if (ResolutionManager.isOverrideActive(this)) {
            doRestore("إيقاف وضع الأداء واستعادة الدقة")
        } else {
            applyDownscale(
                targetHeight = selectedTargetHeight(),
                gamePkg = null,
                after = { refreshDownscaleUI() }
            )
        }
    }

    private fun applyDownscale(targetHeight: Int, gamePkg: String?, after: (() -> Unit)? = null) {
        btnToggleDownscale.isEnabled = false
        executor.execute {
            val (pw, ph) = currentPhysical()
            val (w, h) = ResolutionManager.computeTarget(pw, ph, targetHeight)
            val ok = ResolutionManager.applyDownscale(applicationContext, w, h, gamePkg)
            mainHandler.post {
                btnToggleDownscale.isEnabled = true
                if (ok) {
                    toast("⚡ وضع الأداء نشط — ${w}×${h}")
                } else {
                    toast("تعذر تطبيق الخفض — فعّل Root أو Shizuku أولاً")
                }
                refreshDownscaleUI()
                after?.invoke()
            }
        }
    }

    private fun doRestore(reason: String) {
        executor.execute {
            val ok = ResolutionManager.restoreOriginal(applicationContext)
            mainHandler.post {
                toast(if (ok) "✅ $reason — عادت الدقة الأصلية" else "تعذر تأكيد الاستعادة — راجع سجل العمليات")
                refreshDownscaleUI()
            }
        }
    }

    private fun refreshDownscaleUI() {
        executor.execute {
            val active = ResolutionManager.isOverrideActive(applicationContext)
            mainHandler.post {
                if (active) {
                    btnToggleDownscale.text = "■ إيقاف وضع الأداء واستعادة الدقة"
                    tvDownscaleStatus.text = "الحالة: وضع الأداء نشط — الدقة مخفّضة (تعود تلقائياً عند الخروج من اللعبة)"
                    tvDownscaleStatus.setTextColor(getColor(R.color.nexora_green))
                } else {
                    btnToggleDownscale.text = "⚡ تفعيل وضع الأداء الآن"
                    tvDownscaleStatus.text = "الحالة: الدقة الأصلية مفعّلة"
                    tvDownscaleStatus.setTextColor(getColor(R.color.nexora_warn))
                }
            }
        }
    }

    // ══════════════ تحليل AI ══════════════

    private fun onAnalyzeAi() {
        btnAnalyzeAi.isEnabled = false
        pbAi.progress = 0
        tvAiScore.text = "—"
        tvAiNotes.text = ""
        btnApplyAi.visibility = View.GONE

        executor.execute {
            val report = AIAnalyzer.analyze(applicationContext)
            mainHandler.post {
                val steps = listOf(
                    "جاري فحص شريحة المعالج Mali / Adreno…",
                    "جاري قراءة الذاكرة العشوائية…",
                    "جاري قياس دقة الشاشة والكثافة…",
                    "جاري قراءة الحالة الحرارية…",
                    "جاري رسم التوصية المثلى…"
                )
                val animator = ValueAnimator.ofInt(0, 100)
                animator.duration = 2400
                animator.addUpdateListener { anim ->
                    val v = anim.animatedValue as Int
                    pbAi.progress = v
                    val idx = (v / 21).coerceIn(0, steps.lastIndex)
                    tvAiVerdict.text = steps[idx]
                }
                animator.addListener(object : android.animation.AnimatorListenerAdapter() {
                    override fun onAnimationEnd(animation: android.animation.Animator) {
                        aiReport = report
                        btnAnalyzeAi.isEnabled = true
                        showAiReport(report)
                    }
                })
                animator.start()
            }
        }
    }

    private fun showAiReport(report: AIAnalyzer.AiReport) {
        tvAiScore.text = report.score.toString()
        val scoreColor = when {
            report.score >= 70 -> getColor(R.color.nexora_green)
            report.score >= 45 -> getColor(R.color.nexora_warn)
            else -> getColor(R.color.nexora_danger)
        }
        tvAiScore.setTextColor(scoreColor)
        tvAiVerdict.text = report.verdict
        tvAiVerdict.setTextColor(getColor(R.color.nexora_text))
        tvAiNotes.text = report.notes.joinToString("\n") { "• $it" }
        btnApplyAi.text = "طبّق توصية AI — خفض الدقة إلى ~${report.recommendedHeight}p (+${report.expectedFpsGain}٪ فريمات متوقعة)"
        btnApplyAi.visibility = View.VISIBLE
        LogCenter.log("تحليل AI اكتمل — النتيجة ${report.score}/100 — التوصية ${report.recommendedHeight}p")
    }

    private fun onApplyAiRecommendation() {
        val report = aiReport
        if (report == null) {
            toast("شغّل التحليل أولاً")
            return
        }
        choice = Choice.AI
        applyDownscale(report.recommendedHeight, gamePkg = null) {
            LogCenter.log("طُبّقت توصية AI: ${report.recommendedHeight}p")
        }
    }

    // ══════════════ مكتبة الألعاب ══════════════

    private fun setupGamesLibrary() {
        val inflater = LayoutInflater.from(this)
        for (profile in Profiles.ALL) {
            val row = inflater.inflate(R.layout.item_game, gamesContainer, false)

            val avatar = row.findViewById<TextView>(R.id.tvAvatar)
            avatar.text = profile.name.trim().first().uppercaseChar().toString()
            avatar.background = GradientDrawable().apply {
                shape = GradientDrawable.OVAL
                setColor(profile.avatarColor)
            }

            row.findViewById<TextView>(R.id.tvGameName).text = profile.name
            row.findViewById<TextView>(R.id.tvGamePkg).text = profile.pkg

            val installed = isPackageInstalled(profile.pkg)
            val installState = row.findViewById<TextView>(R.id.tvInstallState)
            if (installed) {
                installState.text = "مثبتة ✓"
                installState.setTextColor(getColor(R.color.nexora_green))
            } else {
                installState.text = "غير مثبتة — الملفات تعمل كمرجع إعدادات"
                installState.setTextColor(getColor(R.color.nexora_text_dim))
            }

            row.findViewById<Button>(R.id.btnLaunch).setOnClickListener { launchGame(profile) }
            row.findViewById<Button>(R.id.btnUltraFps).setOnClickListener { onGenerateConfig(profile, ConfigEngine.TuneMode.ULTRA_FPS) }
            row.findViewById<Button>(R.id.btnSharpness).setOnClickListener { onGenerateConfig(profile, ConfigEngine.TuneMode.SHARPNESS) }

            gamesContainer.addView(row)
        }
    }

    private fun isPackageInstalled(pkg: String): Boolean {
        return try {
            packageManager.getPackageInfo(pkg, 0)
            true
        } catch (_: PackageManager.NameNotFoundException) {
            false
        }
    }

    private fun launchGame(profile: GameProfile) {
        val intent = packageManager.getLaunchIntentForPackage(profile.pkg)
        if (intent == null) {
            toast("${profile.name} غير مثبتة على هذا الجهاز")
            return
        }
        executor.execute {
            if (!ResolutionManager.isOverrideActive(applicationContext)) {
                val target = selectedTargetHeight()
                val (pw, ph) = currentPhysical()
                val (w, h) = ResolutionManager.computeTarget(pw, ph, target)
                ResolutionManager.applyDownscale(applicationContext, w, h, profile.pkg)
            }
            val usageGranted = GameWatcherService.hasUsageAccess(this)
            mainHandler.post {
                if (!usageGranted) {
                    toast("فعّل «صلاحية مراقبة اللعبة» ليعيد NEXORA الدقة تلقائياً عند خروجك")
                }
                try {
                    startActivity(intent)
                    val svc = Intent(this, GameWatcherService::class.java).apply {
                        putExtra(GameWatcherService.EXTRA_GAME_PKG, profile.pkg)
                        putExtra(GameWatcherService.EXTRA_GAME_NAME, profile.name)
                    }
                    startForegroundService(svc)
                    LogCenter.log("▶ تم إطلاق ${profile.name} مع وضع الأداء")
                    refreshDownscaleUI()
                } catch (e: Exception) {
                    toast("تعذر إطلاق ${profile.name}: ${e.message}")
                }
            }
        }
    }

    private fun onGenerateConfig(profile: GameProfile, mode: ConfigEngine.TuneMode) {
        executor.execute {
            val output = ConfigEngine.generate(applicationContext, profile, mode)
            if (mode == ConfigEngine.TuneMode.SHARPNESS) {
                applySystemSharpness()
            }
            mainHandler.post {
                showConfigOutput(output, mode)
                toast("تم توليد ${output.fileName}")
            }
        }
    }

    /** الجزء النظامي من Sharpness Boost: كثافة عرض أعلى + تباين نصي + (أفضل جهد) وضع ألوان حيوي */
    private fun applySystemSharpness() {
        LogCenter.log("═══ Sharpness Boost — تطبيق الجزء النظامي ═══")
        val density = ResolutionManager.bumpDensity(20)
        val contrast = ShellExecutor.run("settings put secure high_text_contrast_enabled 1")
        LogCenter.logResult("high_text_contrast_enabled", contrast)
        val vivid = ShellExecutor.run("settings put system display_color_mode 1")
        LogCenter.logResult("display_color_mode (vivid — إن دعمه الجهاز)", vivid)
        LogCenter.log("ملاحظة: لاستعادة الكثافة الأصلية اضغط «استعادة الدقة الأصلية» (wm density reset)")
    }

    private fun showConfigOutput(output: ConfigEngine.ConfigOutput, mode: ConfigEngine.TuneMode) {
        lastConfig = output
        tvOutputTitle.text = output.fileName
        tvConfigOutput.text = output.content
        tvInstallNote.text = "طريقة التركيب:\n${output.instructions}\n\nالمسار: ${output.path}"
        if (mode == ConfigEngine.TuneMode.ULTRA_FPS) {
            LogCenter.log("⚡ ULTRA FPS جاهز — اجمعه مع وضع الأداء لأفضل النتائج")
        }
    }

    private fun onCopyOutput() {
        val cfg = lastConfig
        if (cfg == null) {
            toast("ولّد ملفاً أولاً من أزرار أي لعبة")
            return
        }
        val clip = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        clip.setPrimaryClip(ClipData.newPlainText(cfg.fileName, cfg.content))
        toast("نُسخ محتوى ${cfg.fileName} ✓")
    }

    private fun onShareOutput() {
        val cfg = lastConfig
        if (cfg == null) {
            toast("ولّد ملفاً أولاً من أزرار أي لعبة")
            return
        }
        val share = Intent(Intent.ACTION_SEND).apply {
            type = "text/plain"
            putExtra(Intent.EXTRA_SUBJECT, "NEXORA — ${cfg.fileName}")
            putExtra(Intent.EXTRA_TEXT, "NEXORA AI Turn — ${cfg.fileName}\n\n${cfg.content}\n\n${cfg.instructions}")
        }
        startActivity(Intent.createChooser(share, "مشاركة الملف عبر…"))
    }

    // ══════════════ Cool Mode ══════════════

    private fun setupCoolModeGrid() {
        var row: LinearLayout? = null
        var index = 0
        for ((pkg, label) in CoolMode.HEAVY_APPS) {
            if (index % 2 == 0) {
                row = LinearLayout(this).apply {
                    orientation = LinearLayout.HORIZONTAL
                    layoutParams = LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT
                    )
                }
                gridCoolApps.addView(row)
            }
            val cb = CheckBox(this).apply {
                text = label
                setTextColor(getColor(R.color.nexora_text))
                isChecked = true
                setTextSize(android.util.TypedValue.COMPLEX_UNIT_SP, 12.5f)
                buttonTintList = android.content.res.ColorStateList.valueOf(getColor(R.color.nexora_cyan))
                layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
            }
            row?.addView(cb)
            coolCheckboxes[pkg] = cb
            index++
        }
    }

    private fun onCoolMode() {
        val targets = coolCheckboxes.filterValues { it.isChecked }.keys.toList()
        tvCoolResult.text = "جاري إيقاف العمليات الخلفية…"
        executor.execute {
            val report = CoolMode.enable(targets)
            mainHandler.post {
                tvCoolResult.text = report
                toast("❄ Cool Mode جاهز")
            }
        }
    }

    // ══════════════ السجل ══════════════

    private fun setupConsole() {
        tvLog.text = ""
        LogCenter.setListener { line ->
            mainHandler.post {
                val current = tvLog.text?.toString() ?: ""
                val newText = if (current.isEmpty()) line else "$current\n$line"
                tvLog.text = if (newText.length > 12000) newText.takeLast(12000) else newText
                logScroll.post { logScroll.fullScroll(ScrollView.FOCUS_DOWN) }
            }
        }
        // التقط أي أسطر سُجّلت قبل الاستماع
        for (line in LogCenter.dump().split("\n")) {
            if (line.isNotBlank()) tvLog.append(line + "\n")
        }
    }

    // ══════════════ أدوات ══════════════

    private fun requestNotificationPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS) !=
                PackageManager.PERMISSION_GRANTED
            ) {
                requestPermissions(arrayOf(android.Manifest.permission.POST_NOTIFICATIONS), 4242)
            }
        }
    }

    private fun toast(message: String) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
    }
}
