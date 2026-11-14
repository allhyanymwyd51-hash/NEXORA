"""IMA-ADPCM encoder/decoder for the engine's streamed music format (.adp).

Block layout (must stay in sync with jni/core/audio.cpp):
  header: 'NXAD' | u16 version | u16 channels | u32 rate | u32 frames | u16 chunk
  block : per channel [i16 predictor][u8 step index]  (channels*3 bytes)
          then per channel chunk/2 bytes of packed nibbles (sample 0 = predictor)
"""
import numpy as np

STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
        73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408,
        449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878,
        2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845,
        8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086,
        29794, 32767]
IDX = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]


def _start_index(x):
    """Pick the initial step index from the block's average slope; starting at 0 makes the
    codec ramp up slowly at every block boundary, which is audible as a tick."""
    slope = float(np.mean(np.abs(np.diff(x.astype(np.int32))))) if len(x) > 2 else 0.0
    i = 0
    while i < 88 and STEP[i] * 2.0 < slope:
        i += 1
    return i


def encode_block(ch_samples, chunk):
    ch = len(ch_samples)
    hdr = bytearray()
    nib = bytearray()
    for c in range(ch):
        x = ch_samples[c]
        hdr += np.int16(int(x[0])).tobytes() + bytes([_start_index(x)])
    for c in range(ch):
        x = ch_samples[c]
        pred = int(x[0])
        idx = _start_index(x)
        bits = []
        for f in range(1, chunk):
            step = STEP[idx]
            diff = int(x[f]) - pred
            nibble = 0
            if diff < 0:
                nibble = 8
                diff = -diff
            if diff >= step:
                nibble |= 4
                diff -= step
            if diff >= step >> 1:
                nibble |= 2
                diff -= step >> 1
            if diff >= step >> 2:
                nibble |= 1
                diff -= step >> 2
            step = STEP[idx]
            d = step >> 3
            if nibble & 1:
                d += step >> 2
            if nibble & 2:
                d += step >> 1
            if nibble & 4:
                d += step
            pred = pred - d if nibble & 8 else pred + d
            pred = max(-32768, min(32767, pred))
            idx = max(0, min(88, idx + IDX[nibble]))
            bits.append(nibble)
        packed = bytearray()
        for i in range(0, len(bits), 2):
            a = bits[i]
            b = bits[i + 1] if i + 1 < len(bits) else 0
            packed.append((a << 4) | b)
        nib += packed[: chunk // 2]
    return bytes(hdr) + bytes(nib)


def write_adp(path, stereo, rate, chunk=512):
    """stereo: float array (frames, 2) in [-1, 1]."""
    pcm = (np.clip(stereo, -1, 1) * 32000).astype(np.int16)
    frames = len(pcm)
    pad = (-frames) % chunk
    if pad:
        pcm = np.vstack([pcm, np.zeros((pad, 2), np.int16)])
    out = bytearray(b'NXAD' + np.uint16(1).tobytes() + np.uint16(2).tobytes() +
                    np.uint32(rate).tobytes() + np.uint32(frames).tobytes() + np.uint16(chunk).tobytes())
    for i in range(0, len(pcm), chunk):
        blk = pcm[i:i + chunk]
        out += encode_block([blk[:, 0].copy(), blk[:, 1].copy()], chunk)
    with open(path, 'wb') as fh:
        fh.write(out)
    return len(out)


def read_adp(path):
    """Reference decoder, used by the tests to prove the format round-trips."""
    d = open(path, 'rb').read()
    assert d[:4] == b'NXAD'
    ch = int(np.frombuffer(d[6:8], '<u2')[0])
    rate = int(np.frombuffer(d[8:12], '<u4')[0])
    frames = int(np.frombuffer(d[12:16], '<u4')[0])
    chunk = int(np.frombuffer(d[16:18], '<u2')[0])
    bb = ch * 3 + ch * (chunk // 2)
    out = np.zeros((frames, 2), np.int16)
    pos, w = 18, 0
    while pos + bb <= len(d) and w < frames:
        hdrs = []
        for c in range(ch):
            o = pos + c * 3
            hdrs.append((int(np.frombuffer(d[o:o + 2], '<i2')[0]), d[o + 2]))
        for c in range(ch):
            pred, idx = hdrs[c]
            nib = pos + ch * 3 + c * (chunk // 2)
            out[w, c] = pred
            for f in range(1, chunk):
                n = f - 1
                byte = d[nib + (n >> 1)]
                v = (byte & 0x0F) if (n & 1) else (byte >> 4)
                step = STEP[idx]
                dd = step >> 3
                if v & 1:
                    dd += step >> 2
                if v & 2:
                    dd += step >> 1
                if v & 4:
                    dd += step
                pred = pred - dd if v & 8 else pred + dd
                pred = max(-32768, min(32767, pred))
                idx = max(0, min(88, idx + IDX[v]))
                if w + f < frames:
                    out[w + f, c] = pred
        pos += bb
        w += chunk
    return out, rate
