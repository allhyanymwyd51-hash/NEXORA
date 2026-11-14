#!/usr/bin/env python3
"""Composes the NEXORA soundtrack as real 44.1 kHz stereo music and encodes it to the
engine's streamed IMA-ADPCM format.

Each track is an actual arrangement (chord progressions, arpeggios, bass, drums, reverb)
rather than a placeholder tone, and runs 3-4 minutes so gameplay never loops a 4-second
clip. Usage: gen_music.py <exploration|social|race|combat|horror> <out.adp> [minutes]
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import adp  # noqa: E402

SR = 44100
rng = np.random.default_rng(7)


def hz(semis):
    return 440.0 * 2.0 ** (semis / 12.0)


def adsr(n, a=0.01, d=0.1, s=0.7, r=0.2):
    env = np.ones(n, np.float32)
    ai, di, ri = int(a * SR), int(d * SR), int(r * SR)
    ai, di, ri = min(ai, n), min(di, max(0, n - ai)), min(ri, n)
    if ai:
        env[:ai] = np.linspace(0, 1, ai)
    if di:
        env[ai:ai + di] = np.linspace(1, s, di)
    env[ai + di:n - ri] = s
    if ri:
        env[n - ri:] *= np.linspace(1, 0, ri)
    return env


def saw(f, n, detune=0.0):
    t = np.arange(n, dtype=np.float32) / SR
    return ((t * (f + detune)) % 1.0) * 2 - 1


def sine(f, n):
    t = np.arange(n, dtype=np.float32) / SR
    return np.sin(2 * np.pi * f * t)


def pad(freqs, n, gain=0.18):
    out = np.zeros((n, 2), np.float32)
    for f in freqs:
        for ch, det in enumerate((-0.6, 0.6)):
            x = saw(f, n, det * 2.0) * 0.5 + sine(f, n) * 0.5
            out[:, ch] += x * adsr(n, 0.35, 0.4, 0.75, 0.6) * gain / max(1.0, len(freqs) ** 0.5)
    return out


def pluck(f, n, gain=0.22):
    x = saw(f, n) * 0.4 + sine(f * 2, n) * 0.25 + sine(f, n) * 0.35
    return x * adsr(n, 0.004, 0.15, 0.35, min(0.35, n / SR)) * gain


def kick(n):
    t = np.arange(n, dtype=np.float32) / SR
    f = 120 * np.exp(-t * 28) + 45
    return (np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 9)).astype(np.float32)


def snare(n):
    t = np.arange(n, dtype=np.float32) / SR
    noise = rng.normal(0, 1, n).astype(np.float32)
    noise = np.convolve(noise, np.ones(6) / 6, mode='same')
    return (noise * 0.6 + np.sin(2 * np.pi * 190 * t) * 0.3) * np.exp(-t * 22)


def hat(n):
    t = np.arange(n, dtype=np.float32) / SR
    noise = rng.normal(0, 1, n).astype(np.float32)
    return (noise - np.convolve(noise, np.ones(12) / 12, mode='same')) * np.exp(-t * 55) * 0.28


def reverb(x, decay=0.35, taps=(0.031, 0.047, 0.071, 0.101, 0.137)):
    out = x.copy()
    for i, d in enumerate(taps):
        k = int(d * SR)
        out[k:] += x[:-k] * (decay ** (i + 1))
    return out


def place(buf, sig, start):
    n = min(len(sig), len(buf) - start)
    if n <= 0:
        return
    if sig.ndim == 1:
        buf[start:start + n, 0] += sig[:n]
        buf[start:start + n, 1] += sig[:n]
    else:
        buf[start:start + n] += sig[:n]


def seconds(mins):
    return int(mins * 60 * SR)


def exploration(mins=4.0):
    out = np.zeros((seconds(mins), 2), np.float32)
    prog = [[0, 4, 7], [-3, 0, 4], [-5, -1, 2], [-7, -3, 0]]
    beat = int(SR * 0.5)
    for b in range(len(out) // (beat * 8)):
        chord = prog[b % 4]
        place(out, pad([hz(s - 12) for s in chord] + [hz(s) for s in chord], beat * 8), b * beat * 8)
        for i in range(16):
            s = chord[i % 3] + (12 if i % 6 > 2 else 0)
            place(out, pluck(hz(s), int(beat * 0.45)), b * beat * 8 + i * int(beat * 0.5))
        place(out, sine(hz(chord[0] - 24), beat * 4) * adsr(beat * 4, 0.02, 0.2, 0.6, 0.3) * 0.22,
              b * beat * 8)
        if b % 2:
            place(out, kick(beat), b * beat * 8 + beat * 4)
        place(out, hat(beat // 2) * 0.5, b * beat * 8 + beat * 2)
        place(out, hat(beat // 2) * 0.4, b * beat * 8 + beat * 6)
    return reverb(out, 0.4)


def social(mins=3.5):
    out = np.zeros((seconds(mins), 2), np.float32)
    prog = [[0, 3, 7, 10], [-2, 2, 5, 9], [-4, 0, 3, 7], [-5, -1, 2, 5]]
    beat = int(SR * 0.62)
    for b in range(len(out) // (beat * 4)):
        chord = prog[b % 4]
        place(out, pad([hz(s - 12) for s in chord], beat * 4, 0.22), b * beat * 4)
        place(out, kick(beat) * 0.9, b * beat * 4)
        place(out, kick(beat) * 0.7, b * beat * 4 + int(beat * 2.5))
        place(out, snare(beat) * 0.5, b * beat * 4 + beat * 2)
        for i in range(8):
            place(out, hat(beat // 2) * (0.5 if i % 2 else 0.3), b * beat * 4 + i * beat // 2)
        if b % 4 == 3:
            for s in chord:
                place(out, pluck(hz(s + 12), beat), b * beat * 4 + beat * 3)
    out = reverb(out, 0.25)
    return out + rng.normal(0, 0.0025, out.shape).astype(np.float32)


def race(mins=3.0):
    out = np.zeros((seconds(mins), 2), np.float32)
    line = [0, 0, 7, 0, 3, 3, 10, 7]
    step = int(SR * 0.125)
    for i in range(len(out) // step):
        s = line[i % 8]
        f = hz(s - 24)
        x = saw(f, step) * 0.5 + sine(f, step) * 0.5
        place(out, x * adsr(step, 0.002, 0.08, 0.5, 0.03) * 0.3, i * step)
        if i % 4 == 0:
            place(out, kick(step * 2), i * step)
        if i % 8 == 4:
            place(out, snare(step * 2) * 0.7, i * step)
        place(out, hat(step // 2) * (0.35 if i % 2 else 0.22), i * step)
        if i % 16 == 0:
            for k in (0, 3, 7, 12):
                place(out, pluck(hz(k), step * 4) * 0.5, i * step)
    return reverb(out, 0.2)


def combat(mins=3.0):
    out = np.zeros((seconds(mins), 2), np.float32)
    beat = int(SR * 0.4)
    for b in range(len(out) // (beat * 4)):
        root = -12 - (b % 4)
        x = np.zeros((beat * 4, 2), np.float32)
        for i, f in enumerate([hz(root), hz(root + 1), hz(root + 6), hz(root + 7)]):
            for ch in range(2):
                x[:, ch] += saw(f, beat * 4, (i - 1.5) * 3) * 0.09
        x *= adsr(beat * 4, 0.05, 0.3, 0.8, 0.4)[:, None]
        place(out, x, b * beat * 4)
        place(out, kick(beat) * 0.8, b * beat * 4)
        place(out, kick(beat) * 0.6, b * beat * 4 + beat * 2)
        place(out, snare(beat) * 0.45, b * beat * 4 + beat * 3)
        for i in range(8):
            place(out, hat(beat // 2) * 0.18, b * beat * 4 + i * beat // 2)
    return reverb(out, 0.3)


def horror(mins=3.0):
    n = seconds(mins)
    out = np.zeros((n, 2), np.float32)
    t = np.arange(n, dtype=np.float32) / SR
    for f in (hz(-24), hz(-23), hz(-12), hz(-11)):
        out[:, 0] += np.sin(2 * np.pi * f * t) * 0.05
        out[:, 1] += np.sin(2 * np.pi * f * 1.003 * t) * 0.05
    out *= (0.6 + 0.4 * np.sin(2 * np.pi * 0.05 * t))[:, None]
    seg = int(SR * 6)
    for start in range(0, n - seg, seg * 2):
        f = hz(int(rng.integers(-18, 6)))
        place(out, saw(f, seg, rng.normal(0, 2)) * 0.12 * adsr(seg, 1.2, 1.5, 0.5, 2.0), start)
    return reverb(out, 0.45)


def normalize(x, peak=0.89):
    m = float(np.max(np.abs(x))) or 1.0
    return x * (peak / m)


TRACKS = {'exploration': exploration, 'social': social, 'race': race, 'combat': combat, 'horror': horror}

if __name__ == '__main__':
    name, path = sys.argv[1], sys.argv[2]
    mins = float(sys.argv[3]) if len(sys.argv) > 3 else 4.0
    audio = normalize(TRACKS[name](mins))
    size = adp.write_adp(path, audio, SR)
    print(f'{path}: {size / 1048576:.2f} MB, {len(audio) / SR:.1f}s')
