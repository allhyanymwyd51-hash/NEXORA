// Account surfaces: public profile, settings, notifications and the owner console.
#include "core/audio.h"
#include "screens.h"
#include "widgets.h"
#include "ws.h"

namespace nx {
namespace app {
namespace screens {

namespace {

struct ProfileData {
  char id[48];
  char username[64];
  char bio[200];
  int miy, playSeconds, maps, friends;
  bool following;
  bool isSelf;
};
ProfileData gProfile;
bool gProfileLoaded = false;

struct NotifRow {
  char id[48];
  char title[120];
  char body[240];
  int at;
  bool read;
};
Array<NotifRow> gNotifs;
int gUnread = 0;
bool gNotifsLoaded = false;

bool gSettingsLoaded = false;

// owner console
struct OwnerStats {
  int users, maps, published, activeSessions, revenueToday;
  int aiJobsRunning;
};
OwnerStats gStats;
struct UserRow {
  char id[48];
  char username[64];
};
Array<UserRow> gUserRows;
char gUserQuery[48] = {0};
char gBroadcast[240] = {0};
bool gOwnerLoaded = false;

ui::ScrollView gScroll;

float relTime(int epoch) {
  if (epoch <= 0) return 0;
  // device clock seconds; both sides are UTC epoch seconds
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (float)((long)ts.tv_sec - epoch);
}

void relLabel(int epoch, char* out, size_t n) {
  float s = relTime(epoch);
  if (s < 60) snprintf(out, n, "%s", tr("الآن", "now"));
  else if (s < 3600) snprintf(out, n, "%d %s", (int)(s / 60), tr("د", "m"));
  else if (s < 86400) snprintf(out, n, "%d %s", (int)(s / 3600), tr("س", "h"));
  else snprintf(out, n, "%d %s", (int)(s / 86400), tr("ي", "d"));
}

}  // namespace

// ===========================================================================
// profile
// ===========================================================================
namespace profile {

void enter() {
  gProfileLoaded = false;
  if (S.uid[0]) {
    char path[80];
    snprintf(path, sizeof path, "/api/users/%s", S.uid);
    api.call(NET_USER_PROFILE, "GET", path, nullptr, 91);
  }
  if (S.uid[0]) api.call(NET_USER_MAPS, "GET", "/api/maps/favorites", nullptr, 92);
}

void leave() {}

void draw(float dt) {
  Engine& e = Engine::get();
  float w = e.width();
  float pad = ui::dp(16);
  float innerW = w - pad * 2;
  background(dt);

  HeaderOpts ho;
  ho.title = tr("ملفي", "Profile");
  ho.backButton = true;
  header(ho);
  float top = ui::dp(74);
  float navH = ui::dp(72) + e.safeBottom();
  gScroll.viewH = e.height() - top - navH;
  gScroll.contentH = ui::dp(520);
  gScroll.update(Rect(0, top, w, gScroll.viewH), dt, true);
  Rect content = gScroll.content(Rect(0, top, w, gScroll.viewH));
  float y = content.y + ui::dp(10);

  // identity card
  Rect card(pad, y, innerW, ui::dp(132));
  ui::card(card, true);
  ui::glow(Rect(card.x - ui::dp(20), card.y - ui::dp(30), card.w * 0.6f, card.h + ui::dp(60)),
           ui::theme.violet.withA(0.16f), 1.0f);
  Rect av(card.x + ui::dp(16), card.y + ui::dp(18), ui::dp(64), ui::dp(64));
  ui::rect(av, ui::theme.violet.withA(0.3f), ui::dp(32));
  char initial[2] = {S.username[0] ? S.username[0] : '?', 0};
  ui::text(initial, av.x, av.y + ui::dp(18), 24, ui::theme.text, av.w, 1, 2);
  ui::text(gProfileLoaded ? gProfile.username : S.username, av.x + av.w + ui::dp(14), card.y + ui::dp(20), 18,
           ui::theme.text, card.w - ui::dp(120), 0, 2);
  if (gProfileLoaded) {
    ui::text(gProfile.bio[0] ? gProfile.bio : tr("لا نبذة بعد", "No bio yet"), av.x + av.w + ui::dp(14),
             card.y + ui::dp(46), 11, ui::theme.textDim, card.w - ui::dp(120), 0, 1);
  }
  char stats[128];
  snprintf(stats, sizeof stats, "%d Miy  •  %d %s  •  %d %s", gProfileLoaded ? gProfile.miy : S.miy,
           gProfileLoaded ? gProfile.playSeconds / 60 : 0, tr("دقيقة لعب", "min played"),
           gProfileLoaded ? gProfile.maps : 0, tr("ماب", "maps"));
  ui::text(stats, av.x + av.w + ui::dp(14), card.y + ui::dp(70), 11.5f, ui::theme.gold, card.w - ui::dp(120), 0, 1);
  Rect editBtn(card.x + ui::dp(16), card.y + ui::dp(94), innerW - ui::dp(32), ui::dp(30));
  if (ui::button("pf_edit", editBtn, tr("تعديل الملف", "Edit profile"), false, true, 11.5f).clicked) {
    go(Screen::Settings);
  }
  y += ui::dp(146);

  // quick tiles
  float tw = (innerW - ui::dp(16)) / 3.0f;
  const char* tiles[3][2] = {{"الاستوديو", "Studio"}, {"المتجر", "Store"}, {"الإشعارات", "Alerts"}};
  Screen targets[3] = {Screen::Studio, Screen::Store, Screen::Notifications};
  for (int i = 0; i < 3; i++) {
    Rect t(pad + i * (tw + ui::dp(8)), y, tw, ui::dp(74));
    ui::card(t, false);
    ui::icon(i == 0 ? "gear" : i == 1 ? "bag" : "bell", Rect(t.x + ui::dp(12), t.y + ui::dp(12), ui::dp(22), ui::dp(22)),
             i == 0 ? ui::theme.cyan : i == 1 ? ui::theme.gold : ui::theme.violet);
    ui::text(tr(tiles[i][0], tiles[i][1]), t.x + ui::dp(12), t.y + ui::dp(44), 11.5f, ui::theme.text, tw - ui::dp(24),
             0, 2);
    ui::Pointer& p = ui::pointer();
    if (t.contains(p.pos.x, p.pos.y) && p.released && p.start.distance(p.pos) < ui::dp(20)) {
      go(targets[i]);
    }
  }
  y += ui::dp(88);

  sectionTitle(tr("الحساب", "Account"), pad, y, innerW);
  y += ui::dp(30);
  const char* rows[4][2] = {{"الإعدادات", "Settings"}, {"الأصدقاء", "Friends"}, {"الاستحواذ والشروط", "Terms"},
                            {"تسجيل الخروج", "Sign out"}};
  for (int i = 0; i < 4; i++) {
    Rect r(pad, y, innerW, ui::dp(48));
    ui::card(r, false);
    ui::text(tr(rows[i][0], rows[i][1]), r.x + ui::dp(14), r.y + ui::dp(14), 12.5f,
             i == 3 ? ui::theme.red : ui::theme.text, r.w - ui::dp(40), 0, 1);
    ui::icon("chevron_left", Rect(r.x + r.w - ui::dp(28), r.y + ui::dp(14), ui::dp(18), ui::dp(18)),
             ui::theme.textMuted);
    ui::Pointer& p = ui::pointer();
    if (r.contains(p.pos.x, p.pos.y) && p.released && p.start.distance(p.pos) < ui::dp(20)) {
      if (i == 0) go(Screen::Settings);
      else if (i == 1) go(Screen::Friends);
      else if (i == 3) {
        api.call(NET_LOGOUT, "POST", "/api/auth/logout", "{}", 99);
        signOut();
      }
    }
    y += ui::dp(54);
  }

  bottomNav(4);
}

}  // namespace profile

// ===========================================================================
// settings
// ===========================================================================
namespace settings {

void enter() {
  gSettingsLoaded = false;
  // settings travel inside /auth/me; the dedicated route only accepts PATCH
  refreshMe();
}

void leave() {}

void draw(float dt) {
  Engine& e = Engine::get();
  float w = e.width();
  float pad = ui::dp(16);
  float innerW = w - pad * 2;
  background(dt);

  HeaderOpts ho;
  ho.title = tr("الإعدادات", "Settings");
  ho.backButton = true;
  header(ho);
  float top = ui::dp(74);
  float navH = ui::dp(72) + e.safeBottom();
  gScroll.viewH = e.height() - top - navH;
  gScroll.contentH = ui::dp(560);
  gScroll.update(Rect(0, top, w, gScroll.viewH), dt, true);
  float y = gScroll.content(Rect(0, top, w, gScroll.viewH)).y + ui::dp(8);

  sectionTitle(tr("اللغة", "Language"), pad, y, innerW);
  y += ui::dp(30);
  const char* langs[2] = {"العربية", "English"};
  int lr = ui::segments("set_lang", Rect(pad, y, innerW, ui::dp(36)), langs, 2, S.language, 12);
  if (lr >= 0 && lr != S.language) {
    S.language = lr;
    saveLocalSettings();
    char body[64];
    snprintf(body, sizeof body, "{\"language\":\"%s\"}", lr == 0 ? "ar" : "en");
    api.call(NET_SETTINGS, "PATCH", "/api/me/settings", body, 96);
  }
  y += ui::dp(50);

  sectionTitle(tr("الصوت", "Audio"), pad, y, innerW);
  y += ui::dp(30);
  struct ToggleRow {
    const char* ar;
    const char* en;
    bool* value;
  };
  ToggleRow toggles[3] = {{"المؤثرات الصوتية", "Sound effects", &S.sfx},
                          {"الموسيقى", "Music", &S.music},
                          {"ميكروفون الماب", "Map microphone", &S.voice}};
  for (int i = 0; i < 3; i++) {
    Rect r(pad, y, innerW, ui::dp(50));
    ui::card(r, false);
    ui::text(tr(toggles[i].ar, toggles[i].en), r.x + ui::dp(14), r.y + ui::dp(16), 12.5f, ui::theme.text,
             r.w - ui::dp(90), 0, 1);
    bool on = *toggles[i].value;
    Rect sw(r.x + r.w - ui::dp(62), r.y + ui::dp(14), ui::dp(48), ui::dp(24));
    ui::rect(sw, on ? ui::theme.violet : Color(1, 1, 1, 0.10f), ui::dp(12));
    ui::rect(Rect(on ? sw.x + sw.w - ui::dp(21) : sw.x + ui::dp(3), sw.y + ui::dp(3), ui::dp(18), ui::dp(18)),
             Color(1, 1, 1, 1), ui::dp(9));
    ui::Pointer& p = ui::pointer();
    if (sw.contains(p.pos.x, p.pos.y) && p.released && p.start.distance(p.pos) < ui::dp(20)) {
      *toggles[i].value = !on;
      audioPlay("ui_click", 0.4f);
      char body[128];
      if (i == 2) snprintf(body, sizeof body, "{\"voiceChat\":%s}", *toggles[i].value ? "true" : "false");
      else if (i == 1) snprintf(body, sizeof body, "{\"music\":%.2f}", *toggles[i].value ? 0.6 : 0.0);
      else snprintf(body, sizeof body, "{\"sound\":%.2f}", *toggles[i].value ? 0.9 : 0.0);
      api.call(NET_SETTINGS, "PATCH", "/api/me/settings", body, 96);
      if (i == 0) audioSetMasterGain(S.sfx ? 1.0f : 0.0f);
      if (i == 1) audioSetMusicGain(S.music ? 0.6f : 0.0f);
      saveLocalSettings();
    }
    y += ui::dp(58);
  }
  y += ui::dp(6);

  sectionTitle(tr("الأداء", "Performance"), pad, y, innerW);
  y += ui::dp(30);
  const char* q[3] = {tr("أداء", "Performance"), tr("متوازن", "Balanced"), tr("جودة", "Quality")};
  int qr = ui::segments("set_quality", Rect(pad, y, innerW, ui::dp(36)), q, 3, S.quality, 12);
  if (qr >= 0 && qr != S.quality) {
    S.quality = qr;
    saveLocalSettings();
  }
  y += ui::dp(50);
  char perf[128];
  snprintf(perf, sizeof perf, "%s: %d FPS  •  %dx%d", tr("الحالي", "Now"), Engine::get().fps(),
           (int)Engine::get().width(), (int)Engine::get().height());
  ui::text(perf, pad, y, 10.5f, ui::theme.textMuted, innerW, 0, 1);
  y += ui::dp(26);

  sectionTitle(tr("الحساب", "Account"), pad, y, innerW);
  y += ui::dp(30);
  Rect emailRow(pad, y, innerW, ui::dp(44));
  ui::card(emailRow, false);
  ui::text(tr("تغيير كلمة المرور", "Change password"), emailRow.x + ui::dp(14), emailRow.y + ui::dp(13), 12, ui::theme.text,
           innerW, 0, 1);
  y += ui::dp(52);
  Rect out(pad, y, innerW, ui::dp(44));
  if (ui::button("set_logout", out, tr("تسجيل الخروج", "Sign out"), false, true, 12).clicked) {
    signOut();
  }
  y += ui::dp(56);
  ui::text(tr("NEXORA 1.0.0 — محرك عوالم بالذكاء الاصطناعي", "NEXORA 1.0.0 — AI world engine"), pad, y, 10,
           ui::theme.textMuted, innerW, 0, 1);

  bottomNav(0);
}

}  // namespace settings

// ===========================================================================
// notifications
// ===========================================================================
namespace notifications {

void enter() {
  gNotifsLoaded = false;
  api.call(NET_NOTIFICATIONS, "GET", "/api/me/notifications", nullptr, 97);
}

void leave() {}

void draw(float dt) {
  Engine& e = Engine::get();
  float w = e.width();
  float pad = ui::dp(16);
  float innerW = w - pad * 2;
  background(dt);
  HeaderOpts ho;
  ho.title = tr("الإشعارات", "Notifications");
  ho.backButton = true;
  header(ho);
  float top = ui::dp(74);
  float navH = ui::dp(72) + e.safeBottom();
  Rect view(0, top, w, e.height() - top - navH);

  if (gUnread > 0) {
    Rect mark(pad, top + ui::dp(4), innerW, ui::dp(34));
    if (ui::button("nt_read_all", mark, tr("تحديد الكل كمقروء", "Mark all as read"), false, true, 11).clicked) {
      api.call(NET_NOTIF_READ, "POST", "/api/me/notifications/read", "{}", 98);
    }
    view.y += ui::dp(42);
    view.h -= ui::dp(42);
  }

  gScroll.viewH = view.h;
  gScroll.contentH = fmaxf(view.h, gNotifs.count * ui::dp(78) + ui::dp(20));
  gScroll.update(view, dt, true);
  Rect content = gScroll.content(view);

  if (!gNotifsLoaded) {
    for (int i = 0; i < 3; i++) {
      ui::rect(Rect(pad, content.y + i * ui::dp(78), innerW, ui::dp(70)), Color(1, 1, 1, 0.035f), ui::dp(14));
    }
  } else if (gNotifs.count == 0) {
    ui::emptyState(Rect(0, view.y, w, view.h), "bell", tr("لا إشعارات", "No notifications"),
                   tr("ستصلك هنا تحديثات المابات والأصدقاء والاقتصاد.",
                      "Map, friend and economy updates land here."),
                   nullptr, nullptr);
  } else {
    for (int i = 0; i < gNotifs.count; i++) {
      NotifRow& n = gNotifs[i];
      Rect r(pad, content.y + i * ui::dp(78), innerW, ui::dp(70));
      ui::card(r, false, n.read ? Color(0, 0, 0, 0) : ui::theme.violet.withA(0.35f));
      if (!n.read) ui::rect(Rect(r.x + ui::dp(8), r.y + ui::dp(26), ui::dp(6), ui::dp(6)), ui::theme.cyan, ui::dp(3));
      ui::text(n.title, r.x + ui::dp(22), r.y + ui::dp(12), 12.5f, ui::theme.text, r.w - ui::dp(90), 0, 2);
      ui::text(n.body, r.x + ui::dp(22), r.y + ui::dp(32), 11, ui::theme.textDim, r.w - ui::dp(90), 0, 1);
      char when[16];
      relLabel(n.at, when, sizeof when);
      ui::text(when, r.x + r.w - ui::dp(56), r.y + ui::dp(12), 9.5f, ui::theme.textMuted, ui::dp(46), 2, 1);
    }
  }
  bottomNav(0);
}

}  // namespace notifications

// ===========================================================================
// owner console
// ===========================================================================
namespace owner {

void enter() {
  gOwnerLoaded = false;
  api.call(NET_ADMIN_OVERVIEW, "GET", "/api/admin/stats", nullptr, 101);
}

void leave() {}

void draw(float dt) {
  Engine& e = Engine::get();
  float w = e.width();
  float pad = ui::dp(16);
  float innerW = w - pad * 2;
  background(dt);
  HeaderOpts ho;
  ho.title = tr("لوحة المالك", "Owner console");
  ho.backButton = true;
  header(ho);
  float top = ui::dp(74);
  float navH = ui::dp(72) + e.safeBottom();
  gScroll.viewH = e.height() - top - navH;
  gScroll.contentH = ui::dp(620);
  gScroll.update(Rect(0, top, w, gScroll.viewH), dt, true);
  float y = gScroll.content(Rect(0, top, w, gScroll.viewH)).y + ui::dp(8);

  if (!gOwnerLoaded) {
    ui::text(tr("جاري تحميل البيانات…", "Loading metrics…"), 0, y + ui::dp(30), 12, ui::theme.textMuted, w, 1, 1);
    return;
  }

  float cw = (innerW - ui::dp(8)) * 0.5f;
  struct Kpi {
    const char* ar;
    const char* en;
    int value;
    Color tint;
  };
  Kpi kpis[6] = {{"المستخدمون", "Users", gStats.users, ui::theme.cyan},
                 {"المابات", "Maps", gStats.maps, ui::theme.violet},
                 {"منشورة", "Published", gStats.published, ui::theme.green},
                 {"جلسات نشطة", "Live sessions", gStats.activeSessions, ui::theme.gold},
                 {"مبيعات اليوم", "Sales today", gStats.revenueToday, ui::theme.red},
                 {"مهام AI", "AI jobs", gStats.aiJobsRunning, ui::theme.cyan}};
  for (int i = 0; i < 6; i++) {
    Rect c(pad + (i % 2) * (cw + ui::dp(8)), y + (i / 2) * ui::dp(78), cw, ui::dp(70));
    ui::card(c, false);
    char v[24];
    snprintf(v, sizeof v, "%d", kpis[i].value);
    ui::text(v, c.x + ui::dp(12), c.y + ui::dp(10), 20, kpis[i].tint, c.w - ui::dp(24), 0, 2);
    ui::text(tr(kpis[i].ar, kpis[i].en), c.x + ui::dp(12), c.y + ui::dp(42), 10.5f, ui::theme.textDim,
             c.w - ui::dp(24), 0, 1);
  }
  y += ui::dp(78) * 3 + ui::dp(10);

  sectionTitle(tr("بحث عن مستخدم", "Find a user"), pad, y, innerW);
  y += ui::dp(28);
  Rect search(pad, y, innerW, ui::dp(42));
  ui::rect(search, Color(1, 1, 1, 0.05f), ui::dp(12));
  ui::textField("own_search", search.inset(ui::dp(10)), gUserQuery, sizeof gUserQuery,
                tr("اسم المستخدم أو البريد", "Username or email"), false, 40);
  y += ui::dp(50);
  if (ui::button("own_find", Rect(pad, y, innerW, ui::dp(40)), tr("بحث", "Search"), false, true, 12).clicked &&
      gUserQuery[0]) {
    char path[160];
    snprintf(path, sizeof path, "/api/admin/users?q=%s", gUserQuery);
    api.call(NET_ADMIN_USERS, "GET", path, nullptr, 102);
  }
  y += ui::dp(52);
  for (int i = 0; i < gUserRows.count && i < 6; i++) {
    Rect r(pad, y, innerW, ui::dp(46));
    ui::card(r, false);
    ui::text(gUserRows[i].username, r.x + ui::dp(12), r.y + ui::dp(13), 12, ui::theme.text, r.w - ui::dp(120), 0, 1);
    Rect ban(r.x + r.w - ui::dp(92), r.y + ui::dp(8), ui::dp(80), ui::dp(30));
    if (ui::button("own_ban", ban, tr("حظر", "Ban"), false, true, 10.5f).clicked) {
      char path[128], body[64];
      snprintf(path, sizeof path, "/api/admin/users/%s/ban", gUserRows[i].id);
      snprintf(body, sizeof body, "{\"reason\":\"policy\"}");
      api.call(NET_ADMIN_BAN, "POST", path, body, 103);
    }
    y += ui::dp(52);
  }

  sectionTitle(tr("بث إشعار", "Broadcast"), pad, y, innerW);
  y += ui::dp(28);
  Rect bcast(pad, y, innerW, ui::dp(60));
  ui::rect(bcast, Color(1, 1, 1, 0.045f), ui::dp(12));
  ui::textField("own_bcast", bcast.inset(ui::dp(10)), gBroadcast, sizeof gBroadcast,
                tr("نص الإشعار لكل اللاعبين", "Message for every player"), false, 200);
  y += ui::dp(70);
  if (ui::button("own_send", Rect(pad, y, innerW, ui::dp(44)), tr("إرسال للجميع", "Send to everyone"), true,
                 gBroadcast[0] != 0, 12)
          .clicked) {
    char body[400];
    snprintf(body, sizeof body, "{\"title\":\"NEXORA\",\"body\":\"%s\"}", gBroadcast);
    api.call(NET_ADMIN_SETTINGS_SAVE, "POST", "/api/admin/broadcast", body, 104);
    gBroadcast[0] = 0;
  }
}

}  // namespace owner

// ===========================================================================
// API results
// ===========================================================================
void profileApi(const ApiEvent& ev) {
  if (ev.op == (int)NET_USER_PROFILE || ev.op == (int)NET_USER_MAPS) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    JsonValue* u = Json::get(j.root(), "user");
    if (!u) u = Json::get(j.root(), "profile");
    if (u) {
      snprintf(gProfile.id, sizeof gProfile.id, "%s", Json::string(Json::get(u, "id")));
      snprintf(gProfile.username, sizeof gProfile.username, "%s", Json::string(Json::get(u, "username")));
      snprintf(gProfile.bio, sizeof gProfile.bio, "%s", Json::string(Json::get(u, "bio")));
      gProfile.miy = Json::integer(Json::get(u, "miy"), S.miy);
      gProfile.playSeconds = Json::integer(Json::get(u, "playSeconds"));
      gProfile.following = Json::boolean(Json::get(u, "following"));
      gProfileLoaded = true;
    }
    gProfile.maps = Json::length(Json::get(j.root(), "maps"));
    if (gProfile.maps == 0) gProfile.maps = Json::integer(Json::get(Json::get(j.root(), "stats"), "maps"),
                                                          gProfile.maps);
    return;
  }
  if (ev.op == (int)NET_LOGOUT) {
    signOut();
    return;
  }
}

void settingsApi(const ApiEvent& ev) {
  if (ev.op == (int)NET_SETTINGS || ev.op == (int)NET_SETTINGS) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    JsonValue* s = Json::get(j.root(), "settings");
    if (!s) s = j.root();
    const char* lang = Json::string(Json::get(s, "language"), nullptr);
    if (lang) {
      int l = strcmp(lang, "ar") == 0 ? 0 : 1;
      if (l != S.language) {
        S.language = l;
        saveLocalSettings();
      }
    }
    JsonValue* v = Json::get(s, "voiceChat");
    if (v) S.voice = Json::boolean(v);
    gSettingsLoaded = true;
    return;
  }
}

void notificationsApi(const ApiEvent& ev) {
  if (ev.op == (int)NET_NOTIFICATIONS) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    gNotifs.clear();
    JsonValue* arr = Json::get(j.root(), "notifications");
    for (int i = 0; i < Json::length(arr); i++) {
      JsonValue* n = Json::at(arr, i);
      NotifRow r{};
      snprintf(r.id, sizeof r.id, "%s", Json::string(Json::get(n, "id")));
      snprintf(r.title, sizeof r.title, "%s", Json::string(Json::get(n, "title")));
      snprintf(r.body, sizeof r.body, "%s", Json::string(Json::get(n, "body")));
      r.at = Json::integer(Json::get(n, "createdAt"));
      r.read = Json::boolean(Json::get(n, "read"));
      gNotifs.push(r);
    }
    gUnread = Json::integer(Json::get(j.root(), "unread"), 0);
    if (!Json::get(j.root(), "unread")) {
      int unread = 0;
      for (int i = 0; i < gNotifs.count; i++) if (!gNotifs[i].read) unread++;
      gUnread = unread;
    }
    S.unread = gUnread;
    gNotifsLoaded = true;
    return;
  }
  if (ev.op == (int)NET_NOTIF_READ && ev.status == 200) {
    for (int i = 0; i < gNotifs.count; i++) gNotifs[i].read = true;
    gUnread = 0;
    S.unread = 0;
    ui::toast(tr("تم تحديد الكل كمقروء", "All marked as read"));
    return;
  }
}

void ownerApi(const ApiEvent& ev) {
  if (ev.op == (int)NET_ADMIN_OVERVIEW) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    JsonValue* st = Json::get(j.root(), "stats");
    if (!st) st = j.root();
    gStats.users = Json::integer(Json::get(st, "users"));
    gStats.maps = Json::integer(Json::get(st, "maps"));
    gStats.published = Json::integer(Json::get(st, "publishedMaps"), Json::integer(Json::get(st, "published")));
    gStats.activeSessions = Json::integer(Json::get(st, "activeSessions"));
    gStats.revenueToday = Json::integer(Json::get(st, "revenueToday"));
    gStats.aiJobsRunning = Json::integer(Json::get(st, "aiJobsRunning"));
    gOwnerLoaded = true;
    return;
  }
  if (ev.op == (int)NET_ADMIN_USERS) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    gUserRows.clear();
    JsonValue* users = Json::get(j.root(), "users");
    for (int i = 0; i < Json::length(users); i++) {
      JsonValue* u = Json::at(users, i);
      UserRow p{};
      snprintf(p.id, sizeof p.id, "%s", Json::string(Json::get(u, "id")));
      snprintf(p.username, sizeof p.username, "%s", Json::string(Json::get(u, "username")));
      gUserRows.push(p);
    }
    return;
  }
  if (ev.op == (int)NET_ADMIN_BAN || ev.op == (int)NET_ADMIN_SETTINGS_SAVE) {
    Json j;
    if (ev.body) j.parse(ev.body);
    if (ev.status == 200) {
      ui::toast(ev.op == (int)NET_ADMIN_BAN ? tr("تم الحظر", "User banned")
                                            : tr("تم إرسال الإشعار", "Broadcast sent"));
    } else if (ev.body) {
      toastError(j.root(), tr("فشلت العملية", "Action failed"));
    }
    return;
  }
}

}  // namespace screens
}  // namespace app
}  // namespace nx
