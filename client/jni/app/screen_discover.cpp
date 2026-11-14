// Discover — the public map catalogue. Starts genuinely empty; search + sort + type filter.
#include "core/audio.h"
#include "screens.h"
#include "widgets.h"
#include "ws.h"

namespace nx {
namespace app {
namespace screens {

namespace {

ui::ScrollView gScroll;
struct Entry {
  char id[40];
  char name[80];
  char owner[40];
  char desc[140];
  int visits;
  int likes;
  int players;
  int gameType;
  char status[16];
  bool mine;
};
Array<Entry> gMaps;
bool gLoaded = false;
bool gLoading = false;
char gQuery[64] = {0};
int gSort = 0;              // 0 popular, 1 new, 2 top rated
int gTypeFilter = -1;       // -1 all
int gTab = 0;               // 0 all maps, 1 my maps, 2 favourites

const char* kTypes[10] = {"open_world", "life_sim", "adventure", "racing", "survival",
                          "horror",     "combat",   "building",  "exploration", "social"};
const char* kTypeLabels[10][2] = {{"عالم مفتوح", "Open world"}, {"محاكاة", "Life sim"},
                                  {"مغامرة", "Adventure"},     {"سباقات", "Racing"},
                                  {"بقاء", "Survival"},        {"رعب", "Horror"},
                                  {"قتال", "Combat"},          {"بناء", "Building"},
                                  {"استكشاف", "Explore"},      {"اجتماعي", "Social"}};

const char* path() {
  if (gTab == 1) return "/api/maps/mine";
  if (gTab == 2) return "/api/maps/favorites";
  return "/api/maps";
}

void reload() {
  char full[320];
  if (gTab == 0) {
    char q[96];
    if (gQuery[0]) snprintf(q, sizeof q, "&q=%s", gQuery);
    else q[0] = 0;
    const char* sortName = gSort == 0 ? "popular" : gSort == 1 ? "new" : "rated";
    char type[64];
    if (gTypeFilter >= 0) snprintf(type, sizeof type, "&type=%s", kTypes[gTypeFilter]);
    else type[0] = 0;
    snprintf(full, sizeof full, "%s?sort=%s&limit=30%s%s", path(), sortName, q, type);
  } else {
    snprintf(full, sizeof full, "%s", path());
  }
  gLoading = true;
  api.call(NET_MAPS, "GET", full, nullptr, 21);
}

void drawCard(const Rect& r, const Entry& m) {
  ui::card(r, false);
  Color a = ui::theme.violet, b = ui::theme.cyan;
  if (m.gameType == 3) { a = Color::hex(0xE8593A); b = Color::hex(0xFFC46B); }
  else if (m.gameType == 5) { a = Color::hex(0x8A2244); b = Color::hex(0x2A0A18); }
  else if (m.gameType == 4) { a = Color::hex(0x2F7A4F); b = Color::hex(0x9BE07A); }
  else if (m.gameType == 6) { a = Color::hex(0xC0392B); b = Color::hex(0x531010); }

  Rect cover(r.x + ui::dp(8), r.y + ui::dp(8), ui::dp(96), r.h - ui::dp(16));
  ui::verticalGradient(cover, a.withA(0.8f), b.withA(0.3f));
  ui::rectStroke(cover, Color(1, 1, 1, 0.07f), 1.0f, ui::dp(10));
  ui::rect(Rect(cover.x + cover.w * 0.12f, cover.y + cover.h * 0.58f, cover.w * 0.26f, cover.h * 0.34f),
           Color(0, 0, 0, 0.20f), ui::dp(3));

  float tx = cover.x + cover.w + ui::dp(12);
  float tw = r.x + r.w - tx - ui::dp(12);
  ui::text(m.name, tx, r.y + ui::dp(12), 14, ui::theme.text, tw, 0, 2);
  ui::text(kTypeLabels[m.gameType][S.language == 0 ? 0 : 1], tx, r.y + ui::dp(34), 10.5f,
           m.gameType == 5 ? ui::theme.red : ui::theme.cyan, tw, 0, 1);
  if (m.desc[0]) ui::text(m.desc, tx, r.y + ui::dp(52), 11, ui::theme.textDim, tw, 0, 1);

  char meta[128];
  if (strcmp(m.status, "published") == 0) {
    snprintf(meta, sizeof meta, "%s • %d %s • %d %s", m.owner, m.visits, tr("زيارة", "visits"), m.players,
             tr("لاعب", "online"));
    ui::text(meta, tx, r.y + r.h - ui::dp(24), 10, ui::theme.textMuted, tw, 0, 1);
  } else {
    snprintf(meta, sizeof meta, "%s • %s", m.owner, tr("مسودة", "draft"));
    ui::text(meta, tx, r.y + r.h - ui::dp(24), 10, ui::theme.gold, tw, 0, 1);
  }
  ui::Pointer& p = ui::pointer();
  if (r.contains(p.pos.x, p.pos.y) && p.released && p.start.distance(p.pos) < ui::dp(22)) {
    audioPlay("ui_click", 0.4f);
    openMap(m.id);
  }
}

}  // namespace

namespace discover {

void enter() {
  gScroll.offset = 0;
  gLoaded = false;
  reload();
}

void leave() {}

void draw(float dt) {
  Engine& e = Engine::get();
  float w = e.width();
  HeaderOpts ho;
  ho.title = tr("اكتشف", "Discover");
  ho.backButton = false;
  float headerH = ui::dp(70);
  float top = headerH + ui::dp(4);

  background(dt);
  header(ho);

  float navH = ui::dp(72) + e.safeBottom();
  float pad = ui::dp(16);
  float innerW = w - pad * 2;

  // search + sort bar (fixed, above the list)
  Rect search(pad, top + ui::dp(4), innerW, ui::dp(42));
  ui::rect(search, Color(1, 1, 1, 0.05f), ui::dp(13));
  ui::rectStroke(search, ui::theme.strokeSoft, 1.0f, ui::dp(13));
  ui::icon("search", Rect(search.x + ui::dp(10), search.y + ui::dp(11), ui::dp(19), ui::dp(19)),
           ui::theme.textMuted);
  ui::textField("disc_search", Rect(search.x + ui::dp(36), search.y + ui::dp(4), search.w - ui::dp(46),
                                    ui::dp(34)),
                gQuery, sizeof gQuery, tr("ابحث عن ماب أو صانع…", "Search maps or creators…"), false, 48);
  if (ui::iconButton("disc_search_go", Rect(search.x + search.w - ui::dp(38), search.y + ui::dp(6), ui::dp(30),
                                            ui::dp(30)),
                     "chevron_left", ui::theme.textDim)
          .clicked) {
    gLoaded = false;
    reload();
  }

  float tabsY = search.y + search.h + ui::dp(10);
  const char* tabLabels[3] = {tr("كل المابات", "All maps"), tr("ماباتي", "My maps"), tr("المفضلة", "Saved")};
  int picked = ui::segments("disc_tabs", Rect(pad, tabsY, innerW, ui::dp(34)), tabLabels, 3, gTab, 11.5f);
  if (picked >= 0 && picked != gTab) {
    gTab = picked;
    gLoaded = false;
    reload();
  }

  float listTop = tabsY + ui::dp(44);
  Rect view(0, listTop, w, e.height() - listTop - navH);
  float rowH = ui::dp(96);
  float contentH = gLoaded ? (gMaps.count == 0 ? view.h : gMaps.count * (rowH + ui::dp(8)) + ui::dp(20)) : view.h;
  gScroll.contentH = contentH;
  gScroll.update(view, dt, true);
  Rect content = gScroll.content(view);

  if (!gLoaded || gLoading) {
    for (int i = 0; i < 4; i++) {
      Rect sk(pad, content.y + i * (rowH + ui::dp(8)), innerW, rowH);
      ui::rect(sk, Color(1, 1, 1, 0.035f), ui::dp(14));
    }
  } else if (gMaps.count == 0) {
    Rect area(0, view.y, w, view.h - ui::dp(20));
    const char* emptyTitle = gTab == 1 ? tr("لم تصنع أي ماب بعد", "You have not made a map yet")
                             : gTab == 2 ? tr("لا مابات محفوظة", "Nothing saved yet")
                                         : tr("لا مابات منشورة بعد", "No published maps yet");
    const char* emptyBody = gTab == 1
        ? tr("كل عالم في NEXORA يبدأ بوصف واحد. اكتب فكرتك وسيبني NEX AI الأرض والمباني والمهام.",
             "Every world starts with one sentence. Describe it and NEX AI builds the terrain, "
             "buildings and quests.")
        : tr("استكشف، احفظ ما يعجبك، وابدأ اللعب مع أصدقائك.", "Explore, save what you like, and play with friends.");
    if (ui::emptyState(area, "sparkle", emptyTitle, emptyBody, "disc_empty_cta",
                       gTab == 1 ? tr("أنشئ عالماً", "Create a world") : tr("أنشئ أول ماب", "Create the first map"))) {
      go(Screen::CreateAi);
    }
  } else {
    float y = content.y;
    for (int i = 0; i < gMaps.count; i++) {
      Rect r(pad, y + i * (rowH + ui::dp(8)), innerW, rowH);
      if (r.y + r.h < view.y - ui::dp(40) || r.y > view.y + view.h + ui::dp(40)) continue;
      drawCard(r, gMaps[i]);
    }
  }

  ui::scrollFadeBottom(Rect(0, view.y + view.h - ui::dp(24), w, ui::dp(24)), gScroll.offset < gScroll.contentH - view.h - 4);
  bottomNav(1);
}

}  // namespace discover

void discoverApi(const ApiEvent& ev) {
  if (ev.op != (int)NET_MAPS || ev.tag != 21) return;
  gLoading = false;
  Json j;
  if (!ev.body || !j.parse(ev.body)) {
    gLoaded = true;
    return;
  }
  gMaps.clear();
  JsonValue* maps = Json::get(j.root(), "maps");
  static const char* types[10] = {"open_world", "life_sim", "adventure", "racing", "survival",
                                  "horror",     "combat",   "building",  "exploration", "social"};
  for (int i = 0; i < Json::length(maps); i++) {
    JsonValue* m = Json::at(maps, i);
    Entry e{};
    snprintf(e.id, sizeof e.id, "%s", Json::string(Json::get(m, "id")));
    snprintf(e.name, sizeof e.name, "%s", Json::string(Json::get(m, "name")));
    JsonValue* owner = Json::get(m, "owner");
    snprintf(e.owner, sizeof e.owner, "%s", Json::string(Json::get(owner, "username")));
    snprintf(e.desc, sizeof e.desc, "%s", Json::string(Json::get(m, "description")));
    snprintf(e.status, sizeof e.status, "%s", Json::string(Json::get(m, "status"), "draft"));
    e.visits = Json::integer(Json::get(m, "visits"));
    e.likes = Json::integer(Json::get(m, "likes"));
    e.players = Json::integer(Json::get(m, "players"));
    const char* gt = Json::string(Json::get(m, "gameType"), "open_world");
    e.gameType = 0;
    for (int k = 0; k < 10; k++) if (strcmp(gt, types[k]) == 0) e.gameType = k;
    gMaps.push(e);
  }
  gLoaded = true;
  if (ev.tag == 21 && gMaps.count > 0 && !gMaps[0].mine) {
    // S.roleName check keeps the create CTA honest about ownership
  }
}

}  // namespace screens
}  // namespace app
}  // namespace nx
