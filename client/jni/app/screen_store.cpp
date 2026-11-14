// Store — 3D-ready avatar items bought with Miy, plus the character preview.
#include "core/audio.h"
#include "screens.h"
#include "widgets.h"

namespace nx {
namespace app {
namespace screens {

namespace {

struct Item {
  char id[40];
  char name[80];
  char category[24];
  char rarity[16];
  int price;
  bool owned;
  bool equipped;
  char color[12];
};
Array<Item> gItems;
bool gLoaded = false;
int gTab = 0;  // 0 all, 1 tops, 2 bottoms, 3 hair, 4 accessories
ui::ScrollView gScroll;
char gSelectedColor[12] = "#5B6BFF";
int gBuyError = 0;
char gBuyMsg[160] = {0};

// Ledger / passes drawer: the buttons above open a real list, not a toast count.
struct LedgerRow {
  int delta;
  int balance;
  char reason[24];
  int at;
};
Array<LedgerRow> gLedger;
struct PassRow {
  char name[64];
  char mapId[48];
  int price;
  int perks;
  int at;
};
Array<PassRow> gPasses;
int gDrawer = 0;        // 0 none, 1 ledger, 2 passes
bool gLedgerLoaded = false;
bool gPassesLoaded = false;

const char* reasonLabel(const char* reason) {
  if (strcmp(reason, "playtime") == 0) return tr("وقت لعب", "Play time");
  if (strcmp(reason, "shop") == 0) return tr("شراء من المتجر", "Store purchase");
  if (strcmp(reason, "gamepass") == 0) return tr("شراء تصريح", "Pass purchase");
  if (strcmp(reason, "gamepass_earning") == 0) return tr("أرباح تصريح", "Pass earnings");
  if (strcmp(reason, "gamepass_refund") == 0) return tr("استرجاع تصريح", "Pass refund");
  if (strcmp(reason, "admin_adjust") == 0 || strcmp(reason, "adj") == 0) return tr("تعديل إداري", "Admin adjustment");
  return tr("حركة", "Movement");
}

const char* kCategories[5] = {"all", "shirt", "pants", "hair", "accessory"};
const char* kTabAr[5] = {"الكل", "قمصان", "بناطيل", "شعر", "إكسسوار"};
const char* kTabEn[5] = {"All", "Tops", "Bottoms", "Hair", "Extras"};

void reload() {
  api.call(NET_SHOP, "GET", "/api/shop", nullptr, 61);
  api.call(NET_INVENTORY, "GET", "/api/inventory", nullptr, 62);
}

Color itemColor(const Item& it) {
  unsigned v = 0;
  for (const char* p = it.id; *p; p++) v = v * 131 + (unsigned char)*p;
  static const unsigned palette[] = {0x7C4DFF, 0x38E1FF, 0x4EE39B, 0xE8C15A, 0xFF5F6D, 0x9AA6C8, 0xFF9F43,
                                     0x5B6BFF};
  return Color::hex(palette[v % 8]);
}

void drawAvatarPreview(const Rect& r) {
  ui::card(r, false);
  ui::verticalGradient(r.inset(2), Color(0.10f, 0.09f, 0.22f, 1.0f), Color(0.05f, 0.06f, 0.13f, 1.0f));
  // mannequin: head, torso, arms, legs — all drawn from primitives so the preview matches
  // exactly the palette the 3D avatar uses
  float cx = r.x + r.w * 0.5f;
  float top = r.y + ui::dp(26);
  float unit = r.h / 210.0f;
  Color skin = Color::hex(0xC98E6A);
  Color shirt = Color::hex(0x5B6BFF);
  Color pants = Color::hex(0x2B2F3D);
  for (int i = 0; i < gItems.count; i++) {
    if (!gItems[i].equipped) continue;
    Color c = itemColor(gItems[i]);
    if (strcmp(gItems[i].category, "shirt") == 0) shirt = c;
    else if (strcmp(gItems[i].category, "pants") == 0) pants = c;
  }
  ui::rect(Rect(cx - 26 * unit, top, 52 * unit, 52 * unit), skin, 26 * unit);                     // head
  ui::rect(Rect(cx - 28 * unit, top - 6 * unit, 56 * unit, 26 * unit), Color::hex(0x241A16), 14 * unit);  // hair
  ui::rect(Rect(cx - 34 * unit, top + 56 * unit, 68 * unit, 70 * unit), shirt, 14 * unit);       // torso
  ui::rect(Rect(cx - 52 * unit, top + 60 * unit, 16 * unit, 62 * unit), shirt * 0.94f, 8 * unit);
  ui::rect(Rect(cx + 36 * unit, top + 60 * unit, 16 * unit, 62 * unit), shirt * 0.94f, 8 * unit);
  ui::rect(Rect(cx - 32 * unit, top + 126 * unit, 30 * unit, 58 * unit), pants, 10 * unit);
  ui::rect(Rect(cx + 2 * unit, top + 126 * unit, 30 * unit, 58 * unit), pants, 10 * unit);
  ui::rect(Rect(cx - 34 * unit, top + 182 * unit, 32 * unit, 12 * unit), Color::hex(0xE6E9F2), 6 * unit);
  ui::rect(Rect(cx + 2 * unit, top + 182 * unit, 32 * unit, 12 * unit), Color::hex(0xE6E9F2), 6 * unit);
}

}  // namespace

namespace store {

void enter() {
  gLoaded = false;
  gScroll.offset = 0;
  reload();
}

void leave() {}

void draw(float dt) {
  Engine& e = Engine::get();
  float w = e.width();
  float pad = ui::dp(16);
  float innerW = w - pad * 2;
  background(dt);

  HeaderOpts ho;
  ho.title = tr("المتجر", "Store");
  header(ho);
  float top = ui::dp(74);
  float navH = ui::dp(72) + e.safeBottom();

  Rect preview(pad, top, ui::dp(120), ui::dp(180));
  drawAvatarPreview(preview);
  ui::text(tr("المعاينة", "Preview"), preview.x, preview.y + preview.h + ui::dp(4), 10, ui::theme.textMuted,
           preview.w, 1, 1);

  float rx = preview.x + preview.w + ui::dp(14);
  float rw = pad + innerW - rx;
  ui::text(tr("رصيدك", "Your balance"), rx, top + ui::dp(6), 11, ui::theme.textDim, rw, 0, 1);
  char bal[40];
  snprintf(bal, sizeof bal, "%d", S.miy);
  ui::text(bal, rx, top + ui::dp(24), 26, ui::theme.text, rw - ui::dp(28), 0, 2);
  ui::icon("miy", Rect(rx + rw - ui::dp(24), top + ui::dp(26), ui::dp(20), ui::dp(20)), ui::theme.gold);
  ui::text(tr("تكسب 5 Miy لكل دقيقة لعب داخل المابات", "Earn 5 Miy per minute playing in maps"), rx,
           top + ui::dp(62), 10, ui::theme.textMuted, rw, 0, 1);

  Rect ledger(rx, top + ui::dp(92), rw * 0.48f, ui::dp(36));
  if (ui::button("store_ledger", ledger, tr("السجل", "Ledger"), false, true, 11).clicked) {
    gDrawer = gDrawer == 1 ? 0 : 1;
    if (gDrawer == 1 && !gLedgerLoaded) api.call(NET_LEDGER, "GET", "/api/economy/ledger?limit=30", nullptr, 63);
  }
  Rect passes(rx + rw * 0.52f, top + ui::dp(92), rw * 0.48f, ui::dp(36));
  if (ui::button("store_passes", passes, tr("تصاريحي", "My passes"), false, true, 11).clicked) {
    gDrawer = gDrawer == 2 ? 0 : 2;
    if (gDrawer == 2 && !gPassesLoaded) api.call(NET_PASSES_MINE, "GET", "/api/me/passes", nullptr, 64);
  }

  float tabsY = top + ui::dp(190);
  const char* labels[5] = {tr(kTabAr[0], kTabEn[0]), tr(kTabAr[1], kTabEn[1]), tr(kTabAr[2], kTabEn[2]),
                           tr(kTabAr[3], kTabEn[3]), tr(kTabAr[4], kTabEn[4])};
  int picked = ui::segments("store_tabs", Rect(pad, tabsY, innerW, ui::dp(34)), labels, 5, gTab, 10.5f);
  if (picked >= 0) gTab = picked;

  // Ledger / passes drawer replaces the grid while open
  if (gDrawer) {
    float dy = tabsY;
    Rect dv(0, dy, w, e.height() - dy - navH);
    ui::panel(Rect(pad, dy, innerW, dv.h - ui::dp(8)), ui::dp(16), false);
    Rect close(dv.x + dv.w - pad - ui::dp(34), dy + ui::dp(10), ui::dp(28), ui::dp(28));
    if (ui::iconButton("store_drawer_x", close, "close").clicked) gDrawer = 0;
    const char* title = gDrawer == 1 ? tr("سجل الحركات", "Ledger") : tr("تصاريحي", "My passes");
    ui::text(title, pad + ui::dp(16), dy + ui::dp(14), 14, ui::theme.text, innerW - ui::dp(80), 0, 2);

    float ly = dy + ui::dp(44);
    if (gDrawer == 1) {
      if (!gLedgerLoaded) {
        ui::text(tr("جاري التحميل…", "Loading…"), 0, ly + ui::dp(20), 11.5f, ui::theme.textMuted, w, 1, 1);
      } else if (gLedger.count == 0) {
        ui::emptyState(Rect(0, ly, w, dv.h - ui::dp(60)), "miy", tr("لا حركات بعد", "No movements yet"),
                       tr("كل Miy تكسبه أو تنفقه يظهر هنا.", "Every Miy you earn or spend appears here."),
                       nullptr, nullptr);
      } else {
        int maxRows = (int)((dv.h - ui::dp(60)) / ui::dp(52));
        for (int i = 0; i < gLedger.count && i < maxRows; i++) {
          LedgerRow& r = gLedger[i];
          Rect row(pad + ui::dp(14), ly + i * ui::dp(52), innerW - ui::dp(28), ui::dp(46));
          ui::rect(row, Color(1, 1, 1, 0.035f), ui::dp(10));
          char amt[32];
          snprintf(amt, sizeof amt, "%s%d", r.delta >= 0 ? "+" : "", r.delta);
          ui::text(amt, row.x + ui::dp(12), row.y + ui::dp(14), 13,
                   r.delta >= 0 ? ui::theme.green : ui::theme.red, ui::dp(70), 0, 2);
          ui::text(reasonLabel(r.reason), row.x + ui::dp(88), row.y + ui::dp(8), 11.5f, ui::theme.text,
                   row.w - ui::dp(170), 0, 1);
          char when[32];
          snprintf(when, sizeof when, "%d", r.balance);
          ui::text(when, row.x + ui::dp(88), row.y + ui::dp(25), 10, ui::theme.textMuted, row.w - ui::dp(170), 0, 1);
        }
      }
    } else {
      if (!gPassesLoaded) {
        ui::text(tr("جاري التحميل…", "Loading…"), 0, ly + ui::dp(20), 11.5f, ui::theme.textMuted, w, 1, 1);
      } else if (gPasses.count == 0) {
        ui::emptyState(Rect(0, ly, w, dv.h - ui::dp(60)), "star", tr("لا تصاريح بعد", "No passes yet"),
                       tr("تصاريح المابات تُشترى من صفحة الماب.", "Map passes are bought from a map's page."),
                       nullptr, nullptr);
      } else {
        int maxRows = (int)((dv.h - ui::dp(60)) / ui::dp(56));
        for (int i = 0; i < gPasses.count && i < maxRows; i++) {
          PassRow& r = gPasses[i];
          Rect row(pad + ui::dp(14), ly + i * ui::dp(56), innerW - ui::dp(28), ui::dp(50));
          ui::rect(row, Color(1, 1, 1, 0.035f), ui::dp(10));
          ui::text(r.name, row.x + ui::dp(12), row.y + ui::dp(8), 12.5f, ui::theme.text, row.w - ui::dp(120), 0, 2);
          char meta[64];
          snprintf(meta, sizeof meta, "%s %d  •  %d %s", tr("دفعت", "Paid"), r.price, r.perks,
                   tr("ميزة", "perks"));
          ui::text(meta, row.x + ui::dp(12), row.y + ui::dp(28), 10, ui::theme.textMuted, row.w - ui::dp(120), 0, 1);
          Rect open(row.x + row.w - ui::dp(96), row.y + ui::dp(10), ui::dp(84), ui::dp(30));
          if (ui::button("store_pass_open", open, tr("افتح الماب", "Open map"), false, true, 10.5f).clicked) {
            gDrawer = 0;
            openMap(r.mapId);
          }
        }
      }
    }
    bottomNav(0);
    return;
  }

  Rect view(0, tabsY + ui::dp(46), w, e.height() - tabsY - ui::dp(46) - navH);
  float cellW = (innerW - ui::dp(8)) * 0.5f;
  float cellH = ui::dp(150);
  int visible = 0;
  for (int i = 0; i < gItems.count; i++) {
    if (gTab > 0 && strcmp(gItems[i].category, kCategories[gTab]) != 0) continue;
    visible++;
  }
  int rows = (visible + 1) / 2;
  gScroll.contentH = rows * (cellH + ui::dp(8)) + ui::dp(16);
  gScroll.update(view, dt, true);
  Rect content = gScroll.content(view);

  if (!gLoaded) {
    for (int i = 0; i < 4; i++) {
      Rect sk(pad + (i % 2) * (cellW + ui::dp(8)), content.y + (i / 2) * (cellH + ui::dp(8)), cellW, cellH);
      ui::rect(sk, Color(1, 1, 1, 0.035f), ui::dp(14));
    }
  } else if (visible == 0) {
    ui::emptyState(Rect(0, view.y, w, view.h), "bag", tr("لا عناصر في هذا القسم", "Nothing in this section"),
                   tr("جرّب قسماً آخر — العناصر الجديدة تظهر هنا فور إضافتها.",
                      "Try another category — new items appear here as soon as they ship."),
                   nullptr, nullptr);
  } else {
    int idx = 0;
    for (int i = 0; i < gItems.count; i++) {
      if (gTab > 0 && strcmp(gItems[i].category, kCategories[gTab]) != 0) continue;
      Item& it = gItems[i];
      Rect c(pad + (idx % 2) * (cellW + ui::dp(8)), content.y + (idx / 2) * (cellH + ui::dp(8)), cellW, cellH);
      idx++;
      if (c.y + c.h < view.y - ui::dp(30) || c.y > view.y + view.h + ui::dp(30)) continue;
      ui::card(c, false, it.rarity[0] && strcmp(it.rarity, "common") != 0 ? ui::theme.gold : Color(0, 0, 0, 0));
      Rect swatch(c.x + ui::dp(10), c.y + ui::dp(10), c.w - ui::dp(20), ui::dp(62));
      Color col = itemColor(it);
      ui::verticalGradient(swatch, col.withA(0.9f), col * 0.35f);
      ui::rectStroke(swatch, Color(1, 1, 1, 0.10f), 1.0f, ui::dp(10));
      ui::text(it.name, c.x + ui::dp(10), c.y + ui::dp(80), 11.5f, ui::theme.text, c.w - ui::dp(20), 0, 2);
      if (strcmp(it.rarity, "common") != 0) {
        ui::tag(Rect(c.x + ui::dp(10), c.y + ui::dp(98), ui::dp(52), ui::dp(18)), it.rarity, ui::theme.gold);
      }
      char price[40];
      if (it.owned) snprintf(price, sizeof price, "%s", it.equipped ? tr("مُلبوس", "Equipped") : tr("مملوك", "Owned"));
      else snprintf(price, sizeof price, "%d", it.price);
      Rect buy(c.x + ui::dp(10), c.y + c.h - ui::dp(38), c.w - ui::dp(20), ui::dp(30));
      bool primary = !it.owned;
      const char* label = it.owned ? (it.equipped ? tr("مُلبوس", "Equipped") : tr("البس", "Wear")) : price;
      if (ui::button(it.id, buy, label, primary, !it.equipped, 11).clicked) {
        if (it.owned) {
          char body[160];
          snprintf(body, sizeof body, "{\"itemId\":\"%s\"}", it.id);
          api.call(NET_EQUIP, "POST", "/api/inventory/equip", body, 66);
        } else if (it.price > S.miy) {
          snprintf(gBuyMsg, sizeof gBuyMsg, "%s", tr("رصيدك غير كافٍ — العب داخل ماب لكسب Miy",
                                                     "Not enough Miy — play in a map to earn more"));
          gBuyError = 1;
          audioPlay("ui_error", 0.6f);
        } else {
          char body[160];
          snprintf(body, sizeof body, "{\"itemId\":\"%s\"}", it.id);
          api.call(NET_SHOP_BUY, "POST", "/api/shop/buy", body, 65);
        }
      }
    }
  }

  if (gBuyError && gBuyMsg[0]) {
    ui::rect(Rect(pad, e.height() - navH - ui::dp(46), innerW, ui::dp(40)), ui::theme.red.withA(0.14f), ui::dp(12));
    ui::text(gBuyMsg, pad + ui::dp(12), e.height() - navH - ui::dp(34), 11, ui::theme.red, innerW - ui::dp(24), 0, 1);
  }

  bottomNav(0);
}

}  // namespace store

void storeApi(const ApiEvent& ev) {
  if (ev.op == (int)NET_SHOP) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    // inventory arrives separately and carries ownership; keep shop prices/names
    Array<Item> fresh;
    JsonValue* items = Json::get(j.root(), "items");
    for (int i = 0; i < Json::length(items); i++) {
      JsonValue* m = Json::at(items, i);
      Item it{};
      snprintf(it.id, sizeof it.id, "%s", Json::string(Json::get(m, "id")));
      // shop items ship bilingual names; the shop card follows the UI language
      const char* nm = S.language == 0 ? Json::string(Json::get(m, "nameAr"))
                                       : Json::string(Json::get(m, "nameEn"));
      if (!nm[0]) nm = Json::string(Json::get(m, "name"));
      snprintf(it.name, sizeof it.name, "%s", nm);
      snprintf(it.category, sizeof it.category, "%s", Json::string(Json::get(m, "category"), "shirt"));
      snprintf(it.rarity, sizeof it.rarity, "%s", Json::string(Json::get(m, "rarity"), "common"));
      it.price = Json::integer(Json::get(m, "price"));
      fresh.push(it);
    }
    gItems.clear();
    for (int i = 0; i < fresh.count; i++) gItems.push(fresh[i]);
    gLoaded = true;
    return;
  }
  if (ev.op == (int)NET_INVENTORY) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    JsonValue* inv = Json::get(j.root(), "items");
    for (int i = 0; i < gItems.count; i++) {
      gItems[i].owned = false;
      gItems[i].equipped = false;
    }
    for (int i = 0; i < Json::length(inv); i++) {
      JsonValue* m = Json::at(inv, i);
      const char* id = Json::string(Json::get(m, "itemId"), Json::string(Json::get(m, "id")));
      bool equipped = Json::boolean(Json::get(m, "equipped"));
      for (int k = 0; k < gItems.count; k++) {
        if (strcmp(gItems[k].id, id) == 0) {
          gItems[k].owned = true;
          gItems[k].equipped = equipped;
        }
      }
    }
    return;
  }
  if (ev.op == (int)NET_SHOP_BUY) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    if (ev.status == 200) {
      gBuyError = 0;
      gBuyMsg[0] = 0;
      ui::toast(tr("تم الشراء", "Purchased"));
      audioPlay("coin_gain", 0.7f);
      S.miy = Json::integer(Json::get(j.root(), "balance"), S.miy);
      saveLocalSettings();
      store::enter();
    } else {
      gBuyError = 1;
      snprintf(gBuyMsg, sizeof gBuyMsg, "%s", apiMessage(j.root(), tr("تعذر الشراء", "Purchase failed")));
    }
    return;
  }
  if (ev.op == (int)NET_EQUIP) {
    Json j;
    if (ev.body) j.parse(ev.body);
    if (ev.status == 200) {
      ui::toast(tr("تم التجهيز", "Equipped"));
      api.call(NET_INVENTORY, "GET", "/api/inventory", nullptr, 62);
    } else if (ev.body) {
      toastError(j.root(), tr("تعذر التجهيز", "Could not equip"));
    }
    return;
  }
  if (ev.op == (int)NET_LEDGER) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    if (ev.status != 200) {
      ui::toast(apiMessage(j.root(), tr("تعذر تحميل السجل", "Could not load the ledger")), 3.0f);
      return;
    }
    gLedger.clear();
    JsonValue* rows = Json::get(j.root(), "entries");
    for (int i = 0; i < Json::length(rows); i++) {
      JsonValue* t = Json::at(rows, i);
      LedgerRow r{};
      r.delta = Json::integer(Json::get(t, "delta"));
      r.balance = Json::integer(Json::get(t, "balance"));
      snprintf(r.reason, sizeof r.reason, "%s", Json::string(Json::get(t, "reason"), ""));
      r.at = Json::integer(Json::get(t, "at"));
      gLedger.push(r);
    }
    gLedgerLoaded = true;
    return;
  }
  if (ev.op == (int)NET_PASSES_MINE) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    if (ev.status != 200) {
      ui::toast(apiMessage(j.root(), tr("تعذر تحميل التصاريح", "Could not load your passes")), 3.0f);
      return;
    }
    gPasses.clear();
    JsonValue* rows = Json::get(j.root(), "passes");
    for (int i = 0; i < Json::length(rows); i++) {
      JsonValue* t = Json::at(rows, i);
      PassRow r{};
      snprintf(r.name, sizeof r.name, "%s", Json::string(Json::get(t, "name")));
      snprintf(r.mapId, sizeof r.mapId, "%s", Json::string(Json::get(t, "mapId")));
      r.price = Json::integer(Json::get(t, "price"));
      r.perks = Json::length(Json::get(t, "perks"));
      r.at = Json::integer(Json::get(t, "purchasedAt"));
      gPasses.push(r);
    }
    gPassesLoaded = true;
    return;
  }
}

}  // namespace screens
}  // namespace app
}  // namespace nx
