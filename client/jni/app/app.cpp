// NEXORA app shell: session state, screen stack, input plumbing, API event dispatch.
#include "app.h"
#include "state.h"
#include "screens.h"
#include "ws.h"

#include "core/audio.h"
#include "core/net.h"
#include "game/gameplay.h"

namespace nx {
namespace app {

State S;

namespace {

const int kStackMax = 20;
Screen gStack[kStackMax];
int gDepth = 0;
bool gBusy = false;
char gBusyLabel[80] = {0};
float gFrameAnim = 0;

// one-touch -> UI pointer state
Vec2 gLastPos(-1, -1);
bool gHadDown = false;
int gActiveTouchId = -1;

char gSessionPath[600];

bool isRtlInternal() { return S.language == 0; }

}  // namespace

bool isRtl() { return isRtlInternal(); }
bool isOwner() { return S.role == Role::Owner; }

// ---------------------------------------------------------------------------
// settings persistence
// ---------------------------------------------------------------------------
void loadLocalSettings() {
  snprintf(gSessionPath, sizeof gSessionPath, "%s/nexora.json", filesDir());
  void* data = nullptr;
  size_t len = 0;
  if (!readFile(gSessionPath, &data, &len)) return;
  Json j;
  if (j.parse((const char*)data)) {
    JsonValue* r = j.root();
    snprintf(S.token, sizeof S.token, "%s", Json::string(Json::get(r, "token")));
    S.userId = Json::integer(Json::get(r, "userId"));
    snprintf(S.username, sizeof S.username, "%s", Json::string(Json::get(r, "username")));
    S.language = Json::integer(Json::get(r, "language"), 0);
    S.quality = Json::integer(Json::get(r, "quality"), 4);
    S.music = Json::boolean(Json::get(r, "music"), true);
    S.sfx = Json::boolean(Json::get(r, "sfx"), true);
    S.voice = Json::boolean(Json::get(r, "voice"), true);
    S.haptics = Json::boolean(Json::get(r, "haptics"), true);
    S.musicVolume = Json::flt(Json::get(r, "musicVolume"), 0.55f);
    S.sfxVolume = Json::flt(Json::get(r, "sfxVolume"), 0.9f);
  }
  free(data);
}

void saveLocalSettings() {
  if (!gSessionPath[0]) snprintf(gSessionPath, sizeof gSessionPath, "%s/nexora.json", filesDir());
  char body[1600];
  snprintf(body, sizeof body,
           "{\"token\":\"%s\",\"userId\":%d,\"uid\":\"%s\",\"username\":\"%s\",\"language\":%d,\"quality\":%d,"
           "\"music\":%s,\"sfx\":%s,\"voice\":%s,\"haptics\":%s,\"musicVolume\":%.3f,\"sfxVolume\":%.3f}",
           S.token, S.userId, S.uid, S.username, S.language, S.quality, S.music ? "true" : "false",
           S.sfx ? "true" : "false", S.voice ? "true" : "false", S.haptics ? "true" : "false",
           (double)S.musicVolume, (double)S.sfxVolume);
  writeFile(gSessionPath, body, strlen(body));
}

// ---------------------------------------------------------------------------
// navigation
// ---------------------------------------------------------------------------
void go(Screen s) {
  if (gDepth > 0 && gStack[gDepth - 1] == s) return;
  if (gDepth > 0) screens::leave(gStack[gDepth - 1]);
  if (gDepth >= kStackMax) {
    for (int i = 0; i < kStackMax - 1; i++) gStack[i] = gStack[i + 1];
    gDepth = kStackMax - 1;
  }
  gStack[gDepth++] = s;
  screens::enter(s);
}

void goBack() {
  if (gDepth <= 1) return;
  screens::leave(gStack[gDepth - 1]);
  gDepth--;
  screens::enter(gStack[gDepth - 1]);
}

void replace(Screen s) {
  if (gDepth > 0) screens::leave(gStack[gDepth - 1]);
  gDepth = gDepth > 0 ? gDepth - 1 : 0;
  go(s);
}

Screen current() { return gDepth ? gStack[gDepth - 1] : Screen::Boot; }
bool busy() { return gBusy; }

void setBusy(bool b, const char* label) {
  gBusy = b;
  if (label) snprintf(gBusyLabel, sizeof gBusyLabel, "%s", label);
  if (!b) gBusyLabel[0] = 0;
}

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------
void init() {
  netInit();
  loadLocalSettings();
  if (S.token[0]) {
    snprintf(api.token, sizeof api.token, "%s", S.token);
    api.userId = S.userId;
    snprintf(api.username, sizeof api.username, "%s", S.username);
  }
  api.start();
  ws::init();
  screens::init();
  if (S.token[0]) {
    refreshMe();
    go(Screen::Home);
  } else {
    go(Screen::Login);
  }
}

void shutdown() {
  if (game::Gameplay::get().active()) game::Gameplay::get().leave();
  ws::shutdown();
  api.stop();
  saveLocalSettings();
}

void refreshMe() { api.call(NET_ME, "GET", "/api/auth/me"); }

void enterMap(int mapId) {
  if (mapId <= 0) return;
  setBusy(true, "جاري تحميل العالم…");
  char path[64];
  snprintf(path, sizeof path, "/api/maps/%d/join", mapId);
  api.call(NET_MAP_JOIN, "POST", path, "{}", mapId);
}

void leaveGameplay() {
  if (game::Gameplay::get().active()) {
    api.call(NET_PLAY_LEAVE, "POST", "/api/play/leave", nullptr);
    game::Gameplay::get().leave();
  }
  ws::leaveMap();
  go(Screen::Home);
  refreshMe();
}

void signOut() {
  api.call(NET_LOGOUT, "POST", "/api/auth/logout", nullptr);
  if (game::Gameplay::get().active()) game::Gameplay::get().leave();
  ws::shutdown();
  S.token[0] = 0;
  S.userId = 0;
  S.username[0] = 0;
  S.role = Role::Player;
  S.miy = 0;
  api.token[0] = 0;
  api.userId = 0;
  api.username[0] = 0;
  saveLocalSettings();
  screens::reset();
  replace(Screen::Login);
}

void applyLogin(JsonValue* root) {
  JsonValue* token = Json::get(root, "token");
  JsonValue* user = Json::get(root, "user");
  if (!token || !user) {
    ui::toast(Json::string(Json::get(root, "message"), "تعذر تسجيل الدخول"));
    return;
  }
  snprintf(S.token, sizeof S.token, "%s", Json::string(token));
  snprintf(api.token, sizeof api.token, "%s", S.token);
  snprintf(S.uid, sizeof S.uid, "%s", Json::string(Json::get(user, "id")));
  S.userId = Json::integer(Json::get(user, "numericId"), 0);
  snprintf(S.username, sizeof S.username, "%s", Json::string(Json::get(user, "username")));
  api.userId = S.userId;
  snprintf(api.username, sizeof api.username, "%s", S.username);
  S.miy = Json::integer(Json::get(user, "miy"));
  snprintf(S.roleName, sizeof S.roleName, "%s", Json::string(Json::get(user, "role"), "player"));
  S.role = strcmp(S.roleName, "owner") == 0 ? Role::Owner
           : strcmp(S.roleName, "moderator") == 0 ? Role::Moderator : Role::Player;
  saveLocalSettings();
  ws::connect();
  screens::reset();
  go(Screen::Home);
}

void applyMe(JsonValue* root) {
  JsonValue* user = Json::get(root, "user");
  if (!user) return;
  snprintf(S.username, sizeof S.username, "%s", Json::string(Json::get(user, "username"), S.username));
  S.miy = Json::integer(Json::get(user, "miy"), S.miy);
  snprintf(S.bio, sizeof S.bio, "%s", Json::string(Json::get(user, "bio")));
  snprintf(S.roleName, sizeof S.roleName, "%s", Json::string(Json::get(user, "role"), "player"));
  S.role = strcmp(S.roleName, "owner") == 0 ? Role::Owner
           : strcmp(S.roleName, "moderator") == 0 ? Role::Moderator : Role::Player;
  S.unread = Json::integer(Json::get(user, "unreadNotifications"));
  S.level = Json::integer(Json::get(user, "level"), 1);
  S.friends = Json::integer(Json::get(root, "friendCount"), S.friends);
  saveLocalSettings();
}

// ---------------------------------------------------------------------------
// frame
// ---------------------------------------------------------------------------
void frame(float dt) {
  Engine& e = Engine::get();
  gFrameAnim += dt;

  ApiEvent ev;
  while (popApiEvent(ev)) {
    screens::onApi(ev);
    if (ev.body) free(ev.body);
  }
  ws::pump();

  screens::tick(dt);
  if (game::Gameplay::get().active()) {
    game::Gameplay::get().draw(dt);
  } else {
    screens::draw(dt);
  }

  if (gBusy) {
    ui::begin();
    ui::rect(Rect(0, 0, e.width(), e.height()), Color(0.02f, 0.03f, 0.07f, 0.60f));
    float cx = e.width() * 0.5f, cy = e.height() * 0.5f;
    float t = fmodf(gFrameAnim * 1.4f, 1.0f);
    for (int i = 0; i < 8; i++) {
      float a = t * TAU + i * (TAU / 8);
      float alpha = 0.12f + 0.88f * (1.0f - (float)i / 8);
      float r = ui::dp(3.5f);
      ui::rect(Rect(cx + cosf(a) * ui::dp(22) - r, cy - ui::dp(28) + sinf(a) * ui::dp(22) - r, r * 2, r * 2),
               ui::theme.violet.withA(alpha), r);
    }
    ui::text(gBusyLabel[0] ? gBusyLabel : "جاري التحميل…", cx, cy + ui::dp(6), 13, ui::theme.textDim, e.width(), 1, 1);
    ui::end();
  }
}

// ---------------------------------------------------------------------------
// input
// ---------------------------------------------------------------------------
void input(Touch* touches, int count, float dt) {
  (void)dt;
  if (game::Gameplay::get().active()) {
    game::Gameplay::get().injectInput(touches, count);
    return;
  }
  // pick one logical pointer: an existing tracked touch, else the newest press
  int idx = -1;
  for (int i = count - 1; i >= 0; i--) {
    if (touches[i].id == gActiveTouchId) { idx = i; break; }
  }
  if (idx < 0) {
    for (int i = count - 1; i >= 0; i--) {
      if (touches[i].down) { idx = i; gActiveTouchId = touches[i].id; break; }
    }
  }
  ui::Pointer p;
  if (idx >= 0) {
    Touch& t = touches[idx];
    p.pos = t.pos;
    p.start = t.start;
    p.down = t.down;
    p.downTime = t.startTime;
    p.pressed = t.down && !gHadDown;
    p.released = !t.down && gHadDown;
    if (!t.down && gHadDown) gActiveTouchId = -1;
    gHadDown = t.down;
    gLastPos = t.pos;
  } else {
    p.down = false;
    p.pos = Vec2(-1, -1);
    p.start = Vec2(-1, -1);
    p.released = gHadDown;
    gHadDown = false;
    gActiveTouchId = -1;
    gLastPos = Vec2(-1, -1);
  }
  ui::feedPointer(p);
}

void textInput(const char* utf8) {
  ui::pushEditableText(utf8);
  screens::onText(utf8);
}

void keyInput(int code) {
  if (code == 67) {  // backspace
    ui::pushBackspace();
    screens::onKey(code);
    return;
  }
  if (code == 4) {  // back
    screens::onBack();
    return;
  }
  screens::onKey(code);
}

void toast(const char* msg) { ui::toast(msg); }

void onMicPermission(bool granted) {
  Engine::get().setMicGranted(granted);
  if (!granted) ui::toast("لم يتم منح إذن الميكروفون");
}

}  // namespace app
}  // namespace nx
