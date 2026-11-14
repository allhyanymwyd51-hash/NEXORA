// NEXORA engine — math core (no stdlib dependency beyond compiler builtins).
#pragma once
#include <math.h>

namespace nx {

constexpr float PI = 3.14159265358979323846f;
constexpr float TAU = 6.28318530717958647692f;
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }
inline float fractf(float v) { return v - floorf(v); }
inline float signf(float v) { return v < 0 ? -1.0f : 1.0f; }
inline float deg2rad(float d) { return d * (PI / 180.0f); }
inline float rad2deg(float r) { return r * (180.0f / PI); }
inline float moveToward(float cur, float target, float maxDelta) {
  float d = target - cur;
  if (fabsf(d) <= maxDelta) return target;
  return cur + signf(d) * maxDelta;
}
// frame-rate independent exponential smoothing
inline float damp(float cur, float target, float lambda, float dt) {
  return lerpf(cur, target, 1.0f - expf(-lambda * dt));
}

struct Vec2 {
  float x = 0, y = 0;
  Vec2() {}
  Vec2(float X, float Y) : x(X), y(Y) {}
  Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
  Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
  Vec2 operator*(float s) const { return {x * s, y * s}; }
  float length() const { return sqrtf(x * x + y * y); }
  float distance(const Vec2& o) const { return (*this - o).length(); }
};

struct Vec3 {
  float x = 0, y = 0, z = 0;
  Vec3() {}
  Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
  Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
  Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
  Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
  float length() const { return sqrtf(x * x + y * y + z * z); }
  float lengthXZ() const { return sqrtf(x * x + z * z); }
  Vec3 normalized() const { float l = length(); return l > 1e-6f ? Vec3{x / l, y / l, z / l} : Vec3{0, 0, 0}; }
  Vec3 flat() const { return {x, 0, z}; }
};
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
  return {lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t)};
}

struct Vec4 {
  float x = 0, y = 0, z = 0, w = 0;
  Vec4() {}
  Vec4(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}
  Vec4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
};

struct Color {
  float r = 1, g = 1, b = 1, a = 1;
  Color() {}
  Color(float R, float G, float B, float A = 1) : r(R), g(G), b(B), a(A) {}
  Color operator*(float s) const { return {r * s, g * s, b * s, a}; }
  Color withA(float A) const { return {r, g, b, A}; }
  static Color hex(unsigned int rgb, float a = 1.0f) {
    return {((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, a};
  }
  static Color lerp(const Color& a, const Color& b, float t) {
    return {lerpf(a.r, b.r, t), lerpf(a.g, b.g, t), lerpf(a.b, b.b, t), lerpf(a.a, b.a, t)};
  }
};

struct Mat4 {
  float m[16];  // column-major, OpenGL convention
  Mat4() { identity(); }
  void identity() {
    for (int i = 0; i < 16; i++) m[i] = 0;
    m[0] = m[5] = m[10] = m[15] = 1;
  }
  static Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; c++) {
      for (int rr = 0; rr < 4; rr++) {
        float s = 0;
        for (int k = 0; k < 4; k++) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
        r.m[c * 4 + rr] = s;
      }
    }
    return r;
  }
  static Mat4 perspective(float fovyRad, float aspect, float znear, float zfar) {
    Mat4 r;
    float f = 1.0f / tanf(fovyRad * 0.5f);
    for (int i = 0; i < 16; i++) r.m[i] = 0;
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zfar + znear) / (znear - zfar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
    return r;
  }
  static Mat4 ortho(float l, float r_, float b, float t, float n, float f) {
    Mat4 r;
    for (int i = 0; i < 16; i++) r.m[i] = 0;
    r.m[0] = 2.0f / (r_ - l);
    r.m[5] = 2.0f / (t - b);
    r.m[10] = -2.0f / (f - n);
    r.m[12] = -(r_ + l) / (r_ - l);
    r.m[13] = -(t + b) / (t - b);
    r.m[14] = -(f + n) / (f - n);
    r.m[15] = 1.0f;
    return r;
  }
  static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = (center - eye).normalized();
    Vec3 s = cross(f, up).normalized();
    Vec3 u = cross(s, f);
    Mat4 r;
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dot(s, eye); r.m[13] = -dot(u, eye); r.m[14] = dot(f, eye);
    return r;
  }
  static Mat4 translate(const Vec3& t) {
    Mat4 r; r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r;
  }
  static Mat4 scale(const Vec3& s) {
    Mat4 r; r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z; return r;
  }
  static Mat4 rotateY(float a) {
    Mat4 r; float c = cosf(a), s = sinf(a);
    r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c; return r;
  }
  static Mat4 rotateX(float a) {
    Mat4 r; float c = cosf(a), s = sinf(a);
    r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c; return r;
  }
  static Mat4 trs(const Vec3& t, float ry, const Vec3& s) {
    Mat4 r = translate(t);
    Mat4 rot = rotateY(ry);
    r = mul(r, rot);
    r = mul(r, scale(s));
    return r;
  }
  Vec4 operator*(const Vec4& v) const {
    return {
      m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
      m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
      m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
      m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w,
    };
  }
};

struct Rect {
  float x = 0, y = 0, w = 0, h = 0;
  Rect() {}
  Rect(float X, float Y, float W, float H) : x(X), y(Y), w(W), h(H) {}
  float right() const { return x + w; }
  float bottom() const { return y + h; }
  Vec2 center() const { return {x + w * 0.5f, y + h * 0.5f}; }
  bool contains(float px, float py) const { return px >= x && px <= x + w && py >= y && py <= y + h; }
  Rect inset(float d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }
  Rect shrink(float l, float t, float r, float b) const { return {x + l, y + t, w - l - r, h - t - b}; }
};

// deterministic hash → [0,1)
inline float hashf(unsigned int x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return (x & 0xFFFFFF) / 16777216.0f;
}
inline float hash2f(int x, int y, int seed = 0) {
  unsigned int h = (unsigned int)(x * 374761393 + y * 668265263 + seed * 2147483647);
  return hashf(h);
}

// value noise + fbm for terrain, clouds and particles
inline float valueNoise(float x, float z, int seed) {
  int ix = (int)floorf(x), iz = (int)floorf(z);
  float fx = x - ix, fz = z - iz;
  float u = smoothstep(fx), v = smoothstep(fz);
  float a = hash2f(ix, iz, seed), b = hash2f(ix + 1, iz, seed);
  float c = hash2f(ix, iz + 1, seed), d = hash2f(ix + 1, iz + 1, seed);
  return lerpf(lerpf(a, b, u), lerpf(c, d, u), v);
}
inline float fbm(float x, float z, int seed, int octaves) {
  float sum = 0, amp = 0.5f, freq = 1.0f, norm = 0;
  for (int i = 0; i < octaves; i++) {
    sum += valueNoise(x * freq, z * freq, seed + i * 17) * amp;
    norm += amp;
    amp *= 0.5f;
    freq *= 2.03f;
  }
  return norm > 0 ? sum / norm : 0;
}

}  // namespace nx
