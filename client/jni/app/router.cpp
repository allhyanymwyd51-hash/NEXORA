// Event dispatch table: routes API results, text input, key presses and realtime
// frames to the screen implementations that declare interest.
#include "screens.h"

#include "game/gameplay.h"
#include "widgets.h"
#include "ws.h"

namespace nx {
namespace app {
namespace screens {

// Per-screen hooks; a screen only implements the ones it needs.
struct Hooks {
  void (*api)(const ApiEvent&);
  void (*text)(const char*);
  void (*key)(int);
  bool (*back)();
  void (*ws)(JsonValue*);
};

// implemented across the screen translation units
void loginApi(const ApiEvent&);
void registerApi(const ApiEvent&);
void homeApi(const ApiEvent&);
void discoverApi(const ApiEvent&);
void createAiApi(const ApiEvent&);
void mapDetailApi(const ApiEvent&);
void studioApi(const ApiEvent&);
void storeApi(const ApiEvent&);
void friendsApi(const ApiEvent&);
void chatApi(const ApiEvent&);
void profileApi(const ApiEvent&);
void settingsApi(const ApiEvent&);
void notificationsApi(const ApiEvent&);
void ownerApi(const ApiEvent&);
void modesApi(const ApiEvent&);

void chatText(const char*);
bool chatBack();
void createAiText(const char*);
bool createAiBack();

namespace {

const int kMax = 16;

Hooks& hooksFor(Screen s) {
  static Hooks table[kMax];
  static bool init = false;
  if (!init) {
    init = true;
    table[(int)Screen::Login] = {loginApi, nullptr, nullptr, nullptr, nullptr};
    table[(int)Screen::Register] = {registerApi, nullptr, nullptr, nullptr, nullptr};
    table[(int)Screen::Home] = {homeApi, nullptr, nullptr, nullptr, nullptr};
    table[(int)Screen::Discover] = {discoverApi, nullptr, nullptr, nullptr, nullptr};
    table[(int)Screen::CreateAi] = {createAiApi, createAiText, nullptr, createAiBack, nullptr};
    table[(int)Screen::MapDetail] = {mapDetailApi, nullptr, nullptr, nullptr, nullptr};
    table[(int)Screen::Studio] = {studioApi, nullptr, nullptr, nullptr, nullptr};
    table[(int)Screen::Store] = {storeApi, nullptr, nullptr, nullptr, nullptr};
    table[(int)Screen::Friends] = {friendsApi, nullptr, nullptr, nullptr, nullptr};
    table[(int)Screen::Chat] = {chatApi, chatText, nullptr, chatBack, nullptr};
    table[(int)Screen::Profile] = {profileApi, nullptr, nullptr, nullptr, nullptr};
    table[(int)Screen::Settings] = {settingsApi, nullptr, nullptr, nullptr, nullptr};
    table[(int)Screen::Notifications] = {notificationsApi, nullptr, nullptr, nullptr, nullptr};
    table[(int)Screen::Owner] = {ownerApi, nullptr, nullptr, nullptr, nullptr};
    table[(int)Screen::Modes] = {modesApi, nullptr, nullptr, nullptr, nullptr};
  }
  int i = (int)s;
  return table[i >= 0 && i < kMax ? i : 0];
}

}  // namespace

void dispatchApi(const ApiEvent& ev) {
  Hooks& h = hooksFor(current());
  if (h.api) h.api(ev);
  // screens that must react regardless of what is on top
  if (ev.op == (int)NET_ME && ev.status == 200) {
    Json j;
    if (ev.body && j.parse(ev.body)) applyMe(j.root());
  }
  if (ev.op == (int)NET_ME && ev.status == 401) {
    signOut();
  }
  if (ev.op == (int)NET_NOTIFICATIONS && ev.status == 200) {
    Json j;
    if (ev.body && j.parse(ev.body)) {
      S.unread = Json::integer(Json::get(j.root(), "unread"), S.unread);
    }
  }
}

void dispatchText(const char* utf8) {
  Hooks& h = hooksFor(current());
  if (h.text) h.text(utf8);
}

void dispatchKey(int code) {
  Hooks& h = hooksFor(current());
  if (h.key) h.key(code);
}

bool dispatchBack() {
  Hooks& h = hooksFor(current());
  if (h.back && h.back()) return true;
  return false;
}

void dispatchWs(JsonValue* root) {
  Hooks& h = hooksFor(current());
  if (h.ws) h.ws(root);
}

}  // namespace screens
}  // namespace app
}  // namespace nx
