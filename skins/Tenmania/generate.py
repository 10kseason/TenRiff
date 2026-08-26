# -*- coding: utf-8 -*-
"""Generate the "Tenmania" TenRiff skin — an osu!mania-flavoured stage.

Tencircle brought the osu! menu over; this one brings the stage. Near-black
plum playfield, osu pink accent, and the white / blue / pink column colouring
that mania players already read without thinking: white and blue alternate the
way IIDX lays out its piano keys, pink marks a scratch column or the centre
anchor of an odd key count.

Every asset is drawn from scratch here — nothing is imported from another game.

Art is authored so the renderer's stretching stays invisible:

  * notes and hold heads/tails vary only vertically, so the wide 4K bar and the
    narrow 16K bar are the same picture at different widths;
  * hold bodies vary only horizontally, so a two-beat LN and a two-bar LN look
    identical;
  * receptors are horizontal bands, because `full_lane_receptors` hands them a
    tall lane-wide box whose aspect swings from 1.4 at 4K to 0.36 at 16K;
  * `gameplay/gear.png` is authored at 980x1080, the renderer's own base-space
    playfield rect, so its stage rails land at 1:1.

Run:  python generate.py
"""
import math
import os
import random

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont, ImageOps

SKIN = os.path.dirname(os.path.abspath(__file__))

# ---------- palette ----------
PINK = (255, 102, 171)
PINK_LIGHT = (255, 183, 216)
PINK_DEEP = (176, 32, 105)
BLUE = (79, 169, 255)
BLUE_LIGHT = (168, 214, 255)
BLUE_DEEP = (23, 96, 190)
SILVER = (150, 163, 196)
STEEL = (226, 232, 246)
WHITE = (255, 255, 255)
CREAM = (255, 240, 247)
PLUM_800 = (58, 16, 48)
PLUM_900 = (36, 9, 32)
PLUM_950 = (21, 6, 20)
INK = (10, 4, 16)

# Per-column art sets. The key is the `lane_map` name used in skin.json, so
# `gameplay/note-{lane}.png` resolves straight out of this table.
LANES = {
    "white": {"light": WHITE, "mid": STEEL, "deep": SILVER, "cap": WHITE},
    "blue": {"light": BLUE_LIGHT, "mid": BLUE, "deep": BLUE_DEEP, "cap": (214, 238, 255)},
    "pink": {"light": PINK_LIGHT, "mid": PINK, "deep": PINK_DEEP, "cap": (255, 224, 239)},
}


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
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(len(a)))


def rgba(color, alpha):
    return (color[0], color[1], color[2], int(alpha))


def multi_gradient(size, stops, horizontal=False):
    """RGBA gradient through (position, RGB or RGBA) stops sorted by position."""
    stops = [(p, c if len(c) == 4 else c + (255,)) for p, c in stops]
    w, h = size
    n = w if horizontal else h
    strip = Image.new("RGBA", (n, 1) if horizontal else (1, n))
    px = strip.load()
    for i in range(n):
        t = i / max(1, n - 1)
        lo = stops[0]
        hi = stops[-1]
        for a, b in zip(stops, stops[1:]):
            if a[0] <= t <= b[0]:
                lo, hi = a, b
                break
        span = max(1e-6, hi[0] - lo[0])
        c = lerp(lo[1], hi[1], (t - lo[0]) / span)
        if horizontal:
            px[i, 0] = c
        else:
            px[0, i] = c
    return strip.resize(size)


def linear_gradient(size, c1, c2, horizontal=False):
    return multi_gradient(size, [(0.0, c1), (1.0, c2)], horizontal)


def rounded_mask(size, radius, scale=4):
    """Anti-aliased rounded-rect mask."""
    w, h = size
    big = Image.new("L", (w * scale, h * scale), 0)
    ImageDraw.Draw(big).rounded_rectangle(
        [0, 0, w * scale - 1, h * scale - 1], radius * scale, fill=255)
    return big.resize(size, Image.LANCZOS)


def glow_blob(size, color, alpha):
    """Radial glow: solid color, alpha fading to nothing at the ellipse edge.

    PIL's radial gradient only reaches white in the corners, so taking it as-is
    leaves ~29% alpha along the edge midpoints and the blob shows up as a
    rectangle. Cutting the ramp at the inscribed circle removes that seam.
    """
    edge = 181.0
    g = Image.radial_gradient("L").point(
        lambda v: int(max(0.0, (edge - v) / edge) ** 1.35 * alpha))
    blob = Image.new("RGBA", size, color + (0,))
    blob.putalpha(g.resize(size, Image.BILINEAR))
    return blob


def paste(base, layer, pos=(0, 0)):
    base.alpha_composite(layer, pos)


def alpha_layer(size, color, painter, ss=3):
    """Anti-aliased shape layer of a single colour.

    Holding RGB constant across the layer and letting `painter` vary only alpha
    keeps the supersample downscale from smearing black into the shape edges.
    """
    w, h = size
    big = Image.new("RGBA", (w * ss, h * ss), color + (0,))
    painter(ImageDraw.Draw(big, "RGBA"), ss)
    return big.resize((w, h), Image.BOX)


def equilateral(cx, cy, side):
    """Up-pointing equilateral triangle, the osu! menu motif."""
    r = side / math.sqrt(3.0)
    return [(cx + r * math.sin(a), cy - r * math.cos(a))
            for a in (0.0, 2.0 * math.pi / 3.0, 4.0 * math.pi / 3.0)]


def triangle_field(size, rng, count, side_range, alpha_range, color, blur=0.0,
                   y_bias=1.0):
    """Drifting triangles. y_bias > 1 crowds them toward the bottom edge."""
    def painter(d, s):
        for _ in range(count):
            cx = rng.uniform(-0.06, 1.06) * size[0]
            cy = (rng.uniform(0.0, 1.0) ** (1.0 / y_bias)) * 1.16 * size[1] - 0.08 * size[1]
            side = rng.uniform(*side_range)
            a = int(rng.uniform(*alpha_range))
            d.polygon([(x * s, y * s) for x, y in equilateral(cx, cy, side)],
                      fill=color + (a,))
    layer = alpha_layer(size, color, painter)
    return layer.filter(ImageFilter.GaussianBlur(blur)) if blur else layer


def vignette(size, strength):
    g = ImageOps.invert(Image.radial_gradient("L").resize(size))
    mask = g.point(lambda v: int((255 - v) * strength))
    black = Image.new("RGBA", size, (0, 0, 0, 255))
    black.putalpha(mask)
    return black


def draw_tracked(d, xy, text, f, fill, tracking):
    x, y = xy
    for ch in text:
        d.text((x, y), ch, font=f, fill=fill, anchor="lm")
        x += d.textlength(ch, font=f) + tracking
    return x - tracking


# =====================================================================
# notes — mania bars. Detail runs in horizontal bands only, so the same
# picture survives a 221px-wide 4K lane and a 37px-wide 16K lane.
# =====================================================================
def note_bar(path, lane, kind="note"):
    """kind: note | head | tail."""
    W, H = 256, 64
    pad = 5
    iw, ih = W - pad * 2, H - pad * 2
    r = 11
    c = LANES[lane]
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    mask = rounded_mask((iw, ih), r)

    if kind == "tail":
        # Tails read as the end of the body, so they sit a stop darker and lose
        # the leading cap that makes a head look hittable.
        stops = [(0.0, c["mid"]), (0.55, c["deep"]), (1.0, c["deep"])]
    else:
        stops = [(0.0, c["light"]), (0.42, c["mid"]), (1.0, c["deep"])]
    core = multi_gradient((iw, ih), stops)
    core.putalpha(mask)

    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    paste(glow, core, (pad, pad))
    glow = glow.filter(ImageFilter.GaussianBlur(6))
    glow.putalpha(glow.split()[3].point(lambda v: int(v * 0.55)))
    paste(img, glow)
    paste(img, core, (pad, pad))

    d = ImageDraw.Draw(img)
    if kind != "tail":
        # Leading cap: the bright edge a mania player actually times against.
        cap_h = 9
        cap = Image.new("RGBA", (iw, cap_h), (0, 0, 0, 0))
        ImageDraw.Draw(cap).rounded_rectangle([0, 0, iw - 1, cap_h + r], r,
                                              fill=rgba(c["cap"], 255))
        cap.putalpha(ImageChops.multiply(cap.split()[3], mask.crop((0, 0, iw, cap_h))))
        paste(img, cap, (pad, pad))
    if kind == "head":
        # Head continues into a body, so its bottom edge is left open.
        d.line([pad + 8, H - pad - 3, W - pad - 9, H - pad - 3],
               fill=rgba(c["light"], 120), width=3)

    outline = rgba(WHITE, 190) if lane != "white" else rgba(PLUM_900, 150)
    d.rounded_rectangle([pad, pad, W - pad - 1, H - pad - 1], r, outline=outline, width=3)
    img.save(path)


def hold_body(path, lane):
    """Constant along the lane, so any LN length stretches without artefacts."""
    W, H = 256, 64
    pad = 10
    c = LANES[lane]
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    iw = W - pad * 2
    # Hollow tube: lit rails down both sides, the middle left thin enough that a
    # tap note landing on top of a long note still reads as the brighter shape.
    fill = multi_gradient(
        (iw, H),
        [(0.0, rgba(c["mid"], 230)), (0.14, rgba(c["deep"], 132)),
         (0.5, rgba(c["deep"], 74)), (0.86, rgba(c["deep"], 132)),
         (1.0, rgba(c["mid"], 230))],
        horizontal=True)
    paste(img, fill, (pad, 0))
    d = ImageDraw.Draw(img)
    d.rectangle([pad, 0, pad + 4, H], fill=rgba(c["cap"], 235))
    d.rectangle([W - pad - 5, 0, W - pad - 1, H], fill=rgba(c["cap"], 235))
    img.save(path)


# =====================================================================
# receptors — the osu!mania stage bottom. `full_lane_receptors` gives these
# the whole lane from the judgement line down, so they are pure horizontal
# bands: nothing here cares how wide the column ends up.
# =====================================================================
def receptor(path, lane, pressed):
    W, H = 256, 224
    c = LANES[lane]
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))

    if pressed:
        # Stage light: the column floods with its own colour on contact.
        body = multi_gradient(
            (W, H),
            [(0.0, rgba(c["light"], 235)), (0.14, rgba(c["mid"], 205)),
             (0.55, rgba(c["deep"], 110)), (1.0, rgba(c["deep"], 26))])
        paste(img, body)
        bloom = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        ImageDraw.Draw(bloom).rectangle([0, 0, W, 46], fill=rgba(c["cap"], 210))
        paste(img, bloom.filter(ImageFilter.GaussianBlur(18)))
        cap_alpha, rail_alpha, key_alpha = 255, 120, 150
    else:
        body = multi_gradient(
            (W, H),
            [(0.0, rgba(c["deep"], 130)), (0.22, rgba(PLUM_900, 190)),
             (1.0, rgba(INK, 224))])
        paste(img, body)
        cap_alpha, rail_alpha, key_alpha = 210, 70, 64

    d = ImageDraw.Draw(img)
    # Cap line sits exactly on the judgement line — the top edge of this rect.
    d.rectangle([0, 0, W, 6], fill=rgba(c["cap"], cap_alpha))
    d.rectangle([0, 7, W, 10], fill=rgba(c["mid"], cap_alpha // 2))
    # Stage rails down both sides of the column.
    d.rectangle([0, 0, 3, H], fill=rgba(WHITE, rail_alpha))
    d.rectangle([W - 4, 0, W, H], fill=rgba(WHITE, rail_alpha))
    # Keycap silhouette, the hint that this column is a physical key.
    d.rounded_rectangle([26, 44, W - 27, H - 34], 18,
                        outline=rgba(c["light"], key_alpha), width=5)
    d.rectangle([0, H - 5, W, H], fill=rgba(c["mid"], cap_alpha // 2))
    img.save(path)


# =====================================================================
# gameplay/gear.png — stage frame drawn under the notes across the whole
# playfield rect (470..1450 x 0..1080 in the renderer's base space).
# =====================================================================
def gear():
    W, H = 980, 1080
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))

    # Barely-there tint so a bright BGA cannot wash the lanes out.
    paste(img, multi_gradient((W, H), [(0.0, rgba(INK, 96)), (0.42, rgba(INK, 34)),
                                       (1.0, rgba(INK, 70))]))

    # Spawn edge: notes fade in instead of popping at the top border.
    paste(img, linear_gradient((W, 190), rgba(INK, 225), rgba(INK, 0)))

    # osu! triangles, faint, only near the top where no note is being read yet.
    rng = random.Random(52719)
    tri = triangle_field((W, 460), rng, 26, (60, 190), (7, 15), PINK_LIGHT, blur=2)
    paste(img, tri)

    # Stage rails: pink at the top, cooling to white at the judgement line.
    rail_w = 9
    rail = multi_gradient((rail_w, H), [(0.0, rgba(PINK_DEEP, 150)),
                                        (0.55, rgba(PINK, 225)),
                                        (0.86, rgba(CREAM, 245)),
                                        (1.0, rgba(CREAM, 245))])
    haze_w = 74
    haze = multi_gradient((haze_w, 1), [(0.0, rgba(PINK, 60)), (1.0, rgba(PINK, 0))],
                          horizontal=True).resize((haze_w, H))
    paste(img, haze, (0, 0))
    paste(img, ImageOps.mirror(haze), (W - haze_w, 0))
    paste(img, rail, (0, 0))
    paste(img, rail, (W - rail_w, 0))

    d = ImageDraw.Draw(img)
    d.line([rail_w + 2, 0, rail_w + 2, H], fill=rgba(WHITE, 60), width=2)
    d.line([W - rail_w - 3, 0, W - rail_w - 3, H], fill=rgba(WHITE, 60), width=2)
    img.save(os.path.join(SKIN, "gameplay", "gear.png"))


# =====================================================================
# gameplay/background.png — decoration lives in the margins; the middle
# 980px the playfield covers stays close to black.
# =====================================================================
def gameplay_background():
    W, H = 1920, 1080
    img = linear_gradient((W, H), PLUM_900, INK)
    paste(img, glow_blob((1280, 1560), PINK_DEEP, 148), (-560, -340))
    paste(img, glow_blob((1280, 1560), PINK_DEEP, 128), (1200, -270))
    paste(img, glow_blob((980, 980), BLUE_DEEP, 74), (-300, 500))
    paste(img, glow_blob((980, 980), BLUE_DEEP, 66), (1240, 460))

    rng = random.Random(9021)
    paste(img, triangle_field((W, H), rng, 20, (300, 620), (8, 14), PINK, blur=10))
    paste(img, triangle_field((W, H), rng, 44, (90, 260), (10, 20), PINK_LIGHT, blur=3,
                              y_bias=1.7))

    # Hold the playfield band dark; the field spans x 470..1450.
    band = multi_gradient((W, 1), [(0.0, rgba(INK, 0)), (0.225, rgba(INK, 205)),
                                   (0.5, rgba(INK, 232)), (0.775, rgba(INK, 205)),
                                   (1.0, rgba(INK, 0))], horizontal=True)
    paste(img, band.resize((W, H)))
    paste(img, vignette((W, H), 0.42))
    img.convert("RGB").save(os.path.join(SKIN, "gameplay", "background.png"))


# =====================================================================
# lobby/background.png
# =====================================================================
def lobby_background():
    W, H = 1920, 1080
    img = multi_gradient((W, H), [(0.0, PLUM_900), (0.5, PLUM_950), (1.0, INK)])
    paste(img, glow_blob((1780, 1420), PINK_DEEP, 104), (-620, -720))
    paste(img, glow_blob((1360, 1180), PINK, 52), (1180, -430))
    paste(img, glow_blob((1900, 1100), PINK_DEEP, 42), (10, 660))

    # Three depths of triangles, crowded toward the bottom the way osu!'s own
    # menu lets them rise out of the footer. Kept at texture strength: the menu
    # panels have to stay the brightest thing on screen.
    rng = random.Random(30517)
    paste(img, triangle_field((W, H), rng, 18, (300, 620), (5, 10), PINK_LIGHT, blur=12,
                              y_bias=1.3))
    paste(img, triangle_field((W, H), rng, 40, (120, 300), (7, 14), PINK, blur=3,
                              y_bias=1.8))
    paste(img, triangle_field((W, H), rng, 64, (40, 140), (11, 22), PINK_LIGHT,
                              y_bias=2.4))

    # A mania stage rising out of the right margin: four lanes of falling bars
    # over a pink judgement line. It fades out toward the centre, where the
    # song list and the title buttons need a quiet ground.
    stage = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(stage, "RGBA")
    lane_w, gap, left, hit_y = 104, 16, 1440, 892
    for lane in range(4):
        x0 = left + lane * (lane_w + gap)
        sd.rectangle([x0, 96, x0 + lane_w, hit_y], fill=rgba(INK, 150))
        sd.line([x0 + lane_w + gap // 2, 96, x0 + lane_w + gap // 2, hit_y],
                fill=rgba(CREAM, 40), width=2)
    for lane, y in ((0, 236), (0, 452), (1, 318), (1, 664), (2, 168),
                    (2, 540), (3, 388), (3, 742)):
        x0 = left + lane * (lane_w + gap)
        col = PINK if lane in (0, 3) else BLUE_LIGHT
        sd.rounded_rectangle([x0 + 5, y, x0 + lane_w - 5, y + 24], 9, fill=rgba(col, 190))
    sd.rectangle([left - 8, hit_y - 5, left + 4 * (lane_w + gap) - gap + 8, hit_y + 5],
                 fill=rgba(PINK, 235))
    sd.rectangle([left - 8, hit_y + 6, left + 4 * (lane_w + gap) - gap + 8, H],
                 fill=rgba(PLUM_800, 130))
    reveal = multi_gradient((W, 1), [(0.0, rgba(INK, 0)), (0.56, rgba(INK, 0)),
                                     (0.80, rgba(INK, 190)), (1.0, rgba(INK, 190))],
                            horizontal=True).resize((W, H))
    top_fade = multi_gradient((1, H), [(0.0, rgba(INK, 0)), (0.16, rgba(INK, 255)),
                                       (0.84, rgba(INK, 255)), (1.0, rgba(INK, 0))]).resize((W, H))
    stage.putalpha(ImageChops.multiply(
        stage.split()[3],
        ImageChops.multiply(reveal.split()[3], top_fade.split()[3])))
    paste(img, stage.filter(ImageFilter.GaussianBlur(3.0)))

    # Calm the middle so the menu's glass panels keep their contrast.
    paste(img, glow_blob((1700, 1160), (0, 0, 0), 92), (110, -40))
    paste(img, vignette((W, H), 0.5))
    img.convert("RGB").save(os.path.join(SKIN, "lobby", "background.png"))


# =====================================================================
# lobby/logo.png — mini stage mark + rounded wordmark
# =====================================================================
def logo():
    H = 268
    mark = 214
    text_left = mark + 66
    word = "TenRiff"
    f_word = font(126, "rounded")
    f_sub = font(27, "semibold")

    probe = ImageDraw.Draw(Image.new("RGBA", (8, 8)))
    word_w = probe.textlength(word, font=f_word) + probe.textlength("!", font=f_word)
    W = int(text_left + word_w + 26)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))

    # Mark: a four-lane stage caught mid-chart.
    top = (H - mark) // 2
    panel = multi_gradient((mark, mark), [(0.0, rgba(PLUM_800, 255)), (1.0, rgba(INK, 255))])
    panel.putalpha(ImageChops.multiply(panel.split()[3], rounded_mask((mark, mark), 30)))
    bloom = panel.filter(ImageFilter.GaussianBlur(16))
    paste(img, bloom, (0, top))
    paste(img, panel, (0, top))

    stage = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(stage)
    pad, lanes, hit = 22, 4, top + mark - 52
    lane_w = (mark - pad * 2) / lanes
    for i in range(1, lanes):
        x = pad + lane_w * i
        sd.line([x, top + pad, x, hit], fill=rgba(CREAM, 46), width=2)
    for lane, y, col in ((0, top + 58, "white"), (1, top + 104, "blue"),
                         (2, top + 74, "blue"), (3, top + 128, "pink")):
        x0 = pad + lane_w * lane + 5
        sd.rounded_rectangle([x0, y, x0 + lane_w - 10, y + 17], 6,
                             fill=rgba(LANES[col]["mid"], 255))
    sd.rectangle([pad - 4, hit - 3, mark - pad + 4, hit + 3], fill=rgba(PINK, 255))
    paste(img, stage.filter(ImageFilter.GaussianBlur(5)))
    paste(img, stage)

    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, top, mark - 1, top + mark - 1], 30,
                        outline=rgba(PINK, 130), width=3)

    # Wordmark: white body, pink "!" the way osu! punctuates its own.
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).text((text_left, 108), word + "!", font=f_word,
                                fill=rgba(INK, 175), anchor="lm")
    paste(img, shadow.filter(ImageFilter.GaussianBlur(9)), (0, 6))
    d.text((text_left, 108), word, font=f_word, fill=rgba(WHITE, 255), anchor="lm")
    d.text((text_left + probe.textlength(word, font=f_word), 108), "!", font=f_word,
           fill=rgba(PINK, 255), anchor="lm")
    draw_tracked(d, (text_left + 5, 200), "MANIA STAGE", f_sub, rgba(PINK_LIGHT, 235), 7.5)

    img.save(os.path.join(SKIN, "lobby", "logo.png"))


# =====================================================================
def main():
    for sub in ("lobby", "gameplay"):
        os.makedirs(os.path.join(SKIN, sub), exist_ok=True)
    g = os.path.join(SKIN, "gameplay")

    lobby_background()
    logo()
    gameplay_background()
    gear()
    for lane in LANES:
        note_bar(os.path.join(g, "note-%s.png" % lane), lane, "note")
        note_bar(os.path.join(g, "hold-head-%s.png" % lane), lane, "head")
        note_bar(os.path.join(g, "hold-tail-%s.png" % lane), lane, "tail")
        hold_body(os.path.join(g, "hold-body-%s.png" % lane), lane)
        receptor(os.path.join(g, "key-idle-%s.png" % lane), lane, pressed=False)
        receptor(os.path.join(g, "key-pressed-%s.png" % lane), lane, pressed=True)
    print("done")


if __name__ == "__main__":
    main()
