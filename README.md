# NEXORA — Unreal Engine 5 Open World RPG

مشروع لعبة عالم مفتوح واقعية مبني من الصفر لمحرك **Unreal Engine 5.4 (C++)**، يتضمن إعدادات رسومية متقدمة (Lumen, Nanite, Virtual Shadow Maps) ونظام مهام وتفاعل متكامل.

## المميزات الأساسية (Features)
- **جودة رسومية واقعية (UE5 Rendering):** تفعيل `Lumen` للإضاءة الديناميكية، `Nanite` للتفاصيل الدقيقة، `Virtual Shadow Maps`، و `DirectX 12` في `Config/DefaultEngine.ini`.
- **شخصية عالم مفتوح (`ANexoraCharacter`):** حركة ثالثة سلسة، نظام ركض مع استهلاك وتجديد طاقة التحمل (Stamina)، ونظام تفاعل بالـ LineTrace.
- **نظام المهام (`UQuestManagerComponent` & `FQuestInfo`):** قبول المهام، تتبع الأهداف، إتمام المهام، وإرسال أحداث (`BlueprintAssignable`) لربطها بواجهة المستخدم (UMG).
- **نظام التفاعل (`IInteractableInterface` & `AQuestGiverNPC`):** واجهة موحدة للتفاعل مع الشخصيات (NPCs) والعناصر في العالم المفتوح.

## كيفية التشغيل (How to Run)
1. تأكد من تثبيت **Unreal Engine 5.4** و **Visual Studio 2022** (مع حزمة Game Development with C++).
2. استنسخ المستودع:
   ```bash
   git clone https://github.com/allhyanymwyd51-hash/NEXORA.git
   ```
3. اضغط بزر الفأرة الأيمن على `NexoraOpenWorld.uproject` واختر **Generate Visual Studio project files**.
4. افتح ملف `.sln` في Visual Studio واضغط **Build**، ثم افتح `NexoraOpenWorld.uproject` في Unreal Engine 5.
5. أنشئ خريطة عالم مفتوح (World Partition) باسم `OpenWorld_Main` داخل `Content/Maps/` وأضف إليها شخصيات `QuestGiverNPC`.
