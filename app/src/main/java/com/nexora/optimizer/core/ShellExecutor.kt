package com.nexora.optimizer.core

import android.content.pm.PackageManager
import rikka.shizuku.Shizuku
import java.io.IOException
import java.io.InputStream
import java.util.concurrent.TimeUnit

/**
 * نتيجة تنفيذ أمر عبر أحد المحركات: Root (su) / Shizuku / Sh عادي.
 */
data class ShellResult(
    val success: Boolean,
    val exitCode: Int,
    val output: String,
    val backend: String
)

/**
 * محرك الأوامر الموحّد في NEXORA.
 *
 * ترتيب المحاولات في الوضع التلقائي (AUTO):
 *   1) Root عبر `su -c`  (أقوى صلاحية)
 *   2) Shizuku           (صلاحية shell بدون روت، عبر الـ binder)
 *   3) sh عادي           (يفشل بأدب ويُظهر تعليمات ADB)
 */
object ShellExecutor {

    enum class Mode { AUTO, ROOT, SHIZUKU }

    @Volatile
    var preferredMode: Mode = Mode.AUTO

    @Volatile
    private var rootCache: Boolean? = null

    // ---------- Shizuku ----------

    fun shizukuBinderAlive(): Boolean = try {
        Shizuku.pingBinder()
    } catch (t: Throwable) {
        false
    }

    fun shizukuPermissionGranted(): Boolean = try {
        Shizuku.checkSelfPermission() == PackageManager.PERMISSION_GRANTED
    } catch (t: Throwable) {
        false
    }

    /** يطلب إذن Shizuku؛ النتيجة تُقرأ لاحقاً عبر [shizukuPermissionGranted] */
    fun shizukuRequestPermission(requestCode: Int): Boolean = try {
        if (Shizuku.pingBinder()) {
            Shizuku.requestPermission(requestCode)
            true
        } else {
            false
        }
    } catch (t: Throwable) {
        LogCenter.log("✗ تعذر طلب إذن Shizuku: ${t.message}")
        false
    }

    fun backendStatusLine(): String {
        val mode = when (preferredMode) {
            Mode.AUTO -> "تلقائي"
            Mode.ROOT -> "Root فقط"
            Mode.SHIZUKU -> "Shizuku فقط"
        }
        val root = when (rootCache) {
            true -> "متاح"
            false -> "غير متاح"
            null -> "لم يُفحص بعد"
        }
        val shizuku = if (!shizukuBinderAlive()) {
            "غير يعمل"
        } else if (shizukuPermissionGranted()) {
            "يعمل ✓ ومأذون"
        } else {
            "يعمل لكن بدون إذن"
        }
        return "الوضع: $mode  •  Root: $root  •  Shizuku: $shizuku"
    }

    // ---------- التنفيذ ----------

    fun run(command: String): ShellResult {
        return when (preferredMode) {
            Mode.ROOT -> runAsRoot(command)
            Mode.SHIZUKU -> runAsShizuku(command)
            Mode.AUTO -> {
                val rootAvailable = rootCache ?: probeRoot()
                if (rootAvailable) return runAsRoot(command)
                if (shizukuBinderAlive()) {
                    return if (shizukuPermissionGranted()) {
                        runAsShizuku(command)
                    } else {
                        ShellResult(
                            false, -3,
                            "Shizuku يعمل لكن الإذن غير ممنوح — اضغط «منح إذن Shizuku» ثم حاول مجدداً",
                            "shizuku"
                        )
                    }
                }
                val plain = runPlain(command)
                if (plain.success) {
                    plain
                } else {
                    ShellResult(
                        false, -4,
                        "لا Root ولا Shizuku على هذا الجهاز. الحل: ثبّت Shizuku وفعّله عبر ADB (أو من خيارات المطوّر على أجهزة غير مرووتة) ثم اضغط «منح إذن Shizuku». بديل: نفّذ الأوامر يدوياً عبر `adb shell`.",
                        "none"
                    )
                }
            }
        }
    }

    /** فحص سريع لتوفّر su */
    private fun probeRoot(): Boolean {
        val r = runAsRoot("id", timeoutSec = 5)
        rootCache = r.success && r.output.contains("uid=")
        return rootCache == true
    }

    fun runAsRoot(command: String, timeoutSec: Long = 12): ShellResult {
        return try {
            val process = ProcessBuilder("su", "-c", command)
                .redirectErrorStream(false)
                .start()
            capture(process, "root", timeoutSec)
        } catch (e: IOException) {
            rootCache = false
            ShellResult(false, -1, "تعذر تشغيل su — الجهاز غير مرووت أو su غير مثبت (${e.message})", "root")
        } catch (t: Throwable) {
            ShellResult(false, -1, "خطأ في محرك Root: ${t.message}", "root")
        }
    }

    fun runAsShizuku(command: String, timeoutSec: Long = 12): ShellResult {
        if (!shizukuBinderAlive()) {
            return ShellResult(false, -3, "Shizuku غير يعمل — افتح تطبيق Shizuku وابدأ الخدمة أولاً", "shizuku")
        }
        if (!shizukuPermissionGranted()) {
            return ShellResult(false, -3, "إذن Shizuku غير ممنوح — اضغط «منح إذن Shizuku»", "shizuku")
        }
        return try {
            // يعمل بصلاحية shell (نفس صلاحيات adb shell) عند تشغيل Shizuku عبر ADB
            val process: Process = Shizuku.newProcess(arrayOf("sh", "-c", command), null, null)
            capture(process, "shizuku", timeoutSec)
        } catch (t: Throwable) {
            ShellResult(false, -3, "فشل تنفيذ الأمر عبر Shizuku: ${t.message}", "shizuku")
        }
    }

    fun runPlain(command: String, timeoutSec: Long = 8): ShellResult {
        return try {
            val process = ProcessBuilder("sh", "-c", command).start()
            capture(process, "sh", timeoutSec)
        } catch (t: Throwable) {
            ShellResult(false, -2, "sh غير متاح: ${t.message}", "sh")
        }
    }

    /** قراءة stdout/stderr على خيطين منفصلين لتفادي deadlock مع الأوامر كثيرة المخرجات */
    private fun capture(process: Process, backend: String, timeoutSec: Long): ShellResult {
        val out = StringBuilder()
        val err = StringBuilder()

        val tOut = Thread { drain(process.inputStream, out) }
        val tErr = Thread { drain(process.errorStream, err) }
        tOut.start()
        tErr.start()

        val finished = try {
            process.waitFor(timeoutSec, TimeUnit.SECONDS)
        } catch (t: Throwable) {
            false
        }
        if (!finished) {
            runCatching { process.destroy() }
        }
        tOut.join(1500)
        tErr.join(1500)

        val code = if (finished) {
            runCatching { process.exitValue() }.getOrDefault(-1)
        } else {
            -1
        }
        val text = buildString {
            append(out.toString().trim())
            val e = err.toString().trim()
            if (e.isNotEmpty()) {
                if (isNotEmpty()) append('\n')
                append(e)
            }
        }
        return ShellResult(code == 0, code, text, backend)
    }

    private fun drain(stream: InputStream, sink: StringBuilder) {
        try {
            stream.bufferedReader().use { reader ->
                reader.forEachLine { line ->
                    synchronized(sink) {
                        if (sink.length < 64 * 1024) sink.appendLine(line)
                    }
                }
            }
        } catch (_: Throwable) {
            // إغلاق المخرجات لا يجب أن يُسقط التنفيذ
        }
    }
}
