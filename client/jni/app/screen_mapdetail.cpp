// Map detail — hero preview, live player count, owner tools, passes and the Play action.
#include "core/audio.h"
#include "game/gameplay.h"
#include "screens.h"
#include "widgets.h"
#include "ws.h"

namespace nx {
namespace app {
namespace screens {

namespace {

struct Pass {
  char id[40];
  char name[60];
  char desc[120];
  int price;
  bool owned;
};

struct Detail {
  char id[40];
  char name[80];
  char desc[400];
  char owner[40];
  char status[16];
  char gameType[24];
  int visits, likes, maxPlayers, version;
  bool liked, favorite, isAi, mine;
  int features;
  char featureNames[8][28];
  int featureCount;
  char weather[16];
  int trees, buildings;
  Array<Pass> passes;
  bool hasWorld;
  float side;
  int quests;
};

Detail gMap;
bool gLoaded = false;
char gJoinError[160] = {0};
float gAnim = 0;

void applyDetail(JsonValue* root) {
  JsonValue* m = Json::get(root, "map");
  if (!m) return;
  gMap.passes.clear();
  gMap.featureCount = 0;
  snprintf(gMap.id, sizeof gMap.id, "%s", Json::string(Json::get(m, "id")));
  snprintf(gMap.name, sizeof gMap.name, "%s", Json::string(Json::get(m, "name")));
  snprintf(gMap.desc, sizeof gMap.desc, "%s", Json::string(Json::get(m, "description")));
  JsonValue* owner = Json::get(m, "owner");
  snprintf(gMap.owner, sizeof gMap.owner, "%s", Json::string(Json::get(owner, "username")));
  snprintf(gMap.status, sizeof gMap.status, "%s", Json::string(Json::get(m, "status"), "draft"));
  snprintf(gMap.gameType, sizeof gMap.gameType, "%s", Json::string(Json::get(m, "gameType"), "open_world"));
  gMap.visits = Json::integer(Json::get(m, "visits"));
  gMap.likes = Json::integer(Json::get(m, "likes"));
  gMap.maxPlayers = Json::integer(Json::get(m, "maxPlayers"), 18);
  gMap.version = Json::integer(Json::get(m, "version"), 1);
  gMap.liked = Json::boolean(Json::get(m, "liked"));
  gMap.favorite = Json::boolean(Json::get(m, "favorite"));
  gMap.isAi = Json::boolean(Json::get(m, "isAi"));
  const char* ownerId = Json::string(Json::get(owner, "id"));
  gMap.mine = ownerId[0] && S.uid[0] && strcmp(ownerId, S.uid) == 0;

  JsonValue* world = Json::get(m, "world");
  gMap.hasWorld = world && Json::get(world, "format") != nullptr;
  if (gMap.hasWorld) {
    gMap.side = Json::flt(Json::get(world, "side"), 0);
    gMap.quests = Json::length(Json::get(world, "quests"));
    JsonValue* env = Json::get(world, "environment");
    snprintf(gMap.weather, sizeof gMap.weather, "%s", Json::string(Json::get(env, "weather"), "clear"));
    JsonValue* stats = Json::get(world, "stats");
    gMap.trees = Json::integer(Json::get(stats, "trees"));
    gMap.buildings = Json::integer(Json::get(stats, "buildings"));
    JsonValue* feats = Json::get(world, "features");
    for (int i = 0; i < Json::length(feats) && gMap.featureCount < 8; i++) {
      snprintf(gMap.featureNames[gMap.featureCount++], 28, "%s", Json::string(Json::at(feats, i)));
    }
  }
  JsonValue* passes = Json::get(m, "passesList");
  for (int i = 0; i < Json::length(passes); i++) {
    JsonValue* p = Json::at(passes, i);
    Pass pass{};
    snprintf(pass.id, sizeof pass.id, "%s", Json::string(Json::get(p, "id")));
    snprintf(pass.name, sizeof pass.name, "%s", Json::string(Json::get(p, "name")));
    snprintf(pass.desc, sizeof pass.desc, "%s", Json::string(Json::get(p, "description")));
    pass.price = Json::integer(Json::get(p, "price"));
    pass.owned = Json::boolean(Json::get(p, "owned"));
    gMap.passes.push(pass);
  }
  gLoaded = true;
}

const char* weatherLabel(const char* w) {
  if (strcmp(w, "rain") == 0) return tr("ممطر", "Rainy");
  if (strcmp(w, "storm") == 0) return tr("عاصف", "Stormy");
  if (strcmp(w, "snow") == 0) return tr("ثلجي", "Snowy");
  if (strcmp(w, "fog") == 0) return tr("ضبابي", "Foggy");
  return tr("صافٍ", "Clear");
}

}  // namespace

namespace mapDetail {

void enter() {
  gLoaded = false;
  gJoinError[0] = 0;
}

void leave() {}

void draw(float dt) {
  gAnim += dt;
  Engine& e = Engine::get();
  float w = e.width();
  background(dt);

  HeaderOpts ho;
  ho.title = gLoaded ? gMap.name : tr("الماب", "Map");
  header(ho);

  float top = ui::dp(74);
  float pad = ui::dp(16);
  float innerW = w - pad * 2;
  float navH = ui::dp(72) + e.safeBottom();

  ui::ScrollView scroll;
  scroll.viewH = e.height() - top - navH;
  scroll.contentH = ui::dp(720);
  Rect view(0, top, w, scroll.viewH);
  scroll.update(view, dt, true);
  Rect content = scroll.content(view);
  float y = content.y + ui::dp(6);

  if (!gLoaded) {
    Rect sk(pad, y, innerW, ui::dp(180));
    ui::rect(sk, Color(1, 1, 1, 0.04f), ui::dp(16));
    ui::text(tr("جاري التحميل…", "Loading…"), 0, y + ui::dp(90), 12, ui::theme.textMuted, w, 1, 1);
    return;
  }

  // ---- hero ----
  Rect hero(pad, y, innerW, ui::dp(190));
  ui::shadow(hero, ui::dp(16), 0.5f);
  Color a = ui::theme.violet, b = Color::hex(0x120C2E);
  if (strcmp(gMap.gameType, "horror") == 0) { a = Color::hex(0x8A2244); b = Color::hex(0x14060C); }
  else if (strcmp(gMap.gameType, "racing") == 0) { a = Color::hex(0xE8593A); b = Color::hex(0x2A1206); }
  else if (strcmp(gMap.gameType, "survival") == 0) { a = Color::hex(0x2F7A4F); b = Color::hex(0x08160E); }
  ui::verticalGradient(hero, a.withA(0.55f), b);
  ui::rectStroke(hero, Color(1, 1, 1, 0.08f), 1.2f, ui::dp(16));
  // horizon + parallax-style layers: a preview of the world's mood, drawn from its own palette
  for (int i = 0; i < 4; i++) {
    float hh = hero.h * (0.16f + i * 0.08f);
    ui::rect(Rect(hero.x, hero.y + hero.h - hh, hero.w, hh), Color(0, 0, 0, 0.16f - i * 0.03f), 0);
  }
  ui::glow(Rect(hero.x + hero.w * 0.6f, hero.y - ui::dp(20), hero.w * 0.6f, ui::dp(160)),
           ui::theme.cyan.withA(0.10f), 1.0f);
  char idline[96];
  snprintf(idline, sizeof idline, "%s • v%d", gMap.isAi ? "NEX AI" : tr("يدوي", "Handcrafted"), gMap.version);
  ui::text(idline, hero.x + ui::dp(14), hero.y + ui::dp(14), 10.5f, ui::theme.textDim, 0, 0, 1);
  if (strcmp(gMap.status, "published") != 0) {
    ui::tag(Rect(hero.x + ui::dp(14), hero.y + ui::dp(34), ui::dp(76), ui::dp(22)), tr("مسودة", "Draft"),
            ui::theme.gold);
  }
  char meta[160];
  snprintf(meta, sizeof meta, "%s • %d %s • %d %s", gMap.owner, gMap.visits, tr("زيارة", "visits"), gMap.maxPlayers,
           tr("لاعب", "players"));
  ui::text(meta, hero.x + ui::dp(14), hero.y + hero.h - ui::dp(30), 11, ui::theme.text.withA(0.85f),
           hero.w - ui::dp(28), 0, 1);
  y += hero.h + ui::dp(14);

  // ---- primary actions ----
  Rect play(pad, y, innerW * 0.62f, ui::dp(50));
  bool canPlay = strcmp(gMap.status, "published") == 0 || gMap.mine;
  const char* playLabel = gMap.mine && strcmp(gMap.status, "published") != 0 ? tr("تجربة اللعب", "Test play")
                                                                             : tr("العب الآن", "Play now");
  if (ui::button("md_play", play, playLabel, true, canPlay && gMap.hasWorld, 15, "play").clicked) {
    audioPlay("ui_confirm", 0.6f);
    enterMap(gMap.id);
  }
  Rect edit(pad + innerW * 0.62f + ui::dp(8), y, innerW * 0.38f - ui::dp(8), ui::dp(50));
  if (gMap.mine) {
    if (ui::button("md_studio", edit, tr("الاستوديو", "Studio"), false, true, 13, "gear").clicked) {
      go(Screen::Studio);
    }
  } else {
    if (ui::button("md_fav", edit, gMap.favorite ? tr("محفوظ", "Saved") : tr("حفظ", "Save"), false, true, 13,
                   "heart")
            .clicked) {
      char path[64];
      snprintf(path, sizeof path, "/api/maps/%s/favorite", gMap.id);
      api.call(NET_MAP_FAVORITE, "POST", path, "{}", 31);
    }
  }
  y += ui::dp(58);

  if (gJoinError[0]) {
    ui::text(gJoinError, pad, y, 11, ui::theme.red, innerW, 0, 1);
    y += ui::dp(20);
  }

  // ---- stats ----
  float statW = (innerW - ui::dp(16)) / 3.0f;
  char visits[16], likes[16], quests[16];
  snprintf(visits, sizeof visits, "%d", gMap.visits);
  snprintf(likes, sizeof likes, "%d", gMap.likes);
  snprintf(quests, sizeof quests, "%d", gMap.quests);
  ui::statPill(Rect(pad, y, statW, ui::dp(52)), "eye", visits, tr("زيارة", "Visits"), ui::theme.cyan);
  ui::statPill(Rect(pad + statW + ui::dp(8), y, statW, ui::dp(52)), "heart", likes, tr("إعجاب", "Likes"),
               ui::theme.red);
  ui::statPill(Rect(pad + (statW + ui::dp(8)) * 2, y, statW, ui::dp(52)), "star", quests, tr("مهمة", "Quests"),
               ui::theme.gold);
  y += ui::dp(66);

  // ---- about ----
  if (gMap.desc[0]) {
    sectionTitle(tr("عن الماب", "About"), pad, y, innerW);
    y += ui::dp(24);
    y += ui::text(gMap.desc, pad, y, 12, ui::theme.textDim, innerW, 0, 1);
    y += ui::dp(16);
  }

  // ---- world facts ----
  sectionTitle(tr("تفاصيل العالم", "World details"), pad, y, innerW);
  y += ui::dp(26);
  Rect facts(pad, y, innerW, ui::dp(96));
  ui::card(facts, false);
  char l1[128], l2[128];
  snprintf(l1, sizeof l1, "%s: %d m²  •  %s: %s", tr("المساحة", "Area"), (int)(gMap.side * gMap.side),
           tr("الطقس", "Weather"), weatherLabel(gMap.weather));
  snprintf(l2, sizeof l2, "%d %s  •  %d %s", gMap.trees, tr("شجرة", "trees"), gMap.buildings,
           tr("مبنى", "buildings"));
  ui::text(l1, facts.x + ui::dp(14), facts.y + ui::dp(12), 11.5f, ui::theme.textDim, innerW - ui::dp(28), 0, 1);
  ui::text(l2, facts.x + ui::dp(14), facts.y + ui::dp(32), 11.5f, ui::theme.textDim, innerW - ui::dp(28), 0, 1);
  float fx = facts.x + ui::dp(14);
  for (int i = 0; i < gMap.featureCount && i < 4; i++) {
    float tw = ui::textMeasure(gMap.featureNames[i], 10, 0, 1) + ui::dp(16);
    if (fx + tw > facts.x + facts.w - ui::dp(10)) break;
    ui::tag(Rect(fx, facts.y + ui::dp(56), tw, ui::dp(22)), gMap.featureNames[i], ui::theme.violet);
    fx += tw + ui::dp(6);
  }
  if (!gMap.hasWorld) {
    ui::text(tr("هذا العالم لم يُبنَ بعد", "This world has not been built yet"), facts.x + ui::dp(14),
             facts.y + ui::dp(58), 11, ui::theme.gold, innerW - ui::dp(28), 0, 1);
  }
  y += ui::dp(110);

  // ---- passes ----
  if (gMap.passes.count > 0) {
    sectionTitle(tr("تصاريح الماب", "Game passes"), pad, y, innerW);
    y += ui::dp(26);
    for (int i = 0; i < gMap.passes.count; i++) {
      Pass& p = gMap.passes[i];
      Rect row(pad, y, innerW, ui::dp(64));
      ui::card(row, false);
      ui::icon("lock", Rect(row.x + ui::dp(12), row.y + ui::dp(20), ui::dp(22), ui::dp(22)),
               p.owned ? ui::theme.green : ui::theme.gold);
      ui::text(p.name, row.x + ui::dp(44), row.y + ui::dp(12), 12.5f, ui::theme.text, innerW - ui::dp(150), 0, 2);
      ui::text(p.desc, row.x + ui::dp(44), row.y + ui::dp(33), 10.5f, ui::theme.textMuted, innerW - ui::dp(150), 0,
               1);
      char price[40];
      if (p.owned) snprintf(price, sizeof price, "%s", tr("مملوك", "Owned"));
      else snprintf(price, sizeof price, "%d Miy", p.price);
      ui::text(price, row.x + row.w - ui::dp(110), row.y + ui::dp(22), 12, p.owned ? ui::theme.green : ui::theme.gold,
               ui::dp(98), 2, 2);
      if (!p.owned) {
        Rect buy(row.x + row.w - ui::dp(110), row.y + ui::dp(40), ui::dp(98), ui::dp(4));
        (void)buy;
      }
      y += ui::dp(70);
    }
    y += ui::dp(6);
  }

  // ---- owner-only actions ----
  if (gMap.mine) {
    sectionTitle(tr("أدوات المالك", "Owner tools"), pad, y, innerW);
    y += ui::dp(26);
    Rect publish(pad, y, innerW, ui::dp(46));
    bool published = strcmp(gMap.status, "published") == 0;
    if (ui::button("md_publish", publish, published ? tr("إلغاء النشر", "Unpublish") : tr("نشر الماب", "Publish map"),
                   !published, gMap.hasWorld, 13.5f)
            .clicked) {
      char path[80];
      snprintf(path, sizeof path, "/api/maps/%s/%s", gMap.id, published ? "unpublish" : "publish");
      api.call(published ? NET_MAP_UPDATE : NET_MAP_PUBLISH, "POST", path, "{}", 32);
    }
    y += ui::dp(52);
    Rect del(pad, y, innerW, ui::dp(46));
    if (ui::button("md_delete", del, tr("حذف الماب", "Delete map"), false, true, 13).clicked) {
      char path[80];
      snprintf(path, sizeof path, "/api/maps/%s", gMap.id);
      api.call(NET_MAP_DELETE, "DELETE", path, nullptr, 33);
    }
    y += ui::dp(56);
  }

  ui::scrollFadeBottom(Rect(0, view.y + view.h - ui::dp(24), w, ui::dp(24)), scroll.offset < scroll.contentH - view.h);
}

}  // namespace mapDetail

void mapDetailApi(const ApiEvent& ev) {
  if (ev.op == (int)NET_MAP_DETAIL) {
    Json j;
    if (ev.status == 200 && ev.body && j.parse(ev.body)) {
      applyDetail(j.root());
    } else if (ev.body && j.parse(ev.body)) {
      toastError(j.root(), tr("تعذر تحميل الماب", "Could not load the map"));
    }
    return;
  }
  if (ev.op == (int)NET_MAP_FAVORITE && ev.status == 200) {
    gMap.favorite = !gMap.favorite;
    ui::toast(gMap.favorite ? tr("تم الحفظ", "Saved") : tr("تمت الإزالة", "Removed"));
    return;
  }
  if ((ev.op == (int)NET_MAP_PUBLISH || ev.op == (int)NET_MAP_UPDATE) && ev.tag == 32) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    if (ev.status == 200) {
      applyDetail(j.root());
      ui::toast(strcmp(gMap.status, "published") == 0 ? tr("تم نشر الماب", "Map published")
                                                      : tr("تم إلغاء النشر", "Unpublished"));
      audioPlay("ui_success", 0.6f);
    } else {
      toastError(j.root(), tr("تعذر تحديث الماب", "Update failed"));
    }
    return;
  }
  if (ev.op == (int)NET_MAP_DELETE && ev.tag == 33) {
    ui::toast(tr("تم حذف الماب", "Map deleted"));
    go(Screen::Discover);
    return;
  }
  if (ev.op == (int)NET_MAP_JOIN) {
    setBusy(false);
    if (ev.status != 200) {
      Json j;
      if (ev.body && j.parse(ev.body)) toastError(j.root(), tr("تعذر الدخول للماب", "Could not join"));
      return;
    }
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    JsonValue* root = j.root();
    JsonValue* map = Json::get(root, "map");
    const char* sessionId = Json::string(Json::get(root, "sessionId"));
    if (!map || !sessionId[0]) {
      ui::toast(tr("جلسة غير صالحة", "Invalid session"));
      return;
    }
    // gameplay parses the join payload itself, so it gets the response body verbatim
    char* raw = (char*)malloc(strlen(ev.body) + 1);
    if (!raw) return;
    strcpy(raw, ev.body);
    if (!game::Gameplay::get().enter(raw, Json::string(Json::get(map, "name")), sessionId,
                                     Json::string(Json::get(map, "id")))) {
      free(raw);
      return;
    }
    free(raw);
    refreshMe();
  }
}

}  // namespace screens
}  // namespace app
}  // namespace nx
