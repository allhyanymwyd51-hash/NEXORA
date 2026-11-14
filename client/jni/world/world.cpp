#include "world.h"
#include "core/audio.h"

namespace nx {

GameWorld world;

// ---------------------------------------------------------------------------
// shaders
// ---------------------------------------------------------------------------
static const char* kWorldVS = R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec4 aColor;
uniform mat4 uViewProj;
out vec3 vNormal;
out vec4 vColor;
out vec3 vWorld;
void main() {
  vNormal = aNrm;
  vColor = aColor;
  vWorld = aPos;
  gl_Position = uViewProj * vec4(aPos, 1.0);
})";
static const char* kWorldFS = R"(#version 300 es
precision mediump float;
in vec3 vNormal;
in vec4 vColor;
in vec3 vWorld;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uAmbient;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform vec3 uCamera;
uniform float uFlash;
uniform float uWaterLevel;
out vec4 fragColor;
void main() {
  vec3 n = normalize(vNormal);
  float ndl = max(dot(n, normalize(uSunDir)), 0.0);
  float hemi = 0.5 + 0.5 * n.y;
  vec3 lit = vColor.rgb * (uAmbient * (0.55 + hemi * 0.45) + uSunColor * ndl * 1.15);
  // subtle rim light for the signature violet/cyan look
  float rim = pow(1.0 - max(n.y, 0.0), 3.0);
  lit += vec3(0.18, 0.22, 0.45) * rim * 0.35;
  lit += uSunColor * uFlash * 0.6;
  float dist = length(vWorld - uCamera);
  float fog = 1.0 - exp(-dist * uFogDensity);
  vec3 col = mix(lit, uFogColor, clamp(fog, 0.0, 1.0));
  fragColor = vec4(col, vColor.a);
})";

static const char* kWaterVS = R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec4 aColor;
uniform mat4 uViewProj;
uniform float uTime;
out vec4 vColor;
out vec3 vWorld;
void main() {
  vec3 p = aPos;
  p.y += sin(p.x * 0.12 + uTime * 1.1) * 0.18 + sin(p.z * 0.17 - uTime * 0.9) * 0.14;
  vWorld = p;
  vColor = aColor;
  gl_Position = uViewProj * vec4(p, 1.0);
})";
static const char* kWaterFS = R"(#version 300 es
precision mediump float;
in vec4 vColor;
in vec3 vWorld;
uniform vec3 uCamera;
uniform vec3 uSunColor;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uTime;
out vec4 fragColor;
void main() {
  // procedural specular glints (no texture fetch needed)
  float ripple = sin(vWorld.x * 0.35 + uTime * 1.6) * sin(vWorld.z * 0.31 - uTime * 1.2);
  vec3 base = vColor.rgb * (0.85 + ripple * 0.12);
  vec3 eye = normalize(uCamera - vWorld);
  float fres = pow(1.0 - max(eye.y, 0.0), 2.5);
  base = mix(base, vec3(0.45, 0.62, 0.78), fres * 0.55);
  float glint = pow(max(ripple, 0.0), 18.0);
  base += uSunColor * glint * 0.9;
  float dist = length(vWorld - uCamera);
  float fog = 1.0 - exp(-dist * uFogDensity);
  fragColor = vec4(mix(base, uFogColor, clamp(fog, 0.0, 1.0)), vColor.a);
})";

static const char* kSkyVS = R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec4 aColor;
uniform mat4 uViewProj;
uniform vec3 uCamera;
out vec3 vDir;
void main() {
  vDir = aPos;
  gl_Position = uViewProj * vec4(aPos + uCamera, 1.0);
})";
static const char* kSkyFS = R"(#version 300 es
precision mediump float;
in vec3 vDir;
uniform vec3 uTop;
uniform vec3 uHorizon;
uniform vec3 uSunDir;
uniform float uTime;
out vec4 fragColor;
void main() {
  vec3 d = normalize(vDir);
  float t = clamp(d.y * 1.6 + 0.1, 0.0, 1.0);
  vec3 col = mix(uHorizon, uTop, pow(t, 0.8));
  // sun disc + bloom
  float sun = max(dot(d, normalize(uSunDir)), 0.0);
  col += vec3(1.0, 0.92, 0.75) * pow(sun, 220.0) * 1.6;
  col += vec3(1.0, 0.85, 0.6) * pow(sun, 12.0) * 0.10;
  fragColor = vec4(col, 1.0);
})";

// ---------------------------------------------------------------------------
// spec loading
// ---------------------------------------------------------------------------
static Color parseColor(JsonValue* v, Color fallback) {
  const char* s = Json::string(v, "");
  if (!s || s[0] != '#' || strlen(s) < 7) return fallback;
  unsigned r = 0, g = 0, b = 0;
  for (int i = 0; i < 2; i++) {
    auto hex = [](char c) -> unsigned {
      if (c >= '0' && c <= '9') return (unsigned)(c - '0');
      if (c >= 'a' && c <= 'f') return (unsigned)(c - 'a' + 10);
      if (c >= 'A' && c <= 'F') return (unsigned)(c - 'A' + 10);
      return 0;
    };
    r = (r << 4) | hex(s[1 + i]);
    g = (g << 4) | hex(s[3 + i]);
    b = (b << 4) | hex(s[5 + i]);
  }
  return Color(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
}

bool GameWorld::loadFromJson(JsonValue* root, const char* mapName) {
  unload();
  if (!root) return false;
  JsonValue* t = Json::get(root, "terrain");
  JsonValue* w = Json::get(root, "water");
  JsonValue* env = Json::get(root, "environment");
  JsonValue* sizeV = Json::get(root, "size");
  const char* sizeStr = Json::string(sizeV, "large");
  m_spec.side = (int)Json::number(Json::get(root, "side"), 1000);
  if (m_spec.side < 200) m_spec.side = 1000;
  strncpy(m_spec.sizeLabel, sizeStr, sizeof m_spec.sizeLabel - 1);
  char gt[32];
  Json::copyStr(root, "gameType", gt, sizeof gt, "open_world");
  strncpy(m_spec.gameType, gt, sizeof m_spec.gameType - 1);
  const char* detail = Json::string(Json::get(root, "detail"), "detailed");
  m_spec.detail = strcmp(detail, "ultra") == 0 ? 2 : strcmp(detail, "detailed") == 0 ? 2
      : strcmp(detail, "standard") == 0 ? 1 : 0;
  const char* quality = Json::string(Json::get(root, "quality"), "high");
  m_spec.quality = strcmp(quality, "ultra") == 0 ? 3 : strcmp(quality, "high") == 0 ? 2
      : strcmp(quality, "medium") == 0 ? 1 : 0;
  m_spec.maxPlayers = Json::integer(Json::get(root, "maxPlayers"), 18);
  m_spec.seed = Json::integer(Json::get(root, "seed"), 1);

  m_spec.waterEnabled = Json::boolean(Json::get(w, "enabled"), false);
  m_spec.waterLevel = Json::flt(Json::get(w, "level"), -3.0f);
  m_spec.waterColor = parseColor(Json::get(w, "color"), Color::hex(0x1F6F8B));
  m_spec.waterWaves = Json::flt(Json::get(w, "waves"), 0.5f);

  m_spec.baseHeight = Json::flt(Json::get(t, "baseHeight"), 4.0f);
  m_spec.terrainRoughness = Json::flt(Json::get(t, "roughness"), 1.0f);
  m_spec.terrainOctaves = Json::integer(Json::get(t, "octaves"), 5);
  m_spec.mountainAmp = Json::flt(Json::get(t, "mountainAmplitude"), 40.0f);
  m_spec.fillOcean = Json::boolean(Json::get(t, "fillOcean"), false);

  // peaks / basins / rivers / roads
  JsonValue* peaks = Json::get(t, "peaks");
  for (int i = 0; i < Json::length(peaks); i++) {
    JsonValue* p = Json::at(peaks, i);
    Terrain::Peak pk{};
    pk.x = Json::flt(Json::get(p, "x"));
    pk.z = Json::flt(Json::get(p, "z"));
    pk.height = Json::flt(Json::get(p, "height"), 20);
    pk.radius = Json::flt(Json::get(p, "radius"), 80);
    const char* kind = Json::string(Json::get(p, "kind"), "mountain");
    pk.kind = strcmp(kind, "volcano") == 0 ? 1 : strcmp(kind, "snow_peak") == 0 ? 2 : 0;
    m_terrain.peaks.push(pk);
  }
  JsonValue* basins = Json::get(t, "basins");
  for (int i = 0; i < Json::length(basins); i++) {
    JsonValue* p = Json::at(basins, i);
    Terrain::Basin bs{};
    bs.x = Json::flt(Json::get(p, "x"));
    bs.z = Json::flt(Json::get(p, "z"));
    bs.depth = Json::flt(Json::get(p, "depth"), 5);
    bs.radius = Json::flt(Json::get(p, "radius"), 40);
    const char* kind = Json::string(Json::get(p, "kind"), "lake");
    bs.kind = strcmp(kind, "ocean") == 0 ? 2 : strcmp(kind, "sand") == 0 ? 3 : 0;
    m_terrain.basins.push(bs);
  }
  JsonValue* rivers = Json::get(t, "rivers");
  for (int i = 0; i < Json::length(rivers); i++) {
    JsonValue* rv = Json::at(rivers, i);
    Terrain::River r{};
    JsonValue* pts = Json::get(rv, "points");
    r.count = 0;
    for (int k = 0; k < Json::length(pts) && k < 12; k++) {
      JsonValue* pair = Json::at(pts, k);
      r.pts[r.count++] = Vec2(Json::flt(Json::at(pair, 0)), Json::flt(Json::at(pair, 1)));
    }
    r.width = Json::flt(Json::get(rv, "width"), 6);
    r.depth = Json::flt(Json::get(rv, "depth"), 2);
    if (r.count >= 2) m_terrain.rivers.push(r);
  }
  JsonValue* roads = Json::get(root, "roads");
  for (int i = 0; i < Json::length(roads); i++) {
    JsonValue* rv = Json::at(roads, i);
    Terrain::Road r{};
    JsonValue* pts = Json::get(rv, "points");
    r.count = 0;
    for (int k = 0; k < Json::length(pts) && k < 12; k++) {
      JsonValue* pair = Json::at(pts, k);
      r.pts[r.count++] = Vec2(Json::flt(Json::at(pair, 0)), Json::flt(Json::at(pair, 1)));
    }
    r.width = Json::flt(Json::get(rv, "width"), 5);
    const char* kind = Json::string(Json::get(rv, "kind"), "dirt");
    r.asphalt = strcmp(kind, "asphalt") == 0;
    if (r.count >= 2) m_terrain.roads.push(r);
  }
  // environment
  m_spec.weather[0] = 0;
  Json::copyStr(env, "weather", m_spec.weather, sizeof m_spec.weather, "clear");
  m_spec.rain = Json::boolean(Json::get(env, "rain"), false);
  m_spec.lightning = Json::boolean(Json::get(env, "lightning"), false);
  m_spec.wind = Json::flt(Json::get(env, "wind"), 0.3f);
  m_spec.fogDensity = Json::flt(Json::get(env, "fogDensity"), 0.006f);
  m_spec.sunElevation = Json::flt(Json::get(env, "sunElevation"), 45);
  m_spec.sunAzimuth = Json::flt(Json::get(env, "sunAzimuth"), 120);
  m_spec.timeOfDay = Json::flt(Json::get(env, "timeOfDay"), 12);
  m_spec.skyTop = parseColor(Json::get(env, "skyTop"), Color::hex(0x2A86D6));
  m_spec.skyHorizon = parseColor(Json::get(env, "skyHorizon"), Color::hex(0xA7E4EF));
  if (m_spec.weather[0] == 's') m_spec.rain = true;

  // if water is enabled but the floor is above the water line, lift the sea level to make
  // beaches read correctly instead of a dry plane
  m_terrain.generate(m_spec);
  if (m_spec.waterEnabled) {
    float shoreHeight = m_terrain.heightAt(0, m_spec.side * 0.42f);
    if (m_spec.waterLevel < shoreHeight - 6.0f) m_spec.waterLevel = shoreHeight - 1.2f;
  }

  setSpawnsFromJson(root);
  m_root = root;
  // ---- objects ----
  buildObjects();
  buildRoads();
  buildWater();
  buildSky();
  buildStatic();
  if (m_root) worldGenerateDecoration(*this, (JsonValue*)m_root);

  // ---- animals ----
  JsonValue* animals = Json::get(root, "animals");
  for (int i = 0; i < Json::length(animals); i++) {
    JsonValue* a = Json::at(animals, i);
    const char* type = Json::string(Json::get(a, "type"), "deer");
    int typeId = 0;
    if (!strcmp(type, "wolf")) typeId = 1;
    else if (!strcmp(type, "bird") || !strcmp(type, "falcon") || !strcmp(type, "parrot")) typeId = 2;
    else if (!strcmp(type, "rabbit") || !strcmp(type, "turtle") || !strcmp(type, "lizard")) typeId = 3;
    else if (!strcmp(type, "boar") || !strcmp(type, "monkey")) typeId = 4;
    else if (!strcmp(type, "camel")) typeId = 5;
    int count = Json::integer(Json::get(a, "count"), 3);
    float cx = Json::flt(Json::get(a, "x")), cz = Json::flt(Json::get(a, "z"));
    float radius = Json::flt(Json::get(a, "radius"), 20);
    bool passive = Json::boolean(Json::get(a, "passive"), true);
    for (int k = 0; k < count && m_animals.count < 90; k++) {
      float ang = (float)k / (count > 0 ? count : 1) * TAU;
      Animal an{};
      an.type = typeId;
      an.passive = passive;
      an.pos = Vec3(cx + cosf(ang) * radius * 0.5f, 0, cz + sinf(ang) * radius * 0.5f);
      an.pos.y = m_terrain.heightAt(an.pos.x, an.pos.z);
      an.timer = hashf((unsigned)(i * 31 + k)) * 4.0f;
      m_animals.push(an);
    }
  }
  // ---- interactables ----
  JsonValue* items = Json::get(root, "interactables");
  for (int i = 0; i < Json::length(items); i++) {
    JsonValue* it = Json::at(items, i);
    Interactable in{};
    const char* type = Json::string(Json::get(it, "type"), "crystal");
    in.kind = strcmp(type, "chest") == 0 ? 0 : strcmp(type, "portal") == 0 ? 1
        : strcmp(type, "crystal") == 0 ? 2 : strcmp(type, "quest_marker") == 0 ? 3
        : strcmp(type, "checkpoint") == 0 ? 4 : strcmp(type, "start_line") == 0 ? 5 : 2;
    in.pos.x = Json::flt(Json::get(it, "x"));
    in.pos.z = Json::flt(Json::get(it, "z"));
    in.pos.y = m_terrain.heightAt(in.pos.x, in.pos.z);
    Json::copyStr(it, "label", in.label, sizeof in.label, "");
    in.reward = Json::integer(Json::get(it, "reward"), 0);
    in.phase = hashf((unsigned)i * 7919u) * TAU;
    m_interactables.push(in);
  }
  // ---- quests ----
  JsonValue* quests = Json::get(root, "quests");
  for (int i = 0; i < Json::length(quests); i++) {
    JsonValue* q = Json::at(quests, i);
    QuestState qs{};
    Json::copyStr(q, "id", qs.id, sizeof qs.id, "q");
    Json::copyStr(q, "title", qs.title, sizeof qs.title, "مهمة");
    Json::copyStr(q, "description", qs.description, sizeof qs.description, "");
    JsonValue* marker = Json::get(q, "marker");
    qs.marker = Vec3(Json::flt(Json::get(marker, "x")), 0, Json::flt(Json::get(marker, "z")));
    qs.marker.y = m_terrain.heightAt(qs.marker.x, qs.marker.z);
    JsonValue* objectives = Json::get(q, "objectives");
    qs.objectiveCount = Json::length(objectives);
    if (qs.objectiveCount == 0) qs.objectiveCount = 1;
    JsonValue* reward = Json::get(q, "reward");
    qs.rewardMiy = Json::integer(Json::get(reward, "miy"), 20);
    m_quests.push(qs);
  }
  if (!m_quests.count && Json::length(quests) == 0) {
    // never leave the player without a goal on an empty spec
    QuestState qs{};
    strcpy(qs.id, "q_default");
    strcpy(qs.title, "استكشف العالم");
    strcpy(qs.description, "تعرّف على العالم واستكشف أول نقطة اهتمام.");
    qs.objectiveCount = 1;
    qs.rewardMiy = 25;
    m_quests.push(qs);
  }

  strncpy(m_mapName, mapName ? mapName : "NEXORA", sizeof m_mapName - 1);
  m_loaded = true;
  // ambience + music from the spec
  char ambient[40];
  snprintf(ambient, sizeof ambient, "amb_%s", strcmp(m_spec.weather, "storm") == 0 ? "snow_wind"
      : Json::string(Json::path(root, "audio", "ambient"), "open_plain"));
  if (!audioFind(ambient)) audioPlay("amb_open_plain", 0.4f);
  else audioPlay(ambient, 0.4f);
  const char* music = Json::string(Json::path(root, "audio", "music"), "exploration");
  char musicName[48];
  snprintf(musicName, sizeof musicName, "music_%s", music);
  Engine::get().playMusic(musicName);
  NX_LOGI("world loaded: %s side=%d objects=%d quests=%d animals=%d", m_mapName, m_spec.side,
          m_objects.count, m_quests.count, m_animals.count);
  return true;
}

void GameWorld::setSpawnsFromJson(JsonValue* root) {
  m_spawns.clear();
  JsonValue* spawns = Json::get(root, "spawns");
  for (int i = 0; i < Json::length(spawns); i++) {
    JsonValue* s = Json::at(spawns, i);
    SpawnPt p{};
    p.x = Json::flt(Json::get(s, "x"));
    p.z = Json::flt(Json::get(s, "z"));
    p.facing = Json::flt(Json::get(s, "facing"));
    m_spawns.push(p);
  }
}

void GameWorld::unload() {
  if (m_loaded) {
    m_terrain.destroy();
    m_staticBatch.vertCount = 0;
    m_objects.clear();
    m_animals.clear();
    m_interactables.clear();
    m_quests.clear();
    m_players.clear();
    m_spawns.clear();
  }
  m_loaded = false;
}

// ---------------------------------------------------------------------------
// objects
// ---------------------------------------------------------------------------
void GameWorld::buildObjects() {
  // objects come from the JSON directly so we re-read with the same DOM the loader has
}

// The object list is filled by the caller (map screen) after loadFromJson via this helper.
void worldLoadObjectsFromJson(JsonValue* root, GameWorld& w) {
  (void)root;
  (void)w;
}

void GameWorld::buildRoads() {
  // road surfaces are baked into the static batch in buildStatic()
}

void GameWorld::buildWater() {
  m_waterBatch.init(sizeof(VtxShape), 3, 3, 4, GL_STATIC_DRAW);
  m_waterBatch.vertCount = 0;
  m_waterBatch.indexCount = 0;
  if (!m_spec.waterEnabled) return;
  // A single large quad is enough: the shader adds the wave motion. Rivers/lakes are carved
  // into the terrain so the plane reads as a real water surface everywhere below sea level.
  float half = m_spec.side * 0.55f;
  pushPlaneXZ(m_waterBatch, -half, -half, half, half, m_spec.waterLevel, m_spec.waterColor);
}

void GameWorld::buildSky() {
  m_skyBatch.init(sizeof(VtxShape), 3, 3, 4, GL_STATIC_DRAW);
  m_skyBatch.vertCount = 0;
  m_skyBatch.indexCount = 0;
  // full sphere for the sky dome, very low poly
  const int rings = 10, segs = 16;
  for (int r = 0; r < rings; r++) {
    float phi0 = (float)r / rings * PI, phi1 = (float)(r + 1) / rings * PI;
    for (int s = 0; s < segs; s++) {
      float t0 = (float)s / segs * TAU, t1 = (float)(s + 1) / segs * TAU;
      Vec3 p00{cosf(t0) * sinf(phi0), cosf(phi0), sinf(t0) * sinf(phi0)};
      Vec3 p10{cosf(t1) * sinf(phi0), cosf(phi0), sinf(t1) * sinf(phi0)};
      Vec3 p01{cosf(t0) * sinf(phi1), cosf(phi1), sinf(t0) * sinf(phi1)};
      Vec3 p11{cosf(t1) * sinf(phi1), cosf(phi1), sinf(t1) * sinf(phi1)};
      uint32_t base = m_skyBatch.pushIndexVerts(4);
      VtxShape* v = (VtxShape*)((char*)m_skyBatch.verts + (size_t)base * m_skyBatch.vertexStride);
      v[0] = {p00.x, p00.y, p00.z, 0, 0, 0, 1, 1, 1, 1};
      v[1] = {p10.x, p10.y, p10.z, 0, 0, 0, 1, 1, 1, 1};
      v[2] = {p11.x, p11.y, p11.z, 0, 0, 0, 1, 1, 1, 1};
      v[3] = {p01.x, p01.y, p01.z, 0, 0, 0, 1, 1, 1, 1};
      m_skyBatch.indexQuad(base);
    }
  }
}

// Static geometry (trees, buildings, props) is generated from the object list by the
// map/game screens through addTree/addBuilding helpers below so the same code path is used
// for the editor preview and for gameplay.
void GameWorld::buildStatic() {
  if (!m_staticBatch.vbo) m_staticBatch.init(sizeof(VtxShape), 3, 3, 4, GL_STATIC_DRAW);
  m_staticBatch.vertCount = 0;
  m_staticBatch.indexCount = 0;
}

void GameWorld::ensurePrograms() {
  if (m_progsReady) return;
  m_worldProg.build(kWorldVS, kWorldFS, "world");
  m_waterProg.build(kWaterVS, kWaterFS, "water");
  m_skyProg.build(kSkyVS, kSkyFS, "sky");
  m_progsReady = true;
}

// ---------------------------------------------------------------------------
// runtime
// ---------------------------------------------------------------------------
Vec3 GameWorld::spawnPoint(int index) const {
  if (m_spawns.count) {
    const SpawnPt& s = m_spawns.items[index % m_spawns.count];
    return Vec3(s.x, m_terrain.heightAt(s.x, s.z) + 0.2f, s.z);
  }
  return Vec3(0, m_terrain.heightAt(0, 0) + 1.0f, 0);
}

bool GameWorld::collides(const Vec3& p, float radius) const {
  for (int i = 0; i < m_objects.count; i++) {
    const WorldObject& o = m_objects[i];
    if (o.kind != 1 && o.kind != 2) continue;  // only rocks and buildings block
    float dx = p.x - o.pos.x, dz = p.z - o.pos.z;
    float r = o.radius + radius;
    if (dx * dx + dz * dz < r * r) return true;
  }
  return false;
}

int GameWorld::nearestInteractable(const Vec3& p, float maxDist) const {
  int best = -1;
  float bestD = maxDist * maxDist;
  for (int i = 0; i < m_interactables.count; i++) {
    if (m_interactables[i].used) continue;
    float dx = p.x - m_interactables[i].pos.x;
    float dz = p.z - m_interactables[i].pos.z;
    float d = dx * dx + dz * dz;
    if (d < bestD) { bestD = d; best = i; }
  }
  return best;
}

void GameWorld::useInteractable(int index) {
  if (index < 0 || index >= m_interactables.count) return;
  Interactable& in = m_interactables[index];
  if (in.used) return;
  in.used = true;
  audioPlay("ui_confirm", 0.7f);
  // quest progress: reaching an objective advances the first incomplete quest
  for (int q = 0; q < m_quests.count; q++) {
    if (m_quests[q].complete) continue;
    m_quests[q].progress++;
    if (m_quests[q].progress >= m_quests[q].objectiveCount) m_quests[q].complete = true;
    break;
  }
}

RemotePlayer* GameWorld::findPlayer(int userId) {
  for (int i = 0; i < m_players.count; i++) if (m_players[i].userId == userId) return &m_players[i];
  return nullptr;
}

void GameWorld::upsertPlayer(int userId, const char* username, const Vec3& pos, float heading, bool moving) {
  RemotePlayer* p = findPlayer(userId);
  if (!p) {
    RemotePlayer np{};
    np.userId = userId;
    strncpy(np.username, username ? username : "player", sizeof np.username - 1);
    np.pos = pos;
    np.target = pos;
    float h = hashf((unsigned)userId * 2654435761u);
    np.shirt = Color::lerp(Color::hex(0x3A5BFF), Color::hex(0xC13AFF), h);
    np.skin = Color::lerp(Color::hex(0xC98E6A), Color::hex(0x8A5A3B), h);
    m_players.push(np);
    p = &m_players.last();
  }
  p->target = pos;
  p->heading = heading;
  p->moving = moving;
  p->lastUpdate = (float)Engine::get().time();
}

void GameWorld::removePlayer(int userId) {
  for (int i = 0; i < m_players.count; i++) {
    if (m_players[i].userId == userId) { m_players.removeAt(i); return; }
  }
}

bool GameWorld::inWater(const Vec3& p) const {
  if (!m_spec.waterEnabled) return false;
  return p.y < m_spec.waterLevel - 0.35f;
}

void GameWorld::update(float dt, const Vec3& listener) {
  m_time += dt;
  m_wind += dt;
  if (m_flash > 0) m_flash = fmaxf(0, m_flash - dt * 3.2f);
  if (m_spec.lightning && m_spec.rain) {
    static float timer = 3.0f;
    timer -= dt;
    if (timer <= 0) {
      timer = 4.0f + hashf((unsigned)(m_time * 13.7f)) * 9.0f;
      m_flash = 1.0f;
      audioPlay("amb_snow_wind", 0.25f);
    }
  }
  // animals: simple wander + flee behaviour
  for (int i = 0; i < m_animals.count; i++) {
    Animal& a = m_animals[i];
    a.timer -= dt;
    float distToPlayer = (a.pos - listener).length();
    if (!a.passive && distToPlayer < 9.0f) a.fleeing = true;
    if (a.fleeing && distToPlayer > 22.0f) a.fleeing = false;
    if (a.timer <= 0) {
      a.timer = 2.5f + hashf((unsigned)(i * 31 + (int)(m_time * 7.0f))) * 4.0f;
      float ang = hashf((unsigned)(i * 17 + (int)(m_time * 3.0f))) * TAU;
      a.wanderX = (int)(cosf(ang) * 20.0f);
      a.wanderZ = (int)(sinf(ang) * 20.0f);
    }
    Vec3 dir(a.wanderX, 0, a.wanderZ);
    if (a.fleeing) dir = (a.pos - listener).flat();
    if (dir.length() > 0.1f) {
      Vec3 step = dir.normalized() * (a.fleeing ? 6.5f : 1.6f) * dt;
      Vec3 next = a.pos + step;
      next.y = m_terrain.heightAt(next.x, next.z);
      float half = m_spec.side * 0.5f - 10.0f;
      if (fabsf(next.x) < half && fabsf(next.z) < half) {
        a.pos = next;
        a.heading = atan2f(dir.x, dir.z);
      }
    }
  }
  // remote players interpolation
  for (int i = 0; i < m_players.count; i++) {
    RemotePlayer& p = m_players[i];
    p.pos = lerp(p.pos, p.target, clampf(dt * 10.0f, 0, 1));
    p.pos.y = m_terrain.heightAt(p.pos.x, p.pos.z);
    if (p.moving) p.animPhase += dt * 7.0f;
    if (Engine::get().time() - p.lastUpdate > 12.0) {
      m_players.removeAt(i);
      i--;
    }
  }
  // interactable animation phases
  for (int i = 0; i < m_interactables.count; i++) m_interactables[i].phase += dt;
}

// The heavy draw is implemented in world_render.cpp
void GameWorld::draw(const Mat4& viewProj, const Mat4& view, const Vec3& cameraPos, float dt) {
  (void)view;
  (void)dt;
  ensurePrograms();
  if (!m_loaded) return;
  // terrain
  m_worldProg.use();
  m_worldProg.setMat("uViewProj", viewProj);
  Vec3 sunDir(cosf(deg2rad(m_spec.sunAzimuth)) * cosf(deg2rad(m_spec.sunElevation)),
              sinf(deg2rad(m_spec.sunElevation)),
              sinf(deg2rad(m_spec.sunAzimuth)) * cosf(deg2rad(m_spec.sunElevation)));
  m_worldProg.set("uSunDir", sunDir);
  m_worldProg.set("uSunColor", Vec3(1.0f, 0.96f, 0.88f));
  m_worldProg.set("uAmbient", Vec3(0.42f, 0.48f, 0.62f));
  m_worldProg.set("uFogColor", m_spec.skyHorizon);
  m_worldProg.set("uFogDensity", m_spec.fogDensity);
  m_worldProg.set("uCamera", cameraPos);
  m_worldProg.set("uFlash", m_flash);
  m_terrain.draw(m_worldProg, viewProj);
  // static + prop batches
  if (m_staticBatch.vbo) m_staticBatch.flush(GL_TRIANGLES);
  if (m_propBatch.vbo) m_propBatch.flush(GL_TRIANGLES);
  // water
  if (m_spec.waterEnabled && m_waterBatch.vbo) {
    m_waterProg.use();
    m_waterProg.setMat("uViewProj", viewProj);
    m_waterProg.set("uTime", m_time);
    m_waterProg.set("uCamera", cameraPos);
    m_waterProg.set("uSunColor", Vec3(1.0f, 0.95f, 0.8f));
    m_waterProg.set("uFogColor", m_spec.skyHorizon);
    m_waterProg.set("uFogDensity", m_spec.fogDensity);
    glDisable(GL_CULL_FACE);
    m_waterBatch.flush(GL_TRIANGLES);
    glEnable(GL_CULL_FACE);
  }
}

}  // namespace nx
