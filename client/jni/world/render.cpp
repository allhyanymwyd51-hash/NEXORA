// Terrain decoration + prop rendering: trees, rocks, buildings, roads, interactables, animals,
// characters. Geometry is generated from the world JSON with deterministic variation and uploaded
// to a few static vertex buffers so draw cost stays low on mobile GPUs.
#include "world.h"
#include "core/audio.h"

namespace nx {

static VtxShape* vtxAt(MeshBatcher& b, uint32_t base) {
  return (VtxShape*)((char*)b.verts + (size_t)base * b.vertexStride);
}

// ---------------------------------------------------------------------------
// vegetation
// ---------------------------------------------------------------------------
static void addTree(MeshBatcher& b, const Vec3& base, float height, int species, const Color& leaf, float rot) {
  // trunk
  Color bark = Color::hex(0x5A4630);
  pushCylinder(b, base, height * 0.055f, height * 0.62f, 5, bark, bark * 1.1f, false);
  if (species == 0) {
    // palm: fronds from the top
    Vec3 top(base.x, base.y + height * 0.62f, base.z);
    for (int i = 0; i < 7; i++) {
      float a = rot + (float)i / 7 * TAU;
      Vec3 dir(cosf(a), -0.35f, sinf(a));
      Vec3 p1 = top + dir * (height * 0.22f);
      Vec3 p2 = top + dir * (height * 0.40f) + Vec3(0, -height * 0.12f, 0);
      pushQuad(b, top, p1 + Vec3(0, height * 0.05f, 0), p2, p1 - dir * (height * 0.02f),
               Vec3(0, 1, 0), leaf);
      pushQuad(b, top, p1 - dir * (height * 0.02f), p2, p1 + Vec3(0, height * 0.05f, 0),
               Vec3(0, -1, 0), leaf * 0.8f);
    }
    pushSphere(b, top, height * 0.04f, 3, 5, Color::hex(0x6B4A22), Color::hex(0x3A2A12));
  } else if (species == 2) {
    // pine: stacked cones
    float y = base.y + height * 0.35f;
    for (int i = 0; i < 3; i++) {
      float r = height * (0.22f - i * 0.05f);
      float hh = height * 0.34f;
      pushCone(b, Vec3(base.x, y, base.z), r, hh, 7, leaf * (1.0f - i * 0.10f));
      y += hh * 0.58f;
    }
  } else {
    // broadleaf: clustered spheres
    Vec3 top(base.x, base.y + height * 0.70f, base.z);
    float r = height * 0.24f;
    pushSphere(b, top, r, 5, 7, leaf * 1.12f, leaf * 0.72f);
    pushSphere(b, top + Vec3(r * 0.5f, -r * 0.25f, r * 0.2f), r * 0.72f, 4, 6, leaf * 1.02f, leaf * 0.66f);
    pushSphere(b, top + Vec3(-r * 0.55f, -r * 0.18f, -r * 0.4f), r * 0.66f, 4, 6, leaf * 1.05f, leaf * 0.70f);
  }
}

static void addRock(MeshBatcher& b, const Vec3& pos, float scale, int kind) {
  Color c = kind == 1 ? Color::hex(0x87745A) : kind == 2 ? Color::hex(0xDCE6F2) : Color::hex(0x6E6E78);
  pushSphere(b, pos + Vec3(0, scale * 0.35f, 0), scale, 4, 6, c * 1.12f, c * 0.72f);
  pushSphere(b, pos + Vec3(scale * 0.6f, scale * 0.2f, scale * 0.2f), scale * 0.62f, 3, 5, c * 1.05f, c * 0.68f);
}

// ---------------------------------------------------------------------------
// buildings
// ---------------------------------------------------------------------------
static void addBuilding(MeshBatcher& b, const Vec3& pos, float w, float d, float h, float rot,
                        const char* style, const char* roof) {
  Color wall = Color::hex(0xC9C4B8);
  Color trim = Color::hex(0x8A8578);
  int windows = 0;
  if (!strcmp(style, "modern")) {
    wall = Color::hex(0x5E6A82);
    trim = Color::hex(0x38415A);
    windows = 1;
  } else if (!strcmp(style, "rural")) {
    wall = Color::hex(0xB99A74);
    trim = Color::hex(0x7E6547);
  } else if (!strcmp(style, "tropical")) {
    wall = Color::hex(0xD8C79C);
    trim = Color::hex(0x8F7B4E);
  } else if (!strcmp(style, "alpine")) {
    wall = Color::hex(0x9C7A5A);
    trim = Color::hex(0x5D4632);
  } else if (!strcmp(style, "arena")) {
    wall = Color::hex(0x4A4450);
    trim = Color::hex(0x7C4DFF);
  } else if (!strcmp(style, "landmark")) {
    wall = Color::hex(0x7C6A9A);
    trim = Color::hex(0x3A2F55);
  }
  Vec3 center(pos.x, pos.y + h * 0.5f, pos.z);
  pushBox(b, center, Vec3(w * 0.5f, h * 0.5f, d * 0.5f), wall * 1.06f, wall * 0.86f, rot, 0.6f);
  // roof
  if (!strcmp(roof, "gable")) {
    Color roofC = Color::hex(0x7A3B33);
    Vec3 top(pos.x, pos.y + h, pos.z);
    float hw = w * 0.56f, hd = d * 0.56f, rh = h * 0.28f + 1.2f;
    float cs = cosf(rot), sn = sinf(rot);
    auto rp = [&](float lx, float ly, float lz) {
      return Vec3(top.x + lx * cs - lz * sn, top.y + ly, top.z + lx * sn + lz * cs);
    };
    Vec3 ridgeA = rp(0, rh, -hd), ridgeB = rp(0, rh, hd);
    pushQuad(b, rp(-hw, 0, -hd), rp(-hw, 0, hd), ridgeB, ridgeA, Vec3(-0.7f, 0.7f, 0), roofC);
    pushQuad(b, rp(hw, 0, -hd), rp(hw, 0, hd), ridgeB, ridgeA, Vec3(0.7f, 0.7f, 0), roofC * 0.9f);
    pushSphere(b, Vec3(top.x, top.y + rh, top.z), 0.1f, 2, 3, roofC, roofC);
  } else if (!strcmp(roof, "hip")) {
    Color roofC = Color::hex(0x6E4A3A);
    float hw = w * 0.58f, hd = d * 0.58f;
    float cs = cosf(rot), sn = sinf(rot);
    auto rp = [&](float lx, float lz, float ly) {
      return Vec3(pos.x + lx * cs - lz * sn, pos.y + ly, pos.z + lx * sn + lz * cs);
    };
    Vec3 apex = rp(0, 0, h + h * 0.3f);
    Vec3 c0 = rp(-hw, -hd, h), c1 = rp(hw, -hd, h), c2 = rp(hw, hd, h), c3 = rp(-hw, hd, h);
    pushQuad(b, c0, c1, apex, apex, Vec3(0, 0.8f, -0.6f), roofC);
    pushQuad(b, c1, c2, apex, apex, Vec3(0.6f, 0.8f, 0), roofC * 0.92f);
    pushQuad(b, c2, c3, apex, apex, Vec3(0, 0.8f, 0.6f), roofC);
    pushQuad(b, c3, c0, apex, apex, Vec3(-0.6f, 0.8f, 0), roofC * 0.92f);
  } else {
    // flat roof with parapet
    pushBox(b, Vec3(pos.x, pos.y + h + 0.25f, pos.z), Vec3(w * 0.52f, 0.25f, d * 0.52f), trim * 1.2f, trim, rot, 0.7f);
  }
  // windows: emissive-looking glass strips
  if (windows) {
    int floors = (int)fmaxf(1.0f, h / 3.4f);
    Color glass = Color::hex(0x9FD8FF);
    for (int f = 0; f < floors && f < 14; f++) {
      float y = pos.y + 1.6f + f * 3.4f;
      if (y > pos.y + h - 0.8f) break;
      float cs = cosf(rot), sn = sinf(rot);
      auto rp = [&](float lx, float lz) {
        return Vec3(pos.x + lx * cs - lz * sn, y, pos.z + lx * sn + lz * cs);
      };
      Vec3 a = rp(-w * 0.34f, -d * 0.505f), bq = rp(w * 0.34f, -d * 0.505f);
      pushQuad(b, a, bq, bq + Vec3(0, 1.4f, 0), a + Vec3(0, 1.4f, 0), Vec3(0, 0, -1), glass);
      Vec3 c = rp(w * 0.505f, -d * 0.34f), dq = rp(w * 0.505f, d * 0.34f);
      pushQuad(b, c, dq, dq + Vec3(0, 1.4f, 0), c + Vec3(0, 1.4f, 0), Vec3(1, 0, 0), glass * 0.9f);
    }
  }
  // door
  float cs = cosf(rot), sn = sinf(rot);
  Vec3 door(pos.x + (-d * 0.5f - 0.02f) * sn, pos.y + 1.05f, pos.z + (-d * 0.5f - 0.02f) * cs);
  pushBox(b, door, Vec3(0.55f, 1.05f, 0.06f), trim * 0.6f, trim * 0.5f, rot, 0.5f);
  (void)cs;
}

// ---------------------------------------------------------------------------
// roads (baked as thin quads following the spline, draped on the terrain)
// ---------------------------------------------------------------------------
static void addRoad(MeshBatcher& b, const Terrain& t, const Terrain::Road& road) {
  Color asphalt = road.asphalt ? Color::hex(0x3A3A42) : Color::hex(0x8A7A5E);
  Color line = road.asphalt ? Color::hex(0xE8D48A) : asphalt * 1.1f;
  for (int s = 0; s + 1 < road.count; s++) {
    Vec2 a = road.pts[s], bq = road.pts[s + 1];
    Vec2 dir = (bq - a);
    float len = dir.length();
    if (len < 0.01f) continue;
    Vec2 n = Vec2(-dir.y / len, dir.x / len);
    float halfW = road.width * 0.5f;
    const int steps = 4;
    for (int k = 0; k < steps; k++) {
      float t0 = (float)k / steps, t1 = (float)(k + 1) / steps;
      Vec2 p0 = a + dir * t0, p1 = a + dir * t1;
      Vec3 v00(p0.x - n.x * halfW, 0, p0.y - n.y * halfW);
      Vec3 v01(p0.x + n.x * halfW, 0, p0.y + n.y * halfW);
      Vec3 v10(p1.x - n.x * halfW, 0, p1.y - n.y * halfW);
      Vec3 v11(p1.x + n.x * halfW, 0, p1.y + n.y * halfW);
      v00.y = t.heightAt(v00.x, v00.z) + 0.06f;
      v01.y = t.heightAt(v01.x, v01.z) + 0.06f;
      v10.y = t.heightAt(v10.x, v10.z) + 0.06f;
      v11.y = t.heightAt(v11.x, v11.z) + 0.06f;
      pushQuad(b, v00, v01, v11, v10, Vec3(0, 1, 0), asphalt);
      if (road.asphalt) {
        Vec3 c0(p0.x - n.x * 0.16f, v00.y + 0.02f, p0.y - n.y * 0.16f);
        Vec3 c1(p0.x + n.x * 0.16f, v01.y + 0.02f, p0.y + n.y * 0.16f);
        Vec3 c2(p1.x + n.x * 0.16f, v11.y + 0.02f, p1.y + n.y * 0.16f);
        Vec3 c3(p1.x - n.x * 0.16f, v10.y + 0.02f, p1.y - n.y * 0.16f);
        pushQuad(b, c0, c1, c2, c3, Vec3(0, 1, 0), line);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// world decoration entry point used by the loader
// ---------------------------------------------------------------------------
void worldGenerateDecoration(GameWorld& w, JsonValue* root) {
  JsonValue* terrain = Json::get(root, "terrain");
  Terrain& t = w.terrain();
  // static batch: trees, rocks, buildings, roads, landmarks
  MeshBatcher& sb = w.staticBatcher();
  sb.vertCount = 0;
  sb.indexCount = 0;
  float density = Engine::get().quality().vegetationDensity;
  int detailLevel = Engine::get().quality().terrainDetail;

  // -- forests --
  JsonValue* forests = Json::get(root, "forests");
  srand((unsigned)Json::integer(Json::get(root, "seed"), 1));
  for (int i = 0; i < Json::length(forests); i++) {
    JsonValue* f = Json::at(forests, i);
    float cx = Json::flt(Json::get(f, "x")), cz = Json::flt(Json::get(f, "z"));
    float radius = Json::flt(Json::get(f, "radius"), 60);
    int count = Json::integer(Json::get(f, "count"), 60);
    const char* sp = Json::string(Json::get(f, "species"), "broadleaf");
    int species = strcmp(sp, "palm") == 0 ? 0 : strcmp(sp, "pine") == 0 ? 2 : 1;
    float h = Json::flt(Json::get(f, "height"), 9);
    int budget = (int)(count * clampf(density, 0.3f, 1.6f));
    if (detailLevel == 0) budget = budget * 2 / 3;
    Color leaf = species == 0 ? Color::hex(0x3E8C4A) : species == 2 ? Color::hex(0x2C5A3A) : Color::hex(0x3E6E33);
    for (int k = 0; k < budget; k++) {
      float ang = hashf((unsigned)(i * 7919 + k * 104729)) * TAU;
      float rad = sqrtf(hashf((unsigned)(i * 3571 + k * 6151))) * radius;
      float px = cx + cosf(ang) * rad;
      float pz = cz + sinf(ang) * rad;
      float py = t.heightAt(px, pz);
      if (w.spec().waterEnabled && py < w.spec().waterLevel + 0.6f) continue;
      float height = h * (0.75f + hashf((unsigned)(k * 3 + i)) * 0.6f);
      Color lf = Color::lerp(leaf, leaf * 1.25f, hashf((unsigned)(k * 17 + i * 5)));
      addTree(sb, Vec3(px, py, pz), height, species, lf, hashf((unsigned)(k * 31)) * TAU);
      if (sb.vertCount > 380000) break;  // budget guard for very dense worlds
      WorldObject o{};
      o.kind = 0;
      o.pos = Vec3(px, py, pz);
      o.radius = height * 0.10f;
      o.height = height;
      o.tint = species;
      w.objects().push(o);
    }
  }
  // -- rocks --
  JsonValue* rocks = Json::get(root, "rocks");
  for (int i = 0; i < Json::length(rocks); i++) {
    JsonValue* r = Json::at(rocks, i);
    float cx = Json::flt(Json::get(r, "x")), cz = Json::flt(Json::get(r, "z"));
    float scale = Json::flt(Json::get(r, "scale"), 1.5f);
    int count = Json::integer(Json::get(r, "count"), 4);
    const char* kind = Json::string(Json::get(r, "kind"), "granite");
    int kindId = strcmp(kind, "sandstone") == 0 ? 1 : strcmp(kind, "snow_rock") == 0 ? 2 : 0;
    for (int k = 0; k < count; k++) {
      float ang = hashf((unsigned)(i * 131 + k * 71)) * TAU;
      float rad = hashf((unsigned)(i * 97 + k * 29)) * 9.0f;
      float px = cx + cosf(ang) * rad, pz = cz + sinf(ang) * rad;
      float py = t.heightAt(px, pz);
      float sc = scale * (0.5f + hashf((unsigned)(k * 13 + i)) * 0.9f);
      addRock(sb, Vec3(px, py, pz), sc, kindId);
      WorldObject o{};
      o.kind = 1;
      o.pos = Vec3(px, py, pz);
      o.radius = sc * 0.9f;
      o.height = sc * 1.2f;
      w.objects().push(o);
    }
  }
  // -- buildings --
  JsonValue* buildings = Json::get(root, "buildings");
  char roofBuf[16];
  for (int i = 0; i < Json::length(buildings); i++) {
    JsonValue* b = Json::at(buildings, i);
    float x = Json::flt(Json::get(b, "x")), z = Json::flt(Json::get(b, "z"));
    float bw = Json::flt(Json::get(b, "w"), 8), bd = Json::flt(Json::get(b, "d"), 8);
    float bh = Json::flt(Json::get(b, "h"), 5);
    float rot = Json::flt(Json::get(b, "rot"), 0);
    const char* style = Json::string(Json::get(b, "style"), "classic");
    Json::copyStr(b, "roof", roofBuf, sizeof roofBuf, "gable");
    // flatten the ground under a building footprint so it does not float or sink
    float y = t.heightAt(x, z);
    addBuilding(sb, Vec3(x, y, z), bw, bd, bh, rot, style, roofBuf);
    WorldObject o{};
    o.kind = 2;
    o.pos = Vec3(x, y, z);
    o.radius = fmaxf(bw, bd) * 0.5f;
    o.height = bh;
    w.objects().push(o);
  }
  // -- roads --
  for (int i = 0; i < t.roads.count; i++) addRoad(sb, t, t.roads[i]);

  // upload the static batch once
  sb.flush(GL_TRIANGLES);
  NX_LOGI("decoration: objects=%d static verts uploaded", w.objects().count);
}

}  // namespace nx
