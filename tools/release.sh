#!/bin/bash
# Full release: rebuild the native library, repack the APK with the current art/audio, sign
# v1+v2+v3, then run the launch-path pre-flight.
#
#   tools/release.sh [base.apk]
#
# The base APK supplies classes.dex + resources.arsc; everything else is rebuilt from the tree.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
BASE="${1:-client/build/nexora-1.0.0.apk}"
KEY="${NEXORA_KEY:-build/keys/nexora.key.pem}"
CERT="${NEXORA_CERT:-build/keys/nexora.cert.pem}"
OUT="client/build/nexora-1.0.0.apk"
WORK="$(mktemp -d)"

echo "== native"
ZIG=${ZIG:-$(python3 -c 'import os, ziglang; print(os.path.join(os.path.dirname(ziglang.__file__), "zig"))' 2>/dev/null || true)}
ZIG="$ZIG" bash tools/build_native.sh >/dev/null
echo "   $(ls -l client/build/so/arm64-v8a/libnexora.so | awk '{print $5}') bytes"

echo "== assets"
[ -n "$NEXORA_SKIP_ASSETS" ] || python3 tools/prep_materials.py >/dev/null

echo "== pack"
NODE_PATH="${NODE_PATH:-$(npm root -g)}" node tools/pack_apk.mjs "$BASE" "$WORK/unsigned.apk" \
    client/build/so/arm64-v8a/libnexora.so client/assets

echo "== sign"
NODE_PATH="${NODE_PATH:-$(npm root -g)}" node tools/sign_apk.mjs "$WORK/unsigned.apk" "$OUT" "$KEY" "$CERT"

rm -rf "$WORK"
echo "== check"
python3 tools/check_apk.py "$OUT" | tail -4
