// Minimal but complete DEFLATE (RFC 1951) + zlib (RFC 1950) decoder.
// The engine ships its own inflate so textures can be real PNG art instead of
// runtime noise: no zlib, no libpng, no NDK.
#pragma once
#include <stddef.h>

namespace nx {

// Raw DEFLATE stream -> caller-owned buffer. Returns false on malformed input.
// `outCapacity` is the exact destination size (PNG IDAT sizes are known from the header).
bool inflateRaw(const unsigned char* in, size_t inSize, unsigned char* out, size_t outCapacity,
                size_t* outWritten = nullptr);

// zlib wrapper: 2-byte header + deflate payload + Adler-32 trailer.
bool inflateZlib(const unsigned char* in, size_t inSize, unsigned char* out, size_t outCapacity,
                 size_t* outWritten = nullptr);

}  // namespace nx
