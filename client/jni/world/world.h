// NEXORA engine — executes an AI-authored world spec (nexora-world/1) into real geometry:
// heightfield terrain with biomes, forests, buildings, roads, rivers/lakes, caves, interactables,
// animals, lighting/sky/weather, quests and spawns.
#pragma once
#include "core/engine.h"
#include "core/gfx.h"
#include "core/json.h"
#include "core/math.h"

namespace nx {

enum class Biome { Ocean = 0, Beach, Grass, Forest, Jungle, Rock, Snow, Sand, Volcanic, Urban, Neon };

struct WorldSpec {
  char name[80] = {0};
  char gameType[32] = "open_world";
  char sizeLabel[32] = "large";
  int side = 1000;
  int detail = 1;      // 0 low..2 high
  int quality = 2;
  int maxPlayers = 18;
  float waterLevel = -999;
  bool waterEnabled = false;
  Color waterColor = Color::hex(0x1F6F8B);
  float waterWaves = 0.5f;
  int terrainOctaves = 5;
  float terrainRoughness = 1.0f;
  float mountainAmp = 40.0f;
  float baseHeight = 4.0f;
  bool fillOcean = false;
  int seed = 1;
  char weather[24] = "clear";
  float fogDensity = 0.006f;
  Color skyTop = Color::hex(0x2A86D6);
  Color skyHorizon = Color::hex(0xA7E4EF);
  float sunElevation = 45;
  float sunAzimuth = 120;
  float timeOfDay = 12;
  bool rain = false;
  bool lightning = false;
  float wind = 0.3f;
  bool hasNight = false;
};

// A placed world object used for culling, collision and interaction.
struct WorldObject {
  int kind;         // 0 tree, 1 rock, 2 building, 3 interactable, 4 animal, 5 landmark
  Vec3 pos;
  float radius;
  float height;
  int tint;         // species/index
  int extra;
};

class Terrain {
 public:
  bool generate(const WorldSpec& spec);
  float heightAt(float x, float z) const;
  Vec3 normalAt(float x, float z) const;
  Biome biomeAt(float x, float z, float h) const;
  Color colorAt(float x, float z, float h) const;
  void buildMesh(int detail);
  void draw(Program& prog, const Mat4& viewProj);
  void destroy();
  bool ready() const { return m_vbo != 0; }

  // heightfield data stays available for collision after the mesh is uploaded
  int gridSize() const { return m_grid; }
  float cellSize() const { return m_cell; }
  float halfSize() const { return (m_grid - 1) * m_cell * 0.5f; }
  float rawHeight(int ix, int iz) const;

  // river / road masks for blending
  bool isWater(float x, float z) const;

  struct Peak { float x, z, height, radius; int kind; };
  struct Basin { float x, z, depth, radius; int kind; };
  struct River { Vec2 pts[12]; int count; float width, depth; };
  Array<Peak> peaks;
  Array<Basin> basins;
  Array<River> rivers;
  struct Road { Vec2 pts[12]; int count; float width; bool asphalt; };
  Array<Road> roads;
  WorldSpec spec;

 private:
  float* m_heights = nullptr;
  uint32_t m_vbo = 0;
  uint32_t m_ibo = 0;
  int m_indexCount = 0;
  int m_grid = 0;
  float m_cell = 1.0f;
};

struct Animal {
  int type = 0;
  Vec3 pos;
  float heading = 0;
  float timer = 0;
  int wanderX = 0, wanderZ = 0;
  bool fleeing = false;
  bool passive = true;
};

struct Interactable {
  int kind = 0;    // 0 chest, 1 portal, 2 crystal, 3 quest marker, 4 checkpoint, 5 start line
  Vec3 pos;
  char label[48] = {0};
  int reward = 0;
  bool used = false;
  float phase = 0;
};

struct QuestState {
  char id[40] = {0};
  char title[80] = {0};
  char description[160] = {0};
  int objectiveCount = 0;
  int progress = 0;
  bool complete = false;
  bool notified = false;   // the HUD announced completion for this quest
  int rewardMiy = 0;
  Vec3 marker;
};

struct RemotePlayer {
  int userId = 0;             // network key (string id hashed into an int)
  char uid[40] = {0};         // stable server id ("usr_…")
  char username[40] = {0};
  Vec3 pos;
  Vec3 target;
  float heading = 0;
  float animPhase = 0;
  bool moving = false;
  bool speaking = false;
  Color shirt = Color::hex(0x5B6BFF);
  Color skin = Color::hex(0xC98E6A);
  float lastUpdate = 0;
};

class GameWorld {
 public:
  bool loadFromJson(JsonValue* world, const char* mapName);
  void unload();
  void update(float dt, const Vec3& listener);
  void draw(const Mat4& viewProj, const Mat4& view, const Vec3& cameraPos, float dt);

  Terrain& terrain() { return m_terrain; }
  const WorldSpec& spec() const { return m_spec; }
  bool loaded() const { return m_loaded; }
  Array<WorldObject>& objects() { return m_objects; }
  Array<Animal>& animals() { return m_animals; }
  Array<Interactable>& interactables() { return m_interactables; }
  Array<QuestState>& quests() { return m_quests; }
  Array<RemotePlayer>& players() { return m_players; }

  Vec3 spawnPoint(int index = 0) const;
  bool collides(const Vec3& p, float radius) const;
  int nearestInteractable(const Vec3& p, float maxDist) const;
  void useInteractable(int index);
  float waterLevel() const { return m_spec.waterLevel; }
  bool inWater(const Vec3& p) const;
  RemotePlayer* findPlayer(int userId);
  RemotePlayer* findByUid(const char* uid);
  void setSpawnsFromJson(JsonValue* root);
  MeshBatcher& staticBatcher() { return m_staticBatch; }
  MeshBatcher& propBatcher() { return m_propBatch; }
  void upsertPlayer(const char* uid, const char* username, const Vec3& pos, float heading, bool moving);
  void removePlayer(int userId);
  // lightning flash + rain intensity for the renderer
  float flashIntensity() const { return m_flash; }
  void triggerFlash() { m_flash = 1.0f; }
  const char* mapName() const { return m_mapName; }
  void setRoot(void* root) { m_root = root; }

 private:
  void buildStatic();
  void buildObjects();
  void buildRoads();
  void buildWater();
  void buildSky();

  Terrain m_terrain;
  WorldSpec m_spec;
  bool m_loaded = false;
  char m_mapName[80] = {0};
  void* m_root = nullptr;

  Array<WorldObject> m_objects;
  Array<Animal> m_animals;
  Array<Interactable> m_interactables;
  Array<QuestState> m_quests;
  Array<RemotePlayer> m_players;
 public:
  struct SpawnPt { float x, z, facing; };
  Array<SpawnPt> m_spawns;

  MeshBatcher m_staticBatch;   // trees, buildings, props (static per map)
  MeshBatcher m_propBatch;     // interactive objects (animated)
  MeshBatcher m_waterBatch;
  MeshBatcher m_skyBatch;
  Program m_worldProg, m_waterProg, m_skyProg, m_instProg;
  bool m_progsReady = false;
  float m_time = 0;
  float m_flash = 0;
  float m_rainAccum = 0;
  float m_wind = 0;
  void ensurePrograms();
};

extern GameWorld world;
// Builds all static decoration (trees, rocks, buildings, roads) from the world JSON and uploads it.
void worldGenerateDecoration(GameWorld& w, JsonValue* root);

}  // namespace nx
