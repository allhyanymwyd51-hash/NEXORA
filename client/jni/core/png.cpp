// PNG (ISO/IEC 15948) reader for the texture pipeline. No dependencies beyond inflate.
#include "png.h"

#include "inflate.h"
#include <stdlib.h>
#include <string.h>

namespace nx {
namespace {

unsigned rd32(const unsigned char* p) {
  return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | (unsigned)p[3];
}

int paeth(int a, int b, int c) {
  int p = a + b - c;
  int pa = p > a ? p - a : a - p;
  int pb = p > b ? p - b : b - p;
  int pc = p > c ? p - c : c - p;
  if (pa <= pb && pa <= pc) return a;
  if (pb <= pc) return b;
  return c;
}

}  // namespace

bool decodePng(const unsigned char* data, size_t size, ImageData& out) {
  static const unsigned char kSig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  if (!data || size < 8) return false;
  for (int i = 0; i < 8; i++) if (data[i] != kSig[i]) return false;

  int width = 0, height = 0, bitDepth = 0, colorType = 0, interlace = 0;
  unsigned char* idat = nullptr;
  size_t idatSize = 0, idatCap = 0;
  unsigned char palette[256 * 3];
  unsigned char trns[256];
  int paletteCount = 0, trnsCount = 0;

  size_t pos = 8;
  while (pos + 8 <= size) {
    unsigned len = rd32(data + pos);
    const unsigned char* type = data + pos + 4;
    const unsigned char* chunk = data + pos + 8;
    if (pos + 12 + (size_t)len > size) break;      // truncated file
    if (type[0] == 'I' && type[1] == 'H' && type[2] == 'D' && type[3] == 'R') {
      if (len < 13) return false;
      width = (int)rd32(chunk);
      height = (int)rd32(chunk + 4);
      bitDepth = chunk[8];
      colorType = chunk[9];
      interlace = chunk[12];
      if (width <= 0 || height <= 0 || width > 8192 || height > 8192) return false;
      if (interlace != 0) return false;           // Adam7 not supported (art is authored flat)
      if (bitDepth != 8 && bitDepth != 16) return false;
      if (colorType != 0 && colorType != 2 && colorType != 3 && colorType != 4 && colorType != 6) return false;
    } else if (type[0] == 'P' && type[1] == 'L' && type[2] == 'T' && type[3] == 'E') {
      paletteCount = (int)(len / 3);
      if (paletteCount > 256) paletteCount = 256;
      for (int i = 0; i < paletteCount * 3; i++) palette[i] = chunk[i];
    } else if (type[0] == 't' && type[1] == 'R' && type[2] == 'N' && type[3] == 'S') {
      trnsCount = (int)len > 256 ? 256 : (int)len;
      for (int i = 0; i < trnsCount; i++) trns[i] = chunk[i];
    } else if (type[0] == 'I' && type[1] == 'D' && type[2] == 'A' && type[3] == 'T') {
      if (idatSize + len > idatCap) {
        size_t cap = idatCap ? idatCap : 65536;
        while (cap < idatSize + len) cap *= 2;
        unsigned char* grown = (unsigned char*)realloc(idat, cap);
        if (!grown) { if (idat) free(idat); return false; }
        idat = grown;
        idatCap = cap;
      }
      for (unsigned i = 0; i < len; i++) idat[idatSize++] = chunk[i];
    } else if (type[0] == 'I' && type[1] == 'E' && type[2] == 'N' && type[3] == 'D') {
      break;
    }
    pos += 12 + (size_t)len;
  }
  if (!width || !idat) { if (idat) free(idat); return false; }

  int channels = colorType == 0 ? 1 : colorType == 2 ? 3 : colorType == 3 ? 1 : colorType == 4 ? 2 : 4;
  int bytesPerSample = bitDepth == 16 ? 2 : 1;
  size_t rowBytes = (size_t)width * channels * bytesPerSample;
  size_t rawSize = (rowBytes + 1) * (size_t)height;
  unsigned char* raw = (unsigned char*)malloc(rawSize);
  if (!raw) { free(idat); return false; }
  size_t written = 0;
  bool ok = inflateZlib(idat, idatSize, raw, rawSize, &written);
  free(idat);
  if (!ok || written < rawSize) { free(raw); return false; }

  // Unfilter into a contiguous, tightly-packed image.
  unsigned char* pixels = (unsigned char*)malloc((size_t)width * (size_t)height * 4);
  if (!pixels) { free(raw); return false; }
  unsigned char* prev = nullptr;
  for (int y = 0; y < height; y++) {
    unsigned char filter = raw[(size_t)y * (rowBytes + 1)];
    unsigned char* line = raw + (size_t)y * (rowBytes + 1) + 1;
    for (size_t x = 0; x < rowBytes; x++) {
      int a = x >= (size_t)(channels * bytesPerSample) ? line[x - channels * bytesPerSample] : 0;
      int b = prev ? prev[x] : 0;
      int c = (prev && x >= (size_t)(channels * bytesPerSample)) ? prev[x - channels * bytesPerSample] : 0;
      switch (filter) {
        case 0: break;
        case 1: line[x] = (unsigned char)(line[x] + a); break;
        case 2: line[x] = (unsigned char)(line[x] + b); break;
        case 3: line[x] = (unsigned char)(line[x] + ((a + b) >> 1)); break;
        case 4: line[x] = (unsigned char)(line[x] + paeth(a, b, c)); break;
        default: free(pixels); free(raw); return false;
      }
    }
    prev = line;
    // expand to RGBA
    for (int x = 0; x < width; x++) {
      unsigned char* dst = pixels + ((size_t)y * width + x) * 4;
      if (bitDepth == 16) {
        // keep the high byte: plenty for textures and halves memory
        const unsigned char* src = line + (size_t)x * channels * 2;
        if (colorType == 0) { dst[0] = dst[1] = dst[2] = src[0]; dst[3] = 255; }
        else if (colorType == 2) { dst[0] = src[0]; dst[1] = src[2]; dst[2] = src[4]; dst[3] = 255; }
        else if (colorType == 3) { int i = src[0]; dst[0] = palette[i * 3]; dst[1] = palette[i * 3 + 1]; dst[2] = palette[i * 3 + 2]; dst[3] = i < trnsCount ? trns[i] : 255; }
        else if (colorType == 4) { dst[0] = dst[1] = dst[2] = src[0]; dst[3] = src[2]; }
        else { dst[0] = src[0]; dst[1] = src[2]; dst[2] = src[4]; dst[3] = src[6]; }
      } else {
        const unsigned char* src = line + (size_t)x * channels;
        if (colorType == 0) { dst[0] = dst[1] = dst[2] = src[0]; dst[3] = 255; }
        else if (colorType == 2) { dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = 255; }
        else if (colorType == 3) {
          int i = src[0];
          if (i >= paletteCount) { dst[0] = dst[1] = dst[2] = 0; dst[3] = 0; }
          else {
            dst[0] = palette[i * 3]; dst[1] = palette[i * 3 + 1]; dst[2] = palette[i * 3 + 2];
            dst[3] = i < trnsCount ? trns[i] : 255;
          }
        } else if (colorType == 4) { dst[0] = dst[1] = dst[2] = src[0]; dst[3] = src[1]; }
        else { dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3]; }
      }
    }
  }
  free(raw);
  out.rgba = pixels;
  out.width = width;
  out.height = height;
  return true;
}

}  // namespace nx
