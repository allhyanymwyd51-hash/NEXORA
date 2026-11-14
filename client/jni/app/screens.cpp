// Screen registry: maps router states onto per-screen implementations and broadcasts events.
#include "screens.h"

#include "game/gameplay.h"
#include "ws.h"

namespace nx {
namespace app {
namespace screens {

namespace {

struct Entry {
  void (*enter)();
  void (*leave)();
  void (*draw)(float);
};

Entry gEntries[16];
bool gReady = false;
Screen gActive = Screen::Boot;
float gScreenTime = 0;

Entry make(void (*e)(), void (*l)(), void (*d)(float)) { return Entry{e, l, d}; }

}  // namespace

void init() {
  if (gReady) return;
  gEntries[(int)Screen::Login] = make(login::enter, login::leave, login::draw);
  gEntries[(int)Screen::Register] = make(registerScreen::enter, registerScreen::leave, registerScreen::draw);
  gEntries[(int)Screen::Home] = make(home::enter, home::leave, home::draw);
  gEntries[(int)Screen::Discover] = make(discover::enter, discover::leave, discover::draw);
  gEntries[(int)Screen::CreateAi] = make(createAi::enter, createAi::leave, createAi::draw);
  gEntries[(int)Screen::Modes] = make(modes::enter, modes::leave, modes::draw);
  gEntries[(int)Screen::MapDetail] = make(mapDetail::enter, mapDetail::leave, mapDetail::draw);
  gEntries[(int)Screen::Studio] = make(studio::enter, studio::leave, studio::draw);
  gEntries[(int)Screen::Store] = make(store::enter, store::leave, store::draw);
  gEntries[(int)Screen::Friends] = make(friends::enter, friends::leave, friends::draw);
  gEntries[(int)Screen::Chat] = make(chat::enter, chat::leave, chat::draw);
  gEntries[(int)Screen::Profile] = make(profile::enter, profile::leave, profile::draw);
  gEntries[(int)Screen::Settings] = make(settings::enter, settings::leave, settings::draw);
  gEntries[(int)Screen::Notifications] = make(notifications::enter, notifications::leave, notifications::draw);
  gEntries[(int)Screen::Owner] = make(owner::enter, owner::leave, owner::draw);
  gReady = true;
}

void reset() {
  for (int i = 0; i < 16; i++) {
    if (gEntries[i].leave) gEntries[i].leave();
    gEntries[i] = Entry{nullptr, nullptr, nullptr};
  }
  gReady = false;
  gActive = Screen::Boot;
  init();
}

void enter(Screen s) {
  if (!gReady) init();
  int i = (int)s;
  gActive = s;
  gScreenTime = 0;
  if (i >= 0 && i < 16 && gEntries[i].enter) gEntries[i].enter();
}

void leave(Screen s) {
  int i = (int)s;
  if (i >= 0 && i < 16 && gEntries[i].leave) gEntries[i].leave();
}

void draw(float dt) {
  gScreenTime += dt;
  if (game::Gameplay::get().active()) return;
  int i = (int)current();
  if (i < 0 || i >= 16 || !gEntries[i].draw) return;
  ui::begin();
  gEntries[i].draw(dt);
  ui::end();
}

void tick(float dt) {
  extern void createAiPoll(float);
  createAiPoll(dt);
}

void onApi(const ApiEvent& ev) {
  // gameplay callbacks first
  if (ev.op == (int)NET_PLAY_HEARTBEAT && game::Gameplay::get().active()) {
    Json j;
    if (ev.body && j.parse(ev.body)) game::Gameplay::get().onHeartbeatResponse(j.root());
  }
  extern void dispatchApi(const ApiEvent&);  // defined per screen group below
  dispatchApi(ev);
}

void onText(const char* utf8) {
  extern void dispatchText(const char*);
  dispatchText(utf8);
}

void onKey(int code) {
  extern void dispatchKey(int);
  if (code == 66) {  // enter submits the focused field
  }
  dispatchKey(code);
}

void onBack() {
  extern bool dispatchBack();
  if (dispatchBack()) return;
  if (game::Gameplay::get().active()) return;  // the pause menu owns the back gesture
  if (current() != Screen::Home && current() != Screen::Login) goBack();
}

void onWsEvent(JsonValue* root) {
  extern void dispatchWs(JsonValue*);
  dispatchWs(root);
}

}  // namespace screens
}  // namespace app
}  // namespace nx
