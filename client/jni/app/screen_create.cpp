// NEX AI creation — prompt, size/detail/quality presets, live staged progress, then studio.
#include "core/audio.h"
#include "game/gameplay.h"
#include "screens.h"
#include "widgets.h"
#include "ws.h"

namespace nx {
namespace app {
namespace screens {

namespace {

char gPrompt[1200] = {0};
char gName[80] = {0};
int gGameType = 0;
int gSizeIdx = 2;      // small, medium, large, huge
int gDetailIdx = 2;    // simple, detailed, ultra
int gQualityIdx = 2;   // low, medium, high, ultra
int gMaxPlayers = 18;
bool gSubmitting = false;

// job state
char gJobId[48] = {0};
int gProgress = 0;
char gStage[32] = {0};
char gStatus[16] = {0};
int gMapId = 0;
char gSummary[200] = {0};
char gError[200] = {0};
float gStagePulse = 0;
bool gShowAdvanced = false;

const char* kTypes[10] = {"open_world", "life_sim", "adventure", "racing", "survival",
                          "horror",     "combat",   "building",  "exploration", "social"};
const char* kTypeAr[10] = {"عالم مفتوح", "محاكاة حياة", "مغامرة", "سباقات", "بقاء",
                           "رعب",        "قتال",       "بناء",  "استكشاف", "اجتماعي"};
const char* kTypeEn[10] = {"Open world", "Life sim", "Adventure", "Racing", "Survival",
                           "Horror",     "Combat",   "Building",  "Exploration", "Social"};
const char* kSizes[4] = {"small", "medium", "large", "huge"};
const char* kSizeAr[4] = {"صغير", "متوسط", "كبير", "ضخم"};
const char* kSizeEn[4] = {"Small", "Medium", "Large", "Huge"};
const char* kSizeMeta[4][2] = {{"≈ 400م", "≈ 400m"}, {"≈ 800م", "≈ 800m"}, {"≈ 1200م", "≈ 1200m"},
                               {"≈ 2400م", "≈ 2400m"}};
const char* kDetails[3] = {"simple", "detailed", "ultra"};
const char* kDetailAr[3] = {"بسيط", "مفصّل", "فائق"};
const char* kDetailEn[3] = {"Simple", "Detailed", "Ultra"};
const char* kQualities[4] = {"low", "medium", "high", "ultra"};
const char* kQualityAr[4] = {"منخفض", "متوسط", "عالي", "AAA"};
const char* kQualityEn[4] = {"Low", "Medium", "High", "AAA"};

const char* S2(const char* ar, const char* en) { return S.language == 0 ? ar : en; }

// Real pipeline stages reported by the server, plus the client's own build step.
struct StageRow {
  const char* key;
  const char* ar;
  const char* en;
};
const StageRow kStages[] = {
    {"analyse", "تحليل الوصف", "Analysing prompt"},
    {"terrain", "توليد التضاريس", "Generating terrain"},
    {"water", "الأنهار والبحار", "Rivers and water"},
    {"biome", "النباتات والمناخ", "Biomes and climate"},
    {"buildings", "المباني والطرق", "Buildings and roads"},
    {"lighting", "الإضاءة والسماء", "Lighting and sky"},
    {"gameplay", "المهام والتفاعل", "Gameplay and quests"},
    {"validate", "التحقق النهائي", "Validating world"},
};

int stageIndex(const char* key) {
  for (int i = 0; i < (int)(sizeof(kStages) / sizeof(kStages[0])); i++) {
    if (strcmp(kStages[i].key, key) == 0) return i;
  }
  return -1;
}

void resetJobKeepError();

void resetJob() {
  gJobId[0] = 0;
  gProgress = 0;
  gStage[0] = 0;
  gStatus[0] = 0;
  gMapId = 0;
  gSummary[0] = 0;
  gError[0] = 0;
  gSubmitting = false;
}

void resetJobKeepError() {
  gJobId[0] = 0;
  gProgress = 0;
  gStage[0] = 0;
  gStatus[0] = 0;
  gSubmitting = false;
}

void submit() {
  if (strlen(gPrompt) < 12) {
    ui::toast(S2("اكتب وصفاً أطول (12 حرفاً على الأقل)", "Please describe the world in at least 12 characters"));
    audioPlay("ui_error", 0.6f);
    return;
  }
  resetJob();
  gSubmitting = true;
  setBusy(false);
  char body[1800];
  // escape the prompt: quotes and control characters would break the JSON
  char esc[1400];
  size_t o = 0;
  for (const char* p = gPrompt; *p && o + 8 < sizeof esc; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == '"' || c == '\\') { esc[o++] = '\\'; esc[o++] = (char)c; }
    else if (c == '\n' || c == '\r') { esc[o++] = ' '; }
    else if (c < 0x20) continue;
    else esc[o++] = (char)c;
  }
  esc[o] = 0;
  char nameEsc[120];
  o = 0;
  for (const char* p = gName; *p && o + 8 < sizeof nameEsc; p++) {
    if (*p == '"' || *p == '\\') { nameEsc[o++] = '\\'; nameEsc[o++] = *p; }
    else nameEsc[o++] = *p;
  }
  nameEsc[o] = 0;
  snprintf(body, sizeof body,
           "{\"prompt\":\"%s\",\"name\":\"%s\",\"gameType\":\"%s\",\"worldSize\":\"%s\",\"detail\":\"%s\","
           "\"quality\":\"%s\",\"maxPlayers\":%d}",
           esc, nameEsc[0] ? nameEsc : "", kTypes[gGameType], kSizes[gSizeIdx], kDetails[gDetailIdx],
           kQualities[gQualityIdx], gMaxPlayers);
  api.call(NET_AI_GENERATE, "POST", "/api/ai/generate", body);
}

void drawStages(float x, float y, float w) {
  int cur = stageIndex(gStage);
  if (strcmp(gStatus, "done") == 0) cur = (int)(sizeof(kStages) / sizeof(kStages[0]));
  for (int i = 0; i < (int)(sizeof(kStages) / sizeof(kStages[0])); i++) {
    bool done = i < cur;
    bool active = i == cur;
    Color tint = done ? ui::theme.green : active ? ui::theme.cyan : ui::theme.textMuted;
    if (active) {
      // pulsing dot for the stage in flight
      float p = 0.5f + 0.5f * sinf(gStagePulse * 4.0f);
      ui::glow(Rect(x - ui::dp(3), y + i * ui::dp(24) - ui::dp(3), ui::dp(14), ui::dp(14)),
               ui::theme.cyan.withA(0.35f * p + 0.1f), 1.0f);
    }
    Rect dot(x, y + i * ui::dp(24) + ui::dp(2), ui::dp(8), ui::dp(8));
    if (done) ui::icon("check", Rect(dot.x - ui::dp(4), dot.y - ui::dp(4), ui::dp(16), ui::dp(16)), tint);
    else ui::rect(dot, tint, ui::dp(4));
    ui::text(S2(kStages[i].ar, kStages[i].en), x + ui::dp(20), y + i * ui::dp(24), 11.5f, tint, w - ui::dp(24), 0,
             active ? 2 : 1);
  }
}

}  // namespace

namespace createAi {

void enter() {
  resetJob();
  api.call(NET_AI_PRESETS, "GET", "/api/ai/presets", nullptr, 41);
}

void leave() {}

void draw(float dt) {
  gStagePulse += dt;
  Engine& e = Engine::get();
  float w = e.width();
  float pad = ui::dp(16);
  float innerW = w - pad * 2;
  background(dt);

  HeaderOpts ho;
  ho.title = S2("NEX AI", "NEX AI");
  ho.backButton = true;
  header(ho);

  float top = ui::dp(74);
  bool generating = gJobId[0] != 0 && strcmp(gStatus, "done") != 0 && strcmp(gStatus, "failed") != 0;

  ui::ScrollView scroll;
  scroll.viewH = e.height() - top;
  scroll.contentH = generating ? ui::dp(420) : ui::dp(900);
  Rect view(0, top, w, scroll.viewH);
  scroll.update(view, dt, generating ? false : true);
  Rect content = scroll.content(view);
  float y = content.y + ui::dp(8);

  if (generating) {
    // ---- progress view ----
    Rect card(pad, y, innerW, ui::dp(300));
    ui::card(card, true);
    ui::rect(Rect(card.x + card.w * 0.5f - ui::dp(24), card.y + ui::dp(24), ui::dp(48), ui::dp(48)),
             Color(0.16f, 0.10f, 0.38f, 1.0f), ui::dp(24));
    ui::glow(Rect(card.x + card.w * 0.5f - ui::dp(54), card.y - ui::dp(6), ui::dp(108), ui::dp(108)),
             ui::theme.violet.withA(0.28f), 1.0f);
    ui::icon("sparkle", Rect(card.x + card.w * 0.5f - ui::dp(13), card.y + ui::dp(35), ui::dp(26), ui::dp(26)),
             ui::theme.cyan);
    ui::text(S2("NEX AI يبني عالمك", "NEX AI is building your world"), card.x, card.y + ui::dp(84), 16,
             ui::theme.text, card.w, 1, 2);
    char pline[220];
    int shown = (int)strlen(gPrompt);
    if (shown > 64) {
      snprintf(pline, sizeof pline, "…%.58s", gPrompt + shown - 58);
    } else {
      snprintf(pline, sizeof pline, "%s", gPrompt);
    }
    ui::text(pline, card.x + ui::dp(16), card.y + ui::dp(108), 11, ui::theme.textDim, card.w - ui::dp(32), 1, 1);

    Rect bar(card.x + ui::dp(20), card.y + ui::dp(134), card.w - ui::dp(40), ui::dp(7));
    ui::rect(bar, Color(1, 1, 1, 0.08f), ui::dp(4));
    ui::rect(Rect(bar.x, bar.y, bar.w * (clampf(gProgress / 100.0f, 0, 1)), bar.h), ui::theme.violet, ui::dp(4));
    char pct[16];
    snprintf(pct, sizeof pct, "%d%%", gProgress);
    ui::text(pct, card.x + ui::dp(20), card.y + ui::dp(148), 11, ui::theme.textDim, 0, 0, 1);

    drawStages(card.x + ui::dp(22), card.y + ui::dp(172), card.w - ui::dp(44));

    Rect cancel(pad, card.y + card.h + ui::dp(12), innerW, ui::dp(44));
    if (ui::button("ai_cancel", cancel, S2("إخفاء (يستمر في الخلفية)", "Hide (keeps building)"), false, true, 12)
            .clicked) {
      goBack();
    }
    return;
  }

  if (strcmp(gStatus, "done") == 0) {
    // ---- success ----
    Rect card(pad, y, innerW, ui::dp(240));
    ui::card(card, true, ui::theme.green);
    ui::icon("check", Rect(card.x + ui::dp(20), card.y + ui::dp(20), ui::dp(28), ui::dp(28)), ui::theme.green);
    ui::text(S2("العالم جاهز", "Your world is ready"), card.x + ui::dp(58), card.y + ui::dp(20), 17, ui::theme.text,
             card.w - ui::dp(74), 0, 2);
    ui::text(gSummary, card.x + ui::dp(20), card.y + ui::dp(58), 11.5f, ui::theme.textDim, card.w - ui::dp(40), 0,
             1);
    Rect play(card.x + ui::dp(20), card.y + ui::dp(120), card.w - ui::dp(40), ui::dp(48));
    if (ui::button("ai_play", play, S2("جرّب اللعب الآن", "Play it now"), true, true, 14, "play").clicked) {
      enterMap(gMapId);
    }
    Rect studio(card.x + ui::dp(20), card.y + ui::dp(176), card.w - ui::dp(40), ui::dp(44));
    if (ui::button("ai_studio", studio, S2("افتح الاستوديو", "Open the studio"), false, true, 13, "gear").clicked) {
      go(Screen::Studio);
    }
    return;
  }

  // ---- composer ----
  ui::text(S2("صِف عالمك", "Describe your world"), pad, y, 20, ui::theme.text, innerW, 0, 2);
  ui::text(S2("اكتب الفكرة، المناخ، الطابع، وما تريد أن يفعله اللاعبون.",
              "Setting, mood, and what players should do in it."),
           pad, y + ui::dp(28), 11.5f, ui::theme.textDim, innerW, 0, 1);
  y += ui::dp(52);

  Rect promptBox(pad, y, innerW, ui::dp(132));
  ui::rect(promptBox, Color(1, 1, 1, 0.045f), ui::dp(14));
  ui::rectStroke(promptBox, ui::focusedId()[0] && strcmp(ui::focusedId(), "ai_prompt") == 0
                                ? ui::theme.violet.withA(0.7f)
                                : ui::theme.strokeSoft,
                 1.0f, ui::dp(14));
  ui::textField("ai_prompt", promptBox.inset(ui::dp(10)), gPrompt, sizeof gPrompt,
                S2("مثال: مدينة نيون ليلية تحت المطر، أبراج زجاجية، وطرق سريعة تربط الأحياء…",
                   "e.g. a neon city at night in the rain, glass towers and highways between districts…"),
                false, 1000);
  char counter[32];
  snprintf(counter, sizeof counter, "%d / 1000", (int)strlen(gPrompt));
  ui::text(counter, promptBox.x + promptBox.w - ui::dp(70), promptBox.y + promptBox.h - ui::dp(18), 9.5f,
           ui::theme.textMuted, ui::dp(60), 2, 1);
  y += promptBox.h + ui::dp(14);

  // quick idea chips: one tap writes a full prompt
  const char* ideas[4][2] = {{"جزيرة استوائية بغابة كثيفة وشلالات", "tropical island with dense jungle and waterfalls"},
                             {"مدينة مستقبلية بأبراج نيون ومترو معلق", "future city with neon towers and a sky metro"},
                             {"قرية جبلية ثلجية فيها كهوف ومهام", "snowy mountain village with caves and quests"},
                             {"صحراء بآثار قديمة وواحات سرية", "desert with ancient ruins and hidden oases"}};
  float chipX = pad;
  for (int i = 0; i < 4; i++) {
    const char* label = S2(ideas[i][0], ideas[i][1]);
    float tw = ui::textMeasure(label, 10, 0, 1) + ui::dp(20);
    if (chipX + tw > pad + innerW) { chipX = pad; y += ui::dp(32); }
    Rect c(chipX, y, tw, ui::dp(28));
    ui::tag(c, label, ui::theme.violet);
    ui::Pointer& p = ui::pointer();
    if (c.contains(p.pos.x, p.pos.y) && p.released && p.start.distance(p.pos) < ui::dp(20)) {
      snprintf(gPrompt, sizeof gPrompt, "%s", label);
      audioPlay("ui_click", 0.4f);
    }
    chipX += tw + ui::dp(6);
  }
  y += ui::dp(44);

  // ---- type picker ----
  sectionTitle(S2("نوع التجربة", "Experience type"), pad, y + ui::dp(4), innerW);
  y += ui::dp(30);
  {
    float gap = ui::dp(6);
    float cw = (innerW - gap * 4) / 5.0f;
    for (int i = 0; i < 10; i++) {
      int row = i / 5, col = i % 5;
      Rect c(pad + col * (cw + gap), y + row * (ui::dp(38) + gap), cw, ui::dp(38));
      bool sel = gGameType == i;
      ui::rect(c, sel ? ui::theme.violet.withA(0.9f) : Color(1, 1, 1, 0.045f), ui::dp(10));
      if (!sel) ui::rectStroke(c, ui::theme.strokeSoft, 1.0f, ui::dp(10));
      ui::text(S2(kTypeAr[i], kTypeEn[i]), c.x, c.y + ui::dp(11), 10, sel ? Color(1, 1, 1) : ui::theme.textDim,
               c.w, 1, sel ? 2 : 1);
      ui::Pointer& p = ui::pointer();
      if (c.contains(p.pos.x, p.pos.y) && p.released && p.start.distance(p.pos) < ui::dp(20)) {
        gGameType = i;
        audioPlay("ui_click", 0.35f);
      }
    }
    y += ui::dp(38) * 2 + gap + ui::dp(16);
  }

  // ---- size ----
  sectionTitle(S2("حجم العالم", "World size"), pad, y, innerW);
  y += ui::dp(28);
  {
    float gap = ui::dp(8);
    float cw = (innerW - gap * 3) / 4.0f;
    for (int i = 0; i < 4; i++) {
      Rect c(pad + i * (cw + gap), y, cw, ui::dp(56));
      bool sel = gSizeIdx == i;
      ui::rect(c, sel ? ui::theme.violet.withA(0.85f) : Color(1, 1, 1, 0.04f), ui::dp(12));
      if (!sel) ui::rectStroke(c, ui::theme.strokeSoft, 1.0f, ui::dp(12));
      ui::text(S2(kSizeAr[i], kSizeEn[i]), c.x, c.y + ui::dp(9), 11.5f, sel ? Color(1, 1, 1) : ui::theme.text,
               c.w, 1, 2);
      ui::text(S2(kSizeMeta[i][0], kSizeMeta[i][1]), c.x, c.y + ui::dp(30), 9.5f,
               sel ? Color(1, 1, 1, 0.8f) : ui::theme.textMuted, c.w, 1, 1);
      ui::Pointer& p = ui::pointer();
      if (c.contains(p.pos.x, p.pos.y) && p.released && p.start.distance(p.pos) < ui::dp(20)) {
        gSizeIdx = i;
        audioPlay("ui_click", 0.35f);
      }
    }
    y += ui::dp(56) + ui::dp(16);
  }

  // ---- detail + quality ----
  sectionTitle(S2("مستوى التفاصيل", "Detail level"), pad, y, innerW);
  y += ui::dp(26);
  {
    const char* labels[3] = {S2(kDetailAr[0], kDetailEn[0]), S2(kDetailAr[1], kDetailEn[1]),
                             S2(kDetailAr[2], kDetailEn[2])};
    int r = ui::segments("ai_detail", Rect(pad, y, innerW, ui::dp(34)), labels, 3, gDetailIdx, 11.5f);
    if (r >= 0) gDetailIdx = r;
    y += ui::dp(46);
  }
  sectionTitle(S2("جودة العرض", "Render quality"), pad, y, innerW);
  y += ui::dp(26);
  {
    const char* labels[4] = {S2(kQualityAr[0], kQualityEn[0]), S2(kQualityAr[1], kQualityEn[1]),
                             S2(kQualityAr[2], kQualityEn[2]), S2(kQualityAr[3], kQualityEn[3])};
    int r = ui::segments("ai_quality", Rect(pad, y, innerW, ui::dp(34)), labels, 4, gQualityIdx, 11.5f);
    if (r >= 0) gQualityIdx = r;
    y += ui::dp(46);
  }

  // ---- advanced ----
  Rect adv(pad, y, innerW, ui::dp(40));
  if (ui::row("ai_adv", adv, "gear", S2("إعدادات متقدمة", "Advanced"), S2("اسم الماب وعدد اللاعبين",
                                                                           "Map name and player cap"))
          .clicked) {
    gShowAdvanced = !gShowAdvanced;
  }
  y += ui::dp(48);
  if (gShowAdvanced) {
    ui::text(S2("اسم الماب (اختياري)", "Map name (optional)"), pad, y, 11, ui::theme.textDim, innerW, 0, 1);
    Rect nameBox(pad, y + ui::dp(18), innerW, ui::dp(44));
    ui::rect(nameBox, Color(1, 1, 1, 0.045f), ui::dp(12));
    ui::rectStroke(nameBox, ui::theme.strokeSoft, 1.0f, ui::dp(12));
    ui::textField("ai_name", nameBox.inset(ui::dp(10)), gName, sizeof gName, S2("ماب بلا اسم", "Untitled world"),
                  false, 60);
    y += ui::dp(74);
    char capLabel[64];
    snprintf(capLabel, sizeof capLabel, "%s: %d", S2("الحد الأقصى للاعبين", "Players per room"), gMaxPlayers);
    ui::text(capLabel, pad, y, 11.5f, ui::theme.textDim, innerW, 0, 1);
    float t = ui::slider("ai_players", Rect(pad, y + ui::dp(20), innerW, ui::dp(30)),
                         (gMaxPlayers - 1) / 59.0f, 0, 1);
    gMaxPlayers = 1 + (int)(t * 59.0f + 0.5f);
    y += ui::dp(64);
  }

  // ---- submit ----
  if (gError[0]) {
    ui::text(gError, pad, y, 11.5f, ui::theme.red, innerW, 0, 1);
    y += ui::dp(20);
  }
  Rect submitBtn(pad, y + ui::dp(4), innerW, ui::dp(52));
  if (ui::button("ai_submit", submitBtn, S2("ابنِ العالم", "Build the world"), true, !gSubmitting, 15, "sparkle")
          .clicked) {
    submit();
  }
  y += ui::dp(64);
  ui::text(S2("توليد كامل قد يستغرق حتى دقيقة. يمكنك إخفاء الشاشة والمتابعة.",
              "A full build can take up to a minute. You can hide this screen and keep going."),
           pad, y, 10, ui::theme.textMuted, innerW, 0, 1);
}

}  // namespace createAi

void createAiApi(const ApiEvent& ev) {
  if (ev.op == (int)NET_AI_GENERATE) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) {
      gError[0] = 0;
      snprintf(gError, sizeof gError, "%s", tr("تعذر الاتصال بالخادم", "Cannot reach the server"));
      return;
    }
    if (ev.status != 202 && ev.status != 200) {
      snprintf(gError, sizeof gError, "%s", apiMessage(j.root(), tr("تعذر بدء التوليد", "Could not start")));
      audioPlay("ui_error", 0.6f);
      return;
    }
    snprintf(gJobId, sizeof gJobId, "%s", Json::string(Json::get(j.root(), "jobId")));
    gMapId = 0;
    gMapId = atoi(Json::string(Json::get(j.root(), "mapId")) + 4);
    gStatus[0] = 0;
    gProgress = 2;
    snprintf(gStage, sizeof gStage, "analyse");
  } else if (ev.op == (int)NET_AI_JOB) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    JsonValue* job = Json::get(j.root(), "job");
    if (!job) return;
    snprintf(gStatus, sizeof gStatus, "%s", Json::string(Json::get(job, "status"), "queued"));
    gProgress = Json::integer(Json::get(job, "progress"), gProgress);
    const char* stage = Json::string(Json::get(job, "stage"), "analyse");
    if (strcmp(stage, gStage) != 0) {
      snprintf(gStage, sizeof gStage, "%s", stage);
      audioPlay("ai_tick", 0.5f);
    }
    snprintf(gSummary, sizeof gSummary, "%s", Json::string(Json::get(job, "summary")));
    if (strcmp(gStatus, "failed") == 0) {
      snprintf(gError, sizeof gError, "%s", Json::string(Json::get(job, "error"), tr("فشل التوليد", "Build failed")));
      resetJobKeepError();
      audioPlay("ui_error", 0.6f);
    } else if (strcmp(gStatus, "done") == 0) {
      gProgress = 100;
      audioPlay("ai_complete", 0.8f);
      ui::toast(tr("تم بناء العالم", "World built"), 3.0f);
      refreshMe();
    }
  } else if (ev.op == (int)NET_AI_PRESETS) {
    // server-side preset list is the source of truth; the local tables mirror it
  }
}


// Polling + back handling, exported through the router hooks below.
static void pollJob(float dt) {
  static float accum = 0;
  if (gJobId[0] && strcmp(gStatus, "done") != 0 && strcmp(gStatus, "failed") != 0) {
    accum += dt;
    if (accum > 0.45f) {
      accum = 0;
      char path[96];
      snprintf(path, sizeof path, "/api/ai/jobs/%s", gJobId);
      api.call(NET_AI_JOB, "GET", path, nullptr);
    }
  }
}

static bool backBlocked() {
  if (gJobId[0] && strcmp(gStatus, "done") != 0 && strcmp(gStatus, "failed") != 0) {
    // the job keeps running server side; leaving only hides the progress view
    ui::toast(tr("التوليد يستمر في الخلفية", "Generation continues in the background"));
  }
  return false;
}

void createAiText(const char*) {}
bool createAiBack() { return backBlocked(); }
void createAiPoll(float dt) { pollJob(dt); }

}  // namespace screens
}  // namespace app
}  // namespace nx
