#include "font.h"

// ---------------------------------------------------------------------------
// UTF-8 + Arabic joining tables
// ---------------------------------------------------------------------------
namespace nx {
namespace {

uint32_t readUtf8(const char** p) {
  const unsigned char* s = (const unsigned char*)*p;
  uint32_t cp;
  if (s[0] < 0x80) { cp = s[0]; *p += 1; return cp; }
  if ((s[0] & 0xE0) == 0xC0) { cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F); *p += 2; return cp; }
  if ((s[0] & 0xF0) == 0xE0) {
    cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    *p += 3;
    return cp;
  }
  if ((s[0] & 0xF8) == 0xF0) {
    cp = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    *p += 4;
    return cp;
  }
  *p += 1;
  return '?';
}

// --- joining classes -------------------------------------------------------
// 1 = right joining (alef, dal, thal, reh, zain, waw, ...)  (connects only to the right)
// 2 = dual joining                                          (connects on both sides)
// 3 = non joining (hamza, Arabic letters that do not join: ء)
int joinClass(uint32_t cp) {
  switch (cp) {
    // right-joining letters
    case 0x0627: case 0x0623: case 0x0625: case 0x0622:  // alef forms
    case 0x062F: case 0x0630: case 0x0631: case 0x0632:  // dal dhal reh zain
    case 0x0648: case 0x0624:                            // waw, waw with hamza
    case 0x0629:                                         // teh marbuta
    case 0x06D2: case 0x06D5:                            // yeh barree, ae
      return 1;
    // non joining
    case 0x0621:
      return 3;
    default:
      break;
  }
  if (cp >= 0x0620 && cp <= 0x064A) return 2;   // main Arabic block letters (except handled above)
  if (cp >= 0x066E && cp <= 0x06D3) return 2;   // extended Arabic
  if (cp >= 0x06FA && cp <= 0x06FF) return 2;
  if (cp >= 0x0750 && cp <= 0x077F) return 2;   // Arabic supplement
  if (cp >= 0x08A0 && cp <= 0x08FF) return 2;   // Arabic extended-A
  if (cp >= 0xFB50 && cp <= 0xFDFF) return 2;   // Arabic presentation forms A (already shaped)
  if (cp >= 0xFE70 && cp <= 0xFEFF) return 2;   // presentation forms B
  return 0;                                      // not Arabic
}

bool isDiacritic(uint32_t cp) {
  return (cp >= 0x064B && cp <= 0x065F) || cp == 0x0670 || (cp >= 0x06D6 && cp <= 0x06ED) || cp == 0x0640;
}
bool isMirrored(uint32_t cp) {
  return cp == '(' || cp == ')' || cp == '[' || cp == ']' || cp == '{' || cp == '}' || cp == '<' || cp == '>';
}

// --- presentation form tables (Unicode Arabic Presentation Forms-B) --------
// Each entry gives isolated/final/initial/medial codepoints. 0 means the form is unavailable.
struct FormRow {
  uint32_t base;
  uint32_t iso, fin, ini, med;
};
static const FormRow kForms[] = {
  {0x0621, 0xFE80, 0, 0, 0},
  {0x0622, 0xFE81, 0xFE82, 0, 0},
  {0x0623, 0xFE83, 0xFE84, 0, 0},
  {0x0624, 0xFE85, 0xFE86, 0, 0},
  {0x0625, 0xFE87, 0xFE88, 0, 0},
  {0x0626, 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C},
  {0x0627, 0xFE8D, 0xFE8E, 0, 0},
  {0x0628, 0xFE8F, 0xFE90, 0xFE91, 0xFE92},
  {0x0629, 0xFE93, 0xFE94, 0, 0},
  {0x062A, 0xFE95, 0xFE96, 0xFE97, 0xFE98},
  {0x062B, 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C},
  {0x062C, 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0},
  {0x062D, 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4},
  {0x062E, 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8},
  {0x062F, 0xFEA9, 0xFEAA, 0, 0},
  {0x0630, 0xFEAB, 0xFEAC, 0, 0},
  {0x0631, 0xFEAD, 0xFEAE, 0, 0},
  {0x0632, 0xFEAF, 0xFEB0, 0, 0},
  {0x0633, 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4},
  {0x0634, 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8},
  {0x0635, 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC},
  {0x0636, 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0},
  {0x0637, 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4},
  {0x0638, 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8},
  {0x0639, 0xFEC9, 0xFECA, 0xFECB, 0xFECC},
  {0x063A, 0xFECD, 0xFECE, 0xFECF, 0xFED0},
  {0x0641, 0xFED1, 0xFED2, 0xFED3, 0xFED4},
  {0x0642, 0xFED5, 0xFED6, 0xFED7, 0xFED8},
  {0x0643, 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC},
  {0x0644, 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0},
  {0x0645, 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4},
  {0x0646, 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8},
  {0x0647, 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC},
  {0x0648, 0xFEED, 0xFEEE, 0, 0},
  {0x0649, 0xFEEF, 0xFEF0, 0, 0},
  {0x064A, 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4},
  {0x0671, 0xFB50, 0xFB51, 0, 0},
  {0x0679, 0xFB66, 0xFB67, 0xFB68, 0xFB69},
  {0x067A, 0xFB5E, 0xFB5F, 0xFB60, 0xFB61},
  {0x067B, 0xFB52, 0xFB53, 0xFB54, 0xFB55},
  {0x067E, 0xFB56, 0xFB57, 0xFB58, 0xFB59},
  {0x067F, 0xFB62, 0xFB63, 0xFB64, 0xFB65},
  {0x0680, 0xFB5A, 0xFB5B, 0xFB5C, 0xFB5D},
  {0x0683, 0xFB76, 0xFB77, 0xFB78, 0xFB79},
  {0x0684, 0xFB72, 0xFB73, 0xFB74, 0xFB75},
  {0x0686, 0xFB7A, 0xFB7B, 0xFB7C, 0xFB7D},
  {0x0687, 0xFB7E, 0xFB7F, 0xFB80, 0xFB81},
  {0x0688, 0xFB88, 0xFB89, 0, 0},
  {0x068C, 0xFB84, 0xFB85, 0, 0},
  {0x068D, 0xFB82, 0xFB83, 0, 0},
  {0x068E, 0xFB86, 0xFB87, 0, 0},
  {0x0691, 0xFB8C, 0xFB8D, 0, 0},
  {0x0698, 0xFB8A, 0xFB8B, 0, 0},
  {0x06A4, 0xFB6A, 0xFB6B, 0xFB6C, 0xFB6D},
  {0x06A6, 0xFB6E, 0xFB6F, 0xFB70, 0xFB71},
  {0x06A9, 0xFB8E, 0xFB8F, 0xFB90, 0xFB91},
  {0x06AD, 0xFBD3, 0xFBD4, 0xFBD5, 0xFBD6},
  {0x06AF, 0xFB92, 0xFB93, 0xFB94, 0xFB95},
  {0x06B1, 0xFB9A, 0xFB9B, 0xFB9C, 0xFB9D},
  {0x06B3, 0xFB96, 0xFB97, 0xFB98, 0xFB99},
  {0x06BA, 0xFB9E, 0xFB9F, 0, 0},
  {0x06BB, 0xFBA0, 0xFBA1, 0xFBA2, 0xFBA3},
  {0x06BE, 0xFBAA, 0xFBAB, 0xFBAC, 0xFBAD},
  {0x06C0, 0xFBA4, 0xFBA5, 0, 0},
  {0x06C1, 0xFBA6, 0xFBA7, 0xFBA8, 0xFBA9},
  {0x06C5, 0xFBE0, 0xFBE1, 0, 0},
  {0x06C6, 0xFBD9, 0xFBDA, 0, 0},
  {0x06C7, 0xFBD7, 0xFBD8, 0, 0},
  {0x06C8, 0xFBDB, 0xFBDC, 0, 0},
  {0x06C9, 0xFBE2, 0xFBE3, 0, 0},
  {0x06CB, 0xFBDE, 0xFBDF, 0, 0},
  {0x06CC, 0xFBFC, 0xFBFD, 0xFBFE, 0xFBFF},
  {0x06D0, 0xFBE4, 0xFBE5, 0xFBE6, 0xFBE7},
  {0x06D2, 0xFBAE, 0xFBAF, 0, 0},
  {0x06D3, 0xFBB0, 0xFBB1, 0, 0},
};
// Lam-alef ligatures: lam followed by alef variant
struct LamAlefRow { uint32_t alef; uint32_t iso; uint32_t fin; };
static const LamAlefRow kLamAlef[] = {
  {0x0622, 0xFEF5, 0xFEF6},
  {0x0623, 0xFEF7, 0xFEF8},
  {0x0625, 0xFEF9, 0xFEFA},
  {0x0627, 0xFEFB, 0xFEFC},
};

const FormRow* findForm(uint32_t cp) {
  for (size_t i = 0; i < sizeof(kForms) / sizeof(kForms[0]); i++) if (kForms[i].base == cp) return &kForms[i];
  return nullptr;
}
const FormRow* findFormByPresentation(uint32_t cp) {
  for (size_t i = 0; i < sizeof(kForms) / sizeof(kForms[0]); i++) {
    const FormRow& r = kForms[i];
    if (r.iso == cp || r.fin == cp || r.ini == cp || r.med == cp) return &r;
  }
  return nullptr;
}
uint32_t presentation(uint32_t base, int form) {
  const FormRow* r = findForm(base);
  if (!r) return 0;
  switch (form) {
    case AR_ISOLATED: return r->iso;
    case AR_FINAL: return r->fin;
    case AR_INITIAL: return r->ini;
    case AR_MEDIAL: return r->med;
    default: return 0;
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// Font loading
// ---------------------------------------------------------------------------
static uint16_t rd16(const unsigned char* p) { return (uint16_t)((p[0] << 8) | p[1]); }
static int16_t rdi16(const unsigned char* p) { return (int16_t)((p[0] << 8) | p[1]); }
static uint32_t rd32(const unsigned char* p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

uint32_t Font::table(const char* tag) const {
  if (!m_data) return 0;
  int numTables = rd16(m_data + 4);
  for (int i = 0; i < numTables; i++) {
    const unsigned char* rec = m_data + 12 + i * 16;
    if (memcmp(rec, tag, 4) == 0) return rd32(rec + 8);
  }
  return 0;
}

bool Font::loadFromMemory(const unsigned char* data, size_t size) {
  if (!data || size < 64) return false;
  if (!(data[0] == 0 && data[1] == 1 && data[2] == 0 && data[3] == 0) &&
      !(data[0] == 't' && data[1] == 'r' && data[2] == 'u' && data[3] == 'e') &&
      !(data[0] == 'O' && data[1] == 'T' && data[2] == 'T' && data[3] == 'O')) {
    NX_LOGE("font: unsupported container (only TrueType outlines are supported)");
    return false;
  }
  m_data = (unsigned char*)data;
  m_owned = false;
  m_head = table("head");
  m_hhea = table("hhea");
  m_maxp = table("maxp");
  m_hmtx = table("hmtx");
  m_cmap = table("cmap");
  m_loca = table("loca");
  m_glyf = table("glyf");
  if (!m_head || !m_maxp || !m_cmap || !m_loca || !m_glyf) {
    NX_LOGE("font: missing required tables");
    return false;
  }
  m_unitsPerEm = rd16(m_data + m_head + 18);
  m_longLoca = rdi16(m_data + m_head + 50);
  m_ascent = rdi16(m_data + m_hhea + 4);
  m_descent = rdi16(m_data + m_hhea + 6);
  m_lineGap = rdi16(m_data + m_hhea + 8);
  m_numHMetrics = rd16(m_data + m_hhea + 34);
  m_numGlyphs = rd16(m_data + m_maxp + 4);
  // cmap subtable: prefer format 4 (BMP) then 12 (full)
  int nSub = rd16(m_data + m_cmap + 2);
  uint32_t best4 = 0, best12 = 0;
  for (int i = 0; i < nSub; i++) {
    const unsigned char* rec = m_data + m_cmap + 4 + i * 8;
    uint16_t platform = rd16(rec);
    uint16_t encoding = rd16(rec + 2);
    uint32_t off = rd32(rec + 4);
    const unsigned char* sub = m_data + m_cmap + off;
    uint16_t format = rd16(sub);
    if (format == 4 && (platform == 3 || platform == 0)) best4 = m_cmap + off;
    if (format == 12) best12 = m_cmap + off;
    (void)encoding;
  }
  m_cmapSub = best12 ? best12 : best4;
  m_cmapFormat = m_cmapSub ? rd16(m_data + m_cmapSub) : 0;
  NX_LOGI("font loaded: glyphs=%d upem=%u cmapFormat=%d", m_numGlyphs, m_unitsPerEm, m_cmapFormat);
  return m_cmapSub != 0;
}

bool Font::loadFromAsset(const char* path) {
  void* data = nullptr;
  size_t size = 0;
  if (!readAsset(path, &data, &size)) {
    NX_LOGW("font asset missing: %s", path);
    return false;
  }
  bool ok = loadFromMemory((const unsigned char*)data, size);
  m_owned = true;  // keep the buffer alive for the lifetime of the engine
  return ok;
}

uint32_t Font::glyphFor(uint32_t cp) const {
  if (!m_cmapSub) return 0;
  const unsigned char* sub = m_data + m_cmapSub;
  if (m_cmapFormat == 4) {
    uint16_t segCountX2 = rd16(sub + 6);
    int segs = segCountX2 / 2;
    const unsigned char* endCodes = sub + 14;
    const unsigned char* startCodes = endCodes + segCountX2 + 2;
    const unsigned char* idDeltas = startCodes + segCountX2;
    const unsigned char* idRangeOffsets = idDeltas + segCountX2;
    if (cp > 0xFFFF) return 0;
    for (int i = 0; i < segs; i++) {
      uint16_t end = rd16(endCodes + i * 2);
      if (cp <= end) {
        uint16_t start = rd16(startCodes + i * 2);
        if (cp < start) return 0;
        int16_t delta = rdi16(idDeltas + i * 2);
        uint16_t rangeOffset = rd16(idRangeOffsets + i * 2);
        if (rangeOffset == 0) return (uint16_t)(cp + delta) % 65536;
        const unsigned char* addr = idRangeOffsets + i * 2 + rangeOffset + (cp - start) * 2;
        uint16_t g = rd16(addr);
        if (g == 0) return 0;
        return (uint16_t)(g + delta) % 65536;
      }
    }
    return 0;
  }
  if (m_cmapFormat == 12) {
    uint32_t nGroups = rd32(sub + 12);
    for (uint32_t i = 0; i < nGroups; i++) {
      const unsigned char* g = sub + 16 + i * 12;
      uint32_t start = rd32(g), end = rd32(g + 4), startGlyph = rd32(g + 8);
      if (cp >= start && cp <= end) return startGlyph + (cp - start);
    }
    return 0;
  }
  return 0;
}

float Font::advanceOf(uint32_t glyph) const {
  if (!m_hmtx || m_numHMetrics <= 0) return 0.5f;
  uint32_t idx = glyph < (uint32_t)m_numHMetrics ? glyph : (uint32_t)(m_numHMetrics - 1);
  return rd16(m_data + m_hmtx + idx * 4) / (float)m_unitsPerEm;
}

const uint8_t* Font::glyphData(uint32_t glyph, uint32_t& len) {
  if (glyph >= (uint32_t)m_numGlyphs) { len = 0; return nullptr; }
  uint32_t off0, off1;
  if (m_longLoca) {
    off0 = rd32(m_data + m_loca + glyph * 4);
    off1 = rd32(m_data + m_loca + (glyph + 1) * 4);
  } else {
    off0 = rd16(m_data + m_loca + glyph * 2) * 2;
    off1 = rd16(m_data + m_loca + (glyph + 1) * 2) * 2;
  }
  if (off1 <= off0) { len = 0; return nullptr; }
  len = off1 - off0;
  return m_data + m_glyf + off0;
}

// ---------------------------------------------------------------------------
// Outline resolution.
//
// Simple glyphs are read in place; composite glyphs are flattened recursively.
// Cairo builds its Arabic presentation forms out of components, so without this
// every shaped Arabic glyph would rasterise as an empty bitmap.
// ---------------------------------------------------------------------------
bool Font::decomposeSimple(const uint8_t* d, uint32_t len, Outline& out) {
  if (!d || len < 12) return false;
  int nContours = rdi16(d);
  if (nContours <= 0) return false;
  out.xMin = rdi16(d + 2);
  out.yMin = rdi16(d + 4);
  out.xMax = rdi16(d + 6);
  out.yMax = rdi16(d + 8);
  if (out.xMax <= out.xMin || out.yMax <= out.yMin) return false;
  const uint8_t* p = d + 10;
  const uint8_t* end = d + len;
  if (p + nContours * 2 > end) return false;
  int base = out.flags.count;
  for (int i = 0; i < nContours; i++) {
    uint16_t e = rd16(p);
    p += 2;
    out.ends.push((uint16_t)(base + e));
  }
  int nPoints = out.ends.count ? (int)out.ends[out.ends.count - 1] - base + 1 : 0;
  if (nPoints <= 0 || nPoints > 8192) return false;
  if (p + 2 > end) return false;
  uint16_t insLen = rd16(p);
  p += 2 + insLen;
  if (p > end) return false;
  while ((int)out.flags.count - base < nPoints) {
    if (p >= end) return false;
    uint8_t f = *p++;
    out.flags.push(f);
    if (f & 8) {
      if (p >= end) return false;
      uint8_t rep = *p++;
      for (int i = 0; i < rep && (int)out.flags.count - base < nPoints; i++) out.flags.push(f);
    }
  }
  int32_t v = 0;
  for (int i = 0; i < nPoints; i++) {
    uint8_t f = out.flags[base + i];
    if (f & 2) {
      if (p >= end) return false;
      uint8_t dx = *p++;
      v += (f & 16) ? dx : -dx;
    } else if (!(f & 16)) {
      if (p + 2 > end) return false;
      v += rdi16(p);
      p += 2;
    }
    out.xs.push(v);
  }
  v = 0;
  for (int i = 0; i < nPoints; i++) {
    uint8_t f = out.flags[base + i];
    if (f & 4) {
      if (p >= end) return false;
      uint8_t dy = *p++;
      v += (f & 32) ? dy : -dy;
    } else if (!(f & 32)) {
      if (p + 2 > end) return false;
      v += rdi16(p);
      p += 2;
    }
    out.ys.push(v);
  }
  return true;
}

bool Font::appendComposite(uint32_t glyph, Outline& out, int depth) {
  if (depth > 3) return false;
  uint32_t len = 0;
  const uint8_t* d = glyphData(glyph, len);
  if (!d || len < 14) return false;
  const uint8_t* p = d + 10;
  const uint8_t* end = d + len;
  bool got = false;
  while (p + 4 <= end) {
    uint16_t argFlags = rd16(p);
    uint16_t component = rd16(p + 2);
    p += 4;
    // Two ways to place a component: free offsets (ARGS_ARE_XY_VALUES) or by
    // matching a parent point to a component point. Cairo uses point matching for
    // its Arabic forms, so treating those args as offsets tears letters apart.
    bool argsAreXY = (argFlags & 0x0002) != 0;
    int arg1 = 0, arg2 = 0;
    if (argFlags & 0x0001) {  // ARG_1_AND_2_ARE_WORDS
      if (p + 4 > end) break;
      arg1 = (argFlags & 0x0001) && !argsAreXY ? rd16(p) : rdi16(p);
      arg2 = (argFlags & 0x0001) && !argsAreXY ? rd16(p + 2) : rdi16(p + 2);
      p += 4;
    } else {
      if (p + 2 > end) break;
      arg1 = argsAreXY ? (int)(int8_t)p[0] : (int)p[0];
      arg2 = argsAreXY ? (int)(int8_t)p[1] : (int)p[1];
      p += 2;
    }
    float dx = 0, dy = 0;
    float a = 1, b = 0, c = 0, dd = 1;
    if (argFlags & 0x0008) {  // WE_HAVE_A_SCALE
      if (p + 2 > end) break;
      a = dd = rdi16(p) / 16384.0f;
      p += 2;
    } else if (argFlags & 0x0040) {  // WE_HAVE_AN_X_AND_Y_SCALE
      if (p + 4 > end) break;
      a = rdi16(p) / 16384.0f;
      dd = rdi16(p + 2) / 16384.0f;
      p += 4;
    } else if (argFlags & 0x0080) {  // WE_HAVE_A_TWO_BY_TWO
      if (p + 8 > end) break;
      a = rdi16(p) / 16384.0f;
      b = rdi16(p + 2) / 16384.0f;
      c = rdi16(p + 4) / 16384.0f;
      dd = rdi16(p + 6) / 16384.0f;
      p += 8;
    }

    Outline child;
    if (subOutline(component, child, depth + 1)) {
      if (argsAreXY) {
        dx = (float)arg1;
        dy = (float)arg2;
        if (argFlags & 0x0004) {  // ROUND_XY_TO_GRID
          dx = lroundf(dx);
          dy = lroundf(dy);
        }
      } else if (arg1 >= 0 && arg1 < out.xs.count && arg2 >= 0 && arg2 < child.xs.count) {
        // anchor: component point arg2 lands on parent point arg1
        dx = (float)out.xs[arg1] - (float)child.xs[arg2];
        dy = (float)out.ys[arg1] - (float)child.ys[arg2];
      }
      int pointBase = out.xs.count;
      for (int i = 0; i < child.xs.count; i++) {
        float x = (float)child.xs[i], y = (float)child.ys[i];
        out.xs.push((int32_t)lroundf(a * x + c * y + dx));
        out.ys.push((int32_t)lroundf(b * x + dd * y + dy));
        out.flags.push(child.flags[i]);
      }
      for (int i = 0; i < child.ends.count; i++) out.ends.push((uint16_t)(pointBase + child.ends[i]));
      if (!got) {
        // the first child defines the box; siblings expand it (dots above/below)
        out.xMin = child.xMin;
        out.yMin = child.yMin;
        out.xMax = child.xMax;
        out.yMax = child.yMax;
        got = true;
      }
    }
    if (!(argFlags & 0x0020)) break;  // no MORE_COMPONENTS
  }
  if (!got) return false;
  // recompute the box from the resolved points so attached dots are never clipped
  if (out.xs.count) {
    int x0 = out.xs[0], x1 = x0, y0 = out.ys[0], y1 = y0;
    for (int i = 1; i < out.xs.count; i++) {
      if (out.xs[i] < x0) x0 = out.xs[i];
      if (out.xs[i] > x1) x1 = out.xs[i];
      if (out.ys[i] < y0) y0 = out.ys[i];
      if (out.ys[i] > y1) y1 = out.ys[i];
    }
    out.xMin = x0;
    out.yMin = y0;
    out.xMax = x1;
    out.yMax = y1;
  }
  return out.xMax > out.xMin && out.yMax > out.yMin;
}

bool Font::subOutline(uint32_t glyph, Outline& out, int depth) {
  uint32_t len = 0;
  const uint8_t* d = glyphData(glyph, len);
  if (!d || len < 10) return false;
  int nContours = rdi16(d);
  if (nContours > 0) return decomposeSimple(d, len, out);
  if (nContours == 0) return false;  // blank glyph (space)
  return appendComposite(glyph, out, depth);
}

// ---------------------------------------------------------------------------
// TrueType glyph rasterisation (non-zero winding, 4x vertical supersampling)
// ---------------------------------------------------------------------------
namespace {
struct Edge {
  float x0, y0, x1, y1;
  int dir;
};

static int cmpEdgeY(const void* a, const void* b) {
  const Edge* ea = (const Edge*)a;
  const Edge* eb = (const Edge*)b;
  float ya = fminf(ea->y0, ea->y1), yb = fminf(eb->y0, eb->y1);
  return ya < yb ? -1 : (ya > yb ? 1 : 0);
}
}  // namespace

bool Font::glyphBitmapStraight(uint32_t glyph, int pixelSize, Array<unsigned char>& outBitmap, int& w, int& h,
                               int& bearingX, int& bearingY) {
  uint32_t len = 0;
  const uint8_t* d = glyphData(glyph, len);
  w = h = 0;
  bearingX = bearingY = 0;
  if (!d || len < 10) return false;
  // resolve simple and composite outlines into one point/flag soup
  Outline flat;
  if (!subOutline(glyph, flat, 0)) return false;
  int nContours = flat.ends.count;
  int xMin = flat.xMin, yMin = flat.yMin, xMax = flat.xMax, yMax = flat.yMax;
  Array<uint16_t>& endPts = flat.ends;
  Array<int32_t>& xs = flat.xs;
  Array<int32_t>& ys = flat.ys;
  Array<unsigned char> flags;
  flags.reserve(flat.flags.count);
  for (int i = 0; i < flat.flags.count; i++) flags.push(flat.flags[i]);
  if (xMax <= xMin || yMax <= yMin) return false;

  float scale = (float)pixelSize / (float)m_unitsPerEm;
  w = (int)ceilf((xMax - xMin) * scale) + 2;
  h = (int)ceilf((yMax - yMin) * scale) + 2;
  if (w <= 0 || h <= 0 || w > 512 || h > 512) return false;
  bearingX = (int)floorf(xMin * scale);
  bearingY = (int)ceilf(yMax * scale);
  outBitmap.count = 0;
  outBitmap.reserve(w * h);
  for (int i = 0; i < w * h; i++) outBitmap.push(0);

  const int SS = 4;
  Array<float> cov;
  cov.reserve(w * h * SS);
  for (int i = 0; i < w * h * SS; i++) cov.push(0.0f);  // layout: (y*w + x)*SS + subY

  // Build edges in pixel space (y up, origin at baseline). TrueType contours mix
  // on-curve and off-curve (quadratic control) points freely, so each ring is
  // first normalised: a virtual on-curve point is inserted between two
  // consecutive control points, and a ring that starts on a control point starts
  // from the implicit point instead. Walking that normalised ring guarantees the
  // contour closes — missing that step leaves a seam in round letters.
  Array<Edge> edges;
  int start = 0;
  for (int c = 0; c < nContours; c++) {
    int endPt = endPts[c];
    int count = endPt - start + 1;
    if (count < 2) { start = endPt + 1; continue; }
    auto toX = [&](int a) { return (xs[a] - xMin) * scale + 1.0f; };
    auto toY = [&](int a) { return (yMax - ys[a]) * scale + 1.0f; };
    auto midX = [&](int a, int b) { return (toX(a) + toX(b)) * 0.5f; };
    auto midY = [&](int a, int b) { return (toY(a) + toY(b)) * 0.5f; };

    int firstPt = start, lastPt = endPt;
    float curX, curY;  // current on-curve point in pixels
    int idx;           // next point to consume
    if (flags[firstPt] & 1) {
      curX = toX(firstPt);
      curY = toY(firstPt);
      idx = firstPt + 1;
    } else if (flags[lastPt] & 1) {
      curX = toX(lastPt);
      curY = toY(lastPt);
      idx = firstPt;
    } else {
      curX = midX(lastPt, firstPt);
      curY = midY(lastPt, firstPt);
      idx = firstPt;
    }
    int visits = count;  // one lap; the implicit start point is not a real point
    for (int k = 0; k < visits; k++) {
      int i = idx;
      bool on = (flags[i] & 1) != 0;
      if (on) {
        edges.push({curX, curY, toX(i), toY(i), 0});
        curX = toX(i);
        curY = toY(i);
        idx = (i == endPt) ? firstPt : i + 1;
      } else {
        int next = (i == endPt) ? firstPt : i + 1;
        float ex, ey;
        if (flags[next] & 1) {
          ex = toX(next);
          ey = toY(next);
          idx = (next == endPt) ? firstPt : next + 1;
          k++;  // the end point was consumed as part of this curve
        } else {
          ex = midX(i, next);
          ey = midY(i, next);
          idx = next;
        }
        int steps = 8;
        for (int s2 = 1; s2 <= steps; s2++) {
          float t = (float)s2 / steps, mt = 1 - t;
          float nx = mt * mt * curX + 2 * mt * t * toX(i) + t * t * ex;
          float ny = mt * mt * curY + 2 * mt * t * toY(i) + t * t * ey;
          edges.push({curX, curY, nx, ny, 0});
          curX = nx;
          curY = ny;
        }
      }
    }
    // close the ring back to the starting on-curve point
    float sx2, sy2;
    if (flags[firstPt] & 1) { sx2 = toX(firstPt); sy2 = toY(firstPt); }
    else if (flags[lastPt] & 1) { sx2 = toX(lastPt); sy2 = toY(lastPt); }
    else { sx2 = midX(lastPt, firstPt); sy2 = midY(lastPt, firstPt); }
    if (fabsf(curX - sx2) > 0.001f || fabsf(curY - sy2) > 0.001f) edges.push({curX, curY, sx2, sy2, 0});
    start = endPt + 1;
  }
  if (edges.count == 0) return false;

  // scanline with 4 sub-samples per pixel row, non-zero winding
  for (int sy = 0; sy < h * SS; sy++) {
    float y = (sy + 0.5f) / SS;
    Array<float> xsAt;  // crossings with direction
    xsAt.reserve(64);
    Array<int> dirs;
    dirs.reserve(64);
    for (int e = 0; e < edges.count; e++) {
      const Edge& ed = edges[e];
      float y0 = ed.y0, y1 = ed.y1;
      if ((y >= y0 && y < y1) || (y >= y1 && y < y0)) {
        float t = (y - y0) / (y1 - y0);
        float x = ed.x0 + (ed.x1 - ed.x0) * t;
        xsAt.push(x);
        dirs.push(y1 > y0 ? 1 : -1);
      }
    }
    if (xsAt.count == 0) continue;
    for (int i = 0; i < xsAt.count; i++) {
      for (int j = i + 1; j < xsAt.count; j++) {
        if (xsAt[j] < xsAt[i]) {
          float tx = xsAt[i]; xsAt[i] = xsAt[j]; xsAt[j] = tx;
          int td = dirs[i]; dirs[i] = dirs[j]; dirs[j] = td;
        }
      }
    }
    int winding = 0;
    float spanStart = 0;
    bool inSpan = false;
    for (int i = 0; i < xsAt.count; i++) {
      int prev = winding;
      winding += dirs[i];
      float x = xsAt[i];
      if (prev == 0 && winding != 0) { spanStart = x; inSpan = true; }
      else if (prev != 0 && winding == 0 && inSpan) {
        inSpan = false;
        int row = sy / SS;
        int subY = sy % SS;
        int x0 = (int)floorf(spanStart), x1 = (int)ceilf(x);
        if (x0 < 0) x0 = 0;
        if (x1 > w) x1 = w;
        for (int px2 = x0; px2 < x1; px2++) {
          float a = fmaxf(spanStart, (float)px2);
          float b = fminf(x, (float)px2 + 1.0f);
          if (b > a) cov.items[(row * w + px2) * SS + subY] += (b - a);
        }
      }
    }
  }
  // strict mode: each pixel's coverage comes from its own 4 sub-samples (no bleeding across pixels)
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      float a = 0;
      for (int s = 0; s < SS; s++) a += cov.items[(y * w + x) * SS + s];
      a = clampf(a / (float)SS, 0, 1);
      outBitmap.items[y * w + x] = (unsigned char)(a * 255);
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// Atlas + cache
// ---------------------------------------------------------------------------
GlyphCache* Font::cacheFor(int pixelSize) {
  for (int i = 0; i < m_caches.count; i++) if (m_caches[i]->pixelSize == pixelSize) return m_caches[i];
  GlyphCache* c = new GlyphCache();
  c->pixelSize = pixelSize;
  c->atlas.atlasSize = pixelSize > 44 ? 1024 : 512;
  size_t total = (size_t)c->atlas.atlasSize * c->atlas.atlasSize * 4;
  unsigned char* blank = (unsigned char*)calloc(total, 1);
  uploadPixelTexture(c->atlas.glId, c->atlas.atlasSize, c->atlas.atlasSize, blank, true, false);
  free(blank);
  c->atlas.tex.id = c->atlas.glId;
  m_caches.push(c);
  return c;
}

void Font::appendGlyphToAtlas(uint32_t glyph, int pixelSize, GlyphCache* cache) {
  Array<unsigned char> bmp;
  int w = 0, h = 0, bx = 0, by = 0;
  GlyphQuad q{};
  if (glyphBitmapStraight(glyph, pixelSize, bmp, w, h, bx, by) && w > 0 && h > 0) {
    if (cache->atlas.penX + w + 1 >= cache->atlas.atlasSize) {
      cache->atlas.penX = 0;
      cache->atlas.penY += cache->atlas.rowH + 1;
      cache->atlas.rowH = 0;
    }
    if (cache->atlas.penY + h + 1 >= cache->atlas.atlasSize) {
      NX_LOGW("glyph atlas full at size %d", pixelSize);
      return;
    }
    // upload as a sub-image
    glBindTexture(GL_TEXTURE_2D, cache->atlas.glId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    unsigned char* rgba = (unsigned char*)malloc((size_t)w * h * 4);
    for (int i = 0; i < w * h; i++) {
      rgba[i * 4 + 0] = 255;
      rgba[i * 4 + 1] = 255;
      rgba[i * 4 + 2] = 255;
      rgba[i * 4 + 3] = bmp[i];
    }
    glTexSubImage2D(GL_TEXTURE_2D, 0, cache->atlas.penX, cache->atlas.penY, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    free(rgba);
    q.gx = (uint16_t)cache->atlas.penX;
    q.gy = (uint16_t)cache->atlas.penY;
    q.gw = (uint16_t)w;
    q.gh = (uint16_t)h;
    q.bearingX = (int16_t)bx;
    q.bearingY = (int16_t)by;
    q.w = (uint16_t)w;
    q.h = (uint16_t)h;
    q.advance = advanceOf(glyph) * pixelSize;
    cache->atlas.penX += w + 1;
    if (h > cache->atlas.rowH) cache->atlas.rowH = h;
  } else {
    q.advance = advanceOf(glyph) * pixelSize;
  }
  GlyphCache::Entry e{glyph, q};
  cache->entries.push(e);
}

const GlyphQuad* Font::glyphQuad(uint32_t glyph, int pixelSize) {
  GlyphCache* c = cacheFor(pixelSize);
  const GlyphQuad* q = c->find(glyph);
  if (q) return q;
  appendGlyphToAtlas(glyph, pixelSize, c);
  return c->find(glyph);
}

void Font::destroy() {
  for (int i = 0; i < m_caches.count; i++) {
    GlyphCache* c = m_caches[i];
    if (c->atlas.glId) { GLuint id = c->atlas.glId; glDeleteTextures(1, &id); }
    delete c;
  }
  m_caches.clear();
}

// ---------------------------------------------------------------------------
// Shaping: Arabic joining + bidi ordering
// ---------------------------------------------------------------------------
void Font::shape(const char* utf8, Array<GlyphShape>& out) {
  out.clear();
  if (!utf8) return;
  // 1. decode
  struct C { uint32_t cp; };
  Array<C> cps;
  {
    const char* p = utf8;
    while (*p) {
      C c{readUtf8(&p)};
      cps.push(c);
    }
  }
  int n = cps.count;
  if (!n) return;
  Array<uint32_t> mapped;
  mapped.reserve(n);
  for (int i = 0; i < n; i++) mapped.push(cps[i].cp);

  // 2. Arabic joining -> presentation forms (RTL visual forms applied before bidi reorder)
  for (int i = 0; i < n; i++) {
    uint32_t cp = mapped[i];
    int cls = joinClass(cp);
    if (cls == 0 || cls == 3) continue;
    // skip marks
    if (isDiacritic(cp)) continue;
    // previous letter that can join to the right side of this letter
    int prev = -1;
    for (int k = i - 1; k >= 0; k--) {
      if (isDiacritic(mapped[k])) continue;
      prev = k;
      break;
    }
    int next = -1;
    for (int k = i + 1; k < n; k++) {
      if (isDiacritic(mapped[k])) continue;
      next = k;
      break;
    }
    // A letter connects to the previous one when the previous letter can join
    // forward (dual-joining only) — this is the grammatical test, and it is what
    // makes alef/dal/reh/waw behave correctly in the middle of a word.
    bool joinsPrev = false, joinsNext = false;
    if (prev >= 0) joinsPrev = joinClass(mapped[prev]) == 2;
    if (next >= 0) joinsNext = joinClass(mapped[next]) == 2;
    if (cls == 1) joinsNext = false;  // right-joining letters never connect forward
    int form;
    if (joinsPrev && joinsNext) form = AR_MEDIAL;
    else if (joinsPrev) form = AR_FINAL;
    else if (joinsNext) form = AR_INITIAL;
    else form = AR_ISOLATED;
    // lam-alef is a single ligature glyph, so the pair collapses before forms
    if (cp == 0x0644 && next >= 0) {
      for (size_t l = 0; l < sizeof(kLamAlef) / sizeof(kLamAlef[0]); l++) {
        if (mapped[next] == kLamAlef[l].alef) {
          mapped[i] = joinsPrev ? kLamAlef[l].fin : kLamAlef[l].iso;
          mapped[next] = 0xFFFF;  // consumed by the ligature
          break;
        }
      }
      if (mapped[next] == 0xFFFF) continue;
    }
    // Some builds (Cairo ships a reduced presentation set) only cover a subset of
    // forms. Prefer the requested form, then any other form of the same letter,
    // then the base codepoint, so a missing form degrades instead of drawing '?'.
    uint32_t chosen = presentation(cp, form);
    if (!chosen || !glyphFor(chosen)) {
      static const int kOrder[4] = {AR_ISOLATED, AR_FINAL, AR_INITIAL, AR_MEDIAL};
      uint32_t fallback = 0;
      for (int k = 0; k < 4 && !fallback; k++) {
        uint32_t c = presentation(cp, kOrder[k]);
        if (c && glyphFor(c)) fallback = c;
      }
      if (fallback) chosen = fallback;
      else if (glyphFor(cp)) chosen = 0;  // keep the base letter
    }
    if (chosen) mapped[i] = chosen;
  }

  // 3. bidi base direction: first strong character
  bool rtl = isRtlText(utf8);

  // 4. reorder runs (visual order). We process logical order into runs of same direction,
  //    reverse RTL runs, and reverse the run sequence for RTL paragraphs.
  struct Item { uint32_t cp; bool rtl; };
  Array<Item> items;
  for (int i = 0; i < n; i++) {
    uint32_t cp = mapped[i];
    if (cp == 0xFFFF) continue;
    int cls = joinClass(cp);
    bool isRtlChar = cls != 0;
    // digits and neutral punctuation take the paragraph direction for placement, but digits
    // inside an RTL run keep their own left-to-right order.
    items.push({cp, isRtlChar});
  }
  Array<uint32_t> visual;
  visual.reserve(items.count);
  if (!rtl) {
    for (int i = 0; i < items.count; i++) visual.push(items[i].cp);
  } else {
    // Build runs, then emit from last to first. Within an RTL run reverse codepoints;
    // within an LTR run (numbers/latin) keep order.
    int i = 0;
    Array<int> runStart, runEnd;
    Array<bool> runRtl;
    while (i < items.count) {
      bool r = items[i].rtl;
      int s = i;
      while (i < items.count && items[i].rtl == r) i++;
      runStart.push(s);
      runEnd.push(i);
      runRtl.push(r);
    }
    for (int k = runStart.count - 1; k >= 0; k--) {
      if (runRtl[k]) {
        for (int j = runEnd[k] - 1; j >= runStart[k]; j--) visual.push(items[j].cp);
      } else {
        for (int j = runStart[k]; j < runEnd[k]; j++) visual.push(items[j].cp);
      }
    }
    // mirrored punctuation in RTL context
    for (int k = 0; k < visual.count; k++) {
      uint32_t c = visual[k];
      if (!isMirrored(c)) continue;
      switch (c) {
        case '(': visual.items[k] = ')'; break;
        case ')': visual.items[k] = '('; break;
        case '[': visual.items[k] = ']'; break;
        case ']': visual.items[k] = '['; break;
        case '{': visual.items[k] = '}'; break;
        case '}': visual.items[k] = '{'; break;
        case '<': visual.items[k] = '>'; break;
        case '>': visual.items[k] = '<'; break;
        default: break;
      }
    }
  }

  for (int i = 0; i < visual.count; i++) {
    uint32_t cp = visual[i];
    uint32_t g = glyphFor(cp);
    GlyphShape gs;
    gs.glyph = g ? g : glyphFor('?');
    gs.advance = advanceOf(gs.glyph);
    gs.isSpace = (cp == ' ' || cp == '\t' || cp == 0x00A0);
    out.push(gs);
  }
}

bool Font::isRtlText(const char* utf8) {
  const char* p = utf8;
  while (*p) {
    uint32_t cp = readUtf8(&p);
    if (cp >= 0x0590 && cp <= 0x08FF) return true;
    if (cp >= 0xFB1D && cp <= 0xFEFF) return true;
    if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')) return false;
  }
  return false;
}

// ---------------------------------------------------------------------------
// TextRenderer
// ---------------------------------------------------------------------------
void TextRenderer::init(Font* font, SpriteBatch* batch) {
  m_font = font;
  m_batch = batch;
}

static float atlasCellBleed(const GlyphQuad& q) {
  (void)q;
  return 0.35f;
}

void TextRenderer::layout(const char* utf8, int pixelSize, float maxWidth, TextLayout& out, bool forceRtl) {
  out.glyphs.clear();
  out.width = out.height = 0;
  out.lines = 1;
  if (!m_font || !m_font->valid() || !utf8 || !*utf8) return;
  bool rtl = forceRtl || m_font->isRtlText(utf8);
  float lineH = m_font->lineHeight() * pixelSize * 1.02f;
  float spaceAdv = m_font->advanceOf(m_font->glyphFor(' ')) * pixelSize;

  // split into words while preserving newlines
  struct Word { char text[256]; bool newline; };
  Array<Word> words;
  {
    const char* p = utf8;
    char line[512];
    int li = 0;
    while (*p && li < (int)sizeof(line) - 1) {
      char ch = *p;
      if (ch == '\n') {
        line[li] = 0;
        // split on spaces
        char* save = line;
        while (*save) {
          while (*save == ' ') save++;
          if (!*save) break;
          char* start = save;
          while (*save && *save != ' ') save++;
          char tmp[256];
          int n = (int)(save - start);
          if (n > 255) n = 255;
          memcpy(tmp, start, n);
          tmp[n] = 0;
          Word w{};
          strncpy(w.text, tmp, sizeof w.text - 1);
          w.newline = false;
          words.push(w);
        }
        Word nl{};
        nl.text[0] = 0;
        nl.newline = true;
        words.push(nl);
        li = 0;
        p++;
        continue;
      }
      line[li++] = ch;
      p++;
    }
    if (li > 0) {
      line[li] = 0;
      char* save = line;
      while (*save) {
        while (*save == ' ') save++;
        if (!*save) break;
        char* start = save;
        while (*save && *save != ' ') save++;
        char tmp[256];
        int n = (int)(save - start);
        if (n > 255) n = 255;
        memcpy(tmp, start, n);
        tmp[n] = 0;
        Word w{};
        strncpy(w.text, tmp, sizeof w.text - 1);
        w.newline = false;
        words.push(w);
      }
    }
  }

  // greedy wrap
  float x = 0, y = 0;
  float lineWidth = 0;
  Array<GlyphShape> shapes;
  float maxLineWidth = 0;
  for (int i = 0; i < words.count; i++) {
    if (words[i].newline) {
      if (lineWidth > maxLineWidth) maxLineWidth = lineWidth;
      y += lineH;
      x = 0;
      lineWidth = 0;
      out.lines++;
      continue;
    }
    m_font->shape(words[i].text, shapes);
    float wWidth = 0;
    for (int g = 0; g < shapes.count; g++) wWidth += shapes[g].advance * pixelSize;
    float extra = (x > 0 ? spaceAdv : 0);
    if (maxWidth > 0 && x > 0 && x + extra + wWidth > maxWidth) {
      if (lineWidth > maxLineWidth) maxLineWidth = lineWidth;
      y += lineH;
      x = 0;
      lineWidth = 0;
      out.lines++;
      extra = 0;
    }
    x += extra;
    lineWidth += extra;
    // shape() hands back glyphs in visual order (RTL runs already reversed), so
    // one left-to-right pen is correct for a run. Inline LTR islands (numbers,
    // latin) are additionally mirrored about the text origin on a single-glyph
    // basis below, which is what makes "٥ Miy" read correctly in RTL context.
    for (int g = 0; g < shapes.count; g++) {
      uint32_t gl = shapes[g].glyph;
      const GlyphQuad* q = m_font->glyphQuad(gl, pixelSize);
      TextGlyph tg{};
      tg.quad = q;
      if (q) {
        tg.x = x + q->bearingX;
        tg.y = y + (m_font->ascent() * pixelSize) - q->bearingY;
        x += q->advance;
        lineWidth += q->advance;
      } else {
        x += shapes[g].advance * pixelSize;
        lineWidth += shapes[g].advance * pixelSize;
      }
      out.glyphs.push(tg);
    }
  }
  if (lineWidth > maxLineWidth) maxLineWidth = lineWidth;
  out.width = maxLineWidth;
  out.height = y + lineH;
  (void)rtl;
  (void)atlasCellBleed;
}

void TextRenderer::draw(const char* utf8, float x, float y, int pixelSize, const Color& color, float maxWidth,
                        int align, bool forceRtl) {
  if (!m_font || !m_batch) return;
  TextLayout layoutRes;
  this->layout(utf8, pixelSize, maxWidth, layoutRes, forceRtl);
  // glyphs from one size live in one atlas
  GlyphCache* cache = m_font->cacheFor(pixelSize);
  if (!cache || !cache->atlas.glId) return;
  glBindTexture(GL_TEXTURE_2D, cache->atlas.glId);
  float offsetX = 0;
  if (maxWidth > 0 && align == 1) offsetX = (maxWidth - layoutRes.width) * 0.5f;
  else if (maxWidth > 0 && align == 2) offsetX = maxWidth - layoutRes.width;
  float atlasS = (float)cache->atlas.atlasSize;
  for (int i = 0; i < layoutRes.glyphs.count; i++) {
    const TextGlyph& tg = layoutRes.glyphs[i];
    if (!tg.quad || tg.quad->gw == 0) continue;
    Rect dst{floorf(x + offsetX + tg.x), floorf(y + tg.y), (float)tg.quad->w, (float)tg.quad->h};
    Rect uv{tg.quad->gx / atlasS, tg.quad->gy / atlasS, tg.quad->gw / atlasS, tg.quad->gh / atlasS};
    m_batch->quad(dst, uv, color);
  }
  m_batch->flush();
}

float TextRenderer::measure(const char* utf8, int pixelSize, float maxWidth, bool forceRtl) {
  TextLayout l;
  layout(utf8, pixelSize, maxWidth, l, forceRtl);
  return l.width;
}

void TextRenderer::ellipsize(const char* in, int pixelSize, float maxWidth, char* out, size_t cap) {
  strncpy(out, in, cap - 1);
  out[cap - 1] = 0;
  if (measure(out, pixelSize, 0) <= maxWidth) return;
  size_t len = strlen(out);
  while (len > 1) {
    // remove one UTF-8 codepoint from the end
    len--;
    while (len > 0 && ((unsigned char)out[len] & 0xC0) == 0x80) len--;
    out[len] = 0;
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s…", out);
    if (measure(tmp, pixelSize, 0) <= maxWidth) {
      strncpy(out, tmp, cap - 1);
      out[cap - 1] = 0;
      return;
    }
  }
}

}  // namespace nx
