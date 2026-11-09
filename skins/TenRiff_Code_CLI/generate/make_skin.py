"""Generate the "TenRiff Code / CLI" skin: a terminal-session look for TenRiff.

Unofficial fan skin inspired by the feel of a coding-agent terminal session
(warm near-black terminal, coral accent, monospace type, rounded box frames,
spinner glyphs). Every image is drawn procedurally here; no third-party art,
logos or mascots are copied.

Usage:  python -X utf8 generate/make_skin.py [--out <skin folder>]   (default: this skin folder)
Requires Pillow and fontTools. Fonts come from the Windows font folder
(Cascadia Mono, Segoe UI Symbol for dingbats, Malgun Gothic for Hangul).
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFilter, ImageFont

SKIN = Path(__file__).resolve().parents[1]  # this skin folder
FONTS = Path("C:/Windows/Fonts")
W, H = 1920, 1080

# ---------------------------------------------------------------- palette
BG = "#0E0D0C"          # terminal background (warm near-black)
PANEL = "#151413"
CARD = "#191816"
BUTTON = "#1D1B19"
BORDER = "#45413C"      # box frame gray
TEXT = "#ECE9E4"        # terminal foreground
MUTED = "#9C978F"       # secondary text
DIM = "#5F5A54"
CORAL = "#D77757"       # agent accent
CORAL_LIGHT = "#EB9A7D"
LAVENDER = "#A9B1F7"    # suggestion / permission
GREEN = "#4EBA65"       # success
RED = "#FF6B80"         # error
YELLOW = "#F2C14E"      # warning
PURPLE = "#AF87FF"      # auto-accept
PINK = "#F26BB5"        # shell mode
TEAL = "#5DB3A8"        # plan mode
DIFF_ADD = "#1F4F2A"
DIFF_DEL = "#5E2430"

SCREENS = ("quick_setup", "title", "options", "multiplayer", "song_select",
           "session_mix", "song_browser", "settings", "settings_audio",
           "settings_graphics", "settings_skins", "settings_input", "settings_calibration",
           "mode_select", "mode_mods", "keymap", "keymap_confirm", "onnx_upscaler_confirm",
           "keymap_test", "result")


def rgb(hex_color: str, alpha: int = 255):
    value = hex_color.lstrip("#")
    return tuple(int(value[i:i + 2], 16) for i in (0, 2, 4)) + (alpha,)


# ---------------------------------------------------------------- terminal text
class Term:
    """Monospace grid renderer on its own transparent layer, with glyph fallback.

    Everything is drawn at full strength and the layer is composited with one
    opacity, so antialiasing stays clean and the opacity really applies.
    """

    _mono_cmap = None

    def __init__(self, size_wh, size: int, line: float = 1.42):
        self.layer = Image.new("RGBA", size_wh, (0, 0, 0, 0))
        self.draw = ImageDraw.Draw(self.layer)
        self.mono = ImageFont.truetype(str(FONTS / "CascadiaMono.ttf"), size)
        self.symbol = ImageFont.truetype(str(FONTS / "seguisym.ttf"), round(size * 0.94))
        self.hangul = ImageFont.truetype(str(FONTS / "malgun.ttf"), round(size * 1.04))
        self.cell = self.mono.getlength("M")
        self.line = round(size * line)
        self.size = size
        if Term._mono_cmap is None:
            Term._mono_cmap = TTFont(str(FONTS / "CascadiaMono.ttf"), lazy=True).getBestCmap()

    @staticmethod
    def wide(ch: str) -> bool:
        code = ord(ch)
        return 0x1100 <= code <= 0x115F or 0xAC00 <= code <= 0xD7A3 or 0x3130 <= code <= 0x318F

    def width(self, text: str) -> int:
        return sum(2 if self.wide(ch) else 1 for ch in text)

    def xy(self, col: float, row: float, x0: float, y0: float):
        return x0 + col * self.cell, y0 + row * self.line

    def text(self, col, row, text, color, x0=0.0, y0=0.0, bg=None):
        """Draw text at a grid cell; returns the column after the last glyph."""
        x, y = self.xy(col, row, x0, y0)
        if bg:
            span = self.width(text) * self.cell
            self.draw.rectangle([x, y - self.line * 0.10, x + span, y + self.line * 0.86], fill=rgb(bg))
        fill = rgb(color)
        for ch in text:
            cells = 2 if self.wide(ch) else 1
            if ch != " ":
                if self.wide(ch):
                    font, dy = self.hangul, -round(self.size * 0.12)
                elif ord(ch) in Term._mono_cmap:
                    font, dy = self.mono, 0
                else:
                    font, dy = self.symbol, round(self.size * 0.02)
                gx = x + (cells * self.cell - font.getlength(ch)) / 2
                self.draw.text((gx, y + dy), ch, font=font, fill=fill)
            x += cells * self.cell
        return col + self.width(text)

    def spans(self, col, row, parts, x0=0.0, y0=0.0):
        for text, color in parts:
            col = self.text(col, row, text, color, x0, y0)
        return col

    def frame(self, col, row, cols, rows, color, x0=0.0, y0=0.0, width=2):
        """Rounded frame on the cell grid, like a terminal box-drawing panel."""
        left, top = self.xy(col + 0.5, row + 0.5, x0, y0)
        right, bottom = self.xy(col + cols - 0.5, row + rows - 0.5, x0, y0)
        self.draw.rounded_rectangle([left, top, right, bottom], radius=round(self.cell * 0.9),
                                    outline=rgb(color), width=width)

    def cursor(self, col, row, color, x0=0.0, y0=0.0):
        x, y = self.xy(col, row, x0, y0)
        self.draw.rectangle([x, y + self.line * 0.04, x + self.cell - 1, y + self.size * 1.2], fill=rgb(color))

    def onto(self, image: Image.Image, opacity: float) -> Image.Image:
        layer = self.layer.copy()
        layer.putalpha(self.layer.getchannel("A").point(lambda v: round(v * opacity)))
        return Image.alpha_composite(image, layer)


def vignette(image: Image.Image, strength: float = 0.45):
    """Darken the corners gently so panels sit on a calm field."""
    w, h = image.size
    keep = Image.new("L", image.size, 0)
    ImageDraw.Draw(keep).ellipse([-w * 0.22, -h * 0.32, w * 1.22, h * 1.32], fill=255)
    keep = keep.filter(ImageFilter.GaussianBlur(170))
    dark = Image.blend(image, Image.new("RGBA", image.size, rgb("#000000")), strength)
    return Image.composite(image, dark, keep)


def base_canvas(color: str = BG) -> Image.Image:
    image = Image.new("RGBA", (W, H), rgb(color))
    # Faint warm glow in the upper left, like a lit terminal pane.
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([-420, -520, 980, 520], fill=rgb(CORAL, 26))
    glow = glow.filter(ImageFilter.GaussianBlur(220))
    return Image.alpha_composite(image, glow)


def scanlines(image: Image.Image, alpha: int = 10) -> Image.Image:
    layer = Image.new("RGBA", image.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    for y in range(0, image.size[1], 3):
        draw.line([(0, y), (image.size[0], y)], fill=(0, 0, 0, alpha))
    return Image.alpha_composite(image, layer)


# ---------------------------------------------------------------- transcript snippets
def transcript_left(term: Term, x0: float, y0: float):
    """A short agent session: welcome box, prompt, tool calls, spinner."""
    term.frame(0, 0, 34, 6, CORAL, x0, y0)
    term.spans(2, 1, [("✻ ", CORAL), ("Welcome to TenRiff Code!", TEXT)], x0, y0)
    term.text(4, 3, "/help for help", MUTED, x0, y0)
    term.text(4, 4, "cwd: ~/songs", MUTED, x0, y0)
    term.spans(0, 7, [("> ", MUTED), ("이 곡 풀콤 부탁해", TEXT)], x0, y0)
    term.spans(0, 9, [("⏺ ", TEXT), ("I'll load the chart and start.", TEXT)], x0, y0)
    term.spans(0, 11, [("⏺ ", GREEN), ("Read", TEXT), ("(chart.bms)", MUTED)], x0, y0)
    term.spans(0, 12, [("  ⎿  ", DIM), ("Read 1,824 notes", MUTED)], x0, y0)
    term.spans(0, 14, [("⏺ ", GREEN), ("Bash", TEXT), ("(tenriff play --keys 7)", MUTED)], x0, y0)
    term.spans(0, 15, [("  ⎿  ", DIM), ("Running…", MUTED)], x0, y0)
    term.spans(0, 17, [("✶ ", CORAL), ("Riffing… ", CORAL), ("(esc to interrupt)", DIM)], x0, y0)


def transcript_right(term: Term, x0: float, y0: float):
    term.spans(0, 0, [("⏺ ", GREEN), ("Update", TEXT), ("(combo.log)", MUTED)], x0, y0)
    term.spans(0, 1, [("  ⎿  ", DIM), ("Updated combo.log", MUTED)], x0, y0)
    term.text(5, 2, " 41 - MISS            ", "#F2A7B4", x0, y0, bg=DIFF_DEL)
    term.text(5, 3, " 41 + PERFECT GREAT   ", "#A8E3B4", x0, y0, bg=DIFF_ADD)
    term.spans(0, 5, [("⏺ ", GREEN), ("Judge", TEXT), ("(lane 4)", MUTED)], x0, y0)
    term.spans(0, 6, [("  ⎿  ", DIM), ("+0.0 ms · ", MUTED), ("PG", YELLOW)], x0, y0)
    term.spans(0, 8, [("✻ ", CORAL), ("Keep going.", TEXT)], x0, y0)


def status_line(term: Term, x0: float, y0: float):
    term.spans(0, 0, [("⏵⏵ ", PURPLE), ("accept riffs on ", PURPLE), ("(shift+tab to cycle)", DIM)], x0, y0)


def prompt_box(term: Term, x0: float, y0: float, cols: int, text: str):
    term.frame(0, 0, cols, 3, BORDER, x0, y0)
    end = term.spans(2, 1, [("> ", MUTED), (text, TEXT)], x0, y0)
    term.cursor(end, 1, TEXT, x0, y0)
    term.text(2, 3, "? for shortcuts", DIM, x0, y0)


# ---------------------------------------------------------------- images
def lobby_background() -> Image.Image:
    # Menus are dense with native panels, so the shared field stays text-free:
    # a lit terminal pane, faint scanlines and a soft vignette only.
    return vignette(scanlines(base_canvas(), 8))


def title_background() -> Image.Image:
    image = base_canvas()
    # Prompt box in the free band between the title buttons and the language button.
    prompt = Term(image.size, 21)
    prompt_box(prompt, 1016, 764, 34, "play --single")
    prompt.spans(22, 3, [("✶ ", CORAL), ("Riffing…", CORAL)], 1016, 764)
    image = prompt.onto(image, 0.62)
    return vignette(scanlines(image, 8))


def gameplay_background() -> Image.Image:
    image = Image.new("RGBA", (W, H), rgb("#0B0A09"))
    left = Term(image.size, 18)
    transcript_left(left, 26, 28)
    image = left.onto(image, 0.42)
    right = Term(image.size, 18)
    transcript_right(right, 1468, 300)
    image = right.onto(image, 0.36)
    status = Term(image.size, 16)
    status.spans(0, 0, [("⏵⏵ ", PURPLE), ("accept riffs on", PURPLE)], 1468, 560)
    status.spans(0, 1, [("✻ ", CORAL), ("esc to interrupt", DIM)], 1468, 560)
    image = status.onto(image, 0.36)
    return scanlines(image, 10)


# ---------------------------------------------------------------- gameplay vectors
def layer(x, y, width, height, color, mix_value=1.0, alpha=1.0, radius=0):
    return dict(x=x, y=y, width=width, height=height, color=color,
                mix=round(mix_value, 3), alpha=round(alpha, 3), radius=radius)


def sprites():
    # Layers 0-1 are reserved outlines (border opacity); the solid body is index 2+.
    note = [layer(0, 3, 128, 26, "#000000", 1.0, 0.85, 0),
            layer(1, 4, 126, 24, "#FFFFFF", 0.0, 0.35, 0),
            layer(1, 4, 126, 24, "#FFFFFF", 0.0, 1.0, 0),       # solid block (lane color)
            layer(1, 4, 126, 2, "#FFFFFF", 0.62, 0.70, 0),     # top highlight
            layer(1, 25, 126, 3, "#000000", 1.0, 0.28, 0)]     # bottom shade
    tail = [layer(0, 10, 128, 13, "#000000", 1.0, 0.85, 0),
            layer(1, 11, 126, 11, "#FFFFFF", 0.0, 0.35, 0),
            layer(1, 11, 126, 11, "#FFFFFF", 0.0, 1.0, 0),
            layer(1, 11, 126, 2, "#FFFFFF", 0.62, 0.62, 0)]
    key_idle = [layer(0, 0, 128, 256, "#070605", radius=4),
                layer(1, 1, 126, 252, "#3B3733", radius=4),
                layer(2, 3, 124, 248, "#171513", radius=3),
                layer(3, 4, 122, 2, "#8C857C", alpha=0.22),
                layer(14, 236, 100, 4, "#FFFFFF", 0.0, 0.55, 0)]        # lane-colored cursor underline
    key_pressed = [layer(0, 0, 128, 256, "#070605", radius=4),
                   layer(1, 1, 126, 252, "#FFFFFF", 0.20, 1.0, 4),
                   layer(2, 3, 124, 248, "#1C1815", 0.80, 1.0, 3),
                   layer(3, 4, 122, 3, "#FFFFFF", 0.30, 0.85),
                   layer(10, 226, 108, 18, "#FFFFFF", 0.0, 1.0, 0)]     # lit block cursor
    return {"note": note, "hold_head": note, "hold_tail": tail,
            "key_idle": key_idle, "key_pressed": key_pressed}


def lane_colors(count: int):
    white, second, accent = TEXT, LAVENDER, CORAL
    if count == 4:
        return [white, second, second, white]
    result = [white if min(i, count - 1 - i) % 2 == 0 else second for i in range(count)]
    if count % 2:
        result[count // 2] = accent
    if count in (8, 12, 16):
        result[0] = result[-1] = accent
    return result


# ---------------------------------------------------------------- manifest
def manifest():
    mono = "Cascadia Mono"
    menu_fonts = ["title", "option", "body", "mono", "menu_button", "header",
                  "song_record_label", "song_record_value", "song_record_detail", "song_title",
                  "song_artist", "result_score", "song_metadata_value", "result_metric",
                  "result_judgement", "result_accuracy", "result_label", "result_body",
                  "result_action", "hud", "stats_label", "stats_value"]
    # Icon and grade faces stay as shipped; every readable text role becomes terminal type.
    fonts = {name: mono for name in menu_fonts}
    backgrounds = {screen: "cli/lobby.png" for screen in SCREENS}
    backgrounds["title"] = "cli/title.png"
    opacities = {screen: 0.82 for screen in SCREENS}
    opacities["title"] = 0.94
    gameplay_metrics = {
        "key_min_height": 44, "key_max_height": 118, "key_inset": 2, "key_inset_ratio": 0.05,
        "key_bottom_gap": 4, "key_face_gap": 5, "judgement_line_width": 2,
        "judgement_glow_height": 5, "lane_divider_opacity": 0.10, "hold_edge_mix": 0.0,
        "hold_core_mix": 0.72, "timing_half_width": 112, "timing_height": 4,
        "timing_range_ms": 80, "title_font_size": 24, "score_font_size": 30,
        "body_font_size": 16, "timing_font_size": 16, "judgement_font_size": 40,
        "combo_font_size": 38, "key_label_font_size": 13}
    gameplay_colors = {
        "key_well": "#070605", "key_led": CORAL, "chassis": "#100F0E",
        "judgement_line": CORAL, "judgement_glow": CORAL + "22",
        # LN body reads as a framed selection: lane-colored rails around a flat, dimmed fill.
        "hold_edge": "#FFFFFF", "hold_core": "#2A2622", "hold_shadow": "#2A2622",
        "burst_core": "#FFF3EC", "burst_trail": CORAL,
        "title": TEXT, "score": TEXT, "body": MUTED, "combo": CORAL,
        "judgement": TEXT, "timing": "#CFC9C1", "key_label": "#B9B2A9"}
    modes = {f"{n}k": {"lane_colors": lane_colors(n)} for n in range(1, 17)}
    modes["7+1"] = {"lane_colors": [CORAL] + lane_colors(7)}
    return {
        "$schema": "../../docs/tenriff-skin.schema.json",
        "format": "tenriff-skin",
        "version": 1,
        "name": "TenRiff Code / CLI",
        "author": "Claude (Claude Code)",
        "lobby": {
            "renderer": "native",
            "background": "cli/lobby.png",
            "logo": "",
            "background_opacity": 0.82,
            "screen_backgrounds": backgrounds,
            "screen_opacities": opacities,
            # Looping videos (tools/make_videos.py); the stills above stay the fallback.
            "background_video": "cli/lobby-loop.mp4",
            "screen_videos": {"title": "cli/title-loop.mp4"},
        },
        "theme": {
            "accent": CORAL, "text": TEXT, "muted": MUTED,
            "card": CARD + "F5", "panel": PANEL + "EE", "footer": "#0B0A09F5",
            "button": BUTTON + "EB", "button_selected": CORAL + "2E",
            "border": BORDER, "judgement_line": CORAL, "lane_divider": "#8C857C",
            "scene_primary": CORAL, "scene_secondary": LAVENDER, "scene_background": BG,
        },
        "native": {
            "metrics": {
                "icon.stroke_width": 1.8, "options_grid.radius": 6, "options_grid.tint": 0.006,
                "options_grid.selected_tint": 0.05, "options_grid.border_width": 1,
                "options_grid.selected_border_width": 2, "generic.line_height": 27,
                "font.header.size": 46, "font.menu_button.size": 30, "font.song_title.size": 22,
                "font.song_record_value.size": 40, "font.result_metric.size": 48,
                "font.result_accuracy.size": 62, "font.result_score.size": 58,
                "font.title.size": 28, "font.option.size": 20, "font.result_action.size": 26,
            },
            "colors": {
                "palette.05090f": "#0B0A09", "palette.101820": "#181614",
                "palette.63e9f2": CORAL, "palette.101d32": "#171412",
                "palette.101421": "#0F0E0D", "palette.211c35": "#1B1512",
                "palette.a499ff": LAVENDER, "palette.a9f5ff": CORAL_LIGHT,
                "palette.c5f7ff": "#F3C7B5", "palette.233344": "#2E2A26",
                "palette.020914": "#0A0908", "palette.0b1620": "#141210",
                "palette.05070a": "#090807", "palette.a4e8f3": CORAL,
                "palette.f4f1ff": TEXT, "palette.ecf6ff": TEXT, "palette.f7fafd": "#F7F4F0",
                "title.primary": "#1E1A17", "title.primary_text": TEXT,
                "title.primary_detail": MUTED,
                "options.border": "#33302C", "options.value": TEXT,
                "options.icon.key_mode": CORAL, "options.icon.keymap": LAVENDER,
                "options.icon.skin": PINK, "options.icon.graphics": PURPLE,
                "options.icon.audio": GREEN, "options.icon.input": YELLOW,
                "options.icon.latency": RED, "options.icon.profile": TEAL,
                "options.icon.mode": CORAL_LIGHT, "options.icon.key_test": "#7FB2F0",
            },
            "motion": {"intensity": 0.35, "entry_seconds": 0.2},
            "fonts": fonts,
        },
        "gameplay": {
            "description": "Terminal session look: monospace HUD, block-cursor notes, coral judgement line.",
            "renderer": "native",
            "background": "cli/gameplay.png",
            "background_video": "cli/gameplay-loop.mp4",
            "background_opacity": 0.9,
            "gear": "", "note": "", "hold_head": "", "hold_body": "", "hold_tail": "",
            "key_idle": "", "key_pressed": "",
            "note_shape": "rect", "note_aspect": "stretch", "note_border": False,
            "black_playfield": True, "lane_background_opacity": 0.02,
            "show_lane_dividers": True, "show_judgement_line": True,
            "show_gear_boundary_line": False, "show_hold_tail": True, "hold_tail_taper": False,
            "judgement_line_glow": True, "judgement_line_brightness": 1.0,
            "key_pulse": True, "key_pulse_brightness": 0.42,
            "hit_burst_style": "pixels", "key_label_position": "bottom",
            "hold_body_opacity": 0.78, "visual_opacity": 1, "note_outline_opacity": 0.55,
            "native": {
                "metrics": gameplay_metrics,
                "colors": gameplay_colors,
                "motion": {"press_response": 120, "release_response": 32, "press_depth": 2,
                           "pressed_light": 0.7, "hit_light": 0.2, "burst_rise": 36,
                           "burst_height": 16, "combo_duration_ms": 140, "combo_scale": 1.04,
                           "combo_lift": -2, "judgement_duration_ms": 480},
                "fonts": {slot: mono for slot in ("title", "score", "body", "combo",
                                                  "judgement", "timing", "key_label")},
                "sprites": sprites(),
            },
            "modes": modes,
        },
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=str(SKIN))
    args = parser.parse_args()
    out = Path(args.out)
    (out / "cli").mkdir(parents=True, exist_ok=True)
    lobby_background().convert("RGB").save(out / "cli" / "lobby.png", optimize=True)
    title_background().convert("RGB").save(out / "cli" / "title.png", optimize=True)
    gameplay_background().convert("RGB").save(out / "cli" / "gameplay.png", optimize=True)
    stale = out / "cli" / "logo.png"  # earlier draft asset; the native menus draw their own wordmark
    if stale.exists():
        stale.unlink()
    text = json.dumps(manifest(), ensure_ascii=False, indent=2) + "\n"
    (out / "skin.json").write_text(text, encoding="utf-8", newline="\n")
    print("wrote", out)


if __name__ == "__main__":
    main()
