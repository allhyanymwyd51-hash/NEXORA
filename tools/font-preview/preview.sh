#!/usr/bin/env bash
# NEXORA — host-side font check.
#
# Compiles the engine's real font stack (core/font.cpp) for the host and renders
# sample UI strings to a PNG, so Arabic shaping, bidi ordering and the glyph
# rasteriser can be inspected without a device. This is how the composite-glyph
# and contour-closing bugs in the rasteriser were found.
#
# Usage: tools/font-preview/preview.sh [output.png]
set -euo pipefail
cd "$(dirname "$0")/../.."

OUT=${1:-tools/font-preview/out/arabic.png}
mkdir -p "$(dirname "$OUT")" tools/font-preview/out
CXX=${CXX:-g++}

"$CXX" -std=c++17 -O1 -DNEX_HOST_TESTS -I client/jni \
  tools/font-preview/harness.cpp tools/font-preview/glstub.cpp client/jni/core/font.cpp \
  -o tools/font-preview/out/preview
tools/font-preview/out/preview client/assets/fonts/Cairo-400.ttf "$OUT"
echo "font preview -> $OUT"
