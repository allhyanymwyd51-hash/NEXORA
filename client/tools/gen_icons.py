#!/usr/bin/env python3
"""NEXORA brand + icon asset generator.

Produces the launcher icon at every density, the adaptive-icon layers, and the in-game brand
textures (glow sprite, particle atlas). Everything is vector-drawn here so the mark is crisp at
48px and at 1024px, and there are no third-party marks in the product.
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RES = os.path.join(ROOT, "res")
ASSETS = os.path.join(ROOT, "assets", "textures")
os.makedirs(ASSETS, exist_ok=True)

BG_TOP = (10, 14, 34)
BG_BOTTOM = (26, 14, 58)
ACCENT = (124, 77, 255)
ACCENT2 = (56, 225, 255)


def vgrad(w, h, top, bottom):
    img = Image.new("RGB", (w, h), top)
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        d.line([(0, y), (w, y)], fill=c)
    return img


def radial_glow(size, color, radius_ratio=1.0, alpha=180):
    img = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(img)
    cx = cy = size / 2
    steps = 48
    for i in range(steps, 0, -1):
        r = (size / 2) * (i / steps) * radius_ratio
        a = int(alpha * (1 - i / steps) ** 1.7)
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=a)
    img = img.filter(ImageFilter.GaussianBlur(size // 24))
    glow = Image.new("RGBA", (size, size), color + (0,))
    glow.putalpha(img)
    return glow


def draw_mark(img, cx, cy, scale, glow=True):
    """The NEXORA mark: a monogram N built from two risers and a light beam."""
    w, h = img.size
    d = ImageDraw.Draw(img, "RGBA")
    bar_w = int(scale * 0.155)
    height = int(scale * 0.62)
    gap = int(scale * 0.30)
    left = cx - gap
    right = cx + gap
    top = cy - height / 2
    bottom = cy + height / 2
    radius = bar_w / 2

    if glow:
        layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        ld = ImageDraw.Draw(layer, "RGBA")
        for gx, gy, col in ((left, top, ACCENT2), (right, bottom, ACCENT)):
            ld.ellipse([gx - scale * 0.30, gy - scale * 0.30, gx + scale * 0.30, gy + scale * 0.30], fill=col + (110,))
        layer = layer.filter(ImageFilter.GaussianBlur(scale * 0.11))
        img.alpha_composite(layer)

    # diagonal beam with a cool gradient
    beam = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    bd = ImageDraw.Draw(beam, "RGBA")
    x0, y0 = left, top + bar_w * 0.1
    x1, y1 = right, bottom - bar_w * 0.1
    steps = 64
    for i in range(steps):
        t = i / (steps - 1)
        px = x0 + (x1 - x0) * t
        py = y0 + (y1 - y0) * t
        col = tuple(int(ACCENT2[k] + (ACCENT[k] - ACCENT2[k]) * t) for k in range(3))
        r = bar_w * 0.40
        bd.ellipse([px - r, py - r, px + r, py + r], fill=col + (255,))
    img.alpha_composite(beam)
    # risers on top of the beam so the monogram reads cleanly at 48px
    for x in (left, right):
        d.rounded_rectangle([x - bar_w / 2, top, x + bar_w / 2, bottom], radius=radius, fill=(238, 242, 255, 255))
    # inner highlight on the risers
    hi = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    hd = ImageDraw.Draw(hi, "RGBA")
    for x in (left, right):
        hd.rounded_rectangle([x - bar_w / 2 + bar_w * 0.08, top + bar_w * 0.10,
                              x - bar_w / 2 + bar_w * 0.34, bottom - bar_w * 0.10],
                             radius=radius * 0.5, fill=(255, 255, 255, 90))
    img.alpha_composite(hi)


def make_icon(size, rounded=True, padding_ratio=0.0, bg=True, transparent_bg=False):
    S = size * 4  # supersample
    base = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    if bg:
        grad = vgrad(S, S, BG_TOP, BG_BOTTOM).convert("RGBA")
        # violet / cyan blooms, clipped to the icon silhouette so nothing bleeds outside
        blooms = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        blooms.alpha_composite(radial_glow(S, ACCENT, 1.15, 120).transpose(Image.FLIP_LEFT_RIGHT))
        blooms.alpha_composite(radial_glow(S, ACCENT2, 0.9, 60))
        mask = Image.new("L", (S, S), 0)
        md = ImageDraw.Draw(mask)
        if rounded:
            md.rounded_rectangle([0, 0, S - 1, S - 1], radius=int(S * 0.235), fill=255)
        else:
            md.rectangle([0, 0, S - 1, S - 1], fill=255)
        grad.alpha_composite(blooms)
        base.paste(grad, (0, 0), mask)
        # subtle border light
        border = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        bd = ImageDraw.Draw(border)
        bd.rounded_rectangle([int(S * 0.004), int(S * 0.004), S - int(S * 0.004), S - int(S * 0.004)],
                             radius=int(S * 0.235), outline=(150, 160, 255, 70), width=max(1, S // 200))
        base.alpha_composite(border)
    elif not transparent_bg:
        base.alpha_composite(vgrad(S, S, BG_TOP, BG_BOTTOM).convert("RGBA"))
    draw_mark(base, S / 2, S / 2, S * (0.92 - padding_ratio * 2))
    if bg and rounded:
        sil = Image.new("L", (S, S), 0)
        ImageDraw.Draw(sil).rounded_rectangle([0, 0, S - 1, S - 1], radius=int(S * 0.235), fill=255)
        base.putalpha(Image.composite(base.getchannel("A"), Image.new("L", (S, S), 0), sil))
    return base.resize((size, size), Image.LANCZOS)


def main():
    sizes = {"mipmap-mdpi": 48, "mipmap-hdpi": 72, "mipmap-xhdpi": 96, "mipmap-xxhdpi": 144,
             "mipmap-xxxhdpi": 192}
    for folder, px in sizes.items():
        d = os.path.join(RES, folder)
        os.makedirs(d, exist_ok=True)
        make_icon(px).save(os.path.join(d, "ic_launcher.png"))
        # adaptive foreground: mark only, transparent, 108dp canvas with 66dp safe zone
        fg = Image.new("RGBA", (px * 108 // 48, px * 108 // 48), (0, 0, 0, 0))
        fg.paste(make_icon(px, rounded=False, bg=False, padding_ratio=0.22),
                 (0, 0), make_icon(px, rounded=False, bg=False, padding_ratio=0.22))
        fgi = Image.new("RGBA", fg.size, (0, 0, 0, 0))
        mark = make_icon(int(fg.size[0] * 0.72), rounded=False, bg=False)
        off = (fg.size[0] - mark.size[0]) // 2
        fgi.alpha_composite(mark, (off, off))
        fgi.save(os.path.join(d, "ic_launcher_foreground.png"))
        bgimg = Image.new("RGBA", (fg.size[0] // 2, fg.size[1] // 2), BG_TOP + (255,))
        bgimg.save(os.path.join(d, "ic_launcher_background.png"))

    # Play-store / README artwork 1024
    make_icon(1024).save(os.path.join(ROOT, "..", "docs", "icon-1024.png")) if os.path.isdir(
        os.path.join(ROOT, "..", "docs")) else make_icon(1024).save(os.path.join(ASSETS, "icon-1024.png"))

    # ---- in-game brand textures -------------------------------------------------
    # glow sprite (soft circular light used by particles and UI bloom)
    g = radial_glow(256, (180, 200, 255), 1.0, 200)
    g.save(os.path.join(ASSETS, "glow.png"))
    g2 = radial_glow(128, ACCENT, 1.0, 220)
    g2.save(os.path.join(ASSETS, "glow_violet.png"))
    g3 = radial_glow(128, ACCENT2, 1.0, 220)
    g3.save(os.path.join(ASSETS, "glow_cyan.png"))

    # particle atlas: 4x4 of soft dots with different sizes/colors
    atlas = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    rnd = random.Random(7)
    for i in range(16):
        cx = (i % 4) * 64 + 32
        cy = (i // 4) * 64 + 32
        col = [(255, 255, 255), ACCENT, ACCENT2, (255, 214, 140)][i % 4]
        r = 6 + (i % 4) * 4
        dot = radial_glow(64, col, 1.0, 230)
        dot = dot.crop((0, 0, 64, 64))
        dot = dot.resize((int(r * 4), int(r * 4)), Image.LANCZOS)
        atlas.alpha_composite(dot, (cx - dot.size[0] // 2, cy - dot.size[1] // 2))
    atlas.save(os.path.join(ASSETS, "particles.png"))

    # logo lockup for the title screen (mark + wordmark space)
    lock = Image.new("RGBA", (1024, 320), (0, 0, 0, 0))
    mark = make_icon(320, rounded=True)
    lock.alpha_composite(mark, (24, 0))
    draw = ImageDraw.Draw(lock)
    # letter-spaced wordmark using the DejaVu geometry as a base shape (text is rendered by the
    # engine with the bundled Cairo font; this is only a fallback texture)
    lock.save(os.path.join(ASSETS, "logo_lockup.png"))
    print("[icon] generated", sizes, "and textures in", ASSETS)


if __name__ == "__main__":
    main()
