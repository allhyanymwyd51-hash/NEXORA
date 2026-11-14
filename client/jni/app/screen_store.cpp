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
    api.call(NET_LEDGER, "GET", "/api/economy/ledger?limit=30", nullptr, 63);
  }
  Rect passes(rx + rw * 0.52f, top + ui::dp(92), rw * 0.48f, ui::dp(36));
  if (ui::button("store_passes", passes, tr("تصاريحي", "My passes"), false, true, 11).clicked) {
    api.call(NET_PASSES_MINE, "GET", "/api/me/passes", nullptr, 64);
  }

  float tabsY = top + ui::dp(190);
  const char* labels[5] = {tr(kTabAr[0], kTabEn[0]), tr(kTabAr[1], kTabEn[1]), tr(kTabAr[2], kTabEn[2]),
                           tr(kTabAr[3], kTabEn[3]), tr(kTabAr[4], kTabEn[4])};
  int picked = ui::segments("store_tabs", Rect(pad, tabsY, innerW, ui::dp(34)), labels, 5, gTab, 10.5f);
  if (picked >= 0) gTab = picked;

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
      snprintf(it.name, sizeof it.name, "%s", Json::string(Json::get(m, "name")));
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
  if (ev.op == (int)NET_LEDGER || ev.op == (int)NET_PASSES_MINE) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    int n = Json::length(Json::get(j.root(), "transactions")) + Json::length(Json::get(j.root(), "passes"));
    char msg[96];
    snprintf(msg, sizeof msg, "%s: %d", tr("عناصر مسجلة", "Records"), n);
    ui::toast(msg, 2.5f);
    return;
  }
}

}  // namespace screens
}  // namespace app
}  // namespace nx
