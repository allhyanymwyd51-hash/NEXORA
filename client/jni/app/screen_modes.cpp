// Modes — the "what do you want to play" hub: curated rows by genre, room counts and covers.
#include "core/audio.h"
#include "screens.h"
#include "widgets.h"
#include "ws.h"

namespace nx {
namespace app {
namespace screens {

namespace {

struct Tile {
  char id[40];
  char name[80];
  int players;
  int visits;
  char gameType[24];
  char owner[48];
};

Array<Tile> gPopular, gFresh;
bool gLoaded = false;
ui::ScrollView gScroll;
float gAnim = 0;

const char* typeLabel(const char* t) {
  if (strcmp(t, "racing") == 0) return tr("سباقات", "Racing");
  if (strcmp(t, "horror") == 0) return tr("رعب", "Horror");
  if (strcmp(t, "survival") == 0) return tr("بقاء", "Survival");
  if (strcmp(t, "adventure") == 0) return tr("مغامرة", "Adventure");
  if (strcmp(t, "building") == 0) return tr("بناء", "Building");
  if (strcmp(t, "combat") == 0) return tr("قتال", "Combat");
  if (strcmp(t, "life_sim") == 0) return tr("محاكاة", "Life sim");
  if (strcmp(t, "exploration") == 0) return tr("استكشاف", "Exploration");
  if (strcmp(t, "social") == 0) return tr("اجتماعي", "Social");
  return tr("عالم مفتوح", "Open world");
}

Color tileColor(const Tile& t) {
  float h = 0;
  for (const char* p = t.id; *p; p++) h += (float)(unsigned char)*p;
  static const unsigned palette[] = {0x7C4DFF, 0x38E1FF, 0x2F7A4F, 0xE8593A, 0x8A2244, 0xE8C15A};
  return Color::hex(palette[(int)h % 6]);
}

void collect(JsonValue* arr, Array<Tile>& out) {
  for (int i = 0; i < Json::length(arr); i++) {
    JsonValue* m = Json::at(arr, i);
    Tile t{};
    snprintf(t.id, sizeof t.id, "%s", Json::string(Json::get(m, "id")));
    snprintf(t.name, sizeof t.name, "%s", Json::string(Json::get(m, "name")));
    t.players = Json::integer(Json::get(m, "players"));
    t.visits = Json::integer(Json::get(m, "visits"));
    snprintf(t.gameType, sizeof t.gameType, "%s", Json::string(Json::get(m, "gameType"), "open_world"));
    snprintf(t.owner, sizeof t.owner, "%s", Json::string(Json::get(Json::get(m, "owner"), "username")));
    out.push(t);
  }
}

// one card, reused by both rails
bool drawTile(const Tile& t, const Rect& r, const char* keySuffix) {
  ui::card(r, false);
  Rect cover(r.x, r.y, r.w, r.h * 0.62f);
  Color c = tileColor(t);
  ui::verticalGradient(cover, c.withA(0.85f), c * 0.28f);
  ui::rectStroke(Rect(r.x, r.y, r.w, r.h), Color(1, 1, 1, 0.07f), 1.0f, ui::dp(14));
  ui::tag(Rect(cover.x + ui::dp(8), cover.y + ui::dp(8), ui::dp(60), ui::dp(20)), typeLabel(t.gameType),
          ui::theme.text.withA(0.18f));
  if (t.players > 0) {
    char live[32];
    snprintf(live, sizeof live, "%d %s", t.players, tr("يلعبون", "playing"));
    ui::rect(Rect(cover.x + ui::dp(8), cover.y + cover.h - ui::dp(26), ui::dp(78), ui::dp(18)),
             Color(0, 0, 0, 0.45f), ui::dp(9));
    ui::rect(Rect(cover.x + ui::dp(14), cover.y + cover.h - ui::dp(19), ui::dp(6), ui::dp(6)), ui::theme.green,
             ui::dp(3));
    ui::text(live, cover.x + ui::dp(24), cover.y + cover.h - ui::dp(24), 9, ui::theme.text, ui::dp(70), 0, 2);
  }
  ui::text(t.name, r.x + ui::dp(10), cover.y + cover.h + ui::dp(8), 11.5f, ui::theme.text, r.w - ui::dp(20), 0, 2);
  char sub[80];
  snprintf(sub, sizeof sub, "%s • %d %s", t.owner, t.visits, tr("زيارة", "visits"));
  ui::text(sub, r.x + ui::dp(10), cover.y + cover.h + ui::dp(26), 9.5f, ui::theme.textMuted, r.w - ui::dp(20), 0, 1);
  ui::Pointer& p = ui::pointer();
  char key[64];
  snprintf(key, sizeof key, "mode_%s_%s", keySuffix, t.id);
  (void)key;
  return r.contains(p.pos.x, p.pos.y) && p.released && p.start.distance(p.pos) < ui::dp(20);
}

}  // namespace

namespace modes {

void enter() {
  gLoaded = false;
  gScroll.offset = 0;
  api.call(NET_MAPS, "GET", "/api/maps?sort=players&limit=12", nullptr, 111);
  api.call(NET_MAPS, "GET", "/api/maps?sort=new&limit=12", nullptr, 112);
}

void leave() {}

void draw(float dt) {
  gAnim += dt;
  Engine& e = Engine::get();
  float w = e.width();
  float pad = ui::dp(16);
  float innerW = w - pad * 2;
  background(dt);

  HeaderOpts ho;
  ho.title = tr("العب", "Play");
  header(ho);
  float top = ui::dp(74);
  float navH = ui::dp(72) + e.safeBottom();

  gScroll.viewH = e.height() - top - navH;
  gScroll.contentH = ui::dp(660);
  gScroll.update(Rect(0, top, w, gScroll.viewH), dt, true);
  float y = gScroll.content(Rect(0, top, w, gScroll.viewH)).y + ui::dp(8);

  // hero: play now
  Rect hero(pad, y, innerW, ui::dp(120));
  ui::shadow(hero, ui::dp(18), 0.5f);
  ui::verticalGradient(hero, ui::theme.violet.withA(0.85f), Color::hex(0x1B1040));
  ui::rectStroke(hero, Color(1, 1, 1, 0.10f), 1.0f, ui::dp(18));
  ui::glow(Rect(hero.x + hero.w * 0.55f, hero.y - ui::dp(20), hero.w * 0.5f, hero.h + ui::dp(40)),
           ui::theme.cyan.withA(0.12f), 1.0f);
  ui::text(tr("عالم عشوائي", "Surprise me"), hero.x + ui::dp(18), hero.y + ui::dp(20), 19, ui::theme.text,
           hero.w - ui::dp(36), 0, 2);
  ui::text(tr("انضم لأكثر ماب مزدحم الآن", "Jump into the busiest map right now"), hero.x + ui::dp(18),
           hero.y + ui::dp(48), 11.5f, ui::theme.text.withA(0.8f), hero.w - ui::dp(36), 0, 1);
  Rect quick(hero.x + ui::dp(18), hero.y + ui::dp(74), hero.w - ui::dp(36), ui::dp(36));
  if (ui::button("modes_random", quick, tr("دخول سريع", "Quick join"), true, gPopular.count > 0, 12, "play")
          .clicked) {
    enterMap(gPopular.count ? gPopular[0].id : "");
  }
  y += ui::dp(134);

  sectionTitle(tr("الأكثر لعباً", "Most played"), pad, y, innerW);
  y += ui::dp(28);
  {
    float cw = ui::dp(150), ch = ui::dp(140);
    if (!gLoaded) {
      for (int i = 0; i < 3; i++) {
        ui::rect(Rect(pad + i * (cw + ui::dp(10)), y, cw, ch), Color(1, 1, 1, 0.035f), ui::dp(14));
      }
    } else if (gPopular.count == 0) {
      ui::text(tr("لا مابات منشورة بعد", "No published maps yet"), pad, y + ui::dp(20), 11.5f, ui::theme.textMuted,
               innerW, 0, 1);
    }
    for (int i = 0; i < gPopular.count && i < 6; i++) {
      Rect r(pad + i * (cw + ui::dp(10)), y, cw, ch);
      if (drawTile(gPopular[i], r, "pop")) enterMap(gPopular[i].id);
    }
  }
  y += ui::dp(152);

  sectionTitle(tr("جديد هذا الأسبوع", "New this week"), pad, y, innerW);
  y += ui::dp(28);
  {
    float cw = ui::dp(150), ch = ui::dp(140);
    if (gLoaded && gFresh.count == 0) {
      ui::text(tr("لا جديد بعد", "Nothing new yet"), pad, y + ui::dp(20), 11.5f, ui::theme.textMuted, innerW, 0, 1);
    }
    for (int i = 0; i < gFresh.count && i < 6; i++) {
      Rect r(pad + i * (cw + ui::dp(10)), y, cw, ch);
      if (drawTile(gFresh[i], r, "new")) enterMap(gFresh[i].id);
    }
  }
  y += ui::dp(156);

  // genre shortcuts
  sectionTitle(tr("تصفح بالنوع", "Browse by genre"), pad, y, innerW);
  y += ui::dp(30);
  const char* genres[6][2] = {{"سباقات", "Racing"},    {"رعب", "Horror"},   {"بقاء", "Survival"},
                              {"بناء", "Building"},    {"مغامرة", "Adventure"}, {"عالم مفتوح", "Open world"}};
  float gw = (innerW - ui::dp(16)) / 3.0f;
  for (int i = 0; i < 6; i++) {
    Rect r(pad + (i % 3) * (gw + ui::dp(8)), y + (i / 3) * (ui::dp(46) + ui::dp(8)), gw, ui::dp(46));
    ui::card(r, false);
    ui::text(tr(genres[i][0], genres[i][1]), r.x, r.y + ui::dp(15), 11.5f, ui::theme.textDim, r.w, 1, 1);
    ui::Pointer& p = ui::pointer();
    if (r.contains(p.pos.x, p.pos.y) && p.released && p.start.distance(p.pos) < ui::dp(20)) {
      go(Screen::Discover);
    }
  }

  bottomNav(1);
}

}  // namespace modes

void modesApi(const ApiEvent& ev) {
  if (ev.op != (int)NET_MAPS) return;
  Json j;
  if (!ev.body || !j.parse(ev.body)) return;
  if (ev.tag == 111) {
    gPopular.clear();
    collect(Json::get(j.root(), "maps"), gPopular);
  } else if (ev.tag == 112) {
    gFresh.clear();
    collect(Json::get(j.root(), "maps"), gFresh);
  }
  gLoaded = true;
}

}  // namespace screens
}  // namespace app
}  // namespace nx
