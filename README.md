# NEXORA — AI Resolution & FPS Tuner

تطبيق أندرويد ذكي يحوّل هاتفك إلى **وضع "DLSS Performance" حقيقي**: يرندر الـ GPU بكسلات أقل بدل أكثر، ويعيد الدقة الأصلية تلقائياً أول ما تخرج من اللعبة — مع مولّد ملفات إعدادات تحسين للألعاب والمحاكيات ووضع تبريد.

> An intelligent on-device game performance brain for Android — Smart Resolution Downscaler, AI optimizer, per-game config injector, and Cool Mode. Built with Kotlin. APK is built automatically by GitHub Actions.

---

## المميزات

### 1) المُخفّض الذكي للدقة (Smart Downscaler — وضع الأداء)
- بضغطة زر واحدة: يغيّر دقة الشاشة مؤقتاً عبر صلاحيات **Root (`su`)** أو **Shizuku** أو **ADB shell** (`wm size WxH`)
- يحفظ الدقة الأصلية قبل أي تغيير، ويعيدها عبر `wm size reset`
- **استعادة تلقائية**: خدمة مراقبة (Foreground Service) تكشف خروجك من اللعبة عبر UsageStats وتعيد الدقة فوراً — حتى لو نسيت
- الدقة تتوفّر بخيارات: 540p / 720p / 900p / **توصية AI**

النتيجة: حمل أقل على المعالج الرسومي (Mali / Adreno) ← فريمات أعلى وثبات أفضل وحرارة أقل.

### 2) مولّد ملفات التحسين (Config & Driver Injector)
لكل لعبة/محاكي ثلاثة أزرار:

| الزر | ماذا يفعل |
|---|---|
| ⚡ **ULTRA FPS** | يولّد ملف إعدادات خفيف: ظلال أقل، مسافات أقل، 120Hz (مثال: `ClientAppSettings.json` لـ Roblox، `GFX.ini` لـ Dolphin، `.cfg` لـ RetroArch، `.env` لـ Winlator، INI مرجعي لبقية الألعاب) |
| ◐ **SHARPNESS BOOST** | يرفع تباين الشاشة ووحدة الألوان والكثافة على مستوى النظام + ملف وضوح حواف للعبة — لتعويض نعومة خفض الدقة |
| ❄ **COOL MODE** | يوقف كل العمليات الخلفية (`cmd activity kill-all`) ويُجهض التطبيقات الثقيلة المختارة (TikTok/فيسبوك/يوتيوب…) ← لا يصل المعالج لمرحلة الـ Thermal Throttling |

يدعم من العلبة: Roblox، GTA SA / Vice City / III، PUBG Mobile، Call of Duty Mobile، Genshin Impact، Mobile Legends، Free Fire، eFootball، Minecraft، Winlator، RetroArch، Dolphin.

### 3) تحليل On-device AI
يقرأ (SoC، دقة الشاشة، الكثافة، معدل التحديث، الرام، الحالة الحرارية) ويرسم **خطة أداء مخصصة لجهازك**: نقاط 0–100، الدقة المستهدفة المثلى، الزيادة المتوقعة في الفريمات — كل شيء محلي، بلا إنترنت.

---

## المتطلبات

NEXORA يحتاج صلاحية **shell** واحدة فقط لتعمل كل الميزات (بدون تعديل على ملفات اللعبة):

- **Root**: يعمل تلقائياً
- **Shizuku** (بدون روت): ثبّته من Play Store، فعّله عبر ADB (`adb shell sh /sdcard/Android/data/moe.shizuku.manager/start.sh`) أو Wireless debugging، ثم اضغط «منح إذن Shizuku» داخل التطبيق
- **ADB يدوي**: نفّذ `adb shell wm size 720x1560` قبل اللعب و `adb shell wm size reset` بعدها — NEXORA يرافقك بالأزرار والمنطق نفسه

للاستعادة التلقائية: فعّل «الوصول إلى بيانات الاستخدام» (Usage Access) من داخل التطبيق.

---

## تحميل الـ APK

جاهز دائماً من صفحة **Releases** في هذا المستودع:
- `NEXORA-v1.0.0-release.apk` — موقّع (يثبّت مباشرة)
- `NEXORA-v1.0.0-debug.apk` — نسخة التطوير

أو من تبويب **Actions → Build NEXORA APK → Artifacts** لآخر بناء.

> Android 8.0 (API 26) فأعلى. الـ APK موقّع بمفتاح تطوير — إن تعذر التثبيت فعّل «مصادر غير معروفة».

---

## البناء بنفسك

عبر GitHub Actions (الأسهل — كل دفعة إلى الفرع تبني APK جديد تلقائياً):

```bash
# محلياً — يتطلب JDK 17 + Gradle 8.7 + Android SDK (compileSdk 34)
gradle :app:assembleDebug
gradle :app:assembleRelease
# الناتج: app/build/outputs/apk/
```

---

## كيف يعمل من الداخل؟

```
MainActivity ────────────────┬── ShellExecutor (AUTO: root → shizuku → sh)
│                            │      ├─ su -c "wm size 720x1440"   (Root)
│                            │      ├─ Shizuku.newProcess(...)    (Shell بدون روت)
│                            │      └─ record كل أمر في LogCenter (وحدة التحكم داخل التطبيق)
│                            ├── ResolutionManager   (حفظ/استعادة الدقة والكثافة الأصلية)
│                            ├── AIAnalyzer          (تقييم on-device: SoC/RAM/شاشة/حرارة)
│                            ├── ConfigEngine        (قوالب ملفات لكل لعبة + حفظ ومشاركة)
│                            ├── CoolMode            (kill-all + force-stop للتطبيقات الثقيلة)
│                            └── GameWatcherService  (Foreground) ← UsageStats ← استعادة تلقائية
```

**الأمان والخصوصية**: لا يجمع أي بيانات، لا إنترنت إطلاقاً، لا يعدّل ملفات أي لعبة. كل ما يفعله: أوامر نظام مشروعة (`wm`/`am`/`settings`) وملفات إعدادات توليدية تقرّر أنت أين تضعها.

> ملاحظة: NEXORA أداة أداء وليس هاكر ولا غش — بعض أزرار "ULTRA FPS" تولّد مرجع إعدادات يطابق خيارات داخل اللعبة لأن كثيراً من الألعاب لا تقرأ ملفات INI الرسمية. أعلام Roblox مثلاً قد تُتجاهل في الإصدارات الحديثة.

---

## بنية المستودع

```
app/src/main/java/com/nexora/optimizer/
├── MainActivity.kt            # الشاشة الرئيسية (RTL عربي)
├── core/
│   ├── ShellExecutor.kt       # محرك الأوامر: Root / Shizuku / sh
│   ├── ResolutionManager.kt   # خفض/استعادة الدقة والكثافة
│   ├── AIAnalyzer.kt          # "الذكاء الاصطناعي" محلي: تقييم وتوصية مخصصة
│   ├── ConfigEngine.kt        # مولّد ملفات ULTRA FPS / SHARPNESS لكل لعبة
│   ├── CoolMode.kt            # وضع التبريد (kill-all + force-stop)
│   └── LogCenter.kt           # سجل العمليات المباشر
├── data/GameProfile.kt        # قاعدة بيانات الألعاب والمحاكيات
└── service/
    ├── GameWatcherService.kt  # مراقبة خروج اللعبة ← استعادة تلقائية
    └── Notifs.kt              # قناة الإشعارات
.github/workflows/build-apk.yml # بناء APK تلقائياً على GitHub (Gradle 8.7 + AGP 8.5.2, SDK 34)
```

---

## الترخيص

MIT — افعل ما تشاء، مع بقاء حقوقك محفوظة لفريق تطوير NEXORA.
