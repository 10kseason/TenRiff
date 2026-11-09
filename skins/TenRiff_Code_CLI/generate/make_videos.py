"""Render the TenRiff Code / CLI looping background videos (silent H.264 MP4).

  cli/lobby-loop.mp4     16 s  menus: drifting spinner glyphs, breathing pane glow
  cli/title-loop.mp4     16 s  lobby loop + prompt box that types a command and spins
  cli/gameplay-loop.mp4  24 s  agent session transcript typing out beside the field

Each loop ends exactly where it starts. Frames are drawn with Pillow (same
terminal renderer as make_skin.py) and piped to ffmpeg (libx264, yuv420p).
Usage: python -X utf8 generate/make_videos.py [--only lobby|title|gameplay] [--seconds N]
"""
from __future__ import annotations

import argparse
import math
import random
import shutil
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_skin as ms  # noqa: E402

SKIN = Path(__file__).resolve().parents[1]  # this skin folder
FPS = 30
SPINNER = ["·", "✢", "✳", "✶", "✻", "✽", "✻", "✶", "✳", "✢"]
VERBS = ["Riffing", "Syncopating", "Judging", "Grooving", "Charting", "Riffing"]


def ease(t: float) -> float:
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def typed(text: str, start: float, cps: float, t: float) -> str:
    if t < start:
        return ""
    return text[:min(len(text), int((t - start) * cps))]


# ---------------------------------------------------------------- shared layers
class Glyphs:
    """Drifting spinner glyphs; positions wrap exactly once per loop."""

    def __init__(self, count: int, seconds: float, seed: int = 7):
        rng = random.Random(seed)
        self.seconds = seconds
        self.items = []
        for _ in range(count):
            self.items.append(dict(
                x=rng.uniform(40, ms.W - 40), y=rng.uniform(0, ms.H),
                size=rng.choice([14, 16, 18, 22, 26]), phase=rng.randrange(len(SPINNER)),
                alpha=rng.uniform(0.07, 0.20), sway=rng.uniform(8, 26), sway_phase=rng.uniform(0, 6.28),
                color=rng.choice([ms.CORAL, ms.CORAL, ms.CORAL, ms.LAVENDER, ms.MUTED])))
        self.fonts = {s: ImageFont.truetype(str(ms.FONTS / "seguisym.ttf"), s) for s in (14, 16, 18, 22, 26)}

    def draw(self, image: Image.Image, frame: int) -> Image.Image:
        t = frame / FPS
        layer = Image.new("RGBA", image.size, (0, 0, 0, 0))
        draw = ImageDraw.Draw(layer)
        # Spinner frames advance every 4 video frames; 480/4 is a multiple of 10.
        step = frame // 4
        for g in self.items:
            y = (g["y"] - ms.H * t / self.seconds) % ms.H
            x = g["x"] + g["sway"] * math.sin(2 * math.pi * t / self.seconds + g["sway_phase"])
            ch = SPINNER[(step + g["phase"]) % len(SPINNER)]
            # Fade in/out near the top and bottom edges so wrapping is invisible.
            edge = min(1.0, y / 120.0, (ms.H - y) / 120.0)
            draw.text((x, y), ch, font=self.fonts[g["size"]], fill=ms.rgb(g["color"], int(255 * g["alpha"] * edge)))
        return Image.alpha_composite(image, layer)


def breathing_canvas(t: float, seconds: float) -> Image.Image:
    image = Image.new("RGBA", (ms.W, ms.H), ms.rgb(ms.BG))
    glow = Image.new("RGBA", (ms.W // 4, ms.H // 4), (0, 0, 0, 0))
    strength = 20 + 10 * math.sin(2 * math.pi * t / seconds)
    ImageDraw.Draw(glow).ellipse([-105, -130, 245, 130], fill=ms.rgb(ms.CORAL, int(strength)))
    glow = glow.filter(ImageFilter.GaussianBlur(55)).resize((ms.W, ms.H), Image.BILINEAR)
    return Image.alpha_composite(image, glow)


class Finisher:
    """Static scanlines + vignette applied after the animated layers."""

    def __init__(self, vignette: bool = True, scan_alpha: int = 8):
        self.scan = Image.new("RGBA", (ms.W, ms.H), (0, 0, 0, 0))
        draw = ImageDraw.Draw(self.scan)
        for y in range(0, ms.H, 3):
            draw.line([(0, y), (ms.W, y)], fill=(0, 0, 0, scan_alpha))
        self.keep = None
        if vignette:
            keep = Image.new("L", (ms.W, ms.H), 0)
            ImageDraw.Draw(keep).ellipse([-ms.W * 0.22, -ms.H * 0.32, ms.W * 1.22, ms.H * 1.32], fill=255)
            self.keep = keep.filter(ImageFilter.GaussianBlur(170))

    def apply(self, image: Image.Image) -> Image.Image:
        image = Image.alpha_composite(image, self.scan)
        if self.keep is not None:
            dark = Image.blend(image, Image.new("RGBA", image.size, (0, 0, 0, 255)), 0.45)
            image = Image.composite(image, dark, self.keep)
        return image


# ---------------------------------------------------------------- loops
def lobby_frames(seconds: float):
    glyphs, finisher = Glyphs(46, seconds), Finisher()
    for frame in range(int(seconds * FPS)):
        t = frame / FPS
        yield finisher.apply(glyphs.draw(breathing_canvas(t, seconds), frame))


def title_frames(seconds: float):
    glyphs, finisher = Glyphs(34, seconds, seed=11), Finisher()
    x0, y0 = 1016, 764
    command = "play --single"
    for frame in range(int(seconds * FPS)):
        t = frame / FPS
        image = glyphs.draw(breathing_canvas(t, seconds), frame)
        prompt = ms.Term(image.size, 21)
        prompt.frame(0, 0, 34, 3, ms.BORDER, x0, y0)
        # 0-1 s idle cursor, 1-2.3 s typing, 3 s enter, 3-11 s spinner, 11-14 s done, then clear.
        shown = typed(command, 1.0, 10.0, t) if t < 3.0 else ""
        end = prompt.spans(2, 1, [("> ", ms.MUTED), (shown, ms.TEXT)], x0, y0)
        if (t % 1.0) < 0.55 or 1.0 <= t < 2.4:
            prompt.cursor(end, 1, ms.TEXT, x0, y0)
        prompt.text(2, 3, "? for shortcuts", ms.DIM, x0, y0)
        if 3.0 <= t < 11.0:
            step = frame // 4
            spin = SPINNER[step % len(SPINNER)]
            prompt.spans(20, 3, [(spin + " ", ms.CORAL), ("Riffing…", ms.CORAL),
                                 (f" ({int(t - 3.0)}s)", ms.DIM)], x0, y0)
        elif 11.0 <= t < 14.5:
            prompt.spans(20, 3, [("✻ ", ms.MUTED), ("Riffed for 8s", ms.MUTED)], x0, y0)
        image = prompt.onto(image, 0.62)
        yield finisher.apply(image)


def gameplay_frames(seconds: float):
    finisher = Finisher(vignette=False, scan_alpha=10)
    base = Image.new("RGBA", (ms.W, ms.H), ms.rgb("#0B0A09"))
    lx, ly, rx, ry = 26, 28, 1468, 300
    out_start = seconds - 2.0           # everything except the welcome box fades out
    for frame in range(int(seconds * FPS)):
        t = frame / FPS
        fade = 1.0 - ease((t - out_start) / 1.6)
        image = base.copy()
        left = ms.Term(image.size, 18)
        left.frame(0, 0, 34, 6, ms.CORAL, lx, ly)
        left.spans(2, 1, [("✻ ", ms.CORAL), ("Welcome to TenRiff Code!", ms.TEXT)], lx, ly)
        left.text(4, 3, "/help for help", ms.MUTED, lx, ly)
        left.text(4, 4, "cwd: ~/songs", ms.MUTED, lx, ly)
        body = ms.Term(image.size, 18)
        end = body.spans(0, 7, [("> ", ms.MUTED), (typed("이 곡 풀콤 부탁해", 0.6, 7.0, t), ms.TEXT)], lx, ly)
        if t < 2.4 and (t % 1.0) < 0.55:
            body.cursor(end, 7, ms.TEXT, lx, ly)
        steps = [
            (2.6, [(0, 9, [("⏺ ", ms.TEXT), ("I'll load the chart and start.", ms.TEXT)])]),
            (3.8, [(0, 11, [("⏺ ", ms.GREEN), ("Read", ms.TEXT), ("(chart.bms)", ms.MUTED)]),
                   (0, 12, [("  ⎿  ", ms.DIM), ("Read 1,824 notes", ms.MUTED)])]),
            (5.2, [(0, 14, [("⏺ ", ms.GREEN), ("Bash", ms.TEXT), ("(tenriff play --keys 7)", ms.MUTED)]),
                   (0, 15, [("  ⎿  ", ms.DIM), ("Running…", ms.MUTED)])]),
        ]
        for start, lines in steps:
            if t >= start:
                for col, row, parts in lines:
                    body.spans(col, row, parts, lx, ly)
        if t >= 6.4:
            step = frame // 4
            elapsed = int(t - 6.4)
            verb = VERBS[min(len(VERBS) - 1, int((t - 6.4) // 4))]
            tokens = 0.3 + 0.17 * (t - 6.4)
            body.spans(0, 17, [(SPINNER[step % len(SPINNER)] + " ", ms.CORAL), (verb + "… ", ms.CORAL),
                               (f"({elapsed}s · ↑ {tokens:.1f}k tokens · esc to interrupt)", ms.DIM)], lx, ly)
        right = ms.Term(image.size, 18)
        if t >= 8.0:
            right.spans(0, 0, [("⏺ ", ms.GREEN), ("Update", ms.TEXT), ("(combo.log)", ms.MUTED)], rx, ry)
            right.spans(0, 1, [("  ⎿  ", ms.DIM), ("Updated combo.log", ms.MUTED)], rx, ry)
        if t >= 8.8:
            right.text(5, 2, " 41 - MISS            ", "#F2A7B4", rx, ry, bg=ms.DIFF_DEL)
        if t >= 9.4:
            right.text(5, 3, " 41 + PERFECT GREAT   ", "#A8E3B4", rx, ry, bg=ms.DIFF_ADD)
        if t >= 13.0:
            right.spans(0, 5, [("⏺ ", ms.GREEN), ("Judge", ms.TEXT), ("(lane 4)", ms.MUTED)], rx, ry)
            right.spans(0, 6, [("  ⎿  ", ms.DIM), ("+0.0 ms · ", ms.MUTED), ("PG", ms.YELLOW)], rx, ry)
        if t >= 17.0:
            right.spans(0, 8, [("✻ ", ms.CORAL), ("Keep going.", ms.TEXT)], rx, ry)
        status = ms.Term(image.size, 16)
        status.spans(0, 0, [("⏵⏵ ", ms.PURPLE), ("accept riffs on", ms.PURPLE)], 1468, 560)
        status.spans(0, 1, [("✻ ", ms.CORAL), ("esc to interrupt", ms.DIM)], 1468, 560)
        image = left.onto(image, 0.42)
        image = body.onto(image, 0.42 * fade)
        image = right.onto(image, 0.36 * fade)
        image = status.onto(image, 0.36)
        yield finisher.apply(image)


def encode(frames, out: Path, seconds: float):
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise SystemExit("ffmpeg with libx264 is required on PATH")
    out.parent.mkdir(parents=True, exist_ok=True)
    cmd = [ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24",
           "-s", f"{ms.W}x{ms.H}", "-r", str(FPS), "-i", "-", "-an", "-c:v", "libx264", "-preset", "slow",
           "-crf", "21", "-tune", "animation", "-pix_fmt", "yuv420p", "-g", str(FPS * 2),
           "-movflags", "+faststart", str(out)]
    process = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    count = 0
    for image in frames:
        process.stdin.write(image.convert("RGB").tobytes())
        count += 1
    process.stdin.close()
    if process.wait() != 0:
        raise SystemExit(f"ffmpeg failed for {out}")
    print(f"{out.name}: {count} frames, {out.stat().st_size / 1e6:.2f} MB", flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=str(SKIN / "cli"))
    parser.add_argument("--only", choices=("lobby", "title", "gameplay"))
    parser.add_argument("--seconds", type=float, default=0, help="override loop length (tests)")
    args = parser.parse_args()
    out = Path(args.out)
    jobs = {"lobby": (lobby_frames, 16.0), "title": (title_frames, 16.0), "gameplay": (gameplay_frames, 24.0)}
    for name, (make, seconds) in jobs.items():
        if args.only and name != args.only:
            continue
        seconds = args.seconds or seconds
        encode(make(seconds), out / f"{name}-loop.mp4", seconds)


if __name__ == "__main__":
    main()
