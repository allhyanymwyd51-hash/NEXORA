#!/usr/bin/env python3
"""Asset pipeline tests: the decoders in jni/core must accept everything we ship.

    python3 tools/test_assets.py
"""
import struct
import sys

import numpy as np

sys.path.insert(0, 'tools')
import adp  # noqa: E402

FAILED = []


def check(name, cond, detail=''):
    print(('ok   ' if cond else 'FAIL ') + name + (f' — {detail}' if detail else ''))
    if not cond:
        FAILED.append(name)


def test_adpcm_roundtrip():
    sr = 44100
    t = np.arange(sr * 2) / sr
    sig = np.stack([0.4 * np.sin(2 * np.pi * 220 * t), 0.4 * np.sin(2 * np.pi * 330 * t)], 1)
    adp.write_adp('/tmp/t_roundtrip.adp', sig, sr)
    out, rate = adp.read_adp('/tmp/t_roundtrip.adp')
    ref = (np.clip(sig, -1, 1) * 32000).astype(np.float64)
    err = out[:len(ref)].astype(np.float64) - ref
    snr = 10 * np.log10(np.mean(ref ** 2) / max(np.mean(err ** 2), 1e-9))
    check('adpcm round-trip snr > 40 dB', snr > 40, f'{snr:.1f} dB')
    check('adpcm sample rate preserved', rate == sr)


def test_shipped_tracks():
    import glob
    for path in sorted(glob.glob('client/assets/audio/music/*.adp')):
        blob = open(path, 'rb').read()
        ch, = struct.unpack('<H', blob[6:8])
        rate, frames = struct.unpack('<II', blob[8:16])
        chunk, = struct.unpack('<H', blob[16:18])
        block = ch * 3 + ch * (chunk // 2)
        ok = blob[:4] == b'NXAD' and ch == 2 and rate == 44100
        ok = ok and (len(blob) - 18) % block == 0
        check(f'{path.split("/")[-1]} header/block layout', ok,
              f'{frames/rate:.0f}s {len(blob)/1048576:.1f} MB')


def test_materials():
    from PIL import Image
    import glob
    for path in sorted(glob.glob('client/assets/textures/world/*.png')):
        im = Image.open(path)
        w, h = im.size
        check(f'{path.split("/")[-1]} is power-of-two', w == h and (w & (w - 1)) == 0, f'{w}x{h}')


if __name__ == '__main__':
    test_adpcm_roundtrip()
    test_shipped_tracks()
    test_materials()
    print('PASS' if not FAILED else f'{len(FAILED)} failed')
    sys.exit(0 if not FAILED else 1)
