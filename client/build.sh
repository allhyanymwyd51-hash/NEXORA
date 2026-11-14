#!/usr/bin/env bash
# NEXORA — Android APK build (no Gradle required).
#
# Pipeline: aapt2 compile/link -> javac -> d8 -> native cross-compile -> zipalign -> apksigner
# Requirements: JDK 17, Android build-tools (aapt2/d8/zipalign/apksigner), platform android-34,
# and a C compiler able to target Android (used via $ZIG or $NDK_CC).
set -euo pipefail

cd "$(dirname "$0")"
ROOT="$(cd .. && pwd)"

# ---------------------------------------------------------------------------
# Toolchain discovery (env first, then the standard SDK layout)
# ---------------------------------------------------------------------------
: "${JAVA_HOME:=/home/user/jdk/jdk-17.0.20.1+1}"
: "${ANDROID_SDK:=/home/user/android-sdk}"
: "${ANDROID_JAR:=$ANDROID_SDK/platforms/android-34/android.jar}"
: "${BUILD_TOOLS:=$ANDROID_SDK/build-tools/34.0.0}"
export PATH="$JAVA_HOME/bin:$BUILD_TOOLS:$PATH"

AAPT2="$BUILD_TOOLS/aapt2"
D8="$BUILD_TOOLS/d8"
ZIPALIGN="$BUILD_TOOLS/zipalign"
APKSIGNER="$BUILD_TOOLS/apksigner"

OUT="build"
OBJ="$OUT/obj"
GEN="$OUT/gen"
CLASSES="$OUT/classes"
DEX="$OUT/dex"
LIBS="$OUT/libs"
STUBS="$OUT/stubs"
APK_UNSIGNED="$OUT/nexora-unsigned.apk"
APK_ALIGNED="$OUT/nexora-aligned.apk"
APK_FINAL="$OUT/nexora-1.0.0.apk"

VERSION_CODE=1
VERSION_NAME="1.0.0"
MIN_SDK=26
TARGET_SDK=34

say() { printf '\033[1;36m[NEXORA]\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m[NEXORA] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

for tool in "$JAVA_HOME/bin/javac" "$AAPT2" "$D8" "$ZIPALIGN" "$APKSIGNER" "$ANDROID_JAR"; do
  [ -e "$tool" ] || die "missing tool: $tool"
done

rm -rf "$OUT"
mkdir -p "$OBJ" "$GEN" "$CLASSES" "$DEX" "$LIBS/arm64-v8a" "$STUBS"

# ---------------------------------------------------------------------------
# 1. Native engine (C++17) -> libnexora.so per ABI
#
# The engine is freestanding: it links with -nostdlib and resolves libc/libm/
# liblog/libandroid/GLES/EGL symbols through the platform loader at runtime.
# Two consequences shape this step:
#   * bionic headers are not available to every cross compiler, so the engine
#     carries its own declarations in jni/compat (always on the include path);
#   * the linker still needs import libraries to record the DT_NEEDED entries,
#     so tiny stub .so files are generated here from $PLATFORM_SYMBOLS.
# ---------------------------------------------------------------------------
NATIVE_SRC=$(find jni -name '*.cpp' -o -name '*.c' | sort)
if [ -z "$NATIVE_SRC" ]; then die "no native sources found in jni/"; fi
NATIVE_INCLUDES=(-I jni -I jni/compat)

# Symbols the platform provides at runtime, grouped by the library that owns them.
LIBC_SYMS="malloc calloc realloc free memcpy memmove memset strlen strcmp strncmp strncpy strcpy strchr strstr
  strncasecmp strtol strtoul atoi atof snprintf vsnprintf srand clock_gettime open close read write lseek
  access abort usleep pthread_create pthread_self pthread_mutex_init pthread_mutex_lock pthread_mutex_unlock pthread_mutex_destroy pthread_join"
LIBM_SYMS="sinf cosf tanf expf powf atan2f sincosf"
LIBDL_SYMS="dlopen dlsym"
LIBLOG_SYMS="__android_log_print"
LIBANDROID_SYMS="AAudio_createStreamBuilder AAudioStreamBuilder_setDirection AAudioStreamBuilder_setSampleRate
  AAudioStreamBuilder_setChannelCount AAudioStreamBuilder_setFormat AAudioStreamBuilder_setPerformanceMode
  AAudioStreamBuilder_setDataCallback AAudioStreamBuilder_openStream AAudioStreamBuilder_delete AAudioStream_close
  AAudioStream_getFramesPerBurst AAudioStream_getSampleRate AAudioStream_setBufferSizeInFrames
  AAudioStream_requestStart AAudioStream_requestStop"

# Writes an empty-bodied stub for every symbol so lld can resolve + record NEEDED
# without pulling in a libc that the target triple cannot provide.
stub_library() {
  local abi="$1" triple="$2" name="$3"; shift 3
  local asm="$STUBS/$abi/$name.s" obj="$STUBS/$abi/$name.o"
  : > "$asm"
  local sym
  for sym in $*; do
    printf '.globl %s\n.type %s, %%function\n%s:\n  ret\n' "$sym" "$sym" "$sym" >> "$asm"
  done
  $CC -target "$triple" -c "$asm" -o "$obj" || die "stub compile failed: $name"
  $CC -target "$triple" -shared -nostdlib -Wl,-soname,"$name.so" -o "$STUBS/$abi/$name.so" "$obj" \
    || die "stub link failed: $name"
}

compile_native() {
  local abi="$1" triple="$2" optflags="$3"
  local outdir="$LIBS/$abi"
  mkdir -p "$STUBS/$abi"
  local objs=() src obj
  for src in $NATIVE_SRC; do
    obj="$OBJ/${abi}_$(echo "$src" | tr '/' '_').o"
    $CC -target "$triple" $optflags -std=c++17 -fPIC -fno-exceptions -fno-rtti -fno-stack-protector \
      -ffunction-sections -fdata-sections -fvisibility=hidden -w \
      -DANDROID -DNEXORA_VERSION='"'"$VERSION_NAME"'"' "${NATIVE_INCLUDES[@]}" \
      -c "$src" -o "$obj" || die "native compile failed: $src"
    objs+=("$obj")
  done

  say "link [$abi] -> libnexora.so"
  stub_library "$abi" "$triple" libc $LIBC_SYMS
  stub_library "$abi" "$triple" libm $LIBM_SYMS
  stub_library "$abi" "$triple" libdl $LIBDL_SYMS
  stub_library "$abi" "$triple" liblog $LIBLOG_SYMS
  stub_library "$abi" "$triple" libandroid $LIBANDROID_SYMS
  stub_library "$abi" "$triple" libGLESv3 nx_gles_placeholder
  stub_library "$abi" "$triple" libEGL nx_egl_placeholder
  stub_library "$abi" "$triple" libjnigraphics nx_jni_placeholder

  # --no-undefined keeps this honest: every engine symbol must be either defined
  # in the objects or listed above as platform-provided.
  $CC -target "$triple" -shared -nostdlib -Wl,--gc-sections -Wl,--no-undefined \
    -Wl,-z,max-page-size=16384 -Wl,--hash-style=sysv -Wl,-soname,libnexora.so \
    -o "$outdir/libnexora.so" "${objs[@]}" \
    "$STUBS/$abi"/libc.so "$STUBS/$abi"/libm.so "$STUBS/$abi"/libdl.so "$STUBS/$abi"/liblog.so \
    "$STUBS/$abi"/libandroid.so "$STUBS/$abi"/libGLESv3.so "$STUBS/$abi"/libEGL.so \
    "$STUBS/$abi"/libjnigraphics.so \
    || die "native link failed"
  $STRIP --strip-unneeded "$outdir/libnexora.so" 2>/dev/null || true
  ls -la "$outdir/libnexora.so"
}

if [ -n "${NDK_CC:-}" ]; then
  CC="$NDK_CC"; STRIP="${NDK_STRIP:-llvm-strip}"
elif [ -n "${ZIG:-}" ]; then
  CC="$ZIG cc"; STRIP="$ZIG objcopy"
else
  die "no native compiler configured (set ZIG or NDK_CC)"
fi
# arm64 only: the armeabi-v7a build needs an armhf-targeted atomics shim and is
# not part of this release.
compile_native "arm64-v8a" "aarch64-linux-android" "-O2 -DNDEBUG"

# ---------------------------------------------------------------------------
# 2. Assets (shaders, fonts, audio bank, generated at build time)
# ---------------------------------------------------------------------------
if [ ! -f assets/audio/music_exploration.wav ] && [ -f "$ROOT/server/scripts/gen-audio.js" ]; then
  say "generating procedural audio bank"
  node "$ROOT/server/scripts/gen-audio.js" >/dev/null
  cp -r "$ROOT/server/assets/audio" assets/ 2>/dev/null || true
fi
# OGG (Vorbis) keeps the APK small; fixed-rate mono/stereo at 22.05 kHz is plenty for SFX/ambience.
if command -v ffmpeg >/dev/null 2>&1; then
  say "transcoding audio to OGG"
  mkdir -p assets/audio_ogg
  for f in assets/audio/*.wav; do
    b=$(basename "$f" .wav)
    ffmpeg -loglevel error -y -i "$f" -c:a libvorbis -q:a 3 -ar 22050 "assets/audio_ogg/$b.ogg" || true
  done
fi

ASSET_LIST="$OUT/assets.list"
: > "$ASSET_LIST"
if [ -d assets ]; then
  while IFS= read -r -d '' f; do
    rel="${f#assets/}"
    case "$rel" in audio/*.wav) continue ;; esac
    echo "${rel%/*}" >> "$ASSET_LIST"
    echo "$f" >> "$ASSET_LIST"
  done < <(find assets -type f -print0 | sort -z)
fi

# ---------------------------------------------------------------------------
# 3. Resources + manifest -> base APK (aapt2)
# ---------------------------------------------------------------------------
say "aapt2 compile resources"
find res -type f | sort > "$OUT/res.list"
"$AAPT2" compile -o "$OUT/compiled.zip" --dir res || die "aapt2 compile failed"

say "aapt2 link"
"$AAPT2" link \
  -o "$OUT/base.apk" \
  -I "$ANDROID_JAR" \
  --manifest AndroidManifest.xml \
  --java "$GEN" \
  --min-sdk-version "$MIN_SDK" \
  --target-sdk-version "$TARGET_SDK" \
  --version-code "$VERSION_CODE" \
  --version-name "$VERSION_NAME" \
  -A assets \
  "$OUT/compiled.zip" \
  || die "aapt2 link failed"

# ---------------------------------------------------------------------------
# 4. Java -> classes -> dex
# ---------------------------------------------------------------------------
say "javac"
mkdir -p "$GEN/src"
if [ -d "$GEN/com" ]; then rsync -a "$GEN/" "$GEN/src/" 2>/dev/null || cp -r "$GEN/com" "$GEN/src/"; fi
JAVA_SRC=$(find src "$GEN/src" -name '*.java' | sort)
[ -n "$JAVA_SRC" ] || die "no java sources"
# shellcheck disable=SC2086
"$JAVA_HOME/bin/javac" -encoding UTF-8 -classpath "$ANDROID_JAR" \
  -Xlint:-options -nowarn -d "$CLASSES" $JAVA_SRC 2> "$OUT/javac.log" \
  || { cat "$OUT/javac.log"; die "javac failed"; }

say "d8 (dex)"
CLASS_FILES=$(find "$CLASSES" -name '*.class')
"$D8" --release --min-api "$MIN_SDK" --output "$DEX" --lib "$ANDROID_JAR" $CLASS_FILES \
  || die "d8 failed"
ls -la "$DEX"

# ---------------------------------------------------------------------------
# 5. Package: base APK + classes.dex + native libs + assets
# ---------------------------------------------------------------------------
say "packaging"
STAGE="$(pwd)/$OUT/stage"
rm -rf "$STAGE"; mkdir -p "$STAGE"
# start from the linked resource APK, then inject dex, native libs and assets
cp "$OUT/base.apk" "$STAGE/base.apk"
( cd "$DEX" && zip -q -X "$STAGE/base.apk" classes*.dex ) || die "failed to add dex"

for abi in $(ls "$LIBS"); do
  mkdir -p "$STAGE/lib/$abi"
  cp "$LIBS/$abi/libnexora.so" "$STAGE/lib/$abi/"
done
if [ -d "$STAGE/lib" ]; then
  ( cd "$STAGE" && zip -q -X -r base.apk lib ) || die "failed to add native libs"
fi

# assets/ (shaders, fonts, textures, audio). Textures and the streamed .adp music are already
# compressed, so they are stored as-is; WAV effects and shaders deflate well.
if [ -d assets ]; then
  ( cd . && zip -q -X -r "$STAGE/base.apk" assets ) || die "failed to add assets"
fi

"$BUILD_TOOLS/zipalign" -c -v 4 "$STAGE/base.apk" >/dev/null 2>&1 || true
mv "$STAGE/base.apk" "$APK_UNSIGNED"

# ---------------------------------------------------------------------------
# 6. Align + sign
# ---------------------------------------------------------------------------
say "zipalign"
# 16 KB alignment: -p keeps .so pages uncompressed and page-aligned, which Android 15
# requires for 16 KB page devices (and is harmless on 4 KB ones).
"$ZIPALIGN" -p -f 4 "$APK_UNSIGNED" "$APK_ALIGNED" || die "zipalign failed"

KEYSTORE="${NEXORA_KEYSTORE:-$ROOT/build/keys/nexora-release.jks}"
KS_PASS="${NEXORA_KEYSTORE_PASS:-nexora-build-pass}"
KEY_ALIAS="${NEXORA_KEY_ALIAS:-nexora}"
if [ ! -f "$KEYSTORE" ]; then
  say "creating release keystore (first build)"
  mkdir -p "$(dirname "$KEYSTORE")"
  "$JAVA_HOME/bin/keytool" -genkeypair -v \
    -keystore "$KEYSTORE" -storetype PKCS12 \
    -storepass "$KS_PASS" -keypass "$KS_PASS" \
    -alias "$KEY_ALIAS" -keyalg RSA -keysize 4096 -validity 10000 \
    -dname "CN=NEXORA, OU=Games, O=NEXORA, L=Jeddah, C=SA" >/dev/null 2>&1 \
    || die "keytool failed"
fi

say "apksigner"
"$APKSIGNER" sign \
  --ks "$KEYSTORE" --ks-pass "pass:$KS_PASS" --key-pass "pass:$KS_PASS" \
  --ks-key-alias "$KEY_ALIAS" \
  --min-sdk-version "$MIN_SDK" \
  --v1-signing-enabled true --v2-signing-enabled true --v3-signing-enabled true \
  --out "$APK_FINAL" "$APK_ALIGNED" || die "apksigner failed"

say "verifying"
"$APKSIGNER" verify --verbose "$APK_FINAL" | head -8
"$AAPT2" dump badging "$APK_FINAL" | head -6

SIZE=$(du -h "$APK_FINAL" | cut -f1)
say "APK ready: $APK_FINAL ($SIZE)"
