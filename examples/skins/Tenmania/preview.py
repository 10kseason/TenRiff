# -*- coding: utf-8 -*-
"""Render PREVIEW_4K-7K-10K-16K.png straight from skin.json.

This is a mock of the renderer, not a screenshot: it rebuilds the playfield
with the same numbers `src/render/MenuWindow.cpp` uses, so a lane that looks
cramped here is cramped in game.

  field bounds        x 470..1450, y 0..1080 in the 1920x1080 base space
  lane width          field_width / keys
  note width          lane_width - 2 * 12px divider gap, times note_width_ratio
  note height         2 * 11px, times note_height_ratio
  judgement line      field_top + field_height * judgement_line_position
  receptor box        lane_left+2 .. lane_right-2, judgement line .. field_bottom
                      (the `full_lane_receptors` path, named osu_gear_layout
                      in the renderer)

Assets are cropped to their alpha bounding box before being stretched, because
that is what the renderer's own alpha trim does to them.

Run:  python preview.py
"""
import io
import json
import os

from PIL import Image, ImageDraw, ImageFont

SKIN = os.path.dirname(os.path.abspath(__file__))
MODES = (4, 7, 10, 16)

# ---- renderer constants (src/render/MenuWindow.cpp) ----
BASE_H = 1080
FIELD_LEFT, FIELD_RIGHT = 470.0, 1450.0
FIELD_TOP, FIELD_BOTTOM = 0.0, 1080.0
NOTE_DIVIDER_GAP_PX = 12.0
NOTE_HEAD_HALF_H = 11.0
NOTE_TAIL_HALF_H = 9.0

CROP_L, CROP_R = 448, 1472
PANEL_SCALE = 0.40
GUTTER, MARGIN, HEADER, CAPTION = 26, 30, 96, 46
INK = (10, 4, 16)
PINK = (255, 102, 171)
PINK_LIGHT = (255, 183, 216)
CREAM = (255, 240, 247)


def font(size, weight="semibold"):
    names = {"bold": ["segoeuib.ttf", "arialbd.ttf"],
             "semibold": ["seguisb.ttf", "segoeuib.ttf", "arialbd.ttf"]}[weight]
    for n in names:
        p = os.path.join(r"C:\Windows\Fonts", n)
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def hex_rgba(value, fallback_alpha=255):
    v = value.lstrip("#")
    r, g, b = int(v[0:2], 16), int(v[2:4], 16), int(v[4:6], 16)
    a = int(v[6:8], 16) if len(v) == 8 else fallback_alpha
    return (r, g, b, a)


_cache = {}


def asset(rel):
    """Load and pre-trim like the renderer's alpha bounding box pass."""
    if rel not in _cache:
        img = Image.open(os.path.join(SKIN, rel)).convert("RGBA")
        box = img.split()[3].getbbox()
        _cache[rel] = img.crop(box) if box else img
    return _cache[rel]


def blit(base, rel, rect, opacity=1.0):
    left, top, right, bottom = (int(round(v)) for v in rect)
    w, h = max(1, right - left), max(1, bottom - top)
    layer = asset(rel).resize((w, h), Image.LANCZOS)
    if opacity < 1.0:
        layer.putalpha(layer.split()[3].point(lambda v: int(v * opacity)))
    base.alpha_composite(layer, (left, top))


def lane_paths(gameplay, keys):
    merged = dict(gameplay)
    merged.update(gameplay["modes"]["%dk" % keys])
    lanes = merged["lane_map"][:keys]
    slots = {}
    for slot in ("note", "hold_head", "hold_body", "hold_tail", "key_idle", "key_pressed"):
        slots[slot] = [merged[slot].replace("{lane}", lanes[i]) for i in range(keys)]
    return merged, lanes, slots


def stage(gameplay, theme, keys, pressed_lanes, notes, holds):
    merged, lanes, slots = lane_paths(gameplay, keys)
    img = Image.open(os.path.join(SKIN, gameplay["background"])).convert("RGBA")
    dim = Image.new("RGBA", img.size, (0, 0, 0, int(255 * (1.0 - gameplay["background_opacity"]))))
    img.alpha_composite(dim)

    field_w = FIELD_RIGHT - FIELD_LEFT
    lane_w = field_w / keys
    note_w = max(16.0, lane_w - 2.0 * NOTE_DIVIDER_GAP_PX) * merged["note_width_ratio"]
    head_h = NOTE_HEAD_HALF_H * merged["note_height_ratio"]
    tail_h = NOTE_TAIL_HALF_H * merged["note_height_ratio"]
    hit_y = FIELD_TOP + (FIELD_BOTTOM - FIELD_TOP) * merged["judgement_line_position"]
    lane_left = lambda i: FIELD_LEFT + lane_w * i
    lane_center = lambda i: lane_left(i) + lane_w * 0.5

    d = ImageDraw.Draw(img, "RGBA")
    # black_playfield + per-lane tint at lane_background_opacity
    d.rectangle([FIELD_LEFT, FIELD_TOP, FIELD_RIGHT, FIELD_BOTTOM], fill=(0, 0, 0, 255))
    tint = int(255 * merged["lane_background_opacity"])
    for i in range(keys):
        c = hex_rgba(merged["lane_colors"][i])
        d.rectangle([lane_left(i), FIELD_TOP, lane_left(i) + lane_w, FIELD_BOTTOM],
                    fill=(c[0], c[1], c[2], tint))
    divider = hex_rgba(theme["lane_divider"], 150)
    for i in range(1, keys):
        d.line([lane_left(i), FIELD_TOP, lane_left(i), FIELD_BOTTOM],
               fill=(divider[0], divider[1], divider[2], 90), width=2)
    # judgement line, then the gear overlay, then receptors, then notes
    jl = hex_rgba(theme["judgement_line"])
    d.rectangle([FIELD_LEFT, hit_y - 3, FIELD_RIGHT, hit_y + 3], fill=jl)
    blit(img, gameplay["gear"], (FIELD_LEFT, FIELD_TOP, FIELD_RIGHT, FIELD_BOTTOM))

    for i in range(keys):
        slot = "key_pressed" if i in pressed_lanes else "key_idle"
        blit(img, slots[slot][i], (lane_left(i) + 2, hit_y, lane_left(i) + lane_w - 2,
                                   FIELD_BOTTOM - 2))

    # The body spans tail-bottom to head-top, and the renderer clamps its
    # bitmap opacity to 0.60 (MenuWindow_draw_gameplay_body.inl).
    body_opacity = min(0.60, merged["hold_body_opacity"])
    for lane, head_y, tail_y in holds:
        cx = lane_center(lane)
        blit(img, slots["hold_body"][lane],
             (cx - note_w / 2, tail_y + tail_h, cx + note_w / 2, head_y - head_h),
             body_opacity)
        blit(img, slots["hold_tail"][lane],
             (cx - note_w / 2, tail_y - tail_h, cx + note_w / 2, tail_y + tail_h))
        blit(img, slots["hold_head"][lane],
             (cx - note_w / 2, head_y - head_h, cx + note_w / 2, head_y + head_h))
    for lane, y in notes:
        cx = lane_center(lane)
        blit(img, slots["note"][lane], (cx - note_w / 2, y - head_h, cx + note_w / 2, y + head_h))

    crop = img.crop((CROP_L, 0, CROP_R, BASE_H))
    size = (int(crop.width * PANEL_SCALE), int(crop.height * PANEL_SCALE))
    return crop.resize(size, Image.LANCZOS), lanes, lane_w, note_w


def chart(keys):
    """A deterministic sample pattern: jacks, a stair, and two long notes."""
    notes, holds, pressed = [], [], set()
    for i in range(keys):
        base = 210 + (i * 137) % 520
        notes.append((i, base))
        notes.append((i, base + 190 + (i % 3) * 55))
        if i % 4 == 2:
            notes.append((i, base + 430))
    for i in (1, keys - 2):
        holds.append((i, 700, 330))
    pressed.update({0, keys // 2, keys - 1})
    notes = [(l, y) for l, y in notes if y < 860]
    return notes, holds, pressed


def main():
    doc = json.load(io.open(os.path.join(SKIN, "skin.json"), encoding="utf-8"))
    gameplay, theme = doc["gameplay"], doc["theme"]

    panels = []
    for keys in MODES:
        notes, holds, pressed = chart(keys)
        panels.append((keys,) + stage(gameplay, theme, keys, pressed, notes, holds))

    pw, ph = panels[0][1].size
    W = MARGIN * 2 + pw * len(panels) + GUTTER * (len(panels) - 1)
    H = HEADER + ph + CAPTION + MARGIN
    sheet = Image.new("RGBA", (W, H), INK + (255,))
    d = ImageDraw.Draw(sheet, "RGBA")
    d.rectangle([0, 0, W, HEADER - 18], fill=(24, 9, 32, 255))
    d.text((MARGIN, 30), "Tenmania", font=font(38, "bold"), fill=CREAM + (255,), anchor="lm")
    d.text((MARGIN + 196, 34), "gameplay preview  ·  rebuilt with the renderer's own geometry",
           font=font(20), fill=PINK_LIGHT + (230,), anchor="lm")

    for idx, (keys, panel, lanes, lane_w, note_w) in enumerate(panels):
        x = MARGIN + idx * (pw + GUTTER)
        sheet.alpha_composite(panel, (x, HEADER))
        d.rectangle([x, HEADER, x + pw - 1, HEADER + ph - 1], outline=(122, 53, 96, 255))
        d.text((x + 4, HEADER + ph + 16), "%dK" % keys, font=font(24, "bold"),
               fill=PINK + (255,), anchor="lm")
        d.text((x + 52, HEADER + ph + 17),
               "lane %.0fpx · note %.0fpx · %s" % (lane_w, note_w, "".join(l[0].upper() for l in lanes)),
               font=font(17), fill=(199, 154, 179, 255), anchor="lm")

    out = os.path.join(SKIN, "PREVIEW_4K-7K-10K-16K.png")
    sheet.convert("RGB").save(out)
    print("wrote", os.path.basename(out), sheet.size)


if __name__ == "__main__":
    main()
