#!/bin/bash
# Native half of the build: compiles every jni/**/*.cpp for arm64 with zig cc and links a
# self-contained libnexora.so against hand-written import stubs for the platform libraries.
# Use this when only C++ changed (the Java/dex side is unchanged and needs a JDK).
#   ZIG=/path/to/zig bash tools/build_native.sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/client"
if [ -z "$ZIG" ]; then
  ZIG=$(python3 -c 'import os, ziglang; print(os.path.join(os.path.dirname(ziglang.__file__), "zig"))' 2>/dev/null || true)
fi
Z="$ZIG"
if [ ! -x "$Z" ]; then echo "zig not found; set ZIG=/path/to/zig" >&2; exit 1; fi
OBJ=build/obj2
OUT=build/so/arm64-v8a
rm -rf "$OBJ"; mkdir -p "$OBJ/stubs" "$OUT"

LIBS="c m dl log android GLESv3 EGL jnigraphics"
LIBC_SYMS="malloc calloc realloc free memcpy memmove memset strlen strcmp strncmp strncpy strcpy strchr strstr strncasecmp strtol strtoul atoi atof snprintf vsnprintf srand clock_gettime open close read write lseek access abort usleep pthread_create pthread_self pthread_mutex_init pthread_mutex_lock pthread_mutex_unlock pthread_mutex_destroy pthread_join"
LIBM_SYMS="sinf cosf tanf expf powf atan2f sincosf"
LIBDL_SYMS="dlopen dlsym"
LIBLOG_SYMS="__android_log_print"
LIBANDROID_SYMS="AAudio_createStreamBuilder AAudioStreamBuilder_setDirection AAudioStreamBuilder_setSampleRate AAudioStreamBuilder_setChannelCount AAudioStreamBuilder_setFormat AAudioStreamBuilder_setPerformanceMode AAudioStreamBuilder_setDataCallback AAudioStreamBuilder_openStream AAudioStreamBuilder_delete AAudioStream_close AAudioStream_getFramesPerBurst AAudioStream_getSampleRate AAudioStream_setBufferSizeInFrames AAudioStream_requestStart AAudioStream_requestStop ANativeWindow_fromSurface ANativeWindow_release ANativeWindow_setBuffersGeometry"

mkstub() {
  local name=$1; shift
  local asm="$OBJ/stubs/$name.s"
  : > "$asm"
  local sym
  for sym in $*; do printf '.globl %s\n.type %s, %%function\n%s:\n  ret\n' "$sym" "$sym" "$sym" >> "$asm"; done
  $Z cc -target aarch64-linux-android -c "$asm" -o "$OBJ/stubs/$name.o"
  $Z cc -target aarch64-linux-android -shared -nostdlib -Wl,-soname,$name.so -o "$OBJ/stubs/lib$name.so" "$OBJ/stubs/$name.o"
}
mkdir -p "$OBJ/stubs"
mkstub c $LIBC_SYMS
mkstub m $LIBM_SYMS
mkstub dl $LIBDL_SYMS
mkstub log $LIBLOG_SYMS
mkstub android $LIBANDROID_SYMS
for L in GLESv3 EGL jnigraphics; do mkstub $L nx_placeholder; done

echo "[1/4] compiling native sources"
N=0
for f in $(find jni -name '*.cpp' | sort); do
  o="$OBJ/$(echo $f | tr '/' '_').o"
  $Z cc -target aarch64-linux-android -O2 -std=c++17 -fPIC -nostdlib -fno-exceptions -fno-rtti \
     -DANDROID -DNEXORA_VERSION='"1.0.0"' -I jni -I jni/compat -c "$f" -o "$o"
  N=$((N+1))
done
echo "  $N objects"

echo "[2/4] linking"
STUBARGS=""
for L in $LIBS; do STUBARGS="$STUBARGS -l:lib$L.so"; done
$Z cc -target aarch64-linux-android -nostdlib -shared -o "$OUT/libnexora.so" \
  $OBJ/*.o -L"$OBJ/stubs" $STUBARGS \
  -Wl,--no-undefined -Wl,-z,max-page-size=16384 -Wl,--hash-style=sysv
ls -l "$OUT/libnexora.so"
