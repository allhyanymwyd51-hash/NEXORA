package com.nexora.optimizer.core

import java.text.SimpleDateFormat
import java.util.Locale

/**
 * سجلّ العمليات المركزي: يجمع كل أمر shell يتم تنفيذه مع نتيجته،
 * ويعرضها داخل وحدة التحكم (Log Console) في الواجهة الرئيسية.
 */
object LogCenter {

    interface Listener {
        fun onLogLine(line: String)
    }

    private val lock = Any()
    private val lines = ArrayDeque<String>()
    private var listener: Listener? = null
    private val timeFormat = SimpleDateFormat("HH:mm:ss", Locale.US)

    fun setListener(l: Listener?) {
        listener = l
    }

    fun log(message: String) {
        val stamp = timeFormat.format(System.currentTimeMillis())
        val line = "[$stamp] $message"
        synchronized(lock) {
            if (lines.size > 300) lines.removeFirst()
            lines.addLast(line)
        }
        listener?.onLogLine(line)
    }

    fun dump(): String = synchronized(lock) { lines.joinToString("\n") }

    fun clear() {
        synchronized(lock) { lines.clear() }
        log("تم مسح السجل")
    }

    /** تسجيل نتيجة أمر shell بصيغة موحّدة */
    fun logResult(label: String, result: ShellResult) {
        val mark = if (result.success) "✓" else "✗"
        val detail = result.output.replace("\n", " ⏎ ").take(220)
        log("$mark [$label] ← ${result.backend} | exit=${result.exitCode} | ${if (detail.isBlank()) "(بدون مخرجات)" else detail}")
    }
}
