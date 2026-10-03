"""Export the shared Studio vector symbols as portable skin PNGs (Pillow).

Run from the source checkout. The game and offline editor only need the PNGs.
--check compares decoded pixels, avoiding encoder-version-dependent byte hashes.
"""
import argparse
import importlib.util
import re
import sys
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('studio_vectors', ROOT.parents[1] / 'tools/generate_native_menu_assets.py')
vectors = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vectors)

# Use the renderer's icon defaults so vector and portable PNG skins agree.
ICON_ROLES = {'keys': 'key_mode', 'keymap': 'keymap', 'skin': 'skin',
              'display': 'graphics', 'audio': 'audio', 'input': 'input',
              'latency': 'latency', 'profile': 'profile', 'sliders': 'mode',
              'keytest': 'key_test'}
options_source = (ROOT.parents[1] / 'src/render/MenuWindow_draw_options_grid.inl').read_text(encoding='utf-8')
ICON_COLORS = dict(re.findall(r'native_palette\("options\.icon\.([^"\n]+)",\s*D2D1::ColorF\(0x([0-9A-F]{6})\)', options_source))
assert set(ICON_ROLES.values()) <= ICON_COLORS.keys()


def render(shapes, color='#F0F0F0'):
    scale = 10.24  # 4x antialiasing for the 256px runtime image.
    result = Image.new('RGBA', (1024, 1024))
    draw = ImageDraw.Draw(result)
    for _, filled, points in shapes:
        points = [(round(x * scale), round(y * scale)) for x, y in points]
        if filled:
            draw.polygon(points, fill=color)
        else:
            draw.line(points, fill=color, width=round(2.16 * scale), joint='curve')
    return result.resize((256, 256), Image.Resampling.LANCZOS)


def backdrop():
    # Flat printed instrument-panel geometry. No noise, glow or baked-in text;
    # the renderer keeps live labels legible at every supported resolution.
    result = Image.new('RGB', (1920, 1080), '#000000')
    draw = ImageDraw.Draw(result)
    draw.rectangle((976, 126, 1919, 1079), fill='#030303')
    for lane in range(10):
        x = 1000 + lane * 90
        draw.line((x, 144, x, 968), fill='#111111', width=1)
        for measure in range(4):
            y = 180 + measure * 208 + (lane * 3 % 5) * 24
            draw.rectangle((x + 14, y, x + 60, y + 5), fill='#191919')
    draw.line((940, 168, 940, 924), fill='#222222', width=1)
    for tick in range(32):
        y = 168 + tick * 24
        draw.line((940 - (12 if tick % 4 == 0 else 5), y, 940, y), fill='#333333', width=1)
    draw.rectangle((938, 240, 942, 312), fill='#606060')
    draw.line((96, 488, 816, 488), fill='#222222', width=1)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    for name, shapes in vectors.ASSETS.items():
        path = ROOT / 'assets' / (name.lower() + '.png')
        role = ICON_ROLES.get(name.lower())
        expected = render(shapes, '#' + ICON_COLORS[role] if role else '#F0F0F0')
        if args.check:
            with Image.open(path) as actual:
                assert actual.size == expected.size and actual.convert('RGBA').tobytes() == expected.tobytes(), path
        else:
            path.parent.mkdir(exist_ok=True)
            expected.save(path)
    expected = backdrop()
    for path in (ROOT/'assets/studio-backdrop.png', ROOT.parents[1]/'assets/menu/title-studio.png'):
        if args.check:
            with Image.open(path) as actual:
                assert actual.convert('RGB').tobytes() == expected.tobytes(), path
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            expected.save(path)
    print(f'Studio PNGs: {len(vectors.ASSETS)} {"verified" if args.check else "generated"}.')


if __name__ == '__main__':
    main()
