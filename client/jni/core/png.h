// PNG decoding: reads 8-bit and 16-bit RGBA/RGB/palette/greyscale PNGs, all five filter
// types, non-interlaced. Paired with inflate.cpp this is a complete decoder for the art
// pipeline's output (and any pngquant/oxipng pass) without libpng or zlib.
#pragma once
#include <stddef.h>
#include <stdlib.h>

namespace nx {

struct ImageData {
  unsigned char* rgba = nullptr;   // 4 bytes per pixel, sRGB, straight alpha
  int width = 0;
  int height = 0;
  void release() { if (rgba) { ::free(rgba); rgba = nullptr; } width = height = 0; }
};

// Decodes a PNG in memory. Returns false for anything malformed or unsupported
// (interlaced files, bit depths below 8) instead of guessing.
bool decodePng(const unsigned char* data, size_t size, ImageData& out);

}  // namespace nx
