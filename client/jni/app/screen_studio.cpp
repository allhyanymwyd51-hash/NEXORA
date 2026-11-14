// Studio — per-map control room: world summary, NEX AI edits, passes, publish state.
#include "core/audio.h"
#include "game/gameplay.h"
#include "screens.h"
#include "widgets.h"
#include "ws.h"

namespace nx {
namespace app {
namespace screens {

namespace {

struct Owned {
  char id[40];
  char name[80];
  char status[16];
  int visits;
  int version;
  bool isAi;
};
Array<Owned> gMine;
bool gLoaded = false;
int gSelected = -1;
char gEditPrompt[300] = {0};
bool gEditing = false;
char gLastChange[200] = {0};
int gTab = 0;
ui::ScrollView gScroll;

// pass creation form
char gPassName[60] = {0};
char gPassDesc[140] = {0};
int gPassPrice = 50;
bool gPassForm = false;

const char* selectedMapId() {
  if (gSelected < 0 || gSelected >= gMine.count) return "";
  return gMine[gSelected].id;
}

void reload() {
  api.call(NET_MY_MAPS, "GET", "/api/maps/mine", nullptr, 51);
}

}  // namespace

namespace studio {

void enter() {
  gLoaded = false;
  gSelected = -1;
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
  ho.title = tr("الاستوديو", "Studio");
  header(ho);

  float top = ui::dp(74);
  float navH = ui::dp(72) + e.safeBottom();
  Rect view(0, top, w, e.height() - top - navH);

  if (!gLoaded) {
    ui::text(tr("جاري التحميل…", "Loading…"), 0, view.y + ui::dp(40), 12, ui::theme.textMuted, w, 1, 1);
    return;
  }
  if (gMine.count == 0) {
    if (ui::emptyState(Rect(0, view.y, w, view.h), "sparkle", tr("لا مابات في الاستوديو", "Your studio is empty"),
                       tr("أنشئ عالماً بـ NEX AI، ثم عد هنا لنشره وإدارته وبيع التصاريح.",
                          "Create a world with NEX AI, then come back to publish and manage it."),
                       "studio_empty_cta", tr("أنشئ عالماً", "Create a world"))) {
      go(Screen::CreateAi);
    }
    bottomNav(2);
    return;
  }

  // ---- picker ----
  float listH = ui::dp(88);
  float y = view.y + ui::dp(6);
  ui::ScrollView pickScroll;
  pickScroll.viewH = listH;
  pickScroll.contentH = gMine.count * (ui::dp(52) + ui::dp(6));
  pickScroll.update(Rect(0, y, w, listH), dt, true);
  Rect pickContent = pickScroll.content(Rect(0, y, w, listH));
  for (int i = 0; i < gMine.count; i++) {
    Rect r(pad, pickContent.y + i * (ui::dp(52) + ui::dp(6)), innerW, ui::dp(52));
    bool sel = gSelected == i;
    ui::rect(r, sel ? ui::theme.violet.withA(0.22f) : Color(1, 1, 1, 0.04f), ui::dp(12));
    if (sel) ui::rectStroke(r, ui::theme.violet.withA(0.5f), 1.2f, ui::dp(12));
    ui::text(gMine[i].name, r.x + ui::dp(12), r.y + ui::dp(9), 12.5f, ui::theme.text, innerW - ui::dp(140), 0, 2);
    char meta[80];
    snprintf(meta, sizeof meta, "%s • v%d • %d %s", strcmp(gMine[i].status, "published") == 0
                                                     ? tr("منشور", "published")
                                                     : tr("مسودة", "draft"),
             gMine[i].version, gMine[i].visits, tr("زيارة", "visits"));
    ui::text(meta, r.x + ui::dp(12), r.y + ui::dp(29), 10, ui::theme.textMuted, innerW - ui::dp(140), 0, 1);
    if (gMine[i].isAi) ui::tag(Rect(r.x + r.w - ui::dp(56), r.y + ui::dp(16), ui::dp(44), ui::dp(20)), "NEX AI",
                               ui::theme.cyan);
    ui::Pointer& p = ui::pointer();
    if (r.contains(p.pos.x, p.pos.y) && p.released && p.start.distance(p.pos) < ui::dp(20)) {
      gSelected = i;
      gTab = 0;
      audioPlay("ui_click", 0.4f);
    }
  }
  y += listH + ui::dp(8);

  if (gSelected < 0) {
    ui::text(tr("اختر ماباً لإدارته", "Pick a map to manage"), 0, y, 11.5f, ui::theme.textMuted, w, 1, 1);
    bottomNav(2);
    return;
  }

  // ---- tabs ----
  const char* tabs[3] = {tr("العالم", "World"), tr("تحرير", "Edit"), tr("التصاريح", "Passes")};
  int picked = ui::segments("studio_tabs", Rect(pad, y, innerW, ui::dp(34)), tabs, 3, gTab, 11.5f);
  if (picked >= 0) gTab = picked;
  y += ui::dp(44);

  Rect body(0, y, w, e.height() - y - navH);
  gScroll.viewH = body.h;
  gScroll.contentH = gTab == 0 ? ui::dp(300) : gTab == 1 ? ui::dp(360) : ui::dp(420);
  gScroll.update(body, dt, true);
  Rect content = gScroll.content(body);
  float cy = content.y;

  Owned& m = gMine[gSelected];

  if (gTab == 0) {
    Rect card(pad, cy, innerW, ui::dp(120));
    ui::card(card, false, strcmp(m.status, "published") == 0 ? ui::theme.green : ui::theme.gold);
    ui::text(m.name, card.x + ui::dp(14), card.y + ui::dp(12), 15, ui::theme.text, card.w - ui::dp(28), 0, 2);
    char line[140];
    snprintf(line, sizeof line, "%s • v%d", strcmp(m.status, "published") == 0 ? tr("منشور", "Published")
                                                                              : tr("مسودة غير منشورة", "Draft"),
             m.version);
    ui::text(line, card.x + ui::dp(14), card.y + ui::dp(34), 11, ui::theme.textDim, card.w - ui::dp(28), 0, 1);
    snprintf(line, sizeof line, "%d %s", m.visits, tr("زيارة", "visits"));
    ui::text(line, card.x + ui::dp(14), card.y + ui::dp(54), 11, ui::theme.textMuted, card.w - ui::dp(28), 0, 1);

    Rect play(card.x + ui::dp(14), card.y + ui::dp(76), (card.w - ui::dp(36)) * 0.5f, ui::dp(34));
    if (ui::button("studio_play", play, tr("تجربة", "Test"), true, true, 11.5f, "play").clicked) {
      enterMap(m.id);
    }
    Rect pub(card.x + ui::dp(14) + play.w + ui::dp(8), card.y + ui::dp(76), play.w, ui::dp(34));
    bool published = strcmp(m.status, "published") == 0;
    if (ui::button("studio_pub", pub, published ? tr("إلغاء النشر", "Unpublish") : tr("نشر", "Publish"), !published,
                   true, 11.5f)
            .clicked) {
      char path[80];
      snprintf(path, sizeof path, "/api/maps/%s/%s", m.id, published ? "unpublish" : "publish");
      api.call(published ? NET_MAP_UPDATE : NET_MAP_PUBLISH, "POST", path, "{}", 52);
    }
    cy += ui::dp(132);

    Rect publishNote(pad, cy, innerW, ui::dp(70));
    ui::card(publishNote, false);
    ui::icon("eye", Rect(publishNote.x + ui::dp(12), publishNote.y + ui::dp(24), ui::dp(22), ui::dp(22)),
             ui::theme.textMuted);
    ui::text(tr("عند النشر يظهر الماب في «اكتشف» ويمكن للاعبين الانضمام.",
                "Publishing lists the map in Discover so players can join."),
             publishNote.x + ui::dp(44), publishNote.y + ui::dp(18), 11, ui::theme.textDim,
             publishNote.w - ui::dp(58), 0, 1);
    cy += ui::dp(82);

    Rect del(pad, cy, innerW, ui::dp(44));
    if (ui::button("studio_del", del, tr("حذف الماب نهائياً", "Delete map permanently"), false, true, 12).clicked) {
      char path[80];
      snprintf(path, sizeof path, "/api/maps/%s", m.id);
      api.call(NET_MAP_DELETE, "DELETE", path, nullptr, 53);
    }
  } else if (gTab == 1) {
    ui::text(tr("عدّل العالم بجملة واحدة", "Change the world with one sentence"), pad, cy, 13, ui::theme.text, innerW,
             0, 2);
    ui::text(tr("مثال: أضف بحيرة كبيرة في المنتصف وزد الأشجار حولها",
                "e.g. add a large lake in the middle and more trees around it"),
             pad, cy + ui::dp(22), 10.5f, ui::theme.textMuted, innerW, 0, 1);
    Rect box(pad, cy + ui::dp(44), innerW, ui::dp(104));
    ui::rect(box, Color(1, 1, 1, 0.045f), ui::dp(13));
    ui::rectStroke(box, ui::theme.strokeSoft, 1.0f, ui::dp(13));
    ui::textField("studio_prompt", box.inset(ui::dp(10)), gEditPrompt, sizeof gEditPrompt,
                  tr("اكتب التعديل المطلوب…", "Describe the change…"), false, 240);
    cy += ui::dp(160);

    if (gLastChange[0]) {
      Rect res(pad, cy, innerW, ui::dp(52));
      ui::card(res, false, ui::theme.green);
      ui::text(gLastChange, res.x + ui::dp(14), res.y + ui::dp(12), 11, ui::theme.textDim, res.w - ui::dp(28), 0, 1);
      cy += ui::dp(62);
    }

    Rect apply(pad, cy, innerW, ui::dp(48));
    bool canEdit = gEditPrompt[0] != 0 && !gEditing;
    if (ui::button("studio_apply", apply, gEditing ? tr("جاري التعديل…", "Applying…") : tr("طبّق التعديل", "Apply"),
                   true, canEdit, 13.5f, "sparkle")
            .clicked) {
      gEditing = true;
      char body[900], esc[520];
      escapeJson(gEditPrompt, esc, sizeof esc);  // free text: quotes would break the body
      snprintf(body, sizeof body, "{\"mapId\":\"%s\",\"prompt\":\"%s\"}", m.id, esc);
      api.call(NET_AI_MODIFY, "POST", "/api/ai/modify", body);
    }
  } else {
    // passes
    if (gEditing) {
      ui::text(tr("جاري إنشاء التصريح…", "Creating…"), pad, cy, 12, ui::theme.textDim, innerW, 0, 1);
      cy += ui::dp(24);
    }
    if (!gPassForm) {
      Rect add(pad, cy, innerW, ui::dp(46));
      if (ui::button("studio_pass_add", add, tr("تصريح جديد", "New game pass"), true, true, 13, "plus").clicked) {
        gPassForm = true;
      }
      cy += ui::dp(56);
    } else {
      ui::text(tr("اسم التصريح", "Pass name"), pad, cy, 11, ui::theme.textDim, innerW, 0, 1);
      Rect n(pad, cy + ui::dp(18), innerW, ui::dp(42));
      ui::rect(n, Color(1, 1, 1, 0.045f), ui::dp(11));
      ui::textField("studio_pass_name", n.inset(ui::dp(10)), gPassName, sizeof gPassName,
                    tr("مثال: VIP", "e.g. VIP"), false, 40);
      cy += ui::dp(70);
      ui::text(tr("الوصف", "Description"), pad, cy, 11, ui::theme.textDim, innerW, 0, 1);
      Rect d(pad, cy + ui::dp(18), innerW, ui::dp(42));
      ui::rect(d, Color(1, 1, 1, 0.045f), ui::dp(11));
      ui::textField("studio_pass_desc", d.inset(ui::dp(10)), gPassDesc, sizeof gPassDesc,
                    tr("ما يحصل عليه المشتري", "What the buyer gets"), false, 120);
      cy += ui::dp(72);
      char pl[64];
      snprintf(pl, sizeof pl, "%s: %d Miy", tr("السعر", "Price"), gPassPrice);
      ui::text(pl, pad, cy, 11.5f, ui::theme.textDim, innerW, 0, 1);
      float t = ui::slider("studio_pass_price", Rect(pad, cy + ui::dp(20), innerW, ui::dp(30)),
                           (gPassPrice - 5) / 995.0f, 0, 1);
      gPassPrice = 5 + (int)(t * 995.0f + 0.5f);
      cy += ui::dp(62);
      Rect create(pad, cy, (innerW - ui::dp(8)) * 0.6f, ui::dp(44));
      if (ui::button("studio_pass_create", create, tr("إنشاء", "Create"), true,
                     gPassName[0] != 0 && !gEditing, 12.5f)
              .clicked) {
        gEditing = true;
        char body[700];
        char nEs[200], dEs[340];
        escapeJson(gPassName, nEs, sizeof nEs);
        escapeJson(gPassDesc, dEs, sizeof dEs);
        snprintf(body, sizeof body, "{\"name\":\"%s\",\"description\":\"%s\",\"price\":%d}", nEs, dEs,
                 gPassPrice);
        char path[80];
        snprintf(path, sizeof path, "/api/maps/%s/passes", m.id);
        api.call(NET_PASS_CREATE, "POST", path, body, 54);
      }
      Rect cancel(pad + create.w + ui::dp(8), cy, innerW - create.w - ui::dp(8), ui::dp(44));
      if (ui::button("studio_pass_cancel", cancel, tr("إلغاء", "Cancel"), false, true, 12).clicked) {
        gPassForm = false;
      }
      cy += ui::dp(56);
    }
    ui::text(tr("تُحتسب مبيعات التصاريح على الخادم ويحصل المالك على 70% من كل عملية.",
                "Sales are verified server side and the creator keeps 70%."),
             pad, cy, 10, ui::theme.textMuted, innerW, 0, 1);
  }

  bottomNav(2);
}

}  // namespace studio

void studioApi(const ApiEvent& ev) {
  if (ev.op == (int)NET_MY_MAPS) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    gMine.clear();
    JsonValue* maps = Json::get(j.root(), "maps");
    for (int i = 0; i < Json::length(maps); i++) {
      JsonValue* m = Json::at(maps, i);
      Owned o{};
      snprintf(o.id, sizeof o.id, "%s", Json::string(Json::get(m, "id")));
      snprintf(o.name, sizeof o.name, "%s", Json::string(Json::get(m, "name")));
      snprintf(o.status, sizeof o.status, "%s", Json::string(Json::get(m, "status"), "draft"));
      o.visits = Json::integer(Json::get(m, "visits"));
      o.version = Json::integer(Json::get(m, "version"), 1);
      o.isAi = Json::boolean(Json::get(m, "isAi"));
      gMine.push(o);
    }
    gLoaded = true;
    if (gSelected < 0 && gMine.count > 0) gSelected = 0;
    return;
  }
  if (ev.op == (int)NET_MAP_PUBLISH || (ev.op == (int)NET_MAP_UPDATE && ev.tag == 52) ||
      ev.op == (int)NET_MAP_DELETE) {
    Json j;
    if (ev.body) j.parse(ev.body);
    if (ev.status == 200) {
      ui::toast(ev.op == (int)NET_MAP_DELETE ? tr("تم الحذف", "Deleted") : tr("تم التحديث", "Updated"));
      gSelected = ev.op == (int)NET_MAP_DELETE ? -1 : gSelected;
      reload();
    } else if (ev.body) {
      toastError(j.root(), tr("تعذر التحديث", "Update failed"));
    }
    return;
  }
  if (ev.op == (int)NET_AI_MODIFY) {
    gEditing = false;
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    if (ev.status == 200) {
      snprintf(gLastChange, sizeof gLastChange, "%s", Json::string(Json::get(j.root(), "summary"),
                                                                  tr("تم تطبيق التعديل", "Change applied")));
      gEditPrompt[0] = 0;
      ui::toast(tr("تم تعديل العالم", "World updated"));
      audioPlay("ai_complete", 0.7f);
      reload();
    } else {
      snprintf(gLastChange, sizeof gLastChange, "%s", apiMessage(j.root(), tr("تعذر التعديل", "Edit failed")));
    }
    return;
  }
  if (ev.op == (int)NET_PASS_CREATE) {
    gEditing = false;
    Json j;
    if (ev.body) j.parse(ev.body);
    if (ev.status == 200 || ev.status == 201) {
      gPassForm = false;
      gPassName[0] = 0;
      gPassDesc[0] = 0;
      ui::toast(tr("تم إنشاء التصريح", "Pass created"));
      audioPlay("ui_success", 0.6f);
    } else if (ev.body) {
      toastError(j.root(), tr("تعذر إنشاء التصريح", "Could not create the pass"));
    }
    return;
  }
  if (ev.op == (int)NET_MAP_JOIN) {
    setBusy(false);
    if (ev.status != 200) return;
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    JsonValue* map = Json::get(j.root(), "map");
    if (!map) return;
    char* raw = (char*)malloc(strlen(ev.body) + 1);
    if (!raw) return;
    strcpy(raw, ev.body);
    game::Gameplay::get().enter(raw, Json::string(Json::get(map, "name")),
                                Json::string(Json::get(j.root(), "sessionId")),
                                Json::string(Json::get(map, "id")));
    free(raw);
  }
}

}  // namespace screens
}  // namespace app
}  // namespace nx
