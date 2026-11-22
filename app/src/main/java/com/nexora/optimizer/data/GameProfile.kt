package com.nexora.optimizer.data

/**
 * ملف تعريف لعبة / محاكي — يحدد
 *  * الاسم المعروض
 *  * اسم الحزمة (Package Name) للإطلاق والكشف عن التثبيت
 *  * نوعه (لعبة / محاكي)
 *  * لون الأفاتار في الواجهة
 */
enum class ProfileCategory { GAME, EMULATOR }

data class GameProfile(
    val id: String,
    val name: String,
    val pkg: String,
    val category: ProfileCategory,
    val avatarColor: Int
)

object Profiles {
    val ALL: List<GameProfile> = listOf(
        GameProfile("roblox", "Roblox", "com.roblox.client", ProfileCategory.GAME, 0xFFEF4444.toInt()),
        GameProfile("gta_sa", "GTA: San Andreas", "com.rockstargames.gtasa", ProfileCategory.GAME, 0xFFF97316.toInt()),
        GameProfile("gta_vc", "GTA: Vice City", "com.rockstargames.gtavc", ProfileCategory.GAME, 0xFFFBBF24.toInt()),
        GameProfile("gta3", "GTA III", "com.rockstargames.gta3", ProfileCategory.GAME, 0xFFFB7185.toInt()),
        GameProfile("pubg", "PUBG Mobile", "com.tencent.ig", ProfileCategory.GAME, 0xFFFACC15.toInt()),
        GameProfile("codm", "Call of Duty: Mobile", "com.activision.callofduty.shooter", ProfileCategory.GAME, 0xFF84CC16.toInt()),
        GameProfile("genshin", "Genshin Impact", "com.miHoYo.GenshinImpact", ProfileCategory.GAME, 0xFFC084FC.toInt()),
        GameProfile("mlbb", "Mobile Legends", "com.mobile.legends", ProfileCategory.GAME, 0xFF38BDF8.toInt()),
        GameProfile("freefire", "Free Fire", "com.dts.freefireth", ProfileCategory.GAME, 0xFFFB923C.toInt()),
        GameProfile("efootball", "eFootball", "jp.konami.pesam", ProfileCategory.GAME, 0xFF34D399.toInt()),
        GameProfile("minecraft", "Minecraft", "com.mojang.minecraftpe", ProfileCategory.GAME, 0xFFA3E635.toInt()),
        GameProfile("winlator", "Winlator (PC)", "com.winlator", ProfileCategory.EMULATOR, 0xFF22D3EE.toInt()),
        GameProfile("retroarch", "RetroArch", "com.retroarch", ProfileCategory.EMULATOR, 0xFFA78BFA.toInt()),
        GameProfile("dolphin", "Dolphin (GameCube/Wii)", "org.dolphinemu.dolphinemu", ProfileCategory.EMULATOR, 0xFF60A5FA.toInt()),
    )

    fun findById(id: String?): GameProfile? = ALL.firstOrNull { it.id == id }
}
