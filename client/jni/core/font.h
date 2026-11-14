// NEXORA engine — TrueType parser, Arabic shaper/bidi, glyph atlas and RTL-aware layout.
#pragma once
#include "gfx.h"

namespace nx {

// Arabic joining classes (subset of the Unicode data that covers Arabic script).
enum ArabicForm { AR_ISOLATED = 0, AR_FINAL = 1, AR_INITIAL = 2, AR_MEDIAL = 3, AR_NONE = 4 };

struct GlyphShape {
  uint32_t glyph;   // font glyph id
  float advance;    // in em units
  bool isSpace;
};

struct GlyphQuad {
  uint16_t gx, gy, gw, gh;    // atlas cell
  int16_t bearingX, bearingY; // pixels at the reference size
  uint16_t w, h;
  float advance;              // pixels at reference size
};

struct FontAtlas {
  Texture tex;
  GLuint glId = 0;
  int atlasSize = 1024;
  int penX = 0, penY = 0, rowH = 0;
};

// Per-style glyph cache (size-specific rasterisation keeps Arabic crisp on phones).
struct GlyphCache {
  int pixelSize = 0;
  FontAtlas atlas;
  struct Entry {
    uint32_t glyph;
    GlyphQuad q;
  };
  Array<Entry> entries;
  const GlyphQuad* find(uint32_t glyph) const {
    for (int i = 0; i < entries.count; i++) if (entries[i].glyph == glyph) return &entries[i].q;
    return nullptr;
  }
};

// Flattened TrueType outline in font units (definition shared with font.cpp;
// kept in the header so composite glyphs can be resolved recursively).
struct Outline {
  Array<int32_t> xs, ys;
  Array<uint8_t> flags;
  Array<uint16_t> ends;  // contour end point indices (absolute)
  int xMin = 0, yMin = 0, xMax = 0, yMax = 0;
};

class Font {
 public:
  bool loadFromMemory(const unsigned char* data, size_t size);
  bool loadFromAsset(const char* path);
  bool valid() const { return m_numGlyphs > 0; }

  // font metrics in em units
  float unitsPerEm() const { return m_unitsPerEm; }
  float ascent() const { return m_ascent / m_unitsPerEm; }
  float descent() const { return -m_descent / m_unitsPerEm; }  // positive value
  float lineHeight() const { return (m_ascent - m_descent + m_lineGap) / m_unitsPerEm; }

  // glyph lookup: unicode codepoint -> glyph index (0 = .notdef)
  uint32_t glyphFor(uint32_t cp) const;
  // Raw glyph outline data used by the rasteriser
  struct OutlinePoint { float x, y; };
  bool glyphOutline(uint32_t glyph, Array<OutlinePoint>& out, int& contourCount, Array<int>& contours);
  bool glyphBitmapStraight(uint32_t glyph, int pixelSize, Array<unsigned char>& outBitmap, int& w, int& h,
                           int& bearingX, int& bearingY);
  float advanceOf(uint32_t glyph) const;

  // shaping
  void shape(const char* utf8, Array<GlyphShape>& out);   // includes Arabic joining + bidi order
  bool isRtlText(const char* utf8);

  // caching
  GlyphCache* cacheFor(int pixelSize);
  const GlyphQuad* glyphQuad(uint32_t glyph, int pixelSize);
  void destroy();

 private:
  unsigned char* m_data = nullptr;
  bool m_owned = false;
  uint32_t m_unitsPerEm = 1000;
  int16_t m_ascent = 0, m_descent = 0, m_lineGap = 0;
  int m_numGlyphs = 0;
  int m_numHMetrics = 0;
  int m_longLoca = 0;
  // table offsets
  uint32_t m_loca = 0, m_glyf = 0, m_hmtx = 0, m_cmap = 0, m_head = 0, m_hhea = 0, m_maxp = 0, m_cmapSub = 0;
  int m_cmapFormat = 0;
  Array<GlyphCache*> m_caches;

  uint32_t table(const char* tag) const;
  // Resolves a glyph into its outline: simple glyphs point straight at the glyf
  // record, composite glyphs are flattened (Cairo builds Arabic presentation
  // forms, and their dots, out of components).
  const uint8_t* glyphData(uint32_t glyph, uint32_t& len);
  bool subOutline(uint32_t glyph, Outline& out, int depth);
  bool appendComposite(uint32_t glyph, Outline& out, int depth);
  bool decomposeSimple(const uint8_t* d, uint32_t len, Outline& out);
  bool rasterizeContour(Array<float>& xs, Array<float>& ys, Array<int>& starts, int pixelSize,
                        Array<unsigned char>& bmp, int& w, int& h, int& bx, int& by);
  void appendGlyphToAtlas(uint32_t glyph, int pixelSize, GlyphCache* cache);
};

// Text layout result: positioned glyph quads ready for SpriteBatch
struct TextGlyph {
  const GlyphQuad* quad;
  float x, y;  // top-left in pixels relative to the text origin
};
struct TextLayout {
  Array<TextGlyph> glyphs;
  float width = 0, height = 0;
  int lines = 1;
};

// A rich text renderer for one font at a given size.
class TextRenderer {
 public:
  void init(Font* font, SpriteBatch* batch);
  // Layout with word wrap (maxWidth<=0 means single line), alignment and RTL/LTR autodetect.
  void layout(const char* utf8, int pixelSize, float maxWidth, TextLayout& out, bool forceRtl = false);
  void draw(const char* utf8, float x, float y, int pixelSize, const Color& color, float maxWidth = 0,
            int align = 0 /*0 left,1 center,2 right*/, bool forceRtl = false);
  float measure(const char* utf8, int pixelSize, float maxWidth = 0, bool forceRtl = false);
  Font* font() { return m_font; }
  // Ellipsize a single line to fit maxWidth.
  void ellipsize(const char* in, int pixelSize, float maxWidth, char* out, size_t cap);

 private:
  Font* m_font = nullptr;
  SpriteBatch* m_batch = nullptr;
};

// Glyph atlas texture binding: atlas cells are sampled with a 1px inset to avoid bleeding.
struct GlyphAtlasBinder {
  static void bind(GlyphCache* cache);
};

}  // namespace nx
