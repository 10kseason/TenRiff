from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont


ROOT = Path(__file__).resolve().parent
GAMEPLAY = ROOT / "gameplay"
LOBBY = ROOT / "lobby"

PALETTE = {
    "pink": "#FF2E93",
    "cyan": "#33E6FF",
    "lime": "#C8FF3D",
    "orange": "#FF9A3D",
}


def rgba(hex_color: str, alpha: int = 255) -> tuple[int, int, int, int]:
    value = hex_color.lstrip("#")
    return (int(value[0:2], 16), int(value[2:4], 16), int(value[4:6], 16), alpha)


def arrow_points(inset: int = 0) -> list[tuple[int, int]]:
    return [
        (128, 18 + inset),
        (232 - inset, 122),
        (178 - inset, 122),
        (178 - inset, 226 - inset),
        (78 + inset, 226 - inset),
        (78 + inset, 122),
        (24 + inset, 122),
    ]


def neon_arrow(color: str) -> Image.Image:
    image = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    glow = Image.new("RGBA", image.size, (0, 0, 0, 0))
    glow_draw = ImageDraw.Draw(glow)
    glow_draw.polygon(arrow_points(), fill=rgba(color, 235))
    image.alpha_composite(glow.filter(ImageFilter.GaussianBlur(18)))

    draw = ImageDraw.Draw(image)
    points = arrow_points()
    draw.polygon(points, fill=rgba(color, 224), outline=(250, 255, 255, 255))
    draw.line(points + [points[0]], fill=(250, 255, 255, 255), width=9, joint="curve")
    inner = [(128, 54), (196, 122), (154, 122), (154, 198), (102, 198), (102, 122), (60, 122)]
    draw.polygon(inner, fill=(5, 11, 20, 230))
    draw.line(inner + [inner[0]], fill=rgba(color, 255), width=6, joint="curve")
    draw.line([(72, 214), (184, 214)], fill=(255, 255, 255, 220), width=4)
    return image


def receptor(color: str, pressed: bool) -> Image.Image:
    image = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    glow = Image.new("RGBA", image.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.rounded_rectangle((18, 18, 238, 238), radius=34, outline=rgba(color, 255), width=16)
    if pressed:
        gd.rounded_rectangle((30, 30, 226, 226), radius=28, fill=rgba(color, 170))
    image.alpha_composite(glow.filter(ImageFilter.GaussianBlur(16 if pressed else 9)))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle(
        (20, 20, 236, 236),
        radius=32,
        fill=(5, 12, 22, 235 if not pressed else 210),
        outline=rgba(color, 255),
        width=8,
    )
    arrow = neon_arrow(color).resize((188, 188), Image.Resampling.LANCZOS)
    if not pressed:
        arrow.putalpha(155)
    image.alpha_composite(arrow, (34, 30 if pressed else 34))
    return image


def hold_body(color: str) -> Image.Image:
    image = Image.new("RGBA", (128, 256), (0, 0, 0, 0))
    glow = Image.new("RGBA", image.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.rectangle((22, 0, 106, 256), fill=rgba(color, 145))
    image.alpha_composite(glow.filter(ImageFilter.GaussianBlur(16)))
    draw = ImageDraw.Draw(image)
    draw.rectangle((28, 0, 100, 256), fill=(5, 12, 20, 220))
    draw.rectangle((28, 0, 38, 256), fill=rgba(color, 245))
    draw.rectangle((90, 0, 100, 256), fill=rgba(color, 245))
    for y in range(-16, 288, 48):
        draw.line([(44, y + 24), (64, y + 6), (84, y + 24)], fill=rgba(color, 210), width=6)
    return image


def find_font(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    candidates = [
        Path("C:/Windows/Fonts/bahnschrift.ttf"),
        Path("C:/Windows/Fonts/seguisb.ttf"),
        Path("C:/Windows/Fonts/arialbd.ttf"),
    ]
    for path in candidates:
        if path.exists():
            return ImageFont.truetype(str(path), size=size)
    return ImageFont.load_default()


def draw_glow_text(image: Image.Image, position: tuple[int, int], text: str, font, color: str) -> None:
    mask = Image.new("L", image.size, 0)
    md = ImageDraw.Draw(mask)
    md.text(position, text, font=font, fill=255, stroke_width=1)
    glow = Image.new("RGBA", image.size, rgba(color, 0))
    glow.putalpha(mask.filter(ImageFilter.GaussianBlur(14)))
    image.alpha_composite(glow)
    draw = ImageDraw.Draw(image)
    draw.text(position, text, font=font, fill=(248, 255, 240, 255), stroke_width=2, stroke_fill=rgba(color, 255))


def logo() -> Image.Image:
    image = Image.new("RGBA", (1100, 280), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    chevron = [(34, 140), (154, 24), (238, 24), (118, 140), (238, 256), (154, 256)]
    draw.polygon(chevron, fill=rgba(PALETTE["lime"], 235))
    draw.line(chevron + [chevron[0]], fill=(255, 255, 255, 255), width=7, joint="curve")
    draw.polygon([(94, 140), (174, 64), (202, 64), (122, 140), (202, 216), (174, 216)], fill=(4, 9, 16, 255))
    draw_glow_text(image, (270, 20), "VELOCITY", find_font(112), PALETTE["cyan"])
    draw_glow_text(image, (278, 148), "CIRCUIT // DANCE GRID", find_font(48), PALETTE["pink"])
    draw.rectangle((276, 242, 1030, 250), fill=rgba(PALETTE["lime"], 245))
    return image


def sprite_preview() -> Image.Image:
    image = Image.new("RGB", (1400, 860), (3, 8, 15))
    draw = ImageDraw.Draw(image)
    logo_image = logo().resize((880, 224), Image.Resampling.LANCZOS)
    image.paste(logo_image, (260, 18), logo_image)
    label_font = find_font(30)
    row_font = find_font(24)
    row_labels = ["NOTE", "KEY IDLE", "KEY PRESSED", "HOLD BODY"]
    for row, label in enumerate(row_labels):
        draw.text((32, 292 + row * 132), label, font=row_font, fill=(159, 194, 200))
    for column, (lane, color) in enumerate(PALETTE.items()):
        x = 228 + column * 286
        draw.text((x + 52, 250), lane.upper(), font=label_font, fill=rgba(color)[:3])
        note = neon_arrow(color).resize((112, 112), Image.Resampling.LANCZOS)
        idle = receptor(color, pressed=False).resize((112, 112), Image.Resampling.LANCZOS)
        pressed = receptor(color, pressed=True).resize((112, 112), Image.Resampling.LANCZOS)
        body = hold_body(color).resize((56, 112), Image.Resampling.LANCZOS)
        for y, sprite in ((286, note), (418, idle), (550, pressed)):
            image.paste(sprite, (x + 58, y), sprite)
        image.paste(body, (x + 86, 682), body)
    return image


def main() -> None:
    GAMEPLAY.mkdir(parents=True, exist_ok=True)
    LOBBY.mkdir(parents=True, exist_ok=True)
    for lane, color in PALETTE.items():
        neon_arrow(color).save(GAMEPLAY / f"note-{lane}.png")
        receptor(color, pressed=False).save(GAMEPLAY / f"key-idle-{lane}.png")
        receptor(color, pressed=True).save(GAMEPLAY / f"key-pressed-{lane}.png")
        hold_body(color).save(GAMEPLAY / f"hold-body-{lane}.png")
    logo().save(LOBBY / "logo.png")
    sprite_preview().save(ROOT / "SPRITES_PREVIEW.png")


if __name__ == "__main__":
    main()
