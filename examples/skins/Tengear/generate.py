# -*- coding: utf-8 -*-
"""Generate the "Tengear" TenRiff skin — modern web-frontend aesthetic.

Design system (Tailwind-ish):
  slate-950 background, indigo(#6366F1)->violet(#8B5CF6) primary gradient,
  cyan(#22D3EE) accent for holds, glassmorphism playfield, button-style notes,
  keycap-style receptors.
"""
import os
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont, ImageOps

SKIN = r"D:\tenriff\examples\skins\Tengear"
SCRATCH = os.path.dirname(os.path.abspath(__file__))

# ---------- palette ----------
SLATE_950 = (7, 11, 20)
SLATE_930 = (11, 17, 32)
SLATE_900 = (15, 23, 42)
SLATE_700 = (51, 65, 85)
SLATE_400 = (148, 163, 184)
SLATE_300 = (203, 213, 225)
SLATE_50 = (248, 250, 252)
INDIGO = (99, 102, 241)
VIOLET = (139, 92, 246)
CYAN = (34, 211, 238)
SKY = (14, 165, 233)
WHITE = (255, 255, 255)


def font(size, weight="bold"):
    names = {
        "bold": ["segoeuib.ttf", "arialbd.ttf"],
        "semibold": ["seguisb.ttf", "segoeuib.ttf", "arialbd.ttf"],
        "regular": ["segoeui.ttf", "arial.ttf"],
    }[weight]
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
        t = i / max(1, n - 1)
        c = lerp(c1, c2, t)
        if horizontal:
            px[i, 0] = c
        else:
            px[0, i] = c
    return strip.resize(size)


def rounded_mask(size, radius, scale=4):
    """Anti-aliased rounded-rect mask."""
    w, h = size
    big = Image.new("L", (w * scale, h * scale), 0)
    d = ImageDraw.Draw(big)
    d.rounded_rectangle([0, 0, w * scale - 1, h * scale - 1], radius * scale, fill=255)
    return big.resize(size, Image.LANCZOS)


def glow_blob(size, color, alpha):
    """Radial glow: solid color, alpha fading from center to edge."""
    g = Image.radial_gradient("L").resize(size)   # 0 center -> 255 edge
    g = ImageOps.invert(g)                        # 255 center -> 0 edge
    g = g.point(lambda v: int(v * alpha / 255))
    blob = Image.new("RGBA", size, color + (0,))
    blob.putalpha(g)
    return blob


def dot_grid(size, step, r, color):
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for y in range(step // 2, size[1], step):
        for x in range(step // 2, size[0], step):
            d.ellipse([x - r, y - r, x + r, y + r], fill=color)
    return layer


def paste(base, layer, pos=(0, 0)):
    base.alpha_composite(layer, pos)


# =====================================================================
# lobby/background.png — SaaS landing-page hero
# =====================================================================
def lobby_background():
    W, H = 1920, 1080
    img = linear_gradient((W, H), SLATE_930, SLATE_950)

    # ambient glow blobs
    paste(img, glow_blob((1400, 1400), INDIGO, 60), (-500, -700))
    paste(img, glow_blob((1600, 1600), VIOLET, 55), (1000, -400))
    paste(img, glow_blob((1400, 1400), CYAN, 35), (300, 600))

    # dot grid
    paste(img, dot_grid((W, H), 48, 1, SLATE_400 + (26,)))

    # giant faint outline wordmark (hero text)
    txt = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(txt)
    f = font(300, "bold")
    tw = d.textlength("TENGEAR", font=f)
    d.text(((W - tw) / 2, H / 2 - 210), "TENGEAR", font=f,
           fill=(0, 0, 0, 0), stroke_width=3, stroke_fill=SLATE_400 + (34,))
    paste(img, txt)

    # gradient hairline under the wordmark
    line = linear_gradient((900, 3), INDIGO + (0,), VIOLET + (170,), horizontal=True)
    half = line.crop((0, 0, 450, 3))
    line2 = Image.new("RGBA", (900, 3))
    line2.alpha_composite(half, (0, 0))
    line2.alpha_composite(ImageOps.mirror(half), (450, 0))
    paste(img, line2, ((W - 900) // 2, H // 2 + 130))

    # faux top navbar (abstract, no text)
    nav = Image.new("RGBA", (W, 72), SLATE_900 + (140,))
    nd = ImageDraw.Draw(nav)
    nd.line([0, 71, W, 71], fill=SLATE_700 + (150,), width=1)
    # logo mark
    mark = Image.new("RGBA", (36, 36))
    grad = linear_gradient((36, 36), INDIGO, VIOLET, horizontal=True)
    grad.putalpha(rounded_mask((36, 36), 10))
    mark.alpha_composite(grad)
    nav.alpha_composite(mark, (48, 18))
    # menu pills
    x = 130
    for w in (72, 96, 60, 84):
        nd.rounded_rectangle([x, 30, x + w, 42], 6, fill=SLATE_400 + (56,))
        x += w + 36
    # right-side CTA button
    cta = linear_gradient((150, 40), INDIGO, VIOLET, horizontal=True)
    cta.putalpha(rounded_mask((150, 40), 20))
    nav.alpha_composite(cta, (W - 150 - 48, 16))
    paste(img, nav)

    # faux browser-window card, bottom-right (frontend nod)
    cw, ch = 560, 340
    card = Image.new("RGBA", (cw, ch))
    body = Image.new("RGBA", (cw, ch), SLATE_900 + (200,))
    body.putalpha(rounded_mask((cw, ch), 18).point(lambda v: int(v * 200 / 255)))
    card.alpha_composite(body)
    cd = ImageDraw.Draw(card)
    cd.rounded_rectangle([0, 0, cw - 1, ch - 1], 18, outline=SLATE_700 + (200,), width=2)
    cd.line([0, 48, cw, 48], fill=SLATE_700 + (170,), width=2)
    for i, col in enumerate([(248, 113, 113), (251, 191, 36), (52, 211, 153)]):
        cd.ellipse([22 + i * 28, 18, 34 + i * 28, 30], fill=col + (220,))
    # fake code lines
    widths = [180, 300, 240, 120, 280, 200, 340, 160]
    colors = [INDIGO, SLATE_400, VIOLET, CYAN, SLATE_400, INDIGO, SLATE_400, VIOLET]
    y = 76
    for w, c in zip(widths, colors):
        cd.rounded_rectangle([36, y, 36 + w, y + 12], 6, fill=c + (110,))
        y += 32
    paste(img, card, (W - cw - 72, H - ch - 84))

    # vignette
    vig = glow_blob((W * 2, H * 2), (0, 0, 0), 255).resize((W, H))
    vig = ImageOps.invert(vig.split()[3]).point(lambda v: int(v * 0.45))
    dark = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    dark.putalpha(vig)
    black = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    black.putalpha(vig)
    paste(img, black)

    img.convert("RGB").save(os.path.join(SKIN, "lobby", "background.png"))


# =====================================================================
# lobby/logo.png — web-style wordmark
# =====================================================================
def logo():
    W, H = 960, 256
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # icon: gradient rounded square with </>
    S = 176
    icon = linear_gradient((S, S), INDIGO, VIOLET, horizontal=True)
    icon.putalpha(rounded_mask((S, S), 44))
    glow = icon.filter(ImageFilter.GaussianBlur(18))
    paste(img, glow, (28, (H - S) // 2))
    paste(img, icon, (28, (H - S) // 2))
    fi = font(92, "bold")
    di = ImageDraw.Draw(img)
    tw = di.textlength("</>", font=fi)
    di.text((28 + (S - tw) / 2, (H - S) // 2 + S / 2 - 62), "</>", font=fi, fill=WHITE + (255,))

    # wordmark: "Ten" white + "Gear" gradient
    fw = font(128, "bold")
    x0 = 28 + S + 44
    ybase = H / 2 - 88
    d.text((x0, ybase), "Ten", font=fw, fill=WHITE + (255,))
    w_ten = d.textlength("Ten", font=fw)
    w_gear = d.textlength("Gear", font=fw)
    gtxt = Image.new("RGBA", (int(w_gear) + 8, 176), (0, 0, 0, 0))
    ImageDraw.Draw(gtxt).text((0, 0), "Gear", font=fw, fill=WHITE + (255,))
    ggrad = linear_gradient(gtxt.size, INDIGO, VIOLET, horizontal=True)
    ggrad.putalpha(gtxt.split()[3])
    paste(img, ggrad, (int(x0 + w_ten), int(ybase)))

    img.save(os.path.join(SKIN, "lobby", "logo.png"))


# =====================================================================
# gameplay/background.png — quiet dark stage
# =====================================================================
def gameplay_background():
    W, H = 1920, 1080
    img = linear_gradient((W, H), SLATE_950, (4, 7, 14))
    paste(img, glow_blob((1200, 1600), INDIGO, 34), (-500, -300))
    paste(img, glow_blob((1200, 1600), VIOLET, 30), (1250, -200))
    paste(img, glow_blob((1600, 900), CYAN, 16), (160, 640))
    paste(img, dot_grid((W, H), 56, 1, SLATE_400 + (14,)))
    img.convert("RGB").save(os.path.join(SKIN, "gameplay", "background.png"))


# =====================================================================
# gameplay/gear.png — glass card over the lanes
# =====================================================================
def gear():
    W, H = 768, 1536
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    # faint glass tint (native lane bg is already black; this only matters over BGA)
    fill = linear_gradient((W, H), SLATE_950 + (60,), SLATE_950 + (100,))
    paste(img, fill)

    # inward neon haze from each edge (cyan->violet vertically, fading inward)
    glow_w = 120
    col = linear_gradient((1, H), CYAN, VIOLET).resize((glow_w, H))
    fall = Image.new("L", (glow_w, 1))
    for x in range(glow_w):
        fall.putpixel((x, 0), int(52 * (1 - x / (glow_w - 1))))
    fall = fall.resize((glow_w, H))
    haze = col.copy()
    haze.putalpha(fall)
    paste(img, haze, (0, 0))
    paste(img, ImageOps.mirror(haze), (W - glow_w, 0))

    # bold side rails: vertical cyan->violet gradient, 24px
    rail_w = 24
    rail = linear_gradient((rail_w, H), CYAN + (245,), VIOLET + (245,))
    paste(img, rail, (0, 0))
    paste(img, rail, (W - rail_w, 0))
    # white inner hairline to pop the rail
    d = ImageDraw.Draw(img)
    d.line([rail_w + 2, 0, rail_w + 2, H], fill=WHITE + (70,), width=3)
    d.line([W - rail_w - 3, 0, W - rail_w - 3, H], fill=WHITE + (70,), width=3)

    # soft inner glow at the very top (spawn edge)
    top = linear_gradient((W, 180), INDIGO + (64,), INDIGO + (0,))
    paste(img, top, (0, 0))
    img.save(os.path.join(SKIN, "gameplay", "gear.png"))


# =====================================================================
# notes — button-style
# =====================================================================
def button_note(path, c1, c2, ghost=False):
    W, H = 256, 88
    pad = 6
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    iw, ih = W - pad * 2, H - pad * 2
    r = 30

    core = linear_gradient((iw, ih), c1, c2, horizontal=True)
    core.putalpha(rounded_mask((iw, ih), r))

    # soft glow behind
    glow = core.filter(ImageFilter.GaussianBlur(7))
    paste(img, glow, (pad, pad))
    paste(img, core, (pad, pad))

    # glossy top highlight
    hi = linear_gradient((iw, ih // 2), WHITE + (86,), WHITE + (0,))
    hi.putalpha(ImageChops.multiply(
        hi.split()[3], rounded_mask((iw, ih), r).crop((0, 0, iw, ih // 2))))
    paste(img, hi, (pad, pad))

    # border
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([pad, pad, pad + iw - 1, pad + ih - 1], r,
                        outline=WHITE + (120 if not ghost else 200,), width=3)
    if ghost:
        # hollow center for the tail: punch out the fill a bit
        inner = Image.new("RGBA", (iw - 16, ih - 16), (0, 0, 0, 0))
        m = rounded_mask((iw - 16, ih - 16), r - 8)
        cut = Image.new("L", (W, H), 0)
        cut.paste(m, (pad + 8, pad + 8))
        a = img.split()[3].point(lambda v: v)
        a.paste(a.point(lambda v: int(v * 0.35)), (0, 0), cut)
        img.putalpha(a)
        d = ImageDraw.Draw(img)
        d.rounded_rectangle([pad, pad, pad + iw - 1, pad + ih - 1], r,
                            outline=WHITE + (200,), width=3)
    img.save(path)


def hold_body():
    W, H = 256, 256
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    pad = 6
    iw = W - pad * 2
    # translucent horizontal gradient fill (cyan family)
    fill = linear_gradient((iw, H), CYAN + (96,), SKY + (96,), horizontal=True)
    paste(img, fill, (pad, 0))
    # bright vertical edges
    d = ImageDraw.Draw(img)
    d.rectangle([pad, 0, pad + 4, H], fill=CYAN + (210,))
    d.rectangle([pad + iw - 5, 0, pad + iw - 1, H], fill=SKY + (210,))
    img.save(os.path.join(SKIN, "gameplay", "hold-body.png"))


# =====================================================================
# keys — keycap / button states
# =====================================================================
def keycap(path, pressed, primary=True):
    """Receptor keycap. primary=True -> indigo/violet lane, False -> white lane."""
    W, H = 256, 180
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    pad = 6
    iw, ih = W - pad * 2, H - pad * 2
    r = 22

    if pressed:
        c1, c2 = (INDIGO, VIOLET) if primary else (SLATE_50, SLATE_300)
        core = linear_gradient((iw, ih), c1, c2, horizontal=True)
        core.putalpha(rounded_mask((iw, ih), r))
        glow = core.filter(ImageFilter.GaussianBlur(9))
        paste(img, glow, (pad, pad))
        paste(img, core, (pad, pad))
        hi = linear_gradient((iw, ih // 2), WHITE + (56,), WHITE + (0,))
        hi.putalpha(ImageChops.multiply(
            hi.split()[3], rounded_mask((iw, ih), r).crop((0, 0, iw, ih // 2))))
        paste(img, hi, (pad, pad))
        d = ImageDraw.Draw(img)
        d.rounded_rectangle([pad, pad, pad + iw - 1, pad + ih - 1], r,
                            outline=WHITE + (150,) if primary else INDIGO + (170,),
                            width=3)
        # bottom accent bar lights up cyan on press
        bar = linear_gradient((iw - 24, 10), CYAN, SKY, horizontal=True)
        bar.putalpha(rounded_mask((iw - 24, 10), 5))
        paste(img, bar, (pad + 12, pad + ih - 24))
    else:
        core = linear_gradient((iw, ih), (30, 41, 59, 245), SLATE_900 + (245,))
        core.putalpha(ImageChops.multiply(core.split()[3], rounded_mask((iw, ih), r)))
        paste(img, core, (pad, pad))
        hi = linear_gradient((iw, ih // 3), WHITE + (30,), WHITE + (0,))
        hi.putalpha(ImageChops.multiply(
            hi.split()[3], rounded_mask((iw, ih), r).crop((0, 0, iw, ih // 3))))
        paste(img, hi, (pad, pad))
        d = ImageDraw.Draw(img)
        d.rounded_rectangle([pad, pad, pad + iw - 1, pad + ih - 1], r,
                            outline=(100, 116, 139, 235), width=4)
        # bottom accent bar hints the lane color while idle
        accent = INDIGO + (235,) if primary else SLATE_300 + (220,)
        d.rounded_rectangle([pad + 12, pad + ih - 32, pad + iw - 12, pad + ih - 16], 7,
                            fill=accent)
    img.save(path)


# =====================================================================
def main():
    os.makedirs(os.path.join(SKIN, "lobby"), exist_ok=True)
    os.makedirs(os.path.join(SKIN, "gameplay"), exist_ok=True)

    lobby_background()
    logo()
    gameplay_background()
    gear()
    g = os.path.join(SKIN, "gameplay")
    button_note(os.path.join(g, "note-primary.png"), INDIGO, VIOLET)
    button_note(os.path.join(g, "note-neutral.png"), SLATE_50, SLATE_300)
    button_note(os.path.join(g, "hold-head.png"), CYAN, SKY)
    button_note(os.path.join(g, "hold-tail.png"), CYAN, SKY, ghost=True)
    hold_body()
    keycap(os.path.join(g, "key-idle-primary.png"), pressed=False, primary=True)
    keycap(os.path.join(g, "key-idle-neutral.png"), pressed=False, primary=False)
    keycap(os.path.join(g, "key-pressed-primary.png"), pressed=True, primary=True)
    keycap(os.path.join(g, "key-pressed-neutral.png"), pressed=True, primary=False)
    print("done")


if __name__ == "__main__":
    main()
