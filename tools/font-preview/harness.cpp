// NEXORA — host-side font harness.
//
// Renders sample UI strings through the engine's real font stack (load -> shape ->
// rasterise) and writes a PNG, so Arabic joining, bidi ordering and the glyph
// rasteriser can be checked without a device. Run via tools/font-preview/preview.sh.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdarg>
#include <vector>
#include <string>
#include "core/font.h"
extern "C" int __android_log_print(int, const char*, const char* fmt, ...) { (void)fmt; return 0; }
namespace nx {
bool readAsset(const char*, void**, size_t*) { return false; }
void uploadPixelTexture(unsigned int&, int, int, const unsigned char*, bool, bool) {}
void SpriteBatch::quad(const Rect&, const Rect&, const Color&, float) {}
void SpriteBatch::flush() {}
}
using namespace nx;

static unsigned char* slurp(const char* p, size_t& n) {
  FILE* f = fopen(p, "rb"); if (!f) { perror(p); exit(1); }
  fseek(f, 0, SEEK_END); n = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char* d = (unsigned char*)malloc(n);
  if (fread(d, 1, n, f) != n) exit(1);
  fclose(f); return d;
}

static void writePng(const char* path, int w, int h, const std::vector<unsigned char>& g) {
  // grayscale -> RGB PNG, hand-rolled (no image libraries in the sandbox)
  std::vector<unsigned char> raw;
  raw.reserve((size_t)(w * 3 + 1) * h);
  for (int y = 0; y < h; y++) {
    raw.push_back(0);
    for (int x = 0; x < w; x++) {
      unsigned char v = g[(size_t)y * w + x];
      raw.push_back(v); raw.push_back(v); raw.push_back(v);
    }
  }
  auto be32 = [](std::vector<unsigned char>& o, unsigned v) {
    o.push_back(v >> 24); o.push_back(v >> 16); o.push_back(v >> 8); o.push_back(v);
  };
  auto crc = [](const unsigned char* d, size_t n) {
    static unsigned table[256]; static bool init = false;
    if (!init) { for (unsigned i = 0; i < 256; i++) { unsigned c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; table[i] = c; } init = true; }
    unsigned c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = table[(c ^ d[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
  };
  auto chunk = [&](std::vector<unsigned char>& out, const char* type, const std::vector<unsigned char>& data) {
    be32(out, (unsigned)data.size());
    std::vector<unsigned char> td(type, type + 4);
    td.insert(td.end(), data.begin(), data.end());
    out.insert(out.end(), td.begin(), td.end());
    be32(out, crc(td.data(), td.size()));
  };
  std::vector<unsigned char> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  std::vector<unsigned char> ihdr; be32(ihdr, (unsigned)w); be32(ihdr, (unsigned)h);
  ihdr.push_back(8); ihdr.push_back(2); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
  chunk(png, "IHDR", ihdr);
  // zlib stream, stored (uncompressed) deflate blocks
  std::vector<unsigned char> z = {0x78, 0x01};
  size_t pos = 0;
  while (pos < raw.size()) {
    size_t n = std::min<size_t>(65535, raw.size() - pos);
    z.push_back(pos + n >= raw.size() ? 1 : 0);
    z.push_back(n & 0xFF); z.push_back((n >> 8) & 0xFF);
    z.push_back(~n & 0xFF); z.push_back(((~n) >> 8) & 0xFF);
    z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
    pos += n;
  }
  unsigned a = 1, b = 0;
  for (unsigned char c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
  be32(z, (b << 16) | a);
  chunk(png, "IDAT", z);
  chunk(png, "IEND", {});
  FILE* f = fopen(path, "wb"); fwrite(png.data(), 1, png.size(), f); fclose(f);
}

int main(int argc, char** argv) {
  size_t n = 0;
  unsigned char* data = slurp(argv[1], n);
  Font font;
  if (!font.loadFromMemory(data, n)) return 1;
  const char* lines[3] = {
      "السلام عليكم",                       // greeting
      "مرحبا بك في نيكسورا",                 // mixed-length Arabic join
      "NEXORA 1.0.0 ماب",                   // LTR island inside an Arabic line
  };
  int px = 56;
  int W = 900, lineH = px + 34;
  int H = lineH * 3 + 24;
  std::vector<unsigned char> canvas((size_t)W * H, 12);  // dark UI background
  for (int li = 0; li < 3; li++) {
    Array<GlyphShape> shaped;
    font.shape(lines[li], shaped);
    bool rtl = font.isRtlText(lines[li]);
    float total = 0;
    for (int i = 0; i < shaped.count; i++) total += shaped[i].advance * px;
    float pen = rtl ? (W - 16 - total) : 16.0f;
    int baseline = 10 + li * lineH + px + 2;
    for (int i = 0; i < shaped.count; i++) {
      Array<unsigned char> bmp; int w = 0, h = 0, bx = 0, by = 0;
      if (font.glyphBitmapStraight(shaped[i].glyph, px, bmp, w, h, bx, by)) {
        for (int y = 0; y < h; y++)
          for (int x = 0; x < w; x++) {
            int dx = (int)pen + bx + x, dy = baseline - by + y;
            if (dx < 0 || dx >= W || dy < 0 || dy >= H) continue;
            unsigned char v = bmp[(size_t)y * w + x];
            unsigned char& dst = canvas[(size_t)dy * W + dx];
            if (v > dst) dst = v;
          }
      }
      pen += shaped[i].advance * px;
    }
  }
  // scale glyph ink over the dark background for readability
  for (size_t i = 0; i < canvas.size(); i++) canvas[i] = (unsigned char)(12 + canvas[i] * 0.95f);
  writePng(argv[2], W, H, canvas);
  printf("wrote %s (%dx%d)\n", argv[2], W, H);
  return 0;
}
