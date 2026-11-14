#include "gameplay.h"
#include "core/ui.h"
#include "core/audio.h"
#include "app/app.h"
#include "app/ws.h"
#include "game/netlocal.h"
#include "app/state.h"

namespace nx {
namespace game {

// ---------------------------------------------------------------------------
// Gameplay
// ---------------------------------------------------------------------------
Gameplay& Gameplay::get() {
  static Gameplay g;
  return g;
}

bool Gameplay::enter(const char* mapJson, const char* mapName, const char* sessionId, const char* mapId) {
  leave();
  Json j;
  if (!j.parse(mapJson)) {
    app::toast("تعذر قراءة بيانات العالم");
    return false;
  }
  JsonValue* root = j.root();
  JsonValue* mapObj = Json::get(root, "map");
  if (mapObj) root = mapObj;
  JsonValue* worldJson = Json::get(root, "world");
  if (!worldJson) worldJson = root;
  if (!world.loadFromJson(worldJson, mapName)) {
    app::toast("العالم غير مكتمل");
    return false;
  }
  snprintf(m_mapId, sizeof m_mapId, "%s", mapId ? mapId : "");
  strncpy(m_mapName, mapName ? mapName : "NEXORA", sizeof m_mapName - 1);
  strncpy(m_sessionId, sessionId ? sessionId : "", sizeof m_sessionId - 1);
  app::gameplayCreate(&m_player, &m_camera);
  Vec3 spawn = world.spawnPoint(0);
  app::gameplaySpawn(m_player, spawn, 0);
  m_active = true;
  m_time = 0;
  m_heartbeatTimer = 15;
  m_netTimer = 0;
  m_chatLog.clear();
  m_particles.clear();
  // avatar details from the logged-in profile
  if (!m_player) return false;
  ws::joinMap(m_mapId, spawn.x, spawn.y, spawn.z, 0);
  Engine::get().playMusic("music_exploration");
  app::toast("بدأت اللعب — 5 Miy لكل دقيقة");
  return true;
}

void Gameplay::leave() {
  if (!m_active) return;
  ws::leaveMap();
  if (m_player) app::gameplayDestroy(m_player, m_camera);
  m_player = nullptr;
  m_camera = nullptr;
  world.unload();
  m_active = false;
}


void Gameplay::toggleChat() {
  m_chatOpen = !m_chatOpen;
  if (m_chatOpen) {
    m_chatInput[0] = 0;
    Engine::get().showKeyboard();
    ui::setFocus("chat");
  } else {
    Engine::get().hideKeyboard();
  }
}

void Gameplay::sendChat(const char* text) {
  if (!text || !*text) return;
  ws::sendChat(text, nullptr);
  char line[240];
  snprintf(line, sizeof line, "%s", text);
  ChatLine cl{};
  snprintf(cl.sender, sizeof cl.sender, "%s", nx::api.username);
  snprintf(cl.body, sizeof cl.body, "%s", line);
  cl.age = 0;
  m_chatLog.push(cl);
  if (m_chatLog.count > 40) m_chatLog.removeAt(0);
}

void Gameplay::onRoomState(JsonValue* root) {
  world.players().clear();
  JsonValue* players = Json::get(root, "players");
  for (int i = 0; i < Json::length(players); i++) {
    JsonValue* p = Json::at(players, i);
    int uid = 0; 
    // the room payload carries the stable user id string; skip our own entry
    if (app::S.uid[0] && strcmp(Json::string(Json::get(p, "userId")), app::S.uid) == 0) continue;
    if (uid == 0 && !app::S.uid[0]) continue;
    Vec3 pos(Json::flt(Json::get(p, "x")), Json::flt(Json::get(p, "y")), Json::flt(Json::get(p, "z")));
    world.upsertPlayer(uid, Json::string(Json::get(p, "username"), "player"), pos,
                       Json::flt(Json::get(p, "heading")), Json::boolean(Json::get(p, "moving")));
  }
}

void Gameplay::onRoomTick(JsonValue* root) {
  JsonValue* players = Json::get(root, "players");
  for (int i = 0; i < Json::length(players); i++) {
    JsonValue* p = Json::at(players, i);
    int uid = 0; 
    // the room payload carries the stable user id string; skip our own entry
    if (app::S.uid[0] && strcmp(Json::string(Json::get(p, "userId")), app::S.uid) == 0) continue;
    if (uid == 0 && !app::S.uid[0]) continue;
    Vec3 pos(Json::flt(Json::get(p, "x")), Json::flt(Json::get(p, "y")), Json::flt(Json::get(p, "z")));
    world.upsertPlayer(uid, Json::string(Json::get(p, "username"), "player"), pos,
                       Json::flt(Json::get(p, "heading")), Json::boolean(Json::get(p, "moving")));
  }
}

void Gameplay::onPlayerJoined(JsonValue* root) {
  JsonValue* p = Json::get(root, "player");
  if (!p) return;
  int uid = 0; 
  if (uid == 0 || uid == app::S.userId) return;
  Vec3 pos(Json::flt(Json::get(p, "x")), Json::flt(Json::get(p, "y")), Json::flt(Json::get(p, "z")));
  world.upsertPlayer(uid, Json::string(Json::get(p, "username"), "player"), pos,
                     Json::flt(Json::get(p, "heading")), false);
  char msg[80];
  snprintf(msg, sizeof msg, "%s انضم للماب", Json::string(Json::get(p, "username"), "لاعب"));
  app::toast(msg);
}

void Gameplay::onPlayerLeft(JsonValue* root) {
  int uid = Json::integer(Json::get(root, "userId"), 0);
  if (uid) world.removePlayer(uid);
}

void Gameplay::onChat(JsonValue* root) {
  JsonValue* m = Json::get(root, "message");
  if (!m) return;
  JsonValue* sender = Json::get(m, "sender");
  ChatLine cl{};
  snprintf(cl.sender, sizeof cl.sender, "%s", Json::string(Json::get(sender, "username"), "?"));
  snprintf(cl.body, sizeof cl.body, "%s", Json::string(Json::get(m, "body"), ""));
  cl.age = 0;
  m_chatLog.push(cl);
  if (m_chatLog.count > 40) m_chatLog.removeAt(0);
}

void Gameplay::onHeartbeatResponse(JsonValue* root) {
  if (Json::boolean(Json::get(root, "capped"), false)) {
    app::toast("وصلت للحد اليومي من مكافآت Miy");
  }
}

void Gameplay::setNetwork(bool connected, const char* reason) {
  m_netConnected = connected;
  snprintf(m_netReason, sizeof m_netReason, "%s", reason ? reason : "");
}

// ---------------------------------------------------------------------------
// update
// ---------------------------------------------------------------------------
void Gameplay::update(float dt) {
  if (!m_active || !m_player) return;
  m_time += dt;
  Engine& e = Engine::get();

  // ---- input ----
  for (int i = 0; i < m_touchCount; i++) {
    Touch t = e.touches()[i];
    // joystick zone: bottom-left quadrant
    if (t.down && t.start.x < e.width() * 0.32f && t.start.y > e.height() * 0.45f) {
      Vec2 delta = t.pos - t.start;
      float maxR = ui::dp(70);
      float len = delta.length();
      Vec2 n = len > 0.001f ? delta * (1.0f / len) : Vec2(0, 0);
      float clamped = fminf(len, maxR) / maxR;
      m_moveAxis = n * clamped;
    }
  }
  // camera look from the right side drag
  static Vec2 lastCamPos = Vec2(0, 0);
  static int lastCamTouch = -1;
  for (int i = 0; i < m_touchCount; i++) {
    Touch& t = m_touchBuf[i];
    if (!t.down) continue;
    if (t.start.x < e.width() * 0.36f || t.start.y < e.height() * 0.16f) continue;
    if (!t.moved) continue;
    if (lastCamTouch != t.id) { lastCamTouch = t.id; lastCamPos = t.pos; continue; }
    Vec2 d = t.pos - lastCamPos;
    lastCamPos = t.pos;
    m_camYaw -= d.x * 0.006f;
    m_camPitch = clampf(m_camPitch + d.y * 0.004f, -0.3f, 1.0f);
  }
  // buttons handled in draw (they need rects); jump/run flags persist
  bool locked = m_chatOpen || m_paused;
  // water slows and the character floats; the world handles the rest
  app::gameplayUpdate(m_player, dt, m_moveAxis.y, m_moveAxis.x, m_runHeld, false, m_jumpRequested, m_camYaw,
                      world.terrain(), world, locked);
  m_jumpRequested = false;
  app::gameplayCameraUpdate(m_camera, dt, app::gameplayEye(m_player), world.terrain(), 0, 0, m_runHeld);
  Vec3 eye = app::gameplayEye(m_player);

  world.update(dt, eye);
  audioUpdateListener(eye, app::gameplayCameraForward(m_camera));

  // ---- interaction prompt ----
  m_nearInteractable = world.nearestInteractable(app::gameplayEye(m_player), 4.5f);
  if (m_nearInteractable >= 0) {
    Interactable& in = world.interactables()[m_nearInteractable];
    snprintf(m_promptText, sizeof m_promptText, "تفاعل: %s", in.label[0] ? in.label : "عنصر");
  }

  // ---- quest completion feedback ----
  for (int i = 0; i < world.quests().count; i++) {
    if (world.quests()[i].complete && !world.quests()[i].notified) {
      world.quests()[i].notified = true;
      char msg[140];
      snprintf(msg, sizeof msg, "أكملت: %s (+%d Miy)", world.quests()[i].title, world.quests()[i].rewardMiy);
      app::toast(msg);
      Engine::get().playSfx("ai_complete", 0.8f);
    }
  }

  // ---- heartbeat (server-authoritative Miy) ----
  m_heartbeatTimer -= dt;
  if (m_heartbeatTimer <= 0 && m_sessionId[0]) {
    m_heartbeatTimer = 20.0f;
    char hb[96];
    snprintf(hb, sizeof hb, "{\"sessionId\":\"%s\"}", m_sessionId);
    api.call(NET_PLAY_HEARTBEAT, "POST", "/api/play/heartbeat", hb);
  }

  // ---- multiplayer position sync (10 Hz) ----
  m_sendTimer -= dt;
  Vec3 p = m_player ? app::gameplayEye(m_player) : Vec3(0, 0, 0);
  float dx = p.x - m_lastSentPos.x, dz = p.z - m_lastSentPos.z;
  if (m_sendTimer <= 0 || (dx * dx + dz * dz) > 0.36f) {
    m_sendTimer = 0.1f;
    m_lastSentPos = p;
    ws::sendMove(p.x, p.y - 1.55f, p.z, m_camYaw, m_moveAxis.length() > 0.1f);
  }
  // voice downlink frames are mixed by the audio engine; mic frames go up here
  if (m_voiceOn) {
    short frame[320];
    int n = Engine::get().popVoiceFrame(frame, 320);
    if (n >= 160) ws::sendVoice(frame, n);
  }
}

// ---------------------------------------------------------------------------
// draw
// ---------------------------------------------------------------------------
void Gameplay::ensureCharProgram() {
  if (m_charProg.id) return;
  static const char* vs = R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec4 aColor;
uniform mat4 uViewProj;
out vec3 vN; out vec4 vC; out vec3 vW;
void main(){ vN=aNrm; vC=aColor; vW=aPos; gl_Position=uViewProj*vec4(aPos,1.0); })";
  static const char* fs = R"(#version 300 es
precision mediump float;
in vec3 vN; in vec4 vC; in vec3 vW;
uniform vec3 uSunDir; uniform vec3 uCamera; uniform vec3 uFogColor; uniform float uFogDensity;
out vec4 fragColor;
void main(){
  vec3 n = normalize(vN);
  float ndl = max(dot(n, normalize(uSunDir)), 0.0);
  vec3 lit = vC.rgb * (vec3(0.42,0.48,0.62) * (0.6 + 0.4*max(n.y,0.0)) + vec3(1.0,0.96,0.88)*ndl*1.1);
  float dist = length(vW-uCamera);
  float fog = 1.0 - exp(-dist*uFogDensity);
  fragColor = vec4(mix(lit, uFogColor, clamp(fog,0.0,1.0)), vC.a);
})";
  m_charProg.build(vs, fs, "characters");
  m_charBatch.init(sizeof(VtxShape), 3, 3, 4);
}

void Gameplay::drawWorld(float dt) {
  Engine& e = Engine::get();
  ensureCharProgram();
  Mat4 proj = Mat4::perspective(deg2rad(62.0f), e.width() / fmaxf(1.0f, e.height()), 0.1f,
                                e.quality().viewDistance);
  Mat4 view = app::gameplayCameraView(m_camera);
  Mat4 vp = Mat4::mul(proj, view);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  world.draw(vp, view, app::gameplayCameraPosition(m_camera), dt);

  // characters (local + remote) in one batch
  m_charBatch.vertCount = 0;
  m_charBatch.indexCount = 0;
  if (m_player) {
    // Character internals are exposed through helper accessors below
    app::AvatarState av{};
    app::gameplayAvatarState(m_player, &av);
    buildCharacterMesh(m_charBatch, av.pos, av.heading, av.look, av.animPhase, av.animSpeed, av.grounded,
                       av.crouching, av.landingPulse);
  }
  for (int i = 0; i < world.players().count; i++) {
    RemotePlayer& rp = world.players()[i];
    Appearance look;
    look.shirt = rp.shirt;
    look.skin = rp.skin;
    buildCharacterMesh(m_charBatch, rp.pos, rp.heading, look, rp.animPhase, rp.moving ? 4.0f : 0.0f, true, false, 0);
  }
  // animals
  for (int i = 0; i < world.animals().count; i++) {
    Animal& a = world.animals()[i];
    Color body = a.type == 1 ? Color::hex(0x5A5A66) : a.type == 2 ? Color::hex(0xE8E8F0)
        : a.type == 3 ? Color::hex(0xD8D0C0) : Color::hex(0x8A6A48);
    Color dark = body * 0.75f;
    float h = a.type == 2 ? 0.35f : a.type == 5 ? 1.9f : 1.1f;
    Vec3 c(a.pos.x, a.pos.y + h * 0.48f, a.pos.z);
    pushBox(m_charBatch, c, Vec3(0.32f, h * 0.26f, 0.55f), body, body * 0.9f, a.heading, 0.7f);
    Vec3 head(a.pos.x + sinf(a.heading) * 0.6f, a.pos.y + h * 0.72f, a.pos.z + cosf(a.heading) * 0.6f);
    pushSphere(m_charBatch, head, a.type == 2 ? 0.14f : 0.22f, 4, 6, body * 1.1f, dark);
    if (a.type == 5) {
      pushBox(m_charBatch, Vec3(head.x, head.y + 0.18f, head.z), Vec3(0.06f, 0.14f, 0.06f), dark, dark, 0, 1.0f);
    }
  }
  if (m_charBatch.vertCount > 0) {
    m_charProg.use();
    m_charProg.setMat("uViewProj", vp);
    Vec3 sunDir(cosf(deg2rad(world.spec().sunAzimuth)) * cosf(deg2rad(world.spec().sunElevation)),
                sinf(deg2rad(world.spec().sunElevation)),
                sinf(deg2rad(world.spec().sunAzimuth)) * cosf(deg2rad(world.spec().sunElevation)));
    m_charProg.set("uSunDir", sunDir);
    m_charProg.set("uCamera", app::gameplayCameraPosition(m_camera));
    m_charProg.set("uFogColor", world.spec().skyHorizon);
    m_charProg.set("uFogDensity", world.spec().fogDensity);
    m_charBatch.flush(GL_TRIANGLES);
  }
  glDisable(GL_DEPTH_TEST);
}

void Gameplay::drawHud() {
  Engine& e = Engine::get();
  float w = e.width(), h = e.height();
  Color textC = ui::theme.text;

  // brand watermark (N monogram, top-left corner per spec)
  Rect brand(ui::dp(14), ui::dp(12), ui::dp(34), ui::dp(34));
  ui::rect(brand, Color(0.05f, 0.07f, 0.15f, 0.72f), ui::dp(10));
  ui::rectStroke(brand, Color(1, 1, 1, 0.10f), 1.0f, ui::dp(10));
  ui::image(brand.inset(ui::dp(7)), ui::theme.cyan, 0.95f);

  // top bar: map name + player count
  char top[120];
  snprintf(top, sizeof top, "%s  •  %d لاعب", m_mapName, world.players().count + 1);
  ui::text(top, ui::dp(58), ui::dp(18), 15, textC.withA(0.92f), 0, 0, 2);
  // connection state
  if (!m_netConnected) {
    char netMsg[140];
    snprintf(netMsg, sizeof netMsg, "انقطع الاتصال — إعادة المحاولة… %s", m_netReason);
    ui::text(netMsg, ui::dp(20), ui::dp(42), 11, ui::theme.gold, 0, 0, 1);
  }

  // quest tracker (top-right)
  float qy = ui::dp(16);
  for (int i = 0; i < world.quests().count && i < 3; i++) {
    QuestState& q = world.quests()[i];
    if (q.complete) continue;
    Rect card(w - ui::dp(214), qy, ui::dp(198), ui::dp(52));
    ui::rect(card, Color(0.04f, 0.06f, 0.13f, 0.72f), ui::dp(10));
    ui::rectStroke(card, Color(1, 1, 1, 0.08f), 1.0f, ui::dp(10));
    ui::text(q.title, card.x + ui::dp(10), card.y + ui::dp(7), 12, ui::theme.cyan, card.w - ui::dp(20), 0, 2);
    char prog[64];
    snprintf(prog, sizeof prog, "%d / %d", q.progress, q.objectiveCount);
    ui::text(prog, card.x + ui::dp(10), card.y + ui::dp(27), 11, ui::theme.textDim, 0, 0, 0);
    Rect bar(card.x + ui::dp(10), card.y + ui::dp(44), card.w - ui::dp(20), ui::dp(3));
    ui::rect(bar, Color(1, 1, 1, 0.12f), ui::dp(2));
    ui::rect(Rect(bar.x, bar.y, bar.w * clampf((float)q.progress / fmaxf(1, q.objectiveCount), 0, 1), bar.h),
             ui::theme.violet, ui::dp(2));
    qy += ui::dp(58);
  }

  // chat log (bottom-left above joystick-free area)
  float cy = h - ui::dp(150);
  for (int i = m_chatLog.count - 1; i >= 0 && i > m_chatLog.count - 5; i--) {
    ChatLine& cl = m_chatLog[i];
    float alpha = clampf(1.0f - cl.age / 14.0f, 0.15f, 1.0f);
    char line[300];
    snprintf(line, sizeof line, "%s: %s", cl.sender, cl.body);
    ui::text(line, ui::dp(18), cy, 12, Color(0.9f, 0.94f, 1, alpha), w * 0.5f, 0, 0);
    cy -= ui::dp(18);
  }

  // interaction prompt near bottom-centre
  if (m_nearInteractable >= 0) {
    float tw = ui::textMeasure(m_promptText, 13, 0, 1);
    Rect pill(w * 0.5f - tw * 0.5f - ui::dp(18), h - ui::dp(190), tw + ui::dp(36), ui::dp(38));
    ui::rect(pill, Color(0.07f, 0.10f, 0.20f, 0.92f), ui::dp(19));
    ui::rectStroke(pill, ui::theme.cyan.withA(0.6f), 1.0f, ui::dp(19));
    ui::text(m_promptText, pill.x + ui::dp(18), pill.y + ui::dp(10), 13, ui::theme.text, 0, 0, 1);
  }
}

void Gameplay::drawJoystick() {
  Engine& e = Engine::get();
  float w = e.width(), h = e.height();
  // fixed joystick base bottom-left
  Vec2 base(w * 0.14f, h * 0.76f);
  float r = ui::dp(62);
  ui::rect(Rect(base.x - r, base.y - r, r * 2, r * 2), Color(1, 1, 1, 0.06f), r);
  ui::rectStroke(Rect(base.x - r, base.y - r, r * 2, r * 2), Color(1, 1, 1, 0.14f), 1.2f, r);
  Vec2 knob = base + m_moveAxis * (r * 0.62f);
  float kr = ui::dp(28);
  ui::rect(Rect(knob.x - kr, knob.y - kr, kr * 2, kr * 2), Color(0.5f, 0.62f, 1.0f, 0.55f), kr);
  ui::glow(Rect(knob.x - kr * 2, knob.y - kr * 2, kr * 4, kr * 4), ui::theme.violet.withA(0.3f), 0.8f);
}

void Gameplay::injectInput(Touch* touches, int count) {
  m_touchCount = count < 10 ? count : 10;
  for (int i = 0; i < m_touchCount; i++) m_touchBuf[i] = touches[i];
}

void Gameplay::draw(float dt) {
  if (!m_active) return;
  update(dt);
  drawWorld(dt);
  // 2D HUD
  ui::begin();
  drawJoystick();
  drawHud();

  Engine& e = Engine::get();
  float w = e.width(), h = e.height();
  // right-side action cluster
  Rect jumpBtn(w - ui::dp(96), h - ui::dp(104), ui::dp(72), ui::dp(72));
  auto jb = ui::button("jump", jumpBtn, "", true, true, 14, "play");
  if (jb.clicked) m_jumpRequested = true;

  Rect runBtn(w - ui::dp(180), h - ui::dp(96), ui::dp(62), ui::dp(62));
  auto rb = ui::iconButton("run", runBtn, "chevron_right", m_runHeld ? ui::theme.cyan : ui::theme.text);
  if (rb.clicked) m_runHeld = !m_runHeld;

  Rect micBtn(w - ui::dp(258), h - ui::dp(96), ui::dp(58), ui::dp(58));
  auto mb = ui::iconButton("mic", micBtn, "mic", m_voiceOn ? ui::theme.green : ui::theme.textDim);
  if (mb.clicked) {
    if (!m_voiceOn) {
      e.requestMicPermission();
      audioMicStart();
      m_voiceOn = true;
      ws::voiceJoin();
      app::toast("المايك مفعّل — يُسمعك لاعبو الماب");
    } else {
      audioMicStop();
      m_voiceOn = false;
      ws::voiceLeave();
      app::toast("تم إيقاف المايك");
    }
  }

  Rect chatBtn(w - ui::dp(330), h - ui::dp(96), ui::dp(58), ui::dp(58));
  auto cb = ui::iconButton("chatbtn", chatBtn, "chat");
  if (cb.clicked) toggleChat();

  // interact button (only when something is in range)
  if (m_nearInteractable >= 0) {
    Rect useBtn(w - ui::dp(96), h - ui::dp(196), ui::dp(72), ui::dp(56));
    if (ui::button("use", useBtn, "تفاعل", true, true, 13).clicked) {
      world.useInteractable(m_nearInteractable);
      // emit a burst of particles
      for (int i = 0; i < 18; i++) {
        Particle p{};
        p.pos = world.interactables()[m_nearInteractable].pos + Vec3(0, 0.8f, 0);
        float a = (float)i / 18 * TAU;
        p.vel = Vec3(cosf(a) * 2.2f, 2.6f + hashf((unsigned)i) * 2.0f, sinf(a) * 2.2f);
        p.life = p.maxLife = 0.9f;
        p.color = i % 2 ? ui::theme.cyan : ui::theme.violet;
        m_particles.push(p);
      }
    }
  }

  // pause / exit
  Rect menuBtn(ui::dp(56), ui::dp(12), ui::dp(40), ui::dp(40));
  auto menu = ui::iconButton("menu", menuBtn, "gear");
  if (menu.clicked) m_paused = !m_paused;

  if (m_paused) {
    ui::rect(Rect(0, 0, w, h), Color(0.02f, 0.03f, 0.07f, 0.82f));
    Rect card(w * 0.5f - ui::dp(180), h * 0.5f - ui::dp(150), ui::dp(360), ui::dp(300));
    ui::panel(card);
    ui::text("إيقاف مؤقت", card.x, card.y + ui::dp(20), 20, ui::theme.text, card.w, 1, 2);
    Rect b1(card.x + ui::dp(24), card.y + ui::dp(76), card.w - ui::dp(48), ui::dp(46));
    if (ui::button("resume", b1, "متابعة اللعب", true).clicked) m_paused = false;
    Rect b2(card.x + ui::dp(24), card.y + ui::dp(132), card.w - ui::dp(48), ui::dp(46));
    if (ui::button("leave", b2, "مغادرة الماب").clicked) {
      m_paused = false;
      app::leaveGameplay();
      ui::end();
      return;
    }
    Rect b3(card.x + ui::dp(24), card.y + ui::dp(188), card.w - ui::dp(48), ui::dp(46));
    ui::text("اضغط زر الميكروفون للمحادثة الصوتية", card.x + ui::dp(24), card.y + ui::dp(196), 11,
             ui::theme.textMuted, card.w - ui::dp(48), 1, 0);
    (void)b3;
  }

  // chat input overlay
  if (m_chatOpen) {
    Rect bar(ui::dp(20), h - ui::dp(64), w - ui::dp(40), ui::dp(46));
    ui::panel(Rect(bar.x - ui::dp(8), bar.y - ui::dp(8), bar.w + ui::dp(16), bar.h + ui::dp(16)), ui::dp(14));
    ui::textField("chat", bar, m_chatInput, sizeof m_chatInput, "اكتب رسالة…", false, 160);
    Rect send(bar.x + bar.w - ui::dp(64), bar.y, ui::dp(64), bar.h);
    if (ui::button("send", send, "إرسال", true, m_chatInput[0] != 0, 13).clicked) {
      sendChat(m_chatInput);
      m_chatInput[0] = 0;
      toggleChat();
    }
  }

  // particles (2D projected would need the VP matrix; draw a soft radial burst at screen centre-ish
  // for feedback while keeping the 3D pass clean)
  for (int i = 0; i < m_particles.count; i++) {
    m_particles[i].life -= dt;
    if (m_particles[i].life <= 0) { m_particles.removeAt(i); i--; }
  }
  ui::end();
}

}  // namespace game
}  // namespace nx
