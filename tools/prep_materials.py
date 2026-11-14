#!/usr/bin/env python3
"""Builds the shipped material set from the source scans in tools/assets_src/.

client/assets/textures/world/:
  <mat>_albedo.png / <mat>_normal.png   1K tiles per material
  terrain_{albedo,normal}_atlas.png     2x2 atlas: grass / sand / rock / snow
  prop_{albedo,normal}_atlas.png        2x2 atlas: brick / planks / cobble / metal
  foliage_*, bark_*                     vegetation

Normal maps come from the albedo luminance gradient so the lighting response matches the
visible detail. Run: /tmp/venv/bin/python tools/prep_materials.py
"""
import os

import numpy as np
from PIL import Image, ImageFilter

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'assets_src') + '/'
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'client', 'assets', 'textures', 'world') + '/'
TERRAIN = ['grass', 'sand', 'rock', 'snow']
PROPS = ['brick', 'planks', 'cobble', 'metal']


def load(name, size=1024):
    for ext in ('.png', '.jpg'):
        p = SRC + name + ext
        if os.path.exists(p):
            im = Image.open(p).convert('RGB')
            s = min(im.size)
            l, t = (im.width - s) // 2, (im.height - s) // 2
            return im.crop((l, t, l + s, t + s)).resize((size, size), Image.LANCZOS)
    raise SystemExit('missing source art: ' + name)


def normal_map(im, strength=2.2):
    g = np.asarray(im.convert('L').filter(ImageFilter.GaussianBlur(1.2)), np.float32) / 255.0
    dx = np.roll(g, -1, 1) - np.roll(g, 1, 1)
    dy = np.roll(g, -1, 0) - np.roll(g, 1, 0)
    nz = np.ones_like(g) * 0.55
    l = np.sqrt(dx * dx + dy * dy + nz * nz)
    out = np.stack([-dx * strength / l * .5 + .5, -dy * strength / l * .5 + .5, nz / l * .5 + .5], -1)
    return Image.fromarray((np.clip(out, 0, 1) * 255).astype(np.uint8))


def atlas(mats, kind):
    a = Image.new('RGB', (2048, 2048))
    for i, m in enumerate(mats):
        a.paste(Image.open(OUT + m + '_' + kind + '.png').convert('RGB'), ((i % 2) * 1024, (i // 2) * 1024))
    return a


def main():
    os.makedirs(OUT, exist_ok=True)
    for m in TERRAIN + PROPS:
        im = load(m)
        im.save(OUT + m + '_albedo.png', optimize=True)
        normal_map(im).save(OUT + m + '_normal.png', optimize=True)
    # vegetation props (alpha-tested foliage, bark for trunks)
    foliage = load('foliage')
    foliage.save(OUT + 'foliage_albedo.png', optimize=True)
    normal_map(foliage).save(OUT + 'foliage_normal.png', optimize=True)
    bark = load('bark')
    bark.save(OUT + 'bark_albedo.png', optimize=True)
    normal_map(bark).save(OUT + 'bark_normal.png', optimize=True)
    for name, mats in (('terrain', TERRAIN), ('prop', PROPS)):
        for kind in ('albedo', 'normal'):
            atlas(mats, kind).save(OUT + name + '_' + kind + '_atlas.png', optimize=True)
    total = sum(os.path.getsize(OUT + f) for f in os.listdir(OUT))
    print('materials:', len(os.listdir(OUT)), 'files,', round(total / 1048576, 1), 'MB')


if __name__ == '__main__':
    main()
