// DEFLATE decoder — written from RFC 1951. Small, allocation-free, no dependencies.
//
// The bit reader pulls LSB-first as the spec requires; length/distance decoding uses the
// standard base/extra-bit tables. Every table below is the one in the RFC, not an
// approximation, because a texture that decodes wrong is worse than one that fails.
#include "inflate.h"

namespace nx {
namespace {

struct Bits {
  const unsigned char* data;
  size_t size;
  size_t pos = 0;      // byte position
  unsigned bitBuf = 0;
  int bitCount = 0;
  bool overrun = false;

  Bits(const unsigned char* d, size_t n) : data(d), size(n) {}

  unsigned take(int need) {
    while (bitCount < need) {
      if (pos >= size) { overrun = true; return 0; }
      bitBuf |= (unsigned)data[pos++] << bitCount;
      bitCount += 8;
    }
    unsigned v = need ? (bitBuf & ((1u << need) - 1)) : 0;
    bitBuf >>= need;
    bitCount -= need;
    return v;
  }
  void alignToByte() { bitBuf = 0; bitCount = 0; }
  unsigned char byte() {
    if (pos >= size) { overrun = true; return 0; }
    return data[pos++];
  }
};

// Canonical Huffman decoding table (count/symbol form, as in the RFC's own example).
struct Huffman {
  short count[16];
  short symbol[288];

  bool build(const unsigned char* lengths, int n) {
    for (int i = 0; i < 16; i++) count[i] = 0;
    for (int i = 0; i < n; i++) count[lengths[i]]++;
    if (count[0] == n) return true;              // nothing coded (valid empty tree)
    int left = 1;
    for (int len = 1; len < 16; len++) {
      left <<= 1;
      left -= count[len];
      if (left < 0) return false;                // over-subscribed
    }
    short offs[16];
    offs[1] = 0;
    for (int len = 1; len < 15; len++) offs[len + 1] = (short)(offs[len] + count[len]);
    for (int i = 0; i < n; i++) {
      if (lengths[i]) symbol[offs[lengths[i]]++] = (short)i;
    }
    return true;
  }

  int decode(Bits& bits) const {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
      code |= (int)bits.take(1);
      int c = count[len];
      if (code - first < c) return symbol[index + (code - first)];
      index += c;
      first += c;
      first <<= 1;
      code <<= 1;
    }
    return -1;
  }
};

const unsigned short kLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const unsigned char kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const unsigned short kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                     257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                                     8193, 12289, 16385, 24577};
const unsigned char kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                                      7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool codes(Bits& bits, unsigned char* out, size_t cap, size_t& written, const Huffman& lit,
           const Huffman& dist) {
  for (;;) {
    if (bits.overrun) return false;
    int sym = lit.decode(bits);
    if (sym < 0) return false;
    if (sym < 256) {
      if (written >= cap) return false;
      out[written++] = (unsigned char)sym;
    } else if (sym == 256) {
      return true;
    } else {
      sym -= 257;
      if (sym >= 29) return false;
      int length = kLenBase[sym] + (int)bits.take(kLenExtra[sym]);
      int dsym = dist.decode(bits);
      if (dsym < 0 || dsym >= 30) return false;
      int distance = kDistBase[dsym] + (int)bits.take(kDistExtra[dsym]);
      if ((size_t)distance > written) return false;   // reference before start
      if (written + (size_t)length > cap) return false;
      size_t from = written - (size_t)distance;
      for (int i = 0; i < length; i++) out[written++] = out[from + (size_t)i];
    }
  }
}

bool stored(Bits& bits, unsigned char* out, size_t cap, size_t& written) {
  bits.alignToByte();
  unsigned len = bits.byte();
  len |= (unsigned)bits.byte() << 8;
  unsigned nlen = bits.byte();
  nlen |= (unsigned)bits.byte() << 8;
  if ((len ^ 0xFFFFu) != nlen || bits.overrun) return false;
  if (written + len > cap) return false;
  for (unsigned i = 0; i < len; i++) out[written++] = bits.byte();
  return !bits.overrun;
}

}  // namespace

bool inflateRaw(const unsigned char* in, size_t inSize, unsigned char* out, size_t outCapacity,
                size_t* outWritten) {
  if (!in || !out) return false;
  Bits bits(in, inSize);
  size_t written = 0;
  int final = 0;
  while (!final) {
    final = (int)bits.take(1);
    int type = (int)bits.take(2);
    if (bits.overrun) return false;
    if (type == 0) {
      if (!stored(bits, out, outCapacity, written)) return false;
    } else if (type == 1) {
      // fixed Huffman tables (RFC 1951 §3.2.6)
      static Huffman lit, dist;
      static bool built = false;
      if (!built) {
        unsigned char lengths[288];
        for (int i = 0; i < 144; i++) lengths[i] = 8;
        for (int i = 144; i < 256; i++) lengths[i] = 9;
        for (int i = 256; i < 280; i++) lengths[i] = 7;
        for (int i = 280; i < 288; i++) lengths[i] = 8;
        lit.build(lengths, 288);
        unsigned char dl[30];
        for (int i = 0; i < 30; i++) dl[i] = 5;
        dist.build(dl, 30);
        built = true;
      }
      if (!codes(bits, out, outCapacity, written, lit, dist)) return false;
    } else if (type == 2) {
      int nlen = (int)bits.take(5) + 257;
      int ndist = (int)bits.take(5) + 1;
      int ncode = (int)bits.take(4) + 4;
      if (nlen > 286 || ndist > 30) return false;
      static const unsigned char order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
      unsigned char lengths[320];
      for (int i = 0; i < 320; i++) lengths[i] = 0;
      for (int i = 0; i < ncode; i++) lengths[order[i]] = (unsigned char)bits.take(3);
      Huffman lencode, distcode;
      if (!lencode.build(lengths, 19)) return false;
      int index = 0;
      while (index < nlen + ndist) {
        int sym = lencode.decode(bits);
        if (sym < 0) return false;
        if (sym < 16) {
          lengths[index++] = (unsigned char)sym;
        } else {
          int len = 0, repeat;
          if (sym == 16) {
            if (index == 0) return false;
            len = lengths[index - 1];
            repeat = 3 + (int)bits.take(2);
          } else if (sym == 17) {
            repeat = 3 + (int)bits.take(3);
          } else {
            repeat = 11 + (int)bits.take(7);
          }
          if (index + repeat > nlen + ndist) return false;
          while (repeat--) lengths[index++] = (unsigned char)len;
        }
      }
      if (lengths[256] == 0) return false;   // no end-of-block code
      if (!lencode.build(lengths, nlen)) return false;
      if (!distcode.build(lengths + nlen, ndist)) return false;
      if (!codes(bits, out, outCapacity, written, lencode, distcode)) return false;
    } else {
      return false;  // type 3 is invalid
    }
  }
  if (outWritten) *outWritten = written;
  return !bits.overrun;
}

bool inflateZlib(const unsigned char* in, size_t inSize, unsigned char* out, size_t outCapacity,
                 size_t* outWritten) {
  if (!in || inSize < 6) return false;
  unsigned cmf = in[0], flg = in[1];
  if ((cmf & 0x0F) != 8) return false;                 // must be deflate
  if (((cmf << 8) | flg) % 31 != 0) return false;      // header checksum
  if (flg & 0x20) return false;                        // preset dictionary unsupported
  return inflateRaw(in + 2, inSize - 6, out, outCapacity, outWritten);
}

}  // namespace nx
