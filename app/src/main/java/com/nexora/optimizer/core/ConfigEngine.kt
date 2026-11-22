package com.nexora.optimizer.core

import android.content.Context
import com.nexora.optimizer.data.GameProfile
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * مولّد ملفات التحسين للألعاب والمحاكيات (Config & Driver Injector).
 *
 * يولّد ملف إعدادات محسّن لكل لعبة + وضعين:
 *   ULTRA_FPS  → فريمات قصوى: ظلال أقل، مسافات أقل، 120Hz
 *   SHARPNESS  → وضوح الحواف: تعويض نعومة خفض الدقة (شاربنينغ + فلترة أنيزوتروبية)
 *
 * الملفات تُحفظ في: Android/data/com.nexora.optimizer/files/configs/
 * ويمكن نسخها أو مشاركتها بضغطة زر.
 */
object ConfigEngine {

    enum class TuneMode { ULTRA_FPS, SHARPNESS }

    data class ConfigOutput(
        val fileName: String,
        val content: String,
        val path: String,
        val instructions: String
    )

    fun generate(context: Context, profile: GameProfile, mode: TuneMode): ConfigOutput {
        val dir = File(context.getExternalFilesDir(null), "configs")
        if (!dir.exists()) dir.mkdirs()

        val (fileName, content, instructions) = when (mode) {
            TuneMode.ULTRA_FPS -> ultraFps(profile)
            TuneMode.SHARPNESS -> sharpness(profile)
        }

        val outFile = File(dir, fileName)
        outFile.writeText(content, Charsets.UTF_8)
        LogCenter.log("✓ تم توليد الملف: ${outFile.absolutePath}")
        return ConfigOutput(fileName, content, outFile.absolutePath, instructions)
    }

    private fun header(profile: GameProfile, mode: TuneMode): String {
        val date = SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US).format(Date())
        val modeName = if (mode == TuneMode.ULTRA_FPS) "ULTRA FPS — فريمات قصوى" else "SHARPNESS BOOST — وضوح الحواف"
        return """
        |# ╔═══════════════════════════════════════════════════════════╗
        |#  NEXORA AI Tuner — ${profile.name}
        |#  النمط: $modeName
        |#  التاريخ: $date
        |#  الفكرة: خفض الحمل على الـ GPU (Mali/Adreno) = فريمات أكثر وحرارة أقل
        |# ╚═══════════════════════════════════════════════════════════╝
        """.trimMargin() + "\n"
    }

    private fun ultraFps(profile: GameProfile): Triple<String, String, String> {
        val h = header(profile, TuneMode.ULTRA_FPS)
        return when (profile.id) {
            "roblox" -> Triple(
                "ClientAppSettings.json",
                """
                {
                  "DFIntTaskSchedulerTargetFps": 120,
                  "DFIntDebugFRMQualityLevelOverride": 1,
                  "FFlagDebugGraphicsPreferVulkan": true
                }
                """.trimIndent() + "\n",
                "ضع الملف مكان: Android/data/com.roblox.client/files/content/app/ClientAppSettings.json\n" +
                    "(انسخه إلى هذا المسار يدوياً — صلاحيات Android 11+ تمنع أي تطبيق من الكتابة هناك مباشرة).\n" +
                    "ملاحظة: Roblox قد يتجاهل بعض الأعلام في الإصدارات الحديثة — الأهم هو DFIntTaskSchedulerTargetFps=120."
            )
            "dolphin" -> Triple(
                "Dolphin_UltraFPS_GFX.ini",
                h + """
                [Settings]
                # 1x = دقة داخلية أصلية = أخف حمل ممكن
                InternalResolution = 1
                EnablePixelLighting = False
                FastDepthCalc = True

                [Hacks]
                # تسريع EFB — فرق كبير على Mali
                EFBToTextureEnable = True
                EFBAccessEnable = False
                EFBMonochrome = False
                SkipDuplicateXFBs = True
                XFBToTextureEnable = True
                VertexRounding = True
                CPUCull = True
                DisableCopyFilter = True
                """.trimIndent() + "\n",
                "افتح Dolphin ← الإعدادات ← المسار: Android/data/org.dolphinemu.dolphinemu/files/Config/GFX.ini\n" +
                    "انسخ الأقسام أعلاه داخل GFX.ini (احتفظ بنسخة احتياطية أولاً)، أو ابدأ من هذا الملف كقالب."
            )
            "retroarch" -> Triple(
                "retroarch_ultra_fps.cfg",
                h + """
                # أداء RetroArch الأقصى
                video_threaded = "true"
                video_max_swapchain_images = "2"
                video_smooth = "false"
                video_shader_enable = "false"
                rewind_enable = "false"
                pause_nonactive = "true"
                cheevos_enable = "false"
                fps_show = "false"
                run_ahead_enabled = "false"
                savestate_auto_load = "false"
                video_refresh_rate = "120.000000"
                """.trimIndent() + "\n",
                "RetroArch: الإعدادات ← الإدخال والإعدادات ← Configuration File\n" +
                    "افتح retroarch.cfg الحالي وأضف السطور أعلاه في نهايته (القيم المكتوبة أخيراً تغلِب).\n" +
                    "المسار المعتاد: Android/data/com.retroarch/files/retroarch.cfg"
            )
            "winlator" -> Triple(
                "winlator_ultra_fps.env",
                h + """
                # متغيرات بيئة لحاوية Winlator — الأداء الأقصى
                # (ضعها في إعدادات الحاوية ← Environment Variables)
                MESA_VK_WSI_PRESENT_MODE=mailbox
                DXVK_ASYNC=1
                DXVK_HUD=fps
                MESA_GL_VERSION_OVERRIDE=4.6
                MESA_EXTENSION_MAX_YEAR=2022
                TU_DEBUG=0
                # إذا كانت اللعبة تعاني على Turnip جرّب:
                # MESA_VK_WSI_PRESENT_MODE=immediate
                """.trimIndent() + "\n",
                "Winlator: افتح الحاوية ← Advanced ← Environment Variables وأضف الأسطر أعلاه.\n" +
                    "فعّل أيضاً: Graphics Driver = Turnip (adrenotools) + DXVK = أحدث إصدار + (اختياري) FSR لرفع الدقة من 720p."
            )
            "minecraft" -> Triple(
                "minecraft_bedrock_notes.ini",
                h + """
                # Minecraft Bedrock — الإعدادات داخل اللعبة (لا يوجد ملف نصي رسمي)
                [Video]
                render_distance = 6          # الأدنى المقبول بصرياً
                simulation_distance = 4
                graphics = Fast              # Bedrock: اضبط Graphics على Fast
                max_fps = 120
                vsync = off
                fancy_graphics = off
                beautiful_skies = off
                smooth_lighting = off
                fancy_leaves = off
                biome_blend = off
                entity_shadows = off
                ao = off
                """.trimIndent() + "\n",
                "Minecraft (Bedrock على Android): الإعدادات ← الفيديو:\n" +
                    "Graphics = Fast، Render Distance = 6، إيقاف Fancy Graphics / Smooth Lighting / Entity Shadows،\n" +
                    "وحدد Max FPS = 120 (يحتاج شاشة 120Hz أو لا قيمة له)."
            )
            else -> Triple(
                "nexora_ultra_fps_${profile.id}.ini",
                h + """
                # ملف إعدادات NEXORA — ${profile.name}
                # إن لم تكن اللعبة تقرأ ملفات INI فهذا مرجع يطابق خيارات داخل اللعبة:
                [Graphics]
                shadows = off
                shadow_quality = lowest
                view_distance = low
                render_distance = low
                texture_quality = low
                anti_aliasing = off
                anisotropic_filtering = off
                motion_blur = off
                bloom = off
                depth_of_field = off
                ambient_occlusion = off
                global_illumination = off
                reflections = off
                particles = minimal

                [Performance]
                fps_limit = 120
                refresh_rate = 120
                vsync = off
                resolution_scale = 0.66     # يطابق خفض الدقة في وضع الأداء
                frame_generation = off      # غير متاح على Android حالياً

                [CPU]
                background_apps = killed    # فعّل Cool Mode قبل اللعب
                power_mode = performance    # من إعدادات البطارية: وضع الأداء
                """.trimIndent() + "\n",
                "الطريقة بحسب اللعبة:\n" +
                    "• PUBG Mobile: Graphics = Smooth + Frame Rate = Extreme/90\n" +
                    "• Call of Duty Mobile: Quality = Low + Frame Rate = Max (90/120)\n" +
                    "• Genshin Impact: Graphics = Lowest + FPS = 60 (أقصى ما تتيحه)\n" +
                    "• Mobile Legends / Free Fire / eFootball: أقصى إعداد فريمات + أدنى رسومات\n" +
                    "فعّل بجانبها وضع الأداء (خفض الدقة) في NEXORA للنتيجة الكاملة."
            )
        }
    }

    private fun sharpness(profile: GameProfile): Triple<String, String, String> {
        val h = header(profile, TuneMode.SHARPNESS)
        return when (profile.id) {
            "retroarch" -> Triple(
                "retroarch_sharpness.cfg",
                h + """
                # وضوح الحواف بدل النعومة بعد خفض الدقة
                video_smooth = "false"
                video_shader_enable = "true"
                video_shader = "shaders/sharp-bilinear.slangp"
                video_bilinear_filter = "false"
                video_scale_integer = "false"
                """.trimIndent() + "\n",
                "RetroArch ← Shaders: حمّل sharp-bilinear أو xBRZ-freescale لتعويض خفض الدقة.\n" +
                    "المسار: Configuration File ← retroarch.cfg وأضف السطور أعلاه."
            )
            "winlator" -> Triple(
                "winlator_sharpness.env",
                h + """
                # تعويض وضوح الصورة بعد خفض دقة الرندر
                # (FSR يدعم الرفع: render 720p ← display أصلية مع حدة ممتازة)
                FSR=1
                # وفي حاوية Winlator فعّل يدوياً: Graphics Driver ← FSR + Sharpening
                """.trimIndent() + "\n",
                "Winlator: الحاوية ← Advanced ← فعّل FSR (FidelityFX Super Resolution)\n" +
                    "واجعل Sharpening بين 70–90. هذا أقرب معادل لـ DLSS على Android."
            )
            "dolphin" -> Triple(
                "Dolphin_Sharpness_GFX.ini",
                h + """
                [Settings]
                # فلترة أنيزوتروبية قصوى + حدة حواف — تعويض ممتاز لخفض الدقة
                MaxAnisotropy = 16
                DisableCopyFilter = True
                """.trimIndent() + "\n",
                "Dolphin ← Graphics Settings ← Enhancements:\n" +
                    "Anisotropic Filtering = 16x + Force Texture Filtering = Neutral.\n" +
                    "ادمج الأقسام أعلاه داخل GFX.ini."
            )
            else -> Triple(
                "nexora_sharpness_${profile.id}.ini",
                h + """
                # ملف وضوح الحواف — لتعويض نعومة الصورة بعد خفض الدقة
                [Display]
                contrast = +8               # رفع التباين يُظهر الحواف أوضح
                saturation = +12            # تشبّع أعلى يعوّض بهتان الدقة المخفضة
                sharpness = +15             # إن توفرت في إعدادات اللعبة/المجسم
                gamma = +2

                [AntiJaggies]
                # بديل الشاربنينغ إذا كانت اللعبة لا توفره:
                texture_filtering = bilinear
                anisotropic_filtering = 8x
                """.trimIndent() + "\n",
                "داخل اللعبة ارفع Contrast قليلاً (+5~+10) أو فعّل خيار Sharpness/Sharpen إن وُجد.\n" +
                    "في NEXORA نفسه زر «Sharpness Boost» يضبط كثافة العرض ومستوى التباين على مستوى النظام."
            )
        }
    }
}
