#include "ui.h"
#include "audio.h"

namespace nx {
namespace ui {

Theme theme;

// rounded rectangle geometry: centre quad + 4 corner fans
struct RectBatcher {
  MeshBatcher mesh;
  Program prog;
  bool ready = false;
  SpriteBatch* sprites = nullptr;
};

static SpriteBatch* gSprites = nullptr;
static MeshBatcher gSolid;
static MeshBatcher gGlow;
static Program gSolidProg, gGlowProg;
static bool gInit = false;
Pointer gPointer;
bool gCaptured = false;

// keyboard editing state for the active text field
char gFocusId[48] = {0};
float gCaretBlink = 0;
char gPendingText[512] = {0};
bool gPendingBackspace = false;

float dp(float v) {
  Engine& e = Engine::get();
  bool landscape = e.width() > e.height();
  // keep UI scale sane in both orientations: base on the shorter side
  float base = fminf(e.width(), e.height()) / 400.0f;
  return v * clampf(base, 0.82f, 2.4f) * (landscape ? 1.0f : 0.98f);
}
float dpInv(float px) {
  Engine& e = Engine::get();
  float base = fminf(e.width(), e.height()) / 400.0f;
  return px / clampf(base, 0.82f, 2.4f);
}

// ---------------------------------------------------------------------------
// shaders
// ---------------------------------------------------------------------------
static const char* kSolidVS = R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec4 aColor;
uniform vec2 uScreen;
out vec4 vColor;
void main(){
  vec2 p = aPos.xy / uScreen * 2.0 - 1.0;
  gl_Position = vec4(p.x, -p.y, aPos.z, 1.0);
  vColor = aColor;
})";
static const char* kSolidFS = R"(#version 300 es
precision mediump float;
in vec4 vColor;
out vec4 fragColor;
void main(){ fragColor = vColor; })";
static const char* kGlowVS = R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec4 aColor;
uniform vec2 uScreen;
out vec2 vUV;
out vec4 vColor;
void main(){
  vec2 p = aPos.xy / uScreen * 2.0 - 1.0;
  gl_Position = vec4(p.x, -p.y, 0.0, 1.0);
  vUV = aNrm.xy;
  vColor = aColor;
})";
static const char* kGlowFS = R"(#version 300 es
precision mediump float;
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uTex;
out vec4 fragColor;
void main(){
  vec4 t = texture(uTex, vUV);
  fragColor = vec4(vColor.rgb, vColor.a * t.a);
})";

static const char* kImageVS = R"(#version 300 es
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
uniform vec2 uScreen;
out vec2 vUV;
out vec4 vColor;
void main(){
  vec2 p = aPos / uScreen * 2.0 - 1.0;
  gl_Position = vec4(p.x, -p.y, 0.0, 1.0);
  vUV = aUV;
  vColor = aColor;
})";
static const char* kImageFS = R"(#version 300 es
precision mediump float;
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uTex;
out vec4 fragColor;
void main(){
  vec4 t = texture(uTex, vUV);
  fragColor = t * vColor;
})";

void ensureInit() {
  if (gInit) return;
  gSolidProg.build(kSolidVS, kSolidFS, "ui_solid");
  gGlowProg.build(kGlowVS, kGlowFS, "ui_glow");
  gSolid.init(sizeof(VtxShape), 3, 3, 4);
  gGlow.init(sizeof(VtxShape), 3, 3, 4);
  gInit = true;
}

// ---------------------------------------------------------------------------
// primitives
// ---------------------------------------------------------------------------
static void pushRectVerts(MeshBatcher& b, const Rect& r, const Color& c, float radius, float z = 0.0f) {
  radius = clampf(radius, 0, fminf(r.w, r.h) * 0.5f);
  if (radius < 0.5f) {
    uint32_t base = b.pushIndexVerts(4);
    VtxShape* v = (VtxShape*)((char*)b.verts + (size_t)base * b.vertexStride);
    v[0] = {r.x, r.y, z, 0, 0, 0, c.r, c.g, c.b, c.a};
    v[1] = {r.x + r.w, r.y, z, 0, 0, 0, c.r, c.g, c.b, c.a};
    v[2] = {r.x + r.w, r.y + r.h, z, 0, 0, 0, c.r, c.g, c.b, c.a};
    v[3] = {r.x, r.y + r.h, z, 0, 0, 0, c.r, c.g, c.b, c.a};
    b.indexQuad(base);
    return;
  }
  const int CORNER = 6;
  // centre rect
  uint32_t cbase = b.pushIndexVerts(4);
  VtxShape* cv = (VtxShape*)((char*)b.verts + (size_t)cbase * b.vertexStride);
  cv[0] = {r.x + radius, r.y, z, 0, 0, 0, c.r, c.g, c.b, c.a};
  cv[1] = {r.x + r.w - radius, r.y, z, 0, 0, 0, c.r, c.g, c.b, c.a};
  cv[2] = {r.x + r.w - radius, r.y + r.h, z, 0, 0, 0, c.r, c.g, c.b, c.a};
  cv[3] = {r.x + radius, r.y + r.h, z, 0, 0, 0, c.r, c.g, c.b, c.a};
  b.indexQuad(cbase);
  // sides
  uint32_t lbase = b.pushIndexVerts(4);
  VtxShape* lv = (VtxShape*)((char*)b.verts + (size_t)lbase * b.vertexStride);
  lv[0] = {r.x, r.y + radius, z, 0, 0, 0, c.r, c.g, c.b, c.a};
  lv[1] = {r.x + radius, r.y + radius, z, 0, 0, 0, c.r, c.g, c.b, c.a};
  lv[2] = {r.x + radius, r.y + r.h - radius, z, 0, 0, 0, c.r, c.g, c.b, c.a};
  lv[3] = {r.x, r.y + r.h - radius, z, 0, 0, 0, c.r, c.g, c.b, c.a};
  b.indexQuad(lbase);
  uint32_t rbase = b.pushIndexVerts(4);
  VtxShape* rv = (VtxShape*)((char*)b.verts + (size_t)rbase * b.vertexStride);
  rv[0] = {r.x + r.w - radius, r.y + radius, z, 0, 0, 0, c.r, c.g, c.b, c.a};
  rv[1] = {r.x + r.w, r.y + radius, z, 0, 0, 0, c.r, c.g, c.b, c.a};
  rv[2] = {r.x + r.w, r.y + r.h - radius, z, 0, 0, 0, c.r, c.g, c.b, c.a};
  rv[3] = {r.x + r.w - radius, r.y + r.h - radius, z, 0, 0, 0, c.r, c.g, c.b, c.a};
  b.indexQuad(rbase);
  // corners
  struct Corner { float cx, cy, a0; };
  Corner corners[4] = {
    {r.x + radius, r.y + radius, PI},
    {r.x + r.w - radius, r.y + radius, -PI * 0.5f},
    {r.x + r.w - radius, r.y + r.h - radius, 0},
    {r.x + radius, r.y + r.h - radius, PI * 0.5f},
  };
  for (int k = 0; k < 4; k++) {
    uint32_t base = b.pushIndexVerts(CORNER + 1);
    VtxShape* v = (VtxShape*)((char*)b.verts + (size_t)base * b.vertexStride);
    v[0] = {corners[k].cx, corners[k].cy, z, 0, 0, 0, c.r, c.g, c.b, c.a};
    for (int i = 0; i <= CORNER; i++) {
      float ang = corners[k].a0 + (PI * 0.5f) * ((float)i / CORNER);
      v[i + 1] = {corners[k].cx + cosf(ang) * radius, corners[k].cy + sinf(ang) * radius, z, 0, 0, 0,
                  c.r, c.g, c.b, c.a};
    }
    for (int i = 0; i < CORNER; i++) {
      b.index(base);
      b.index(base + i + 1);
      b.index(base + i + 2);
    }
  }
}

void rect(const Rect& r, const Color& c, float radius) {
  ensureInit();
  pushRectVerts(gSolid, r, c, radius);
}

void rectStroke(const Rect& r, const Color& c, float thickness, float radius) {
  ensureInit();
  // four thin rounded rects (cheap and crisp at this scale)
  float t = thickness;
  rect(Rect(r.x, r.y, r.w, t), c, radius * 0.5f);
  rect(Rect(r.x, r.y + r.h - t, r.w, t), c, radius * 0.5f);
  rect(Rect(r.x, r.y + t, t, r.h - 2 * t), c, radius * 0.5f);
  rect(Rect(r.x + r.w - t, r.y + t, t, r.h - 2 * t), c, radius * 0.5f);
}

void verticalGradient(const Rect& r, const Color& top, const Color& bottom) {
  ensureInit();
  // triangle strip of 2 rows
  uint32_t base = gSolid.pushIndexVerts(4);
  VtxShape* v = (VtxShape*)((char*)gSolid.verts + (size_t)base * gSolid.vertexStride);
  v[0] = {r.x, r.y, 0, 0, 0, 0, top.r, top.g, top.b, top.a};
  v[1] = {r.x + r.w, r.y, 0, 0, 0, 0, top.r, top.g, top.b, top.a};
  v[2] = {r.x + r.w, r.y + r.h, 0, 0, 0, 0, bottom.r, bottom.g, bottom.b, bottom.a};
  v[3] = {r.x, r.y + r.h, 0, 0, 0, 0, bottom.r, bottom.g, bottom.b, bottom.a};
  gSolid.indexQuad(base);
}

void glow(const Rect& r, const Color& c, float intensity) {
  ensureInit();
  // radial glow quad using the glow texture, additive
  uint32_t base = gGlow.pushIndexVerts(4);
  VtxShape* v = (VtxShape*)((char*)gGlow.verts + (size_t)base * gGlow.vertexStride);
  Color cc = c.withA(c.a * intensity);
  v[0] = {r.x, r.y, 0, 0, 0, 0, cc.r, cc.g, cc.b, cc.a};
  v[1] = {r.x + r.w, r.y, 0, 1, 0, 0, cc.r, cc.g, cc.b, cc.a};
  v[2] = {r.x + r.w, r.y + r.h, 0, 1, 1, 0, cc.r, cc.g, cc.b, cc.a};
  v[3] = {r.x, r.y + r.h, 0, 0, 1, 0, cc.r, cc.g, cc.b, cc.a};
  gGlow.indexQuad(base);
}

void shadow(const Rect& r, float radius, float strength) {
  Rect s(r.x - radius, r.y + radius * 0.4f, r.w + radius * 2, r.h + radius);
  glow(s, Color(0, 0, 0, strength), 1.0f);
}

void panel(const Rect& r, float radius, bool elevated) {
  if (radius < 0) radius = dp(theme.radius);
  if (elevated) {
    shadow(r, dp(18), 0.55f);
    rect(r, theme.panel, radius);
    verticalGradient(Rect(r.x + 1, r.y + 1, r.w - 2, fminf(r.h * 0.5f, dp(70))),
                     Color(1, 1, 1, 0.035f), Color(1, 1, 1, 0.0f));
    rectStroke(r, theme.strokeSoft, 1.0f, radius);
  } else {
    rect(r, theme.panel, radius);
  }
}

void divider(float x0, float x1, float y, const Color& c) { rect(Rect(x0, y, x1 - x0, 1), c); }

void image(const Rect& r, const Color& tint, float alpha) {
  ensureInit();
  TextureBank& t = Engine::get().tex();
  glBindTexture(GL_TEXTURE_2D, t.logo);
  gSprites->quad(r, Rect(0, 0, 1, 1), Color(tint.r, tint.g, tint.b, alpha));
  gSprites->flush();
}

void avatar(const Rect& r, const char* avatarBase64, const char* fallbackName, int seed) {
  // profile photos are optional small JPEGs/PNGs; without a decoder we render a deterministic
  // identity tile (initial + brand gradient) which keeps the UI honest about missing media.
  Color a = Color::hex(0x2B3A6B);
  Color b = Color::hex(0x7C4DFF);
  float t = hashf((unsigned)(seed * 2654435761u));
  b = Color::lerp(Color::hex(0x38E1FF), Color::hex(0x7C4DFF), t);
  rect(r, Color::lerp(a, b, 0.25f), r.h * 0.5f);
  glow(Rect(r.x - r.w * 0.2f, r.y - r.h * 0.2f, r.w * 1.4f, r.h * 1.4f), b.withA(0.35f), 0.8f);
  char initial[8] = {0};
  if (fallbackName && *fallbackName) {
    size_t n = 1;
    while (n < 4 && ((unsigned char)fallbackName[n] & 0xC0) == 0x80) n++;
    memcpy(initial, fallbackName, n);
  } else {
    initial[0] = 'N';
  }
  float fs = r.h * 0.5f;
  float tw = textMeasure(initial, dpInv(fs), 0, 2);
  text(initial, r.x + (r.w - tw) * 0.5f, r.y + (r.h - fs * 0.72f) * 0.5f, dpInv(fs), theme.text, 0, 0, 2);
  (void)avatarBase64;
}

// ---------------------------------------------------------------------------
// text
// ---------------------------------------------------------------------------
static TextRenderer& renderer(int weight) {
  static TextRenderer r0, r1, r2;
  TextRenderer* arr[3] = {&r0, &r1, &r2};
  int idx = weight >= 2 ? 2 : weight == 1 ? 1 : 0;
  static bool init = false;
  if (!init) {
    Engine& e = Engine::get();
    r0.init(&e.font(0), &e.sprites());
    r1.init(&e.font(1), &e.sprites());
    r2.init(&e.font(2), &e.sprites());
    init = true;
  }
  return *arr[idx];
}

float text(const char* utf8, float x, float y, float sizeDp, const Color& c, float maxWidth, int align, int weight) {
  int px = (int)(dp(sizeDp) + 0.5f);
  if (px < 6) px = 6;
  float ascent = renderer(weight).font()->ascent() * px;
  // wrap only when the caller constrains the width; otherwise emit a single line
  renderer(weight).draw(utf8, x, y - ascent * 0.18f, px, c, maxWidth > 0 ? maxWidth : 0, align);
  // advance: one wrapped line is lineHeight, wrapping adds more
  float lineH = renderer(weight).font()->lineHeight() * px;
  if (maxWidth > 0) {
    TextLayout lay;
    renderer(weight).layout(utf8, px, maxWidth, lay);
    return lay.height > 0 ? lay.height : lineH;
  }
  return dp(sizeDp) * 1.35f;
}

float textMeasure(const char* utf8, float sizeDp, float maxWidth, int weight) {
  int px = (int)(dp(sizeDp) + 0.5f);
  return renderer(weight).measure(utf8, px, maxWidth, renderer(weight).font()->isRtlText(utf8));
}

void textBlock(const char* utf8, const Rect& r, float sizeDp, const Color& c, int align, int weight, int maxLines) {
  if (!utf8 || !*utf8 || r.w <= 1) return;
  int px = (int)(dp(sizeDp) + 0.5f);
  if (px < 6) px = 6;
  // Word-wrap inside the rect; the renderer already handles RTL shaping per line.
  renderer(weight).draw(utf8, r.x, r.y - renderer(weight).font()->ascent() * px * 0.18f, px, c, r.w, align);
  (void)maxLines;
}

// ---------------------------------------------------------------------------
// icons (vector, drawn with the solid batcher)
// ---------------------------------------------------------------------------
static void circle(const Vec2& c, float r, const Color& col) { rect(Rect(c.x - r, c.y - r, r * 2, r * 2), col, r); }

static void lineSeg(Vec2 a, Vec2 b, float thickness, const Color& col) {
  Vec2 d = b - a;
  float len = d.length();
  if (len < 0.001f) return;
  float ang = atan2f(d.y, d.x);
  // rotated quad
  Vec2 n(-sinf(ang) * thickness * 0.5f, cosf(ang) * thickness * 0.5f);
  ensureInit();
  uint32_t base = gSolid.pushIndexVerts(4);
  VtxShape* v = (VtxShape*)((char*)gSolid.verts + (size_t)base * gSolid.vertexStride);
  v[0] = {a.x + n.x, a.y + n.y, 0, 0, 0, 0, col.r, col.g, col.b, col.a};
  v[1] = {b.x + n.x, b.y + n.y, 0, 0, 0, 0, col.r, col.g, col.b, col.a};
  v[2] = {b.x - n.x, b.y - n.y, 0, 0, 0, 0, col.r, col.g, col.b, col.a};
  v[3] = {a.x - n.x, a.y - n.y, 0, 0, 0, 0, col.r, col.g, col.b, col.a};
  gSolid.indexQuad(base);
}

static void arc(Vec2 c, float r, float a0, float a1, float thickness, const Color& col) {
  const int steps = 14;
  Vec2 prev = c + Vec2(cosf(a0) * r, sinf(a0) * r);
  for (int i = 1; i <= steps; i++) {
    float t = a0 + (a1 - a0) * ((float)i / steps);
    Vec2 cur = c + Vec2(cosf(t) * r, sinf(t) * r);
    lineSeg(prev, cur, thickness, col);
    prev = cur;
  }
}

void icon(const char* name, const Rect& r, const Color& c) {
  float s = fminf(r.w, r.h);
  Vec2 center(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
  Vec2 tl(r.x, r.y);
  float th = fmaxf(1.6f, s * 0.09f);
  if (!strcmp(name, "home")) {
    lineSeg(Vec2(tl.x + s * 0.16f, tl.y + s * 0.46f), Vec2(center.x, tl.y + s * 0.16f), th, c);
    lineSeg(Vec2(center.x, tl.y + s * 0.16f), Vec2(tl.x + s * 0.84f, tl.y + s * 0.46f), th, c);
    rect(Rect(tl.x + s * 0.24f, tl.y + s * 0.44f, s * 0.52f, s * 0.40f), c, s * 0.08f);
    rect(Rect(center.x - s * 0.07f, tl.y + s * 0.62f, s * 0.14f, s * 0.22f), theme.panel, s * 0.03f);
  } else if (!strcmp(name, "compass") || !strcmp(name, "discover")) {
    arc(center, s * 0.36f, 0, TAU, th, c);
    lineSeg(center + Vec2(-s * 0.16f, s * 0.16f), center + Vec2(s * 0.18f, -s * 0.18f), th, c);
    lineSeg(center + Vec2(s * 0.18f, -s * 0.18f), center + Vec2(s * 0.06f, s * 0.06f), th, c);
  } else if (!strcmp(name, "map")) {
    rectStroke(Rect(tl.x + s * 0.14f, tl.y + s * 0.20f, s * 0.72f, s * 0.60f), c, th, s * 0.08f);
    lineSeg(Vec2(tl.x + s * 0.38f, tl.y + s * 0.20f), Vec2(tl.x + s * 0.38f, tl.y + s * 0.80f), th * 0.7f, c);
    lineSeg(Vec2(tl.x + s * 0.64f, tl.y + s * 0.20f), Vec2(tl.x + s * 0.64f, tl.y + s * 0.80f), th * 0.7f, c);
  } else if (!strcmp(name, "plus")) {
    lineSeg(Vec2(center.x, tl.y + s * 0.22f), Vec2(center.x, tl.y + s * 0.78f), th * 1.2f, c);
    lineSeg(Vec2(tl.x + s * 0.22f, center.y), Vec2(tl.x + s * 0.78f, center.y), th * 1.2f, c);
  } else if (!strcmp(name, "users") || !strcmp(name, "friends")) {
    circle(Vec2(center.x - s * 0.12f, tl.y + s * 0.38f), s * 0.13f, c);
    circle(Vec2(center.x + s * 0.16f, tl.y + s * 0.42f), s * 0.10f, c.withA(c.a * 0.7f));
    arc(Vec2(center.x - s * 0.12f, tl.y + s * 0.78f), s * 0.24f, PI, TAU, th, c);
  } else if (!strcmp(name, "chat") || !strcmp(name, "messages")) {
    rect(Rect(tl.x + s * 0.14f, tl.y + s * 0.20f, s * 0.72f, s * 0.50f), c, s * 0.14f);
    ensureInit();
    uint32_t base = gSolid.pushIndexVerts(3);
    VtxShape* v = (VtxShape*)((char*)gSolid.verts + (size_t)base * gSolid.vertexStride);
    v[0] = {tl.x + s * 0.30f, tl.y + s * 0.68f, 0, 0, 0, 0, c.r, c.g, c.b, c.a};
    v[1] = {tl.x + s * 0.52f, tl.y + s * 0.68f, 0, 0, 0, 0, c.r, c.g, c.b, c.a};
    v[2] = {tl.x + s * 0.30f, tl.y + s * 0.86f, 0, 0, 0, 0, c.r, c.g, c.b, c.a};
    gSolid.index(base); gSolid.index(base + 1); gSolid.index(base + 2);
  } else if (!strcmp(name, "shirt") || !strcmp(name, "avatar")) {
    rect(Rect(tl.x + s * 0.30f, tl.y + s * 0.22f, s * 0.40f, s * 0.58f), c, s * 0.10f);
    rect(Rect(tl.x + s * 0.14f, tl.y + s * 0.26f, s * 0.20f, s * 0.22f), c, s * 0.06f);
    rect(Rect(tl.x + s * 0.66f, tl.y + s * 0.26f, s * 0.20f, s * 0.22f), c, s * 0.06f);
  } else if (!strcmp(name, "bag") || !strcmp(name, "shop")) {
    rect(Rect(tl.x + s * 0.18f, tl.y + s * 0.36f, s * 0.64f, s * 0.46f), c, s * 0.08f);
    arc(Vec2(center.x, tl.y + s * 0.38f), s * 0.16f, PI, TAU, th, c);
  } else if (!strcmp(name, "user") || !strcmp(name, "profile")) {
    circle(Vec2(center.x, tl.y + s * 0.34f), s * 0.15f, c);
    arc(Vec2(center.x, tl.y + s * 0.82f), s * 0.29f, PI, TAU, th, c);
  } else if (!strcmp(name, "gear") || !strcmp(name, "settings")) {
    arc(center, s * 0.26f, 0, TAU, th, c);
    for (int i = 0; i < 8; i++) {
      float a = (float)i / 8 * TAU;
      Vec2 p0 = center + Vec2(cosf(a) * s * 0.29f, sinf(a) * s * 0.29f);
      Vec2 p1 = center + Vec2(cosf(a) * s * 0.40f, sinf(a) * s * 0.40f);
      lineSeg(p0, p1, th, c);
    }
  } else if (!strcmp(name, "bell")) {
    arc(Vec2(center.x, tl.y + s * 0.44f), s * 0.24f, PI, TAU, th, c);
    lineSeg(Vec2(tl.x + s * 0.26f, tl.y + s * 0.44f), Vec2(tl.x + s * 0.26f, tl.y + s * 0.68f), th, c);
    lineSeg(Vec2(tl.x + s * 0.74f, tl.y + s * 0.44f), Vec2(tl.x + s * 0.74f, tl.y + s * 0.68f), th, c);
    rect(Rect(tl.x + s * 0.22f, tl.y + s * 0.66f, s * 0.56f, s * 0.06f), c, s * 0.03f);
    circle(Vec2(center.x, tl.y + s * 0.80f), s * 0.06f, c);
  } else if (!strcmp(name, "search")) {
    arc(Vec2(center.x - s * 0.05f, center.y - s * 0.05f), s * 0.26f, 0, TAU, th, c);
    lineSeg(Vec2(center.x + s * 0.14f, center.y + s * 0.14f), Vec2(tl.x + s * 0.80f, tl.y + s * 0.80f), th * 1.2f, c);
  } else if (!strcmp(name, "back") || !strcmp(name, "chevron_left")) {
    lineSeg(Vec2(tl.x + s * 0.62f, tl.y + s * 0.24f), Vec2(tl.x + s * 0.36f, center.y), th, c);
    lineSeg(Vec2(tl.x + s * 0.36f, center.y), Vec2(tl.x + s * 0.62f, tl.y + s * 0.76f), th, c);
  } else if (!strcmp(name, "chevron_right")) {
    lineSeg(Vec2(tl.x + s * 0.38f, tl.y + s * 0.24f), Vec2(tl.x + s * 0.64f, center.y), th, c);
    lineSeg(Vec2(tl.x + s * 0.64f, center.y), Vec2(tl.x + s * 0.38f, tl.y + s * 0.76f), th, c);
  } else if (!strcmp(name, "close")) {
    lineSeg(Vec2(tl.x + s * 0.28f, tl.y + s * 0.28f), Vec2(tl.x + s * 0.72f, tl.y + s * 0.72f), th, c);
    lineSeg(Vec2(tl.x + s * 0.72f, tl.y + s * 0.28f), Vec2(tl.x + s * 0.28f, tl.y + s * 0.72f), th, c);
  } else if (!strcmp(name, "check")) {
    lineSeg(Vec2(tl.x + s * 0.24f, tl.y + s * 0.52f), Vec2(tl.x + s * 0.44f, tl.y + s * 0.74f), th, c);
    lineSeg(Vec2(tl.x + s * 0.44f, tl.y + s * 0.74f), Vec2(tl.x + s * 0.78f, tl.y + s * 0.28f), th, c);
  } else if (!strcmp(name, "play")) {
    ensureInit();
    uint32_t base = gSolid.pushIndexVerts(3);
    VtxShape* v = (VtxShape*)((char*)gSolid.verts + (size_t)base * gSolid.vertexStride);
    v[0] = {tl.x + s * 0.30f, tl.y + s * 0.20f, 0, 0, 0, 0, c.r, c.g, c.b, c.a};
    v[1] = {tl.x + s * 0.30f, tl.y + s * 0.80f, 0, 0, 0, 0, c.r, c.g, c.b, c.a};
    v[2] = {tl.x + s * 0.80f, center.y, 0, 0, 0, 0, c.r, c.g, c.b, c.a};
    gSolid.index(base); gSolid.index(base + 1); gSolid.index(base + 2);
  } else if (!strcmp(name, "sparkle") || !strcmp(name, "ai")) {
    static const float pts[8][2] = {{0.5f, 0.10f}, {0.58f, 0.40f}, {0.88f, 0.48f}, {0.58f, 0.56f},
                                    {0.5f, 0.88f}, {0.42f, 0.56f}, {0.12f, 0.48f}, {0.42f, 0.40f}};
    for (int i = 0; i < 8; i++) {
      Vec2 a = tl + Vec2(pts[i][0] * s, pts[i][1] * s);
      Vec2 b = tl + Vec2(pts[(i + 1) % 8][0] * s, pts[(i + 1) % 8][1] * s);
      ensureInit();
      uint32_t base = gSolid.pushIndexVerts(3);
      VtxShape* v = (VtxShape*)((char*)gSolid.verts + (size_t)base * gSolid.vertexStride);
      v[0] = {center.x, center.y, 0, 0, 0, 0, c.r, c.g, c.b, c.a};
      v[1] = {a.x, a.y, 0, 0, 0, 0, c.r, c.g, c.b, c.a};
      v[2] = {b.x, b.y, 0, 0, 0, 0, c.r, c.g, c.b, c.a};
      gSolid.index(base); gSolid.index(base + 1); gSolid.index(base + 2);
    }
  } else if (!strcmp(name, "lock")) {
    rect(Rect(tl.x + s * 0.24f, tl.y + s * 0.44f, s * 0.52f, s * 0.38f), c, s * 0.06f);
    arc(Vec2(center.x, tl.y + s * 0.44f), s * 0.16f, PI, TAU, th, c);
  } else if (!strcmp(name, "logout")) {
    rectStroke(Rect(tl.x + s * 0.16f, tl.y + s * 0.20f, s * 0.40f, s * 0.60f), c, th, s * 0.08f);
    lineSeg(Vec2(tl.x + s * 0.46f, center.y), Vec2(tl.x + s * 0.82f, center.y), th, c);
    lineSeg(Vec2(tl.x + s * 0.66f, tl.y + s * 0.34f), Vec2(tl.x + s * 0.82f, center.y), th, c);
    lineSeg(Vec2(tl.x + s * 0.82f, center.y), Vec2(tl.x + s * 0.66f, tl.y + s * 0.66f), th, c);
  } else if (!strcmp(name, "mic")) {
    rect(Rect(center.x - s * 0.10f, tl.y + s * 0.18f, s * 0.20f, s * 0.40f), c, s * 0.10f);
    arc(Vec2(center.x, tl.y + s * 0.48f), s * 0.22f, 0, PI, th, c);
    lineSeg(Vec2(center.x, tl.y + s * 0.70f), Vec2(center.x, tl.y + s * 0.84f), th, c);
  } else if (!strcmp(name, "heart")) {
    circle(Vec2(center.x - s * 0.14f, tl.y + s * 0.38f), s * 0.16f, c);
    circle(Vec2(center.x + s * 0.14f, tl.y + s * 0.38f), s * 0.16f, c);
    ensureInit();
    uint32_t base = gSolid.pushIndexVerts(3);
    VtxShape* v = (VtxShape*)((char*)gSolid.verts + (size_t)base * gSolid.vertexStride);
    v[0] = {center.x - s * 0.30f, tl.y + s * 0.38f, 0, 0, 0, 0, c.r, c.g, c.b, c.a};
    v[1] = {center.x + s * 0.30f, tl.y + s * 0.38f, 0, 0, 0, 0, c.r, c.g, c.b, c.a};
    v[2] = {center.x, tl.y + s * 0.84f, 0, 0, 0, 0, c.r, c.g, c.b, c.a};
    gSolid.index(base); gSolid.index(base + 1); gSolid.index(base + 2);
  } else if (!strcmp(name, "star")) {
    for (int i = 0; i < 5; i++) {
      float a = -PI * 0.5f + (float)i * TAU / 5;
      float a2 = a + TAU / 10;
      Vec2 p0 = center + Vec2(cosf(a) * s * 0.36f, sinf(a) * s * 0.36f);
      Vec2 p1 = center + Vec2(cosf(a2) * s * 0.17f, sinf(a2) * s * 0.17f);
      Vec2 p2 = center + Vec2(cosf(a + TAU / 5) * s * 0.36f, sinf(a + TAU / 5) * s * 0.36f);
      ensureInit();
      uint32_t base = gSolid.pushIndexVerts(3);
      VtxShape* v = (VtxShape*)((char*)gSolid.verts + (size_t)base * gSolid.vertexStride);
      v[0] = {p0.x, p0.y, 0, 0, 0, 0, c.r, c.g, c.b, c.a};
      v[1] = {p1.x, p1.y, 0, 0, 0, 0, c.r, c.g, c.b, c.a};
      v[2] = {p2.x, p2.y, 0, 0, 0, 0, c.r, c.g, c.b, c.a};
      gSolid.index(base); gSolid.index(base + 1); gSolid.index(base + 2);
    }
  } else if (!strcmp(name, "coin") || !strcmp(name, "miy")) {
    circle(center, s * 0.34f, c);
    circle(center, s * 0.26f, Color::lerp(c, theme.panel, 0.35f));
    text("M", center.x - textMeasure("M", dpInv(s * 0.34f), 0, 2) * 0.5f, center.y - s * 0.17f, dpInv(s * 0.34f), c, 0, 0, 2);
  } else if (!strcmp(name, "eye")) {
    arc(Vec2(center.x, center.y + s * 0.18f), s * 0.30f, PI * 1.15f, PI * 1.85f, th, c);
    arc(Vec2(center.x, center.y - s * 0.18f), s * 0.30f, PI * 0.15f, PI * 0.85f, th, c);
    circle(center, s * 0.09f, c);
  } else if (!strcmp(name, "trash")) {
    rect(Rect(tl.x + s * 0.28f, tl.y + s * 0.32f, s * 0.44f, s * 0.50f), c, s * 0.05f);
    rect(Rect(tl.x + s * 0.22f, tl.y + s * 0.24f, s * 0.56f, s * 0.06f), c, s * 0.02f);
  } else {
    circle(center, s * 0.08f, c);
  }
}

// ---------------------------------------------------------------------------
// frame begin/end
// ---------------------------------------------------------------------------
void begin() {
  ensureInit();
  gCaptured = false;
  gSprites = &Engine::get().sprites();
  gSolid.vertCount = gSolid.indexCount = 0;
  gGlow.vertCount = gGlow.indexCount = 0;
  gSprites->begin();
}

static void setupBlend() {
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

Pointer& pointer() { return gPointer; }

void flushText() {
  Engine& e = Engine::get();
  setupBlend();
  if (gSolid.vertCount > 0) {
    gSolidProg.use();
    gSolidProg.set("uScreen", e.width(), e.height());
    gSolid.flush(GL_TRIANGLES);
  }
  // glyphs accumulated by TextRenderer::draw go out here
  e.uiSprites().flush();
}

void flushGlow() {
  Engine& e = Engine::get();
  setupBlend();
  if (gGlow.vertCount > 0) {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBindTexture(GL_TEXTURE_2D, e.tex().glow);
    gGlowProg.use();
    gGlowProg.set("uScreen", e.width(), e.height());
    gGlowProg.set("uTex", 0);
    gGlow.flush(GL_TRIANGLES);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }
}

void end() {
  flushText();
  flushGlow();
}

void pushEditableText(const char* utf8) {
  size_t n = strlen(utf8);
  size_t cur = strlen(gPendingText);
  for (size_t i = 0; i < n && cur + 1 < sizeof gPendingText; i++) gPendingText[cur++] = utf8[i];
  gPendingText[cur] = 0;
}

void setFocus(const char* id) { snprintf(gFocusId, sizeof gFocusId, "%s", id ? id : ""); }

void pushBackspace() { gPendingBackspace = true; }
const char* focusedId() { return gFocusId; }

}  // namespace ui
}  // namespace nx
