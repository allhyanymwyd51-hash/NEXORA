// In-map gameplay screen: 3D rendering, virtual controls, HUD, multiplayer and voice.
#pragma once
#include "core/engine.h"
#include "core/json.h"
#include "world/avatar.h"
#include "world/world.h"

namespace nx {
namespace game {

struct ChatLine {
  char sender[40];
  char body[240];
  float age;
};

class Gameplay {
 public:
  static Gameplay& get();
  bool enter(const char* mapJson, const char* mapName, const char* sessionId, const char* mapId);
  void leave();
  bool active() const { return m_active; }
  void update(float dt);
  void draw(float dt);
  void toggleChat();
  void sendChat(const char* text);

  // network events routed from the app
  void onRoomState(JsonValue* root);
  void onRoomTick(JsonValue* root);
  void onPlayerJoined(JsonValue* root);
  void onPlayerLeft(JsonValue* root);
  void onChat(JsonValue* root);
  void onHeartbeatResponse(JsonValue* root);
  void setNetwork(bool connected, const char* reason);
  const char* mapId() const { return m_mapId; }
  // raw touches while the 3D scene is up (the UI layer is not driven in-world)
  void injectInput(Touch* touches, int count);
  const char* sessionId() const { return m_sessionId; }

 private:
  Gameplay() {}
  void drawHud();
  void drawJoystick();
  void drawWorld(float dt);
  void handleTouches();
  void interact();
  Vec3 projectToScreen(const Vec3& world) const;

  bool m_active = false;
  char m_mapId[48] = {0};
  char m_sessionId[48] = {0};
  char m_mapName[80] = {0};
  float m_time = 0;
  float m_heartbeatTimer = 0;
  float m_netTimer = 0;
  bool m_netConnected = false;
  char m_netReason[80] = {0};

  // local player input
  Vec2 m_moveAxis;
  float m_camYaw = 0;
  float m_camPitch = 0.2f;
  bool m_runHeld = false;
  bool m_jumpRequested = false;
  bool m_voiceOn = false;
  bool m_chatOpen = false;
  char m_chatInput[200] = {0};
  bool m_paused = false;

  void* m_player = nullptr;
  void* m_camera = nullptr;
  Vec3 m_lastSentPos;
  float m_sendTimer = 0;

  Array<ChatLine> m_chatLog;
  float m_toastTimer = 0;
  char m_toastText[120] = {0};

  // interaction prompt
  int m_nearInteractable = -1;
  char m_promptText[120] = {0};

  // enemy/pickup feedback particles
  struct Particle {
    Vec3 pos, vel;
    float life, maxLife;
    Color color;
  };
  Array<Particle> m_particles;
  Touch m_touchBuf[10];
  int m_touchCount = 0;

  // avatar render batch
  MeshBatcher m_charBatch;
  Program m_charProg;
  void ensureCharProgram();
};

}  // namespace game
}  // namespace nx
