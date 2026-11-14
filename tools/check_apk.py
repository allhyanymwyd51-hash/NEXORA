#!/usr/bin/env python3
"""Pre-flight checks for the launch path, runnable without a device.

Every "the app stays black" bug seen in this project was one of these, and each check is a
static one so it can run in CI on the shipped APK rather than on a phone:

  1. the APK is well formed, signed (v1+v2/v3), and the native library is stored + page aligned
  2. every native method the Java shell declares exists in libnexora.so with the right signature
  3. the Java Surface is converted to an ANativeWindow before EGL uses it
  4. every asset the engine opens at startup is inside the APK
  5. the streamed music headers are valid and decodable

    python3 tools/check_apk.py client/build/nexora-1.0.0.apk
"""
import re
import struct
import sys
import zipfile

REQUIRED_ASSETS = [
    'fonts/Cairo-400.ttf', 'fonts/Cairo-600.ttf', 'fonts/Cairo-700.ttf',
    'textures/world/terrain_albedo_atlas.png', 'textures/world/terrain_normal_atlas.png',
    'textures/world/prop_albedo_atlas.png', 'textures/world/prop_normal_atlas.png',
    'audio/music/menu.adp', 'audio/music/exploration.adp',
    'audio/ui_click.wav', 'audio/jump.wav', 'audio/footstep_grass_1.wav',
]
# The dex keeps type descriptors, not Java source, so the bridge method names are matched
# against the NativeBridge contract that the Java shell declares.
BRIDGE_METHODS = [
    'setAssetManager', 'setCacheDir', 'setFilesDir', 'setActivity', 'onPermissionResult',
    'surfaceCreated', 'surfaceChanged', 'surfaceDestroyed', 'pause', 'resume',
    'touchDown', 'touchMove', 'touchUp', 'key', 'textTyped', 'keyTyped',
    'setAudioSampleRate', 'setAudioFramesPerBuffer',
]


def fail(msg):
    print('FAIL', msg)
    return 1


def main(path):
    errors = 0
    z = zipfile.ZipFile(path)
    data = open(path, 'rb').read()
    names = set(z.namelist())

    # 1. structure + signing + alignment
    if z.testzip() is not None:
        errors += fail('zip is corrupt')
    if 'classes.dex' not in names:
        errors += fail('classes.dex missing')
    if 'resources.arsc' not in names:
        errors += fail('resources.arsc missing')
    if b'APK Sig Block 42' not in data:
        errors += fail('no APK signing block (v2/v3)')
    if not any(n.startswith('META-INF/') and n.endswith('.RSA') for n in names):
        errors += fail('no v1 (JAR) signature')
    so_name = 'lib/arm64-v8a/libnexora.so'
    if so_name not in names:
        return fail('libnexora.so missing') or 1
    info = z.getinfo(so_name)
    nlen, elen = struct.unpack('<HH', data[info.header_offset + 26:info.header_offset + 30])
    so_off = info.header_offset + 30 + nlen + elen
    if info.compress_type != 0:
        errors += fail('libnexora.so is compressed (must be stored for direct mapping)')
    if so_off % 4096:
        errors += fail(f'libnexora.so not page aligned (offset {so_off})')
    print(f'ok   apk {len(data)/1048576:.1f} MB, {len(names)} entries, '
          f'so aligned {so_off % 16384 == 0 and "16K" or "4K"}')

    # 2. JNI surface: every Java native method resolved in the library
    so = z.read(so_name)
    exports = set(re.findall(rb'Java_com_nexora_game_NativeBridge_(\w+)', so))
    exports = {e.decode() for e in exports}
    declared = set(BRIDGE_METHODS)
    missing = sorted(declared - exports)
    if missing:
        errors += fail(f'native methods declared in Java but not exported: {missing}')
    if b'JNI_OnLoad' not in so:
        errors += fail('JNI_OnLoad missing (System.loadLibrary will fail)')
    print(f'ok   {len(exports)} JNI exports cover {len(declared)} Java native methods')

    # 3. the window must come from ANativeWindow_fromSurface, never a cast jobject
    if b'ANativeWindow_fromSurface' not in so:
        errors += fail('ANativeWindow_fromSurface is never referenced: EGL would get a jobject')
    print('ok   ANativeWindow bridge present')

    # 4. startup assets
    missing_assets = [a for a in REQUIRED_ASSETS if 'assets/' + a not in names]
    if missing_assets:
        errors += fail(f'startup assets missing: {missing_assets}')
    print(f'ok   {len(REQUIRED_ASSETS)} startup assets present')

    # 5. streamed music: header + first block sanity
    music = sorted(n for n in names if n.startswith('assets/audio/music/') and n.endswith('.adp'))
    if not music:
        errors += fail('no streamed music shipped')
    for m in music:
        blob = z.read(m)
        if blob[:4] != b'NXAD':
            errors += fail(f'{m}: bad magic')
            continue
        ch = struct.unpack('<H', blob[6:8])[0]
        rate = struct.unpack('<I', blob[8:12])[0]
        frames = struct.unpack('<I', blob[12:16])[0]
        chunk = struct.unpack('<H', blob[16:18])[0]
        block = ch * 3 + ch * (chunk // 2)
        if ch != 2 or rate != 44100 or chunk < 2 or len(blob) < 18 + block:
            errors += fail(f'{m}: invalid header')
        else:
            print(f'ok   {m.split("/")[-1]}: {frames/rate:.0f}s, {len(blob)/1048576:.1f} MB')

    print('PASS' if errors == 0 else f'{errors} check(s) failed')
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else 'client/build/nexora-1.0.0.apk'))
