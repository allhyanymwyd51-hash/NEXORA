#include "world.h"

namespace nx {

// ---------------------------------------------------------------------------
// heightfield generation from the AI world spec
// ---------------------------------------------------------------------------
static float falloff(float x, float z, float half, float margin) {
  float d = fmaxf(fabsf(x), fabsf(z)) / half;
  float inner = 1.0f - margin;
  if (d <= inner) return 1.0f;
  return clampf(1.0f - (d - inner) / margin, 0.0f, 1.0f);
}

bool Terrain::generate(const WorldSpec& specIn) {
  spec = specIn;
  // detail budget: never exceed ~1M vertices on phones
  int grid;
  switch (spec.detail) {
    case 0: grid = 129; break;
    case 1: grid = 193; break;
    default: grid = 257; break;
  }
  if (spec.side > 1800 && grid > 193) grid = 257;
  m_grid = grid;
  m_cell = (float)spec.side / (float)(grid - 1);
  m_heights = (float*)malloc(sizeof(float) * grid * grid);
  float half = spec.side * 0.5f;

  for (int iz = 0; iz < grid; iz++) {
    for (int ix = 0; ix < grid; ix++) {
      float x = -half + ix * m_cell;
      float z = -half + iz * m_cell;
      float h = spec.baseHeight;
      // mountains: smooth radial peaks with ridged noise
      float mtn = 0;
      for (int i = 0; i < peaks.count; i++) {
        const Peak& p = peaks[i];
        float dx = x - p.x, dz = z - p.z;
        float d = sqrtf(dx * dx + dz * dz);
        if (d > p.radius * 1.6f) continue;
        float t = clampf(1.0f - d / (p.radius * 1.6f), 0, 1);
        float prof = powf(t, p.kind == 0 ? 1.6f : 2.1f);
        if (p.kind == 1) prof *= 1.25f;  // volcano cone
        float ridge = fbm(x * 0.012f + i * 3.1f, z * 0.012f - i * 1.7f, spec.seed + i, 3);
        mtn += p.height * prof * (0.72f + ridge * 0.5f);
      }
      h += mtn;
      // basins (lakes carve the terrain below the water line)
      for (int i = 0; i < basins.count; i++) {
        const Basin& b = basins[i];
        float dx = x - b.x, dz = z - b.z;
        float d = sqrtf(dx * dx + dz * dz);
        if (b.kind == 2) {  // ocean ring
          if (d > b.radius) {
            float t = clampf((d - b.radius) / (half * 0.14f), 0, 1);
            h = lerpf(h, -b.depth, smoothstep(t));
          }
          continue;
        }
        if (d > b.radius) continue;
        float t = clampf(1.0f - d / b.radius, 0, 1);
        h -= b.depth * powf(t, 1.3f);
      }
      // rivers carve a channel along their polyline
      for (int r = 0; r < rivers.count; r++) {
        const River& river = rivers[r];
        for (int s = 0; s + 1 < river.count; s++) {
          Vec2 a = river.pts[s], b = river.pts[s + 1];
          Vec2 ab = b - a;
          float len2 = ab.x * ab.x + ab.y * ab.y;
          float t = len2 > 0 ? clampf(((x - a.x) * ab.x + (z - a.y) * ab.y) / len2, 0, 1) : 0;
          Vec2 closest(a.x + ab.x * t, a.y + ab.y * t);
          float d = sqrtf((x - closest.x) * (x - closest.x) + (z - closest.y) * (z - closest.y));
          float w = river.width;
          if (d < w) {
            float k = 1.0f - d / w;
            h -= river.depth * k * k;
          }
        }
      }
      // fine detail
      float rough = spec.terrainRoughness;
      h += (fbm(x * 0.008f, z * 0.008f, spec.seed, spec.terrainOctaves) - 0.5f) * 9.0f * rough;
      h += (fbm(x * 0.05f, z * 0.05f, spec.seed + 91, 3) - 0.5f) * 1.6f * rough;
      // island falloff: land sinks into the sea towards the border
      h *= falloff(x, z, half, 0.12f);
      m_heights[iz * grid + ix] = h;
    }
  }
  // waterline default: ocean maps flood above 0 so beaches read correctly
  if (spec.waterEnabled && spec.fillOcean) {
    // nothing extra: ocean basin already carved; the water plane sits at spec.waterLevel
  }
  buildMesh(spec.detail);
  return true;
}

float Terrain::rawHeight(int ix, int iz) const {
  if (ix < 0) ix = 0;
  if (iz < 0) iz = 0;
  if (ix >= m_grid) ix = m_grid - 1;
  if (iz >= m_grid) iz = m_grid - 1;
  return m_heights[iz * m_grid + ix];
}

float Terrain::heightAt(float x, float z) const {
  if (!m_heights) return 0;
  float half = (m_grid - 1) * m_cell * 0.5f;
  float fx = (x + half) / m_cell;
  float fz = (z + half) / m_cell;
  int ix = (int)floorf(fx), iz = (int)floorf(fz);
  float tx = fx - ix, tz = fz - iz;
  float h00 = rawHeight(ix, iz), h10 = rawHeight(ix + 1, iz);
  float h01 = rawHeight(ix, iz + 1), h11 = rawHeight(ix + 1, iz + 1);
  return lerpf(lerpf(h00, h10, tx), lerpf(h01, h11, tx), tz);
}

Vec3 Terrain::normalAt(float x, float z) const {
  float d = m_cell * 0.6f;
  float hl = heightAt(x - d, z), hr = heightAt(x + d, z);
  float hd = heightAt(x, z - d), hu = heightAt(x, z + d);
  Vec3 n(hl - hr, 2.0f * d, hd - hu);
  return n.normalized();
}

Biome Terrain::biomeAt(float x, float z, float h) const {
  if (spec.waterEnabled && h < spec.waterLevel + 0.2f) return Biome::Ocean;
  if (h < spec.waterLevel + 1.6f && spec.waterEnabled) return Biome::Beach;
  // latitude-ish temperature + altitude drive the palette
  bool snow = false;
  bool sand = false;
  bool volcanic = false;
  for (int i = 0; i < basins.count; i++) {
    const Basin& b = basins[i];
    float d = sqrtf((x - b.x) * (x - b.x) + (z - b.z) * (z - b.z));
    if (b.kind == 3 && d < b.radius * 1.5f) sand = true;
  }
  for (int i = 0; i < peaks.count; i++) {
    const Peak& p = peaks[i];
    float d = sqrtf((x - p.x) * (x - p.x) + (z - p.z) * (z - p.z));
    if (d > p.radius * 1.4f) continue;
    if (p.kind == 1) volcanic = true;
    if (p.kind == 2 && h > p.height * 0.42f) snow = true;
  }
  if (volcanic) return Biome::Volcanic;
  if (snow || h > 95.0f) return Biome::Snow;
  if (h > 70.0f) return Biome::Rock;
  if (sand) return Biome::Sand;
  float forestNoise = fbm(x * 0.0035f, z * 0.0035f, spec.seed + 7, 3);
  if (forestNoise > 0.56f) return Biome::Forest;
  return Biome::Grass;
}

Color Terrain::colorAt(float x, float z, float h) const {
  Biome b = biomeAt(x, z, h);
  float n = fbm(x * 0.05f, z * 0.05f, spec.seed + 33, 3);
  float shade = 0.82f + n * 0.36f;
  Color c;
  switch (b) {
    case Biome::Ocean: c = Color::hex(0x1B4A5A); break;
    case Biome::Beach: c = Color::hex(0xD9C89A); break;
    case Biome::Sand: c = Color::hex(0xC9B07A); break;
    case Biome::Grass: c = Color::hex(0x4E7A3A); break;
    case Biome::Forest: c = Color::hex(0x3A6230); break;
    case Biome::Jungle: c = Color::hex(0x2F5C2A); break;
    case Biome::Rock: c = Color::hex(0x6E6E76); break;
    case Biome::Snow: c = Color::hex(0xE8EEF6); break;
    case Biome::Volcanic: c = Color::hex(0x3A2B2B); break;
    case Biome::Urban: c = Color::hex(0x5A5E66); break;
    case Biome::Neon: c = Color::hex(0x2A2340); break;
    default: c = Color::hex(0x4E7A3A);
  }
  return Color(c.r * shade, c.g * shade, c.b * shade, 1.0f);
}

bool Terrain::isWater(float x, float z) const {
  if (!spec.waterEnabled) return false;
  return heightAt(x, z) < spec.waterLevel;
}

void Terrain::buildMesh(int detail) {
  (void)detail;
  if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
  if (m_ibo) { glDeleteBuffers(1, &m_ibo); m_ibo = 0; }
  // chunked mesh with per-vertex colour (biome + height blend) and normals
  int grid = m_grid;
  int verts = grid * grid;
  VtxShape* v = (VtxShape*)malloc(sizeof(VtxShape) * verts);
  float half = (grid - 1) * m_cell * 0.5f;
  for (int iz = 0; iz < grid; iz++) {
    for (int ix = 0; ix < grid; ix++) {
      float x = -half + ix * m_cell;
      float z = -half + iz * m_cell;
      float h = rawHeight(ix, iz);
      Vec3 n = normalAt(x, z);
      Color c = colorAt(x, z, h);
      // slope-based rock blending
      float slope = 1.0f - n.y;
      if (slope > 0.35f && h > spec.waterLevel + 1.0f) {
        Color rock = Color::hex(0x6A6A72);
        c = Color::lerp(c, rock, clampf((slope - 0.35f) * 2.2f, 0, 0.85f));
      }
      v[iz * grid + ix] = {x, h, z, n.x, n.y, n.z, c.r, c.g, c.b, 1.0f};
    }
  }
  int quads = (grid - 1) * (grid - 1);
  uint32_t* idx = (uint32_t*)malloc(sizeof(uint32_t) * quads * 6);
  int k = 0;
  for (int iz = 0; iz < grid - 1; iz++) {
    for (int ix = 0; ix < grid - 1; ix++) {
      uint32_t a = iz * grid + ix;
      uint32_t b2 = a + 1;
      uint32_t c = a + grid;
      uint32_t d = c + 1;
      idx[k++] = a; idx[k++] = c; idx[k++] = b2;
      idx[k++] = b2; idx[k++] = c; idx[k++] = d;
    }
  }
  glGenBuffers(1, &m_vbo);
  glGenBuffers(1, &m_ibo);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(VtxShape) * verts), v, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(sizeof(uint32_t) * k), idx, GL_STATIC_DRAW);
  m_indexCount = k;
  free(v);
  free(idx);
  // heightfield kept for physics
  NX_LOGI("terrain: %dx%d, %d tris, side=%.0f", grid, grid, k / 3, (float)spec.side);
}

void Terrain::draw(Program& prog, const Mat4& viewProj) {
  if (!m_vbo) return;
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
  int stride = sizeof(VtxShape);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)12);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)24);
  glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  (void)viewProj;
}

void Terrain::destroy() {
  if (m_vbo) glDeleteBuffers(1, &m_vbo);
  if (m_ibo) glDeleteBuffers(1, &m_ibo);
  if (m_heights) free(m_heights);
  m_vbo = m_ibo = 0;
  m_heights = nullptr;
  peaks.clear();
  basins.clear();
  rivers.clear();
  roads.clear();
}

}  // namespace nx
