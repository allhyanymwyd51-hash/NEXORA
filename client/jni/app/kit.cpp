// Shared screen chrome: background, header, bottom navigation, typography helpers.
#include "core/audio.h"
#include "screens.h"

namespace nx {
namespace app {
namespace screens {

static const char* kNavIcons[5] = {"home", "compass", "plus", "users", "user"};
static const char* kNavAr[5] = {"الرئيسية", "اكتشف", "أنشئ", "الأصدقاء", "حسابي"};
static const char* kNavEn[5] = {"Home", "Discover", "Create", "Friends", "Me"};

const char* tr(const char* ar, const char* en) { return S.language == 0 ? ar : en; }

const char* apiMessage(JsonValue* root, const char* fallback) {
  const char* m = Json::string(Json::get(root, "message"), "");
  return m && *m ? m : fallback;
}

bool parseOk(JsonValue* root) { return Json::boolean(Json::get(root, "ok"), false); }

void toastError(JsonValue* root, const char* fallback) {
  const char* msg = apiMessage(root, fallback);
  ui::toast(msg, 3.0f);
  audioPlay("ui_error", 0.6f);
}

void openMap(int mapId) {
  setBusy(true, tr("جاري فتح الماب…", "Opening map…"));
  char path[64];
  snprintf(path, sizeof path, "/api/maps/%d", mapId);
  api.call(NET_MAP_DETAIL, "GET", path, nullptr, mapId);
}

// ---------------------------------------------------------------------------
// background: layered gradient + soft brand bloom
// ---------------------------------------------------------------------------
void background(float dt) {
  Engine& e = Engine::get();
  static float t = 0;
  t += dt;
  float w = e.width(), h = e.height();
  ui::verticalGradient(Rect(0, 0, w, h), ui::theme.bg1, ui::theme.bg0);
  // two slow drifting light pools keep the flat dark surfaces alive
  float cx = w * (0.72f + 0.04f * sinf(t * 0.11f));
  float cy = h * (0.18f + 0.03f * cosf(t * 0.14f));
  float r = fmaxf(w, h) * 0.55f;
  ui::glow(Rect(cx - r, cy - r, r * 2, r * 2), ui::theme.violet.withA(0.16f), 1.0f);
  float cx2 = w * (0.16f + 0.03f * cosf(t * 0.09f));
  float cy2 = h * (0.86f + 0.02f * sinf(t * 0.12f));
  ui::glow(Rect(cx2 - r * 0.8f, cy2 - r * 0.8f, r * 1.6f, r * 1.6f), ui::theme.cyan.withA(0.09f), 1.0f);
}

// ---------------------------------------------------------------------------
// header
// ---------------------------------------------------------------------------
Rect header(const HeaderOpts& opts) {
  Engine& e = Engine::get();
  float w = e.width();
  float top = ui::dp(8);
  float hgt = ui::dp(54);
  Rect bar(0, top, w, hgt);
  // frosted strip so content scrolling underneath stays readable
  ui::rect(Rect(0, 0, w, top + hgt), Color(0.035f, 0.05f, 0.11f, 0.86f), 0);
  ui::divider(0, w, top + hgt, Color(1, 1, 1, 0.05f));

  float x = ui::dp(14);
  if (opts.backButton) {
    Rect back(x, top + ui::dp(9), ui::dp(38), ui::dp(38));
    if (ui::iconButton("hdr_back", back, S.language == 0 ? "chevron_right" : "chevron_left").clicked) {
      audioPlay("ui_back", 0.4f);
      goBack();
    }
    x += ui::dp(46);
  } else if (opts.closeButton) {
    Rect close(x, top + ui::dp(9), ui::dp(38), ui::dp(38));
    if (ui::iconButton("hdr_close", close, "close").clicked) goBack();
    x += ui::dp(46);
  } else {
    // brand mark instead of a back arrow on root screens
    Rect mark(x, top + ui::dp(11), ui::dp(32), ui::dp(32));
    ui::rect(mark, Color(0.10f, 0.09f, 0.24f, 0.9f), ui::dp(9));
    ui::image(mark.inset(ui::dp(6)), ui::theme.cyan, 0.95f);
    x += ui::dp(42);
  }

  float rightPad = ui::dp(14);
  float walletW = ui::dp(104);
  if (opts.wallet) {
    Rect wallet(w - rightPad - walletW, top + ui::dp(11), walletW, ui::dp(32));
    ui::rect(wallet, Color(0.09f, 0.10f, 0.20f, 0.92f), ui::dp(16));
    ui::rectStroke(wallet, ui::theme.gold.withA(0.28f), 1.0f, ui::dp(16));
    ui::icon("miy", Rect(wallet.x + ui::dp(7), wallet.y + ui::dp(8), ui::dp(16), ui::dp(16)), ui::theme.gold);
    char buf[32];
    snprintf(buf, sizeof buf, "%d", S.miy);
    ui::text(buf, wallet.x + ui::dp(29), wallet.y + ui::dp(8), 13, ui::theme.text, wallet.w - ui::dp(34), 2, 2);
    rightPad += walletW + ui::dp(8);
  }
  if (S.unread > 0 || opts.wallet) {
    Rect bell(w - rightPad - ui::dp(36), top + ui::dp(9), ui::dp(36), ui::dp(36));
    if (ui::iconButton("hdr_bell", bell, "bell", ui::theme.textDim, S.unread > 0).clicked) go(Screen::Notifications);
  }
  ui::text(opts.title, x, top + ui::dp(16), 16, ui::theme.text, w - x - rightPad - ui::dp(4),
           S.language == 0 ? 2 : 0, 2);
  return bar;
}

// ---------------------------------------------------------------------------
// bottom navigation
// ---------------------------------------------------------------------------
float bottomNav(float selected) {
  Engine& e = Engine::get();
  float w = e.width(), h = e.height();
  float navH = ui::dp(62);
  float y = h - navH - e.safeBottom();
  ui::rect(Rect(0, y - ui::dp(10), w, navH + ui::dp(10) + e.safeBottom()), Color(0.04f, 0.055f, 0.12f, 0.97f), 0);
  ui::divider(0, w, y - ui::dp(10), Color(1, 1, 1, 0.06f));
  float slot = w / 5.0f;
  for (int i = 0; i < 5; i++) {
    float cx = slot * i + slot * 0.5f;
    bool active = fabsf(selected - i) < 0.01f;
    Rect hit(cx - slot * 0.5f, y, slot, navH);
    // the centre slot is the primary action: a raised violet pill
    if (i == 2) {
      Rect pill(cx - ui::dp(25), y + ui::dp(4), ui::dp(50), ui::dp(38));
      ui::glow(Rect(pill.x - ui::dp(8), pill.y - ui::dp(8), pill.w + ui::dp(16), pill.h + ui::dp(16)),
               ui::theme.violet.withA(0.36f), 1.0f);
      ui::rect(pill, active ? ui::theme.violet : ui::theme.violetDim, ui::dp(19));
      ui::icon("plus", Rect(pill.x + ui::dp(15), pill.y + ui::dp(11), ui::dp(20), ui::dp(20)), Color(1, 1, 1, 0.96f));
      ui::text(tr(kNavAr[i], kNavEn[i]), cx, y + ui::dp(44), 9.5f,
               active ? ui::theme.text : ui::theme.textMuted, slot, 1, 1);
    } else {
      Color tint = active ? ui::theme.violet : ui::theme.textMuted;
      ui::icon(kNavIcons[i], Rect(cx - ui::dp(11), y + ui::dp(10), ui::dp(22), ui::dp(22)), tint);
      ui::text(tr(kNavAr[i], kNavEn[i]), cx, y + ui::dp(37), 9.5f, tint, slot, 1, active ? 2 : 1);
      if (active) ui::rect(Rect(cx - ui::dp(9), y + ui::dp(2), ui::dp(18), ui::dp(2.4f)), ui::theme.violet, ui::dp(1));
    }
    ui::Pointer& p = ui::pointer();
    bool inRect = hit.contains(p.pos.x, p.pos.y);
    if (inRect && p.released && p.start.distance(p.pos) < ui::dp(22)) {
      audioPlay("ui_click", 0.45f);
      Screen targets[5] = {Screen::Home, Screen::Discover, Screen::CreateAi, Screen::Friends, Screen::Profile};
      if (current() != targets[i]) go(targets[i]);
    }
  }
  return navH + ui::dp(10) + e.safeBottom();
}

void sectionTitle(const char* label, float x, float y, float w, bool action, const char* actionLabel) {
  ui::text(label, x, y, 15, ui::theme.text, w, S.language == 0 ? 2 : 0, 2);
  if (action && actionLabel) {
    float aw = ui::textMeasure(actionLabel, 12, 0, 1);
    float ax = S.language == 0 ? x + ui::dp(4) : x + w - aw;
    ui::text(actionLabel, ax, y + ui::dp(2), 12, ui::theme.cyan, 0, 0, 1);
  }
}

}  // namespace screens
}  // namespace app
}  // namespace nx
