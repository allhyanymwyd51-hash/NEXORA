// Home — the 10-section root hub: continue, quick actions, featured, NEX AI, social, modes.
#include "core/audio.h"
#include "screens.h"
#include "widgets.h"
#include "ws.h"

namespace nx {
namespace app {
namespace screens {

namespace {

ui::ScrollView gScroll;
float gLoadedAt = 0;
bool gHeroAnim = false;

struct FeaturedMap {
  char id[40];
  char name[80];
  char owner[40];
  int visitors;
  int gameType;
};

Array<FeaturedMap> gFeatured;
bool gFeaturedLoaded = false;

const char* kModes[10][2] = {
    {"عالم مفتوح", "Open world"}, {"محاكاة حياة", "Life sim"},      {"مغامرة", "Adventure"},
    {"سباقات", "Racing"},         {"بقاء", "Survival"},             {"رعب", "Horror"},
    {"قتال", "Combat"},           {"بناء", "Building"},             {"استكشاف", "Exploration"},
    {"اجتماعي", "Social"},
};
const char* kModeIcons[10] = {"compass", "heart", "map", "play", "shield", "eye", "sword", "plus", "compass", "users"};

void drawHero(float x, float y, float w) {
  // Primary call to action: the NEX AI generator. This is the product's signature surface.
  Rect hero(x, y, w, ui::dp(150));
  ui::shadow(hero, ui::dp(18), 0.5f);
  ui::rect(hero, ui::theme.panel, ui::dp(20));
  ui::verticalGradient(Rect(hero.x + 1, hero.y + 1, hero.w - 2, hero.h - 2), Color(0.16f, 0.10f, 0.36f, 0.95f),
                       Color(0.07f, 0.08f, 0.18f, 0.95f));
  ui::rectStroke(hero, ui::theme.violet.withA(0.35f), 1.2f, ui::dp(20));
  ui::glow(Rect(hero.x + hero.w - ui::dp(130), hero.y - ui::dp(30), ui::dp(190), ui::dp(190)),
           ui::theme.cyan.withA(0.10f), 1.0f);

  ui::icon("sparkle", Rect(hero.x + (isRtl() ? hero.w - ui::dp(46) : ui::dp(18)), hero.y + ui::dp(18), ui::dp(26),
                           ui::dp(26)),
           ui::theme.cyan);
  ui::text(tr("NEX AI", "NEX AI"), hero.x + ui::dp(18), hero.y + ui::dp(52), 21, ui::theme.text,
           hero.w - ui::dp(36), 0, 2);
  ui::text(tr("صِف عالمك… وسنبنيه لك بالكامل", "Describe a world — we build it"), hero.x + ui::dp(18),
           hero.y + ui::dp(80), 12, ui::theme.textDim, hero.w - ui::dp(36), 0, 1);

  Rect cta(hero.x + ui::dp(18), hero.y + hero.h - ui::dp(52), fminf(hero.w - ui::dp(36), ui::dp(210)), ui::dp(40));
  if (ui::button("home_ai", cta, tr("أنشئ عالماً", "Create a world"), true, true, 13.5f, "sparkle").clicked) {
    go(Screen::CreateAi);
  }
  ui::text(tr("من صغير إلى AAA", "Small → AAA"), hero.x + ui::dp(18) + cta.w + ui::dp(12),
           hero.y + hero.h - ui::dp(40), 10.5f, ui::theme.textMuted, 0, 0, 1);
}

void quickAction(const char* id, const Rect& r, const char* iconName, const char* label, const Color& tint,
                 Screen target, int badge = 0) {
  ui::rect(r, Color(1, 1, 1, 0.04f), ui::dp(14));
  ui::rectStroke(r, Color(1, 1, 1, 0.05f), 1.0f, ui::dp(14));
  Rect tile(r.x + ui::dp(8), r.y + ui::dp(8), ui::dp(30), ui::dp(30));
  ui::rect(tile, tint.withA(0.16f), ui::dp(9));
  ui::icon(iconName, tile.inset(ui::dp(7)), tint);
  ui::text(label, r.x + ui::dp(8), r.y + r.h - ui::dp(26), 10.5f, ui::theme.textDim, r.w - ui::dp(14), 0, 1);
  if (badge > 0) {
    char b[8];
    snprintf(b, sizeof b, "%d", badge);
    float bw = fmaxf(ui::dp(16), ui::textMeasure(b, 9.5f, 0, 2) + ui::dp(8));
    ui::rect(Rect(r.x + r.w - bw - ui::dp(6), r.y + ui::dp(6), bw, ui::dp(15)), ui::theme.red, ui::dp(8));
    ui::text(b, r.x + r.w - bw - ui::dp(6), r.y + ui::dp(8), 9.5f, Color(1, 1, 1), bw, 1, 2);
  }
  ui::Pointer& p = ui::pointer();
  if (r.contains(p.pos.x, p.pos.y) && p.released && p.start.distance(p.pos) < ui::dp(22)) {
    audioPlay("ui_click", 0.4f);
    go(target);
  }
  (void)id;
}

void mapCard(const Rect& r, const FeaturedMap& m) {
  ui::card(r, false);
  // Generative cover: the game type's palette, never a random stock photo.
  Color a = ui::theme.violet, b = ui::theme.cyan;
  switch (m.gameType) {
    case 3: a = Color::hex(0xE8593A); b = Color::hex(0xFFC46B); break;
    case 5: a = Color::hex(0x8A2244); b = Color::hex(0x2A0A18); break;
    case 4: a = Color::hex(0x2F7A4F); b = Color::hex(0x9BE07A); break;
    case 6: a = Color::hex(0xC0392B); b = Color::hex(0x531010); break;
    default: break;
  }
  Rect cover(r.x + ui::dp(8), r.y + ui::dp(8), r.w - ui::dp(16), r.h - ui::dp(62));
  ui::verticalGradient(cover, a.withA(0.85f), b.withA(0.35f));
  ui::rectStroke(cover, Color(1, 1, 1, 0.08f), 1.0f, ui::dp(10));
  // simple horizon silhouette so the tile reads as a world, not a colour swatch
  for (int i = 0; i < 3; i++) {
    float h = cover.h * (0.22f + 0.12f * i);
    ui::rect(Rect(cover.x + cover.w * (0.08f + 0.28f * i), cover.y + cover.h - h, cover.w * 0.22f, h),
             Color(0, 0, 0, 0.18f), ui::dp(4));
  }
  ui::text(m.name, r.x + ui::dp(10), r.y + r.h - ui::dp(48), 12, ui::theme.text, r.w - ui::dp(20), 0, 2);
  char meta[80];
  snprintf(meta, sizeof meta, "%s • %d", m.owner, m.visitors);
  ui::text(meta, r.x + ui::dp(10), r.y + r.h - ui::dp(28), 10, ui::theme.textMuted, r.w - ui::dp(20), 0, 1);
  ui::Pointer& p = ui::pointer();
  if (r.contains(p.pos.x, p.pos.y) && p.released && p.start.distance(p.pos) < ui::dp(22)) {
    audioPlay("ui_click", 0.4f);
    openMap(m.id);
  }
}

void drawModes(float x, float y, float w) {
  sectionTitle(tr("أنواع المابات", "Map types"), x, y, w, true, tr("الكل", "All"));
  float chipH = ui::dp(62);
  float gap = ui::dp(8);
  float cw = (w - gap * 3) / 4.0f;
  for (int i = 0; i < 10; i++) {
    int row = i / 4, col = i % 4;
    Rect c(x + col * (cw + gap), y + ui::dp(26) + row * (chipH + gap), cw, chipH);
    ui::rect(c, Color(1, 1, 1, 0.038f), ui::dp(12));
    ui::icon(kModeIcons[i], Rect(c.x + c.w * 0.5f - ui::dp(11), c.y + ui::dp(10), ui::dp(22), ui::dp(22)),
             ui::theme.violet);
    ui::text(tr(kModes[i][0], kModes[i][1]), c.x, c.y + ui::dp(38), 9.5f, ui::theme.textDim, c.w, 1, 1);
    ui::Pointer& p = ui::pointer();
    if (c.contains(p.pos.x, p.pos.y) && p.released && p.start.distance(p.pos) < ui::dp(22)) {
      audioPlay("ui_click", 0.4f);
      go(Screen::Discover);
    }
  }
}

}  // namespace

namespace home {

void enter() {
  gScroll.offset = 0;
  gLoadedAt = 0;
  api.call(NET_MAPS, "GET", "/api/maps?sort=popular&limit=8", nullptr, 11);
  api.call(NET_FRIENDS, "GET", "/api/friends", nullptr, 12);
  api.call(NET_NOTIFICATIONS, "GET", "/api/me/notifications?limit=1", nullptr, 13);
}

void leave() {}

void draw(float dt) {
  Engine& e = Engine::get();
  float w = e.width();
  HeaderOpts ho;
  ho.title = tr("الرئيسية", "Home");
  ho.backButton = false;
  float headerH = ui::dp(70);
  float contentTop = headerH + ui::dp(4);

  background(dt);
  header(ho);

  float navH = ui::dp(72) + e.safeBottom();
  Rect view(0, contentTop, w, e.height() - contentTop - navH);
  float pad = ui::dp(16);
  float innerW = w - pad * 2;

  // content height is composed from fixed blocks so the scroll feels right
  float contentH = ui::dp(150) + ui::dp(16);              // hero
  contentH += ui::dp(84) + ui::dp(24);                    // quick actions
  contentH += ui::dp(150) + ui::dp(28);                   // featured
  contentH += ui::dp(216) + ui::dp(28);                   // modes
  contentH += ui::dp(96) + ui::dp(28);                    // social
  contentH += ui::dp(120);                                // footer space
  gScroll.contentH = contentH;
  gScroll.update(view, dt, true);
  Rect content = gScroll.content(view);

  float y = content.y;
  float x = pad;

  drawHero(x, y, innerW);
  y += ui::dp(150) + ui::dp(16);

  // ---- quick actions ----
  {
    float gap = ui::dp(8);
    float qw = (innerW - gap * 3) / 4.0f;
    float qh = ui::dp(76);
    quickAction("qa_store", Rect(x, y, qw, qh), "bag", tr("المتجر", "Store"), ui::theme.gold, Screen::Store);
    quickAction("qa_modes", Rect(x + (qw + gap), y, qw, qh), "compass", tr("استكشاف", "Explore"), ui::theme.cyan,
                Screen::Discover);
    quickAction("qa_friends", Rect(x + (qw + gap) * 2, y, qw, qh), "users", tr("الأصدقاء", "Friends"),
                ui::theme.green, Screen::Friends, S.unread);
    quickAction("qa_chat", Rect(x + (qw + gap) * 3, y, qw, qh), "chat", tr("الدردشة", "Chat"), ui::theme.violet,
                Screen::Chat);
    y += qh + ui::dp(24);
  }

  // ---- featured ----
  sectionTitle(tr("مابات مميزة", "Featured maps"), x, y, innerW, true, tr("عرض الكل", "See all"));
  if (ui::button("home_all", Rect(x + innerW - ui::dp(78), y - ui::dp(6), ui::dp(78), ui::dp(26)), tr("الكل", "All"),
                 false, true, 10.5f)
          .clicked) {
    go(Screen::Discover);
  }
  y += ui::dp(28);
  if (!gFeaturedLoaded) {
    for (int i = 0; i < 3; i++) {
      Rect sk(x + i * (ui::dp(150) + ui::dp(10)), y, ui::dp(150), ui::dp(120));
      ui::rect(sk, Color(1, 1, 1, 0.04f), ui::dp(14));
    }
  } else if (gFeatured.count == 0) {
    // Honest empty state: no invented maps, no fake visit counts.
    Rect empty(x, y, innerW, ui::dp(126));
    ui::card(empty, false);
    ui::icon("compass", Rect(x + ui::dp(18), y + ui::dp(26), ui::dp(30), ui::dp(30)), ui::theme.textMuted);
    ui::text(tr("لا توجد مابات منشورة بعد", "No published maps yet"), x + ui::dp(60), y + ui::dp(24), 14,
             ui::theme.text, innerW - ui::dp(76), 0, 2);
    ui::text(tr("كن أول من ينشر عالماً على NEXORA", "Be the first to publish a world"), x + ui::dp(60),
             y + ui::dp(46), 11, ui::theme.textMuted, innerW - ui::dp(76), 0, 1);
    Rect btn(x + ui::dp(18), y + ui::dp(74), ui::dp(160), ui::dp(36));
    if (ui::button("home_empty_cta", btn, tr("أنشئ مابك الأول", "Create your first map"), true, true, 11.5f,
                   "sparkle")
            .clicked) {
      go(Screen::CreateAi);
    }
  } else {
    float cw = ui::dp(150);
    for (int i = 0; i < gFeatured.count && i < 6; i++) {
      Rect r(x + i * (cw + ui::dp(10)), y, cw, ui::dp(120));
      mapCard(r, gFeatured[i]);
    }
  }
  y += ui::dp(150) + ui::dp(28);

  // ---- modes ----
  drawModes(x, y, innerW);
  y += ui::dp(216) + ui::dp(28);

  // ---- social strip ----
  sectionTitle(tr("أصدقاؤك", "Your friends"), x, y, innerW, true, tr("إدارة", "Manage"));
  y += ui::dp(26);
  Rect soc(x, y, innerW, ui::dp(66));
  ui::card(soc, false);
  if (S.friends == 0 && S.unread == 0) {
    ui::icon("users", Rect(x + ui::dp(18), y + ui::dp(18), ui::dp(28), ui::dp(28)), ui::theme.textMuted);
    ui::text(tr("لا أصدقاء بعد — أضف أصدقاء للعب معهم", "No friends yet — add some to play together"),
             x + ui::dp(58), y + ui::dp(16), 12, ui::theme.text, innerW - ui::dp(130), 0, 1);
    Rect add(x + innerW - ui::dp(84), y + ui::dp(16), ui::dp(70), ui::dp(34));
    if (ui::button("home_addfriend", add, tr("إضافة", "Add"), true, true, 11).clicked) go(Screen::Friends);
  } else {
    char line[96];
    snprintf(line, sizeof line, "%d %s", S.friends, tr("صديق", "friends"));
    ui::text(line, x + ui::dp(18), y + ui::dp(12), 14, ui::theme.text, innerW - ui::dp(36), 0, 2);
    char sub[96];
    snprintf(sub, sizeof sub, "%d %s", S.unread, tr("إشعار غير مقروء", "unread notifications"));
    ui::text(sub, x + ui::dp(18), y + ui::dp(34), 11, ui::theme.textMuted, innerW - ui::dp(36), 0, 1);
    Rect view2(x + innerW - ui::dp(96), y + ui::dp(16), ui::dp(82), ui::dp(32));
    if (ui::button("home_openfriends", view2, tr("فتح", "Open"), false, true, 11).clicked) go(Screen::Friends);
  }

  // footer: version + owner entry stays invisible unless the account really is the owner
  y += ui::dp(96);
  char ver[64];
  snprintf(ver, sizeof ver, "NEXORA v%s • %d FPS", NEXORA_VERSION, Engine::get().fps());
  ui::text(ver, 0, y, 10, ui::theme.textMuted, w, 1, 1);

  bottomNav(0);
}

}  // namespace home

void homeApi(const ApiEvent& ev) {
  if (ev.op == (int)NET_MAPS && ev.tag == 11) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    gFeatured.clear();
    JsonValue* maps = Json::get(j.root(), "maps");
    for (int i = 0; i < Json::length(maps); i++) {
      JsonValue* m = Json::at(maps, i);
      FeaturedMap f{};
      snprintf(f.id, sizeof f.id, "%s", Json::string(Json::get(m, "id")));
      snprintf(f.name, sizeof f.name, "%s", Json::string(Json::get(m, "name")));
      JsonValue* owner = Json::get(m, "owner");
      snprintf(f.owner, sizeof f.owner, "%s", Json::string(Json::get(owner, "username")));
      f.visitors = Json::integer(Json::get(m, "visits"));
      const char* gt = Json::string(Json::get(m, "gameType"), "open_world");
      static const char* types[] = {"open_world", "life_sim", "adventure", "racing", "survival",
                                    "horror",     "combat",   "building",  "exploration", "social"};
      f.gameType = 0;
      for (int k = 0; k < 10; k++) if (strcmp(gt, types[k]) == 0) f.gameType = k;
      gFeatured.push(f);
    }
    gFeaturedLoaded = true;
  } else if (ev.op == (int)NET_FRIENDS) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    S.friends = Json::length(Json::get(j.root(), "friends"));
  } else if (ev.op == (int)NET_NOTIFICATIONS) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    S.unread = Json::integer(Json::get(j.root(), "unread"), S.unread);
  }
}

}  // namespace screens
}  // namespace app
}  // namespace nx
