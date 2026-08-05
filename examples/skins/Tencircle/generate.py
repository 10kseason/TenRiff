# -*- coding: utf-8 -*-
"""Generate the "Tencircle" TenRiff skin — an osu!-flavoured lobby.

Aimed at players moving over from osu!, so the lobby leans on the cues they
already read at a glance: hot pink on near-black plum, a drifting field of
up-pointing triangles, hit-circle rings with approach rings, and a rounded
wordmark. Every asset here is drawn from scratch — nothing is imported from
another game.

Only the lobby slots are produced; gameplay stays on TenRiff's native gear.

Run:  python generate.py
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageOps

SKIN = os.path.dirname(os.path.abspath(__file__))

# ---------- palette ----------
PINK = (255, 102, 171)
PINK_LIGHT = (255, 162, 206)
PINK_DEEP = (214, 51, 132)
MAGENTA = (120, 26, 84)
PLUM_800 = (60, 14, 48)
PLUM_950 = (22, 6, 20)
INK = (34, 4, 22)
CREAM = (255, 238, 247)
WHITE = (255, 255, 255)


def font(size, family="rounded"):
    names = {
        # Arial Rounded MT Bold carries the soft osu!-ish wordmark shape.
        "rounded": ["ARLRDBD.TTF", "segoeuib.ttf", "arialbd.ttf"],
        "bold": ["segoeuib.ttf", "arialbd.ttf"],
        "semibold": ["seguisb.ttf", "segoeuib.ttf", "arialbd.ttf"],
    }[family]
    for n in names:
        p = os.path.join(r"C:\Windows\Fonts", n)
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(len(a)))


def linear_gradient(size, c1, c2, horizontal=False):
    """RGBA gradient image; c1/c2 are RGB or RGBA."""
    if len(c1) == 3:
        c1 = c1 + (255,)
    if len(c2) == 3:
        c2 = c2 + (255,)
    w, h = size
    n = w if horizontal else h
    strip = Image.new("RGBA", (n, 1) if horizontal else (1, n))
    px = strip.load()
    for i in range(n):
        c = lerp(c1, c2, i / max(1, n - 1))
        if horizontal:
            px[i, 0] = c
        else:
            px[0, i] = c
    return strip.resize(size)


def glow_blob(size, color, alpha):
    """Radial glow: solid color, alpha fading from center to edge."""
    g = Image.radial_gradient("L").resize(size)   # 0 center -> 255 edge
    g = ImageOps.invert(g)                        # 255 center -> 0 edge
    g = g.point(lambda v: int(v * alpha / 255))
    blob = Image.new("RGBA", size, color + (0,))
    blob.putalpha(g)
    return blob


def paste(base, layer, pos=(0, 0)):
    base.alpha_composite(layer, pos)


def alpha_layer(size, color, painter, ss=3):
    """Anti-aliased shape layer of a single colour.

    Holding RGB constant across the whole layer and letting `painter` vary only
    alpha means the supersample downscale can never smear black into the shape
    edges — the usual halo you get from resizing a transparent RGBA layer.
    """
    w, h = size
    big = Image.new("RGBA", (w * ss, h * ss), color + (0,))
    painter(ImageDraw.Draw(big, "RGBA"), ss)
    return big.resize((w, h), Image.BOX)


def disk_mask(size, cx, cy, r, ss=4):
    w, h = size
    big = Image.new("L", (w * ss, h * ss), 0)
    ImageDraw.Draw(big).ellipse(
        [(cx - r) * ss, (cy - r) * ss, (cx + r) * ss, (cy + r) * ss], fill=255)
    return big.resize((w, h), Image.BOX)


def equilateral(cx, cy, side):
    """Up-pointing equilateral triangle, the osu! menu motif."""
    r = side / math.sqrt(3.0)          # circumradius
    return [(cx + r * math.sin(a), cy - r * math.cos(a))
            for a in (0.0, 2.0 * math.pi / 3.0, 4.0 * math.pi / 3.0)]


def triangle_field(size, rng, count, side_range, alpha_range, color, blur=0.0):
    def painter(d, s):
        for _ in range(count):
            cx = rng.uniform(-0.06, 1.06) * size[0]
            cy = rng.uniform(-0.10, 1.10) * size[1]
            side = rng.uniform(*side_range)
            a = int(rng.uniform(*alpha_range))
            d.polygon([(x * s, y * s) for x, y in equilateral(cx, cy, side)],
                      fill=color + (a,))
    layer = alpha_layer(size, color, painter)
    return layer.filter(ImageFilter.GaussianBlur(blur)) if blur else layer


def circle_layer(size, color, rings=(), disks=(), ss=3):
    """rings: (cx, cy, radius, thickness, alpha). disks: (cx, cy, radius, alpha)."""
    def painter(d, s):
        for cx, cy, r, a in disks:
            d.ellipse([(cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s],
                      fill=color + (int(a),))
        for cx, cy, r, t, a in rings:
            d.ellipse([(cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s],
                      outline=color + (int(a),), width=max(1, int(t * s)))
    return alpha_layer(size, color, painter, ss=ss)


def draw_tracked(d, xy, text, f, fill, tracking):
    x, y = xy
    for ch in text:
        d.text((x, y), ch, font=f, fill=fill, anchor="lm")
        x += d.textlength(ch, font=f) + tracking
    return x - tracking


# =====================================================================
# lobby/background.png
# =====================================================================
def lobby_background():
    W, H = 1920, 1080
    img = linear_gradient((W, H), MAGENTA, PLUM_950)
    paste(img, linear_gradient((W, H), PLUM_800 + (0,), PLUM_950 + (170,)))

    # ambient pink light, brightest where osu!'s menu glow sits
    paste(img, glow_blob((1900, 1700), PINK_DEEP, 168), (-560, -820))
    paste(img, glow_blob((1500, 1400), PINK, 104), (1180, -470))
    paste(img, glow_blob((2000, 1250), PINK_DEEP, 76), (-40, 590))

    # triangle field in three depths — far ones blurred, near ones crisp
    rng = random.Random(21804)
    paste(img, triangle_field((W, H), rng, 22, (320, 680), (10, 18), PINK_LIGHT, blur=11))
    paste(img, triangle_field((W, H), rng, 46, (130, 320), (13, 26), PINK, blur=3))
    paste(img, triangle_field((W, H), rng, 72, (44, 150), (20, 42), PINK_LIGHT))

    # hit circles: pink body, white ring, wider approach ring.
    # Placed in the side margins the menu screens leave open.
    bodies = [(250, 336, 182, 52), (1690, 812, 230, 44), (966, 1030, 118, 34)]
    rings = [
        (250, 336, 196, 26, 78), (250, 336, 268, 6, 54),
        (1690, 812, 246, 30, 66), (1690, 812, 332, 7, 44),
        (966, 1030, 130, 18, 46),
    ]
    paste(img, circle_layer((W, H), PINK, disks=bodies))
    paste(img, circle_layer((W, H), CREAM, rings=rings))

    # calm the middle so the menu's glass buttons keep their contrast
    wash = glow_blob((1740, 1100), (0, 0, 0), 52)
    paste(img, wash, (90, 0))

    # vignette
    vig = glow_blob((W * 2, H * 2), (0, 0, 0), 255).resize((W, H))
    vig = ImageOps.invert(vig.split()[3]).point(lambda v: int(v * 0.34))
    black = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    black.putalpha(vig)
    paste(img, black)

    img.convert("RGB").save(os.path.join(SKIN, "lobby", "background.png"))


# =====================================================================
# lobby/logo.png — hit-circle mark + rounded wordmark
# =====================================================================
def logo():
    H = 270
    text_left = 286
    word = "TenRiff"
    f_word = font(132, "rounded")
    f_num = font(84, "rounded")
    f_sub = font(28, "semibold")

    probe = ImageDraw.Draw(Image.new("RGBA", (8, 8)))
    word_w = probe.textlength(word, font=f_word) + probe.textlength("!", font=f_word)
    W = int(text_left + word_w + 28)

    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    cx, cy = 138, 135
    ring_r, ring_w = 104, 19
    body_r = ring_r - ring_w / 2 + 1

    # pink body with a bloom behind it
    body = linear_gradient((W, H), PINK_LIGHT, PINK_DEEP)
    body.putalpha(disk_mask((W, H), cx, cy, body_r))
    paste(img, body.filter(ImageFilter.GaussianBlur(20)))
    paste(img, body)

    # white ring + approach ring
    paste(img, circle_layer((W, H), CREAM, ss=4,
                            rings=[(cx, cy, ring_r, ring_w, 255),
                                   (cx, cy, 128, 6, 150)]))
    # combo-number nod to the name
    ImageDraw.Draw(img).text((cx, cy + 2), "10", font=f_num, fill=WHITE + (255,),
                             anchor="mm")

    # wordmark: white body, pink "!" the way osu! punctuates its own
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ds = ImageDraw.Draw(shadow)
    ds.text((text_left, 112), word + "!", font=f_word, fill=INK + (160,), anchor="lm")
    paste(img, shadow.filter(ImageFilter.GaussianBlur(9)), (0, 6))

    d = ImageDraw.Draw(img)
    d.text((text_left, 112), word, font=f_word, fill=WHITE + (255,), anchor="lm")
    d.text((text_left + probe.textlength(word, font=f_word), 112), "!", font=f_word,
           fill=PINK + (255,), anchor="lm")

    draw_tracked(d, (text_left + 5, 206), "RHYTHM ENGINE", f_sub,
                 PINK_LIGHT + (238,), 7.0)

    img.save(os.path.join(SKIN, "lobby", "logo.png"))


def main():
    os.makedirs(os.path.join(SKIN, "lobby"), exist_ok=True)
    lobby_background()
    logo()
    print("done")


if __name__ == "__main__":
    main()
