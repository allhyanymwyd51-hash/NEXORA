#include "gfx.h"
#include "png.h"
#include "engine.h"

namespace nx {

TextureBank textures;

// ---------------------------------------------------------------------------
// Program
// ---------------------------------------------------------------------------
static GLuint compile(GLenum type, const char* src, const char* name) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[2048];
    GLsizei n = 0;
    glGetShaderInfoLog(s, sizeof log, &n, log);
    NX_LOGE("shader compile failed (%s): %s", name, log);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

bool Program::build(const char* vs, const char* fs, const char* debugName) {
  GLuint v = compile(GL_VERTEX_SHADER, vs, debugName);
  GLuint f = compile(GL_FRAGMENT_SHADER, fs, debugName);
  if (!v || !f) return false;
  id = glCreateProgram();
  glAttachShader(id, v);
  glAttachShader(id, f);
  glLinkProgram(id);
  glDeleteShader(v);
  glDeleteShader(f);
  GLint ok = 0;
  glGetProgramiv(id, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[2048];
    GLsizei n = 0;
    glGetProgramInfoLog(id, sizeof log, &n, log);
    NX_LOGE("program link failed (%s): %s", debugName, log);
    glDeleteProgram(id);
    id = 0;
    return false;
  }
  return true;
}

GLint Program::loc(const char* name) { return glGetUniformLocation(id, name); }
void Program::destroy() { if (id) { glDeleteProgram(id); id = 0; } }

// ---------------------------------------------------------------------------
// MeshBatcher
// ---------------------------------------------------------------------------
void MeshBatcher::init(int stride, int a0, int a1, int a2, int a3, GLenum usageMode) {
  vertexStride = stride;
  attribSizes[0] = a0; attribSizes[1] = a1; attribSizes[2] = a2; attribSizes[3] = a3;
  int off = 0;
  attribCount = 0;
  for (int i = 0; i < 4; i++) {
    attribOffsets[i] = off;
    if (attribSizes[i] > 0) { off += attribSizes[i] * 4; attribCount = i + 1; }
  }
  usage = usageMode;
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ibo);
  vertCap = 4096;
  verts = malloc((size_t)vertCap * vertexStride);
  indexCap = 8192;
  indices = (uint32_t*)malloc((size_t)indexCap * sizeof(uint32_t));
}

void* MeshBatcher::push(int n) {
  if (vertCount + n > vertCap) {
    while (vertCap < vertCount + n) vertCap *= 2;
    verts = realloc(verts, (size_t)vertCap * vertexStride);
  }
  void* p = (char*)verts + (size_t)vertCount * vertexStride;
  vertCount += n;
  return p;
}

uint32_t MeshBatcher::pushIndexVerts(int n) {
  uint32_t base = (uint32_t)vertCount;
  push(n);
  return base;
}

void MeshBatcher::index(uint32_t i) {
  if (indexCount + 1 > indexCap) {
    indexCap *= 2;
    indices = (uint32_t*)realloc(indices, (size_t)indexCap * sizeof(uint32_t));
  }
  indices[indexCount++] = i;
}

void MeshBatcher::indexQuad(uint32_t base) {
  index(base + 0); index(base + 1); index(base + 2);
  index(base + 0); index(base + 2); index(base + 3);
}

void MeshBatcher::flush(GLenum mode) {
  if (useIndices) {
    if (indexCount == 0) return;
  } else if (vertCount == 0) {
    return;
  }
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  if (vertCount > 0) {
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)vertCount * vertexStride), nullptr, usage);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)vertCount * vertexStride), verts, usage);
  }
  int strideBytes = vertexStride;
  for (int i = 0; i < attribCount; i++) {
    glEnableVertexAttribArray((GLuint)i);
    glVertexAttribPointer((GLuint)i, attribSizes[i], GL_FLOAT, GL_FALSE, strideBytes,
                          (const void*)(intptr_t)attribOffsets[i]);
  }
  if (useIndices) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)((size_t)indexCount * sizeof(uint32_t)), nullptr, usage);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)((size_t)indexCount * sizeof(uint32_t)), indices, usage);
    glDrawElements(mode, indexCount, GL_UNSIGNED_INT, nullptr);
  } else {
    glDrawArrays(mode, 0, vertCount);
  }
  for (int i = 0; i < attribCount; i++) glDisableVertexAttribArray((GLuint)i);
  vertCount = 0;
  indexCount = 0;
}

void MeshBatcher::destroy() {
  if (vbo) glDeleteBuffers(1, &vbo);
  if (ibo) glDeleteBuffers(1, &ibo);
  if (verts) free(verts);
  if (indices) free(indices);
  vbo = ibo = 0;
  verts = nullptr;
  indices = nullptr;
}

// ---------------------------------------------------------------------------
// Primitive builders
// ---------------------------------------------------------------------------
static VtxShape makeVtx(const Vec3& p, const Vec3& n, const Color& c) {
  // Untextured by default (texStrength 0) so existing geometry keeps its vertex colours;
  // pushBoxTextured and the terrain override the tile fields.
  return {p.x, p.y, p.z, n.x, n.y, n.z, c.r, c.g, c.b, c.a, p.x, p.z, 0.0f, 0.0f, 0.0f, 0.0f};
}

// Quad tagged with a prop-atlas cell. UVs are world metres so a wall keeps a constant
// texel density regardless of its size.
void pushBoxTextured(MeshBatcher& b, const Vec3& center, const Vec3& half, const Color& top, const Color& side,
                     float rotY, float shadeDown, float tileTop, float tileSide) {
  float cs = cosf(rotY), sn = sinf(rotY);
  auto rot = [&](float x, float z) { return Vec3{x * cs - z * sn, 0, x * sn + z * cs}; };
  Vec3 c[8];
  for (int i = 0; i < 8; i++) {
    float sx = (i & 1) ? half.x : -half.x;
    float sy = (i & 2) ? half.y : -half.y;
    float sz = (i & 4) ? half.z : -half.z;
    Vec3 r = rot(sx, sz);
    c[i] = Vec3{center.x + r.x, center.y + sy, center.z + r.z};
  }
  struct Face { Vec3 a, bb, cc, d, n; Color col; float tile; };
  Face faces[6] = {
      {c[2], c[3], c[7], c[6], Vec3{0, 1, 0}, top, tileTop},
      {c[0], c[4], c[5], c[1], Vec3{0, -1, 0}, side * shadeDown, tileTop},
      {c[1], c[5], c[7], c[3], rot(1, 0), side * 0.92f, tileSide},
      {c[0], c[2], c[6], c[4], rot(-1, 0), side * 0.82f, tileSide},
      {c[2], c[0], c[1], c[3], rot(0, 1), side, tileSide},
      {c[6], c[7], c[5], c[4], rot(0, -1), side * 0.78f, tileSide},
  };
  for (const Face& f : faces) {
    pushQuad(b, f.a, f.bb, f.cc, f.d, f.n, f.col);
    uint32_t base = (uint32_t)(b.vertCount - 4);
    VtxShape* v = (VtxShape*)((char*)b.verts + (size_t)base * b.vertexStride);
    for (int i = 0; i < 4; i++) {
      v[i].u = v[i].x;
      v[i].v = v[i].y + v[i].z;   // unwrap around the box without per-face bookkeeping
      v[i].tileU = f.tile;
      v[i].tileV = f.tile;
      v[i].propFlag = 1.0f;
      v[i].texStrength = 1.0f;
    }
  }
}

void pushQuad(MeshBatcher& b, const Vec3& a, const Vec3& bb, const Vec3& c, const Vec3& d, const Vec3& n, const Color& col) {
  uint32_t base = b.pushIndexVerts(4);
  VtxShape* v = (VtxShape*)((char*)b.verts + (size_t)base * b.vertexStride);
  v[0] = makeVtx(a, n, col);
  v[1] = makeVtx(bb, n, col);
  v[2] = makeVtx(c, n, col);
  v[3] = makeVtx(d, n, col);
  b.indexQuad(base);
}

void pushBox(MeshBatcher& b, const Vec3& center, const Vec3& half, const Color& top, const Color& side, float rotY,
             float shadeDown) {
  float cs = cosf(rotY), sn = sinf(rotY);
  auto rot = [&](float x, float z) {
    return Vec3{x * cs - z * sn, 0, x * sn + z * cs};
  };
  Vec3 c[8];
  for (int i = 0; i < 8; i++) {
    float sx = (i & 1) ? half.x : -half.x;
    float sy = (i & 2) ? half.y : -half.y;
    float sz = (i & 4) ? half.z : -half.z;
    Vec3 r = rot(sx, sz);
    c[i] = Vec3{center.x + r.x, center.y + sy, center.z + r.z};
  }
  Color bottom = side * shadeDown;
  // +Y
  pushQuad(b, c[2], c[3], c[7], c[6], Vec3{0, 1, 0}, top);
  // -Y
  pushQuad(b, c[0], c[4], c[5], c[1], Vec3{0, -1, 0}, bottom);
  // +X
  Vec3 nx = rot(1, 0);
  pushQuad(b, c[1], c[5], c[7], c[3], nx, side * 0.92f);
  // -X
  Vec3 nnx = rot(-1, 0);
  pushQuad(b, c[0], c[2], c[6], c[4], nnx, side * 0.82f);
  // +Z
  Vec3 nz = rot(0, 1);
  pushQuad(b, c[4], c[6], c[7], c[5], nz, side * 0.88f);
  // -Z
  Vec3 nnz = rot(0, -1);
  pushQuad(b, c[0], c[1], c[3], c[2], nnz, side * 0.78f);
}

void pushBoxTinted(MeshBatcher& b, const Vec3& mn, const Vec3& mx, const Color& top, const Color& side, const Color& bottom) {
  Vec3 c000{mn.x, mn.y, mn.z}, c100{mx.x, mn.y, mn.z}, c110{mx.x, mx.y, mn.z}, c010{mn.x, mx.y, mn.z};
  Vec3 c001{mn.x, mn.y, mx.z}, c101{mx.x, mn.y, mx.z}, c111{mx.x, mx.y, mx.z}, c011{mn.x, mx.y, mx.z};
  pushQuad(b, c010, c011, c111, c110, Vec3{0, 1, 0}, top);
  pushQuad(b, c000, c100, c101, c001, Vec3{0, -1, 0}, bottom);
  pushQuad(b, c100, c110, c111, c101, Vec3{1, 0, 0}, side);
  pushQuad(b, c000, c001, c011, c010, Vec3{-1, 0, 0}, side);
  pushQuad(b, c001, c101, c111, c011, Vec3{0, 0, 1}, side);
  pushQuad(b, c000, c010, c110, c100, Vec3{0, 0, -1}, side);
}

void pushCylinder(MeshBatcher& b, const Vec3& base, float radius, float height, int segs, const Color& side,
                  const Color& top, bool capTop, float radiusTop) {
  if (radiusTop < 0) radiusTop = radius;
  uint32_t start = b.pushIndexVerts(segs * 2);
  VtxShape* v = (VtxShape*)((char*)b.verts + (size_t)start * b.vertexStride);
  for (int i = 0; i < segs; i++) {
    float a = (float)i / segs * TAU;
    float ca = cosf(a), sa = sinf(a);
    Vec3 n{ca, 0, sa};
    v[i * 2 + 0] = makeVtx({base.x + ca * radius, base.y, base.z + sa * radius}, n, side);
    v[i * 2 + 1] = makeVtx({base.x + ca * radiusTop, base.y + height, base.z + sa * radiusTop}, n, side * 1.06f);
  }
  for (int i = 0; i < segs; i++) {
    int j = (i + 1) % segs;
    b.index(start + i * 2);
    b.index(start + j * 2);
    b.index(start + j * 2 + 1);
    b.index(start + i * 2);
    b.index(start + j * 2 + 1);
    b.index(start + i * 2 + 1);
  }
  if (capTop) {
    if (radiusTop < 0.001f) {
      uint32_t tip = b.pushIndexVerts(1);
      ((VtxShape*)((char*)b.verts + (size_t)tip * b.vertexStride))[0] =
          makeVtx({base.x, base.y + height, base.z}, Vec3{0, 1, 0}, top);
      for (int i = 0; i < segs; i++) {
        float a0 = (float)i / segs * TAU, a1 = (float)(i + 1) / segs * TAU;
        uint32_t ring = b.pushIndexVerts(2);
        VtxShape* rv = (VtxShape*)((char*)b.verts + (size_t)ring * b.vertexStride);
        rv[0] = makeVtx({base.x + cosf(a0) * radius, base.y + height, base.z + sinf(a0) * radius}, Vec3{0, 1, 0}, top);
        rv[1] = makeVtx({base.x + cosf(a1) * radius, base.y + height, base.z + sinf(a1) * radius}, Vec3{0, 1, 0}, top);
        b.index(tip);
        b.index(ring);
        b.index(ring + 1);
      }
    } else {
      uint32_t center = b.pushIndexVerts(1);
      ((VtxShape*)((char*)b.verts + (size_t)center * b.vertexStride))[0] =
          makeVtx({base.x, base.y + height, base.z}, Vec3{0, 1, 0}, top);
      for (int i = 0; i < segs; i++) {
        float a0 = (float)i / segs * TAU, a1 = (float)(i + 1) / segs * TAU;
        uint32_t ring = b.pushIndexVerts(2);
        VtxShape* rv = (VtxShape*)((char*)b.verts + (size_t)ring * b.vertexStride);
        rv[0] = makeVtx({base.x + cosf(a0) * radiusTop, base.y + height, base.z + sinf(a0) * radiusTop}, Vec3{0, 1, 0}, top);
        rv[1] = makeVtx({base.x + cosf(a1) * radiusTop, base.y + height, base.z + sinf(a1) * radiusTop}, Vec3{0, 1, 0}, top);
        b.index(center);
        b.index(ring);
        b.index(ring + 1);
      }
    }
  }
}

void pushCone(MeshBatcher& b, const Vec3& base, float radius, float height, int segs, const Color& side) {
  uint32_t apex = b.pushIndexVerts(1);
  ((VtxShape*)((char*)b.verts + (size_t)apex * b.vertexStride))[0] =
      makeVtx({base.x, base.y + height, base.z}, Vec3{0, 1, 0}, side * 1.1f);
  for (int i = 0; i < segs; i++) {
    float a0 = (float)i / segs * TAU, a1 = (float)(i + 1) / segs * TAU;
    float am = (a0 + a1) * 0.5f;
    Vec3 n{cosf(am), 0.6f, sinf(am)};
    uint32_t ring = b.pushIndexVerts(2);
    VtxShape* rv = (VtxShape*)((char*)b.verts + (size_t)ring * b.vertexStride);
    rv[0] = makeVtx({base.x + cosf(a0) * radius, base.y, base.z + sinf(a0) * radius}, n, side);
    rv[1] = makeVtx({base.x + cosf(a1) * radius, base.y, base.z + sinf(a1) * radius}, n, side * 0.88f);
    b.index(apex);
    b.index(ring);
    b.index(ring + 1);
  }
}

void pushSphere(MeshBatcher& b, const Vec3& center, float radius, int rings, int segs, const Color& top, const Color& bottom) {
  for (int r = 0; r < rings; r++) {
    float v0 = (float)r / rings, v1 = (float)(r + 1) / rings;
    float phi0 = v0 * PI, phi1 = v1 * PI;
    Color c0 = Color::lerp(top, bottom, v0), c1 = Color::lerp(top, bottom, v1);
    for (int s = 0; s < segs; s++) {
      float t0 = (float)s / segs * TAU, t1 = (float)(s + 1) / segs * TAU;
      Vec3 p00{cosf(t0) * sinf(phi0), cosf(phi0), sinf(t0) * sinf(phi0)};
      Vec3 p10{cosf(t1) * sinf(phi0), cosf(phi0), sinf(t1) * sinf(phi0)};
      Vec3 p01{cosf(t0) * sinf(phi1), cosf(phi1), sinf(t0) * sinf(phi1)};
      Vec3 p11{cosf(t1) * sinf(phi1), cosf(phi1), sinf(t1) * sinf(phi1)};
      uint32_t base = b.pushIndexVerts(4);
      VtxShape* v = (VtxShape*)((char*)b.verts + (size_t)base * b.vertexStride);
      v[0] = makeVtx(center + p00 * radius, p00, c0);
      v[1] = makeVtx(center + p10 * radius, p10, c0);
      v[2] = makeVtx(center + p11 * radius, p11, c1);
      v[3] = makeVtx(center + p01 * radius, p01, c1);
      b.indexQuad(base);
    }
  }
}

void pushPlaneXZ(MeshBatcher& b, float x0, float z0, float x1, float z1, float y, const Color& col) {
  uint32_t base = b.pushIndexVerts(4);
  VtxShape* v = (VtxShape*)((char*)b.verts + (size_t)base * b.vertexStride);
  v[0] = makeVtx({x0, y, z0}, Vec3{0, 1, 0}, col);
  v[1] = makeVtx({x1, y, z0}, Vec3{0, 1, 0}, col);
  v[2] = makeVtx({x1, y, z1}, Vec3{0, 1, 0}, col);
  v[3] = makeVtx({x0, y, z1}, Vec3{0, 1, 0}, col);
  b.indexQuad(base);
}

void pushDisc(MeshBatcher& b, const Vec3& center, float radius, int segs, const Color& col, float y) {
  (void)y;
  Vec3 n{0, 1, 0};
  uint32_t c0 = b.pushIndexVerts(1);
  ((VtxShape*)((char*)b.verts + (size_t)c0 * b.vertexStride))[0] = makeVtx(center, n, col);
  for (int i = 0; i < segs; i++) {
    float a0 = (float)i / segs * TAU, a1 = (float)(i + 1) / segs * TAU;
    uint32_t ring = b.pushIndexVerts(2);
    VtxShape* rv = (VtxShape*)((char*)b.verts + (size_t)ring * b.vertexStride);
    rv[0] = makeVtx({center.x + cosf(a0) * radius, center.y, center.z + sinf(a0) * radius}, n, col);
    rv[1] = makeVtx({center.x + cosf(a1) * radius, center.y, center.z + sinf(a1) * radius}, n, col);
    b.index(c0);
    b.index(ring);
    b.index(ring + 1);
  }
}

void pushRing(MeshBatcher& b, const Vec3& center, float rInner, float rOuter, int segs, const Color& col, float y) {
  (void)y;
  Vec3 n{0, 1, 0};
  uint32_t base = b.pushIndexVerts(segs * 2);
  VtxShape* v = (VtxShape*)((char*)b.verts + (size_t)base * b.vertexStride);
  for (int i = 0; i < segs; i++) {
    float a = (float)i / segs * TAU;
    float ca = cosf(a), sa = sinf(a);
    v[i * 2 + 0] = makeVtx({center.x + ca * rInner, center.y, center.z + sa * rInner}, n, col);
    v[i * 2 + 1] = makeVtx({center.x + ca * rOuter, center.y, center.z + sa * rOuter}, n, col * 1.04f);
  }
  for (int i = 0; i < segs; i++) {
    int j = (i + 1) % segs;
    b.index(base + i * 2); b.index(base + i * 2 + 1); b.index(base + j * 2 + 1);
    b.index(base + i * 2); b.index(base + j * 2 + 1); b.index(base + j * 2);
  }
}

void pushCapsule(MeshBatcher& b, const Vec3& a, const Vec3& bb, float radius, int segs, const Color& col) {
  Vec3 axis = bb - a;
  float len = axis.length();
  if (len < 1e-4f) return;
  Vec3 dir = axis * (1.0f / len);
  Vec3 up{0, 1, 0};
  Vec3 side = cross(dir, up);
  if (side.length() < 1e-3f) side = Vec3{1, 0, 0};
  side = side.normalized();
  Vec3 fwd = cross(side, dir).normalized();
  for (int i = 0; i < segs; i++) {
    float t0 = (float)i / segs * TAU, t1 = (float)(i + 1) / segs * TAU;
    Vec3 n0 = side * cosf(t0) + fwd * sinf(t0);
    Vec3 n1 = side * cosf(t1) + fwd * sinf(t1);
    uint32_t base = b.pushIndexVerts(4);
    VtxShape* v = (VtxShape*)((char*)b.verts + (size_t)base * b.vertexStride);
    v[0] = makeVtx(a + n0 * radius, n0, col);
    v[1] = makeVtx(a + n1 * radius, n1, col);
    v[2] = makeVtx(bb + n1 * radius, n1, col);
    v[3] = makeVtx(bb + n0 * radius, n0, col);
    b.indexQuad(base);
  }
  // caps
  pushSphere(b, a, radius, 3, segs, col, col);
  pushSphere(b, bb, radius, 3, segs, col, col);
}

// ---------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------
void uploadPixelTexture(GLuint& id, int w, int h, const unsigned char* rgba, bool linear, bool repeat) {
  if (!id) glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
}

// Textures are generated procedurally at runtime (see TextureBank::build) so the APK carries no
// bitmap dependencies and art stays consistent across every device.
static GLuint makeRGBA(int w, int h, const unsigned char* px, bool linear, bool repeat, bool mips);

void Texture::destroy() {
  if (id) { glDeleteTextures(1, &id); id = 0; }
  width = height = 0;
}

bool Texture::loadFromMemory(const unsigned char* data, size_t size, bool mipmaps, bool linear) {
  ImageData img;
  if (!decodePng(data, size, img)) return false;
  destroy();
  id = makeRGBA(img.width, img.height, img.rgba, linear, false, mipmaps);
  width = img.width;
  height = img.height;
  img.release();
  return id != 0;
}

bool Texture::loadFromFile(const char* path, bool mipmaps, bool repeat) {
  void* raw = nullptr;
  size_t size = 0;
  if (!readAsset(path, &raw, &size) || !raw) return false;
  ImageData img;
  bool ok = decodePng((const unsigned char*)raw, size, img);
  free(raw);
  if (!ok) return false;
  destroy();
  id = makeRGBA(img.width, img.height, img.rgba, true, repeat, mipmaps);
  width = img.width;
  height = img.height;
  img.release();
  return id != 0;
}

// ---------------------------------------------------------------------------
// Procedural textures: no external dependencies, tiny APK, consistent art direction.
// ---------------------------------------------------------------------------
static GLuint makeRGBA(int w, int h, const unsigned char* px, bool linear, bool repeat, bool mips) {
  GLuint id = 0;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  if (mips) {
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
  }
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
  return id;
}

void TextureBank::build() {
  // white
  unsigned char wpx[4] = {255, 255, 255, 255};
  white = makeRGBA(1, 1, wpx, true, false, false);

  // radial glow: alpha falloff with slight violet core
  {
    const int S = 128;
    unsigned char* px = (unsigned char*)malloc(S * S * 4);
    for (int y = 0; y < S; y++) {
      for (int x = 0; x < S; x++) {
        float dx = (x - S / 2.0f) / (S / 2.0f), dy = (y - S / 2.0f) / (S / 2.0f);
        float d = sqrtf(dx * dx + dy * dy);
        float a = clampf(1.0f - d, 0, 1);
        a = a * a * a;
        float core = clampf(1.0f - d * 1.6f, 0, 1);
        px[(y * S + x) * 4 + 0] = (unsigned char)(200 + 55 * core);
        px[(y * S + x) * 4 + 1] = (unsigned char)(190 + 50 * core);
        px[(y * S + x) * 4 + 2] = 255;
        px[(y * S + x) * 4 + 3] = (unsigned char)(a * 255);
      }
    }
    glow = makeRGBA(S, S, px, true, false, false);
    free(px);
  }
  // particles atlas 4x4
  {
    const int T = 64, S = T * 4;
    unsigned char* px = (unsigned char*)calloc(S * S * 4, 1);
    for (int i = 0; i < 16; i++) {
      int ox = (i % 4) * T, oy = (i / 4) * T;
      float radius = 0.25f + 0.06f * (i % 4);
      for (int y = 0; y < T; y++) {
        for (int x = 0; x < T; x++) {
          float dx = (x - T / 2.0f) / T, dy = (y - T / 2.0f) / T;
          float d = sqrtf(dx * dx + dy * dy);
          float a = clampf(1.0f - d / radius, 0, 1);
          a = a * a;
          int idx = ((oy + y) * S + ox + x) * 4;
          px[idx + 0] = 230;
          px[idx + 1] = 235;
          px[idx + 2] = 255;
          px[idx + 3] = (unsigned char)(a * 255);
        }
      }
    }
    particles = makeRGBA(S, S, px, true, false, false);
    free(px);
  }
  // tiling noise (grayscale) used for ground detail + water ripple
  {
    const int S = 256;
    unsigned char* px = (unsigned char*)malloc(S * S * 4);
    for (int y = 0; y < S; y++) {
      for (int x = 0; x < S; x++) {
        float n = fbm(x * 0.08f, y * 0.08f, 11, 4) * 0.6f + fbm(x * 0.31f, y * 0.31f, 29, 3) * 0.4f;
        unsigned char v = (unsigned char)(clampf(n, 0, 1) * 255);
        int idx = (y * S + x) * 4;
        px[idx + 0] = v; px[idx + 1] = v; px[idx + 2] = v; px[idx + 3] = 255;
      }
    }
    noise = makeRGBA(S, S, px, true, true, true);
    free(px);
  }
  // soft cloud (for sky dome + fog wisps)
  {
    const int S = 256;
    unsigned char* px = (unsigned char*)malloc(S * S * 4);
    for (int y = 0; y < S; y++) {
      for (int x = 0; x < S; x++) {
        float n = fbm(x * 0.02f, y * 0.02f, 77, 5);
        float a = clampf((n - 0.45f) * 2.4f, 0, 1);
        int idx = (y * S + x) * 4;
        px[idx + 0] = 255; px[idx + 1] = 255; px[idx + 2] = 255;
        px[idx + 3] = (unsigned char)(a * 210);
      }
    }
    cloud = makeRGBA(S, S, px, true, true, true);
    free(px);
  }
  // ground detail: fine grain + darker speckles, tiles at 1m
  {
    const int S = 128;
    unsigned char* px = (unsigned char*)malloc(S * S * 4);
    for (int y = 0; y < S; y++) {
      for (int x = 0; x < S; x++) {
        float n = fbm(x * 0.35f, y * 0.35f, 5, 3);
        float g = 0.82f + n * 0.3f;
        int idx = (y * S + x) * 4;
        px[idx + 0] = (unsigned char)(clampf(g, 0, 1) * 255);
        px[idx + 1] = (unsigned char)(clampf(g, 0, 1) * 255);
        px[idx + 2] = (unsigned char)(clampf(g, 0, 1) * 255);
        px[idx + 3] = 255;
      }
    }
    ground = makeRGBA(S, S, px, true, true, true);
    free(px);
  }
  // foliage card: layered leaf clusters with alpha
  {
    const int S = 128;
    unsigned char* px = (unsigned char*)calloc(S * S * 4, 1);
    for (int y = 0; y < S; y++) {
      for (int x = 0; x < S; x++) {
        float dx = (x - S / 2.0f) / (S / 2.0f), dy = (y - S / 2.0f) / (S / 2.0f);
        float r = sqrtf(dx * dx + dy * dy);
        float n = fbm(x * 0.09f, y * 0.09f, 91, 4);
        float edge = 0.72f + n * 0.22f;
        float a = clampf((edge - r) * 4.0f, 0, 1);
        float shade = 0.55f + n * 0.6f;
        int idx = (y * S + x) * 4;
        px[idx + 0] = (unsigned char)(clampf(0.35f * shade, 0, 1) * 255);
        px[idx + 1] = (unsigned char)(clampf(0.85f * shade, 0, 1) * 255);
        px[idx + 2] = (unsigned char)(clampf(0.42f * shade, 0, 1) * 255);
        px[idx + 3] = (unsigned char)(a * 235);
      }
    }
    foliage = makeRGBA(S, S, px, true, false, true);
    free(px);
  }
  // brand glyph texture for the NEXORA monogram (drawn once, used by UI + HUD watermark)
  {
    const int S = 128;
    unsigned char* px = (unsigned char*)calloc(S * S * 4, 1);
    for (int y = 0; y < S; y++) {
      for (int x = 0; x < S; x++) {
        float u = (x + 0.5f) / S, v = (y + 0.5f) / S;
        float bar = 0.14f;
        float lx = 0.34f, rx = 0.64f;
        float d = 1e9f;
        d = fminf(d, fabsf(u - lx) - bar * 0.5f);
        d = fminf(d, fabsf(u - rx) - bar * 0.5f);
        // diagonal band from (lx,0.16) to (rx,0.86)
        float band = (u - lx) - (v - 0.20f) * (rx - lx) / 0.62f;
        float t = clampf((v - 0.20f) / 0.62f, 0, 1);
        float bx = lx + (rx - lx) * t;
        float dd = fabsf(u - bx) - 0.055f;
        if (v > 0.18f && v < 0.84f) d = fminf(d, dd);
        float a = clampf(-d / 0.012f, 0, 1);
        int idx = (y * S + x) * 4;
        px[idx + 0] = 240; px[idx + 1] = 244; px[idx + 2] = 255;
        px[idx + 3] = (unsigned char)(a * 255);
      }
    }
    logo = makeRGBA(S, S, px, true, false, false);
    free(px);
  }
}

// Decodes one shipped material atlas (PNG) into a GL texture.
static GLuint loadAtlas(const char* path) {
  void* raw = nullptr;
  size_t size = 0;
  if (!readAsset(path, &raw, &size) || !raw) {
    NX_LOGW("atlas missing: %s", path);
    return 0;
  }
  ImageData img;
  if (!decodePng((const unsigned char*)raw, size, img)) {
    NX_LOGW("atlas corrupt: %s", path);
    free(raw);
    return 0;
  }
  GLuint tex = makeRGBA(img.width, img.height, img.rgba, true, true, true);
  NX_LOGI("atlas %s %dx%d", path, img.width, img.height);
  img.release();
  free(raw);
  return tex;
}

void TextureBank::loadWorldMaterials() {
  texTerrainAlbedo = loadAtlas("textures/world/terrain_albedo_atlas.png");
  texTerrainNormal = loadAtlas("textures/world/terrain_normal_atlas.png");
  texPropAlbedo = loadAtlas("textures/world/prop_albedo_atlas.png");
  texPropNormal = loadAtlas("textures/world/prop_normal_atlas.png");
  texFoliage = loadAtlas("textures/world/foliage_albedo.png");
  texBark = loadAtlas("textures/world/bark_albedo.png");
  texTerrainLoaded = (texTerrainAlbedo && texTerrainNormal) ? 1 : 0;
  NX_LOGI("world materials: terrain=%d prop=%d foliage=%u", texTerrainLoaded,
          (texPropAlbedo && texPropNormal) ? 1 : 0, texFoliage);
}

void TextureBank::destroy() {
  GLuint ids[] = {white, glow, particles, noise, cloud, foliage, ground, logo, shopAtlas,
                  texTerrainAlbedo, texTerrainNormal, texPropAlbedo, texPropNormal, texFoliage, texBark};
  for (GLuint id : ids) if (id) glDeleteTextures(1, &id);
}

// ---------------------------------------------------------------------------
// SpriteBatch
// ---------------------------------------------------------------------------
static const char* kSpriteVS = R"(#version 300 es
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
uniform vec2 uScreen;
out vec2 vUV;
out vec4 vColor;
void main() {
  vec2 p = aPos / uScreen * 2.0 - 1.0;
  gl_Position = vec4(p.x, -p.y, 0.0, 1.0);
  vUV = aUV;
  vColor = aColor;
})";
static const char* kSpriteFS = R"(#version 300 es
precision mediump float;
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uTex;
out vec4 fragColor;
void main() {
  fragColor = texture(uTex, vUV) * vColor;
})";

Program gSpriteProgram;

void SpriteBatch::init(GLuint texId) {
  mesh.init(sizeof(VtxUI), 2, 2, 4);
  tex = texId;
  if (!gSpriteProgram.id) gSpriteProgram.build(kSpriteVS, kSpriteFS, "sprite");
}

void SpriteBatch::begin() { mesh.vertCount = 0; mesh.indexCount = 0; }

void SpriteBatch::quad(const Rect& r, const Rect& uv, const Color& c, float rot) {
  uint32_t base = mesh.pushIndexVerts(4);
  VtxUI* v = (VtxUI*)((char*)mesh.verts + (size_t)base * mesh.vertexStride);
  float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
  float cs = cosf(rot), sn = sinf(rot);
  float hw = r.w * 0.5f, hh = r.h * 0.5f;
  const float px[4] = {-hw, hw, hw, -hw};
  const float py[4] = {-hh, -hh, hh, hh};
  for (int i = 0; i < 4; i++) {
    float x = px[i] * cs - py[i] * sn + cx;
    float y = px[i] * sn + py[i] * cs + cy;
    v[i].x = x;
    v[i].y = y;
    v[i].u = i == 0 || i == 3 ? uv.x : uv.x + uv.w;
    v[i].v = i < 2 ? uv.y : uv.y + uv.h;
    v[i].r = c.r; v[i].g = c.g; v[i].b = c.b; v[i].a = c.a;
  }
  mesh.indexQuad(base);
}

void SpriteBatch::quadTinted(const Rect& r, const Color& c) { quad(r, Rect(0, 0, 1, 1), c); }

void SpriteBatch::flush() {
  if (mesh.vertCount == 0) return;
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  gSpriteProgram.use();
  gSpriteProgram.set("uScreen", Engine::get().width(), Engine::get().height());
  gSpriteProgram.set("uTex", 0);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  mesh.flush(GL_TRIANGLES);
}

void SpriteBatch::destroy() { mesh.destroy(); }

}  // namespace nx
