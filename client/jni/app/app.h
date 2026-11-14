// NEXORA app layer — entry points called by the engine, shared state and screen routing.
#pragma once
#include "core/engine.h"
#include "core/json.h"
#include "core/ui.h"
#include "world/world.h"

namespace nx {
namespace app {

// ---- engine -> app hooks ----
void init();
void shutdown();
// Drops every GL handle the app owns, on the render thread, before the EGL context dies.
void releaseGpu();
void frame(float dt);
void input(Touch* touches, int count, float dt);
void textInput(const char* utf8);
void keyInput(int code);
void toast(const char* msg);
void onMicPermission(bool granted);
void gameplayCreate(void** outPlayer, void** outCamera);
void gameplayDestroy(void* player, void* camera);
void gameplaySpawn(void* player, const Vec3& p, float face);
void gameplayUpdate(void* player, float dt, float fwd, float strafe, bool run, bool crouch, bool jump, float yaw,
                    const Terrain& terrain, GameWorld& world, bool locked);
Vec3 gameplayEye(void* player);
void gameplayAvatarState(void* player, void* out);
void gameplayCameraUpdate(void* cam, float dt, const Vec3& focus, const Terrain& terrain, float dx, float dy,
                          bool running);
Mat4 gameplayCameraView(void* cam);
Vec3 gameplayCameraForward(void* cam);
Vec3 gameplayCameraPosition(void* cam);

// ---- navigation ----
enum class Screen {
  Boot, Login, Register, Home, Discover, CreateAi, MapDetail, Studio, Play, Store, Friends, Chat, Profile,
  Settings, Notifications, Owner, Modes
};

// screens the router can show; each is implemented in its own translation unit
struct ScreenVtbl {
  void (*enter)();
  void (*leave)();
  void (*update)(float dt);
  void (*draw)(float dt);
};
void registerScreens();
void go(Screen s);
void goBack();
Screen current();
void replace(Screen s);
void refreshMe();
// Point the client at a server: "host", "host:8080" or a full http(s):// URL.
// Empty clears the override and restores the build default.
void setServerAddress(const char* address);
void applyServerAddress();
void enterMap(const char* mapId);
void leaveGameplay();
bool busy();
void setBusy(bool b, const char* label = nullptr);

}  // namespace app
}  // namespace nx
