// NEXORA engine — GPU resources: shaders, meshes, textures, batchers, framebuffers.
#pragma once
#include "gl.h"
#include "math.h"
#include "sys.h"

namespace nx {

// ---------------------------------------------------------------------------
// Shader program
// ---------------------------------------------------------------------------
struct Program {
  GLuint id = 0;
  void* uniformCache = nullptr;
  bool build(const char* vs, const char* fs, const char* debugName);
  void use() const { glUseProgram(id); }
  GLint loc(const char* name);
  void set(const char* n, float v) { GLint l = loc(n); if (l >= 0) glUniform1f(l, v); }
  void set(const char* n, int v) { GLint l = loc(n); if (l >= 0) glUniform1i(l, v); }
  void set(const char* n, float a, float b) { GLint l = loc(n); if (l >= 0) glUniform2f(l, a, b); }
  void set(const char* n, float a, float b, float c) { GLint l = loc(n); if (l >= 0) glUniform3f(l, a, b, c); }
  void set(const char* n, float a, float b, float c, float d) { GLint l = loc(n); if (l >= 0) glUniform4f(l, a, b, c, d); }
  void set(const char* n, const Vec3& v) { GLint l = loc(n); if (l >= 0) glUniform3f(l, v.x, v.y, v.z); }
  void set(const char* n, const Color& c) { GLint l = loc(n); if (l >= 0) glUniform4f(l, c.r, c.g, c.b, c.a); }
  void setMat(const char* n, const Mat4& m) { GLint l = loc(n); if (l >= 0) glUniformMatrix4fv(l, 1, GL_FALSE, m.m); }
  void destroy();
};

// ---------------------------------------------------------------------------
// Shared vertex layouts
// ---------------------------------------------------------------------------
struct VtxShape {   // 3D unlit/vertex-lit world geometry
  float x, y, z;
  float nx, ny, nz;
  float r, g, b, a;
};
struct VtxUI {      // 2D screen-space
  float x, y;
  float u, v;
  float r, g, b, a;
};
struct VtxLine {
  float x, y, z;
  float r, g, b, a;
};
struct VtxParticle {
  float x, y, z;
  float size;
  float r, g, b, a;
};
struct VtxSky {
  float x, y, z;
};

// ---------------------------------------------------------------------------
// Batcher: accumulates vertices, uploads once per flush (orphan-buffer streaming).
// ---------------------------------------------------------------------------
struct MeshBatcher {
  GLuint vbo = 0, ibo = 0;
  void* verts = nullptr;
  uint32_t* indices = nullptr;
  int vertCount = 0, vertCap = 0;
  int indexCount = 0, indexCap = 0;
  int vertexStride = 0;
  int attribCount = 0;
  int attribSizes[4] = {0, 0, 0, 0};
  int attribOffsets[4] = {4, 4, 4, 4};
  bool useIndices = true;
  GLenum usage = GL_STREAM_DRAW;

  void init(int stride, int a0, int a1 = 0, int a2 = 0, int a3 = 0, GLenum usageMode = GL_STREAM_DRAW);
  void* push(int n);          // returns pointer to n vertices of `vertexStride` bytes
  uint32_t pushIndexVerts(int n);  // reserves n vertices, returns base index for indexed quads
  void index(uint32_t i);     // push an index
  void indexQuad(uint32_t base);
  void flush(GLenum mode);
  void destroy();
};

// ---------------------------------------------------------------------------
// Built-in primitive builders (all append into a MeshBatcher)
// ---------------------------------------------------------------------------
void pushBox(MeshBatcher& b, const Vec3& center, const Vec3& half, const Color& top, const Color& side, float rotY = 0,
             float shadeDown = 0.62f);
void pushBoxTinted(MeshBatcher& b, const Vec3& min, const Vec3& max, const Color& top, const Color& side, const Color& bottom);
void pushQuad(MeshBatcher& b, const Vec3& a, const Vec3& bb, const Vec3& c, const Vec3& d, const Vec3& n, const Color& col);
void pushCylinder(MeshBatcher& b, const Vec3& base, float radius, float height, int segs, const Color& side, const Color& top,
                  bool capTop = true, float radiusTop = -1);
void pushCone(MeshBatcher& b, const Vec3& base, float radius, float height, int segs, const Color& side);
void pushSphere(MeshBatcher& b, const Vec3& center, float radius, int rings, int segs, const Color& top, const Color& bottom);
void pushPlaneXZ(MeshBatcher& b, float x0, float z0, float x1, float z1, float y, const Color& col);
void pushRing(MeshBatcher& b, const Vec3& center, float rInner, float rOuter, int segs, const Color& col, float y = 0);
void pushDisc(MeshBatcher& b, const Vec3& center, float radius, int segs, const Color& col, float y = 0);
// character capsule parts used by remote players + local avatar
void pushCapsule(MeshBatcher& b, const Vec3& a, const Vec3& bb, float radius, int segs, const Color& col);

// ---------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------
struct Texture {
  GLuint id = 0;
  int width = 0, height = 0;
  bool loadFromFile(const char* assetPath, bool mipmaps = true, bool repeat = false);
  bool loadFromMemory(const unsigned char* data, size_t size, bool mipmaps = true, bool linear = true);
  bool createEmpty(int w, int h, GLenum format = GL_RGBA8, const void* data = nullptr, bool linear = true);
  void bind(int unit = 0) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id);
  }
  void destroy();
};

// Procedural texture generators (no external files needed)
struct TextureBank {
  GLuint white = 0, glow = 0, particles = 0, noise = 0, cloud = 0, foliage = 0, ground = 0, logo = 0;
  GLuint shopAtlas = 0;
  void build();
  void destroy();
};
extern TextureBank textures;

// ---------------------------------------------------------------------------
// Sprites: screen-space tinted quads with atlas UVs (UI icons, particles)
// ---------------------------------------------------------------------------
struct SpriteBatch {
  MeshBatcher mesh;
  GLuint program = 0;
  GLuint tex = 0;
  void init(GLuint texId);
  void begin();
  void quad(const Rect& r, const Rect& uv, const Color& c, float rot = 0);
  void quadTinted(const Rect& r, const Color& c);  // solid white texture
  void flush();
  void destroy();
};

// upload helpers shared with UI
void uploadPixelTexture(GLuint& id, int w, int h, const unsigned char* rgba, bool linear = true, bool repeat = false);

}  // namespace nx
