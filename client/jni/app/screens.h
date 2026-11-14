// NEXORA screens — one translation unit per surface, sharing this contract.
#pragma once
#include "app.h"
#include "state.h"
#include "core/net.h"
#include "core/ui.h"

namespace nx {
namespace app {
namespace screens {

void init();
// Rebuild per-screen GL state after the surface (and its context) was recreated.
void releaseGpu();
void reset();
void enter(Screen s);
void leave(Screen s);
void draw(float dt);
void tick(float dt);   // per-frame work that runs even when another screen is on top
void onApi(const ApiEvent& ev);
void onText(const char* utf8);
void onKey(int code);
void onBack();
void onWsEvent(JsonValue* root);

// shared chrome -------------------------------------------------------------
void background(float dt);
struct HeaderOpts {
  const char* title = "";
  bool backButton = true;
  bool wallet = true;
  bool closeButton = false;
};
Rect header(const HeaderOpts& opts);
float bottomNav(float selected);  // returns the height used
void sectionTitle(const char* label, float x, float y, float w, bool action = false, const char* actionLabel = "");
const char* tr(const char* ar, const char* en);

// shared helpers ------------------------------------------------------------
void toastError(JsonValue* root, const char* fallback);
bool parseOk(JsonValue* root);
const char* apiMessage(JsonValue* root, const char* fallback);
void openMap(const char* mapId);

// screens ------------------------------------------------------------------
namespace login {
void enter(); void leave(); void draw(float dt);
}
namespace registerScreen {
void enter(); void leave(); void draw(float dt);
}
namespace home {
void enter(); void leave(); void draw(float dt);
}
namespace discover {
void enter(); void leave(); void draw(float dt);
}
namespace createAi {
void enter(); void leave(); void draw(float dt);
}
namespace modes {
void enter(); void leave(); void draw(float dt);
}
namespace mapDetail {
void enter(); void leave(); void draw(float dt);
}
namespace studio {
void enter(); void leave(); void draw(float dt);
}
namespace store {
void enter(); void leave(); void draw(float dt);
}
namespace friends {
void enter(); void leave(); void draw(float dt);
}
namespace chat {
void enter(); void leave(); void draw(float dt);
}
namespace profile {
void enter(); void leave(); void draw(float dt);
}
namespace settings {
void enter(); void leave(); void draw(float dt);
}
namespace notifications {
void enter(); void leave(); void draw(float dt);
}
namespace owner {
void enter(); void leave(); void draw(float dt);
}

}  // namespace screens
}  // namespace app
}  // namespace nx
