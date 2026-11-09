"""Generate the bundled Modern Common skins; no runtime Python dependency.

Existing folder IDs, v1 format, original manifests and art remain portable.
Only the named bundled skins are written. Run --check for a read-only audit.
"""
from __future__ import annotations

import argparse
import io
import json
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
THEMES = {
    'Tencircle': ('Tencircle', '#A9A4FF', '#788CF2'),
    'Tengear': ('Tengear', '#6EDAE5', '#3CABC7'),
    'Tenmania': ('Tenmania', '#F4A8CA', '#C87FAB'),
    'TenRiff_AgentPrism_Universal_1K-16K': ('Agent Prism', '#B5B5FA', '#8A8BDD'),
    'TenRiff_NeonStep_Arcade_Universal_4K-10K': ('Neon Step', '#8DBFFF', '#5D92E1'),
    'TenRiff_Pulse_Mania_Universal_4K-10K': ('Pulse Mania', '#80DFC0', '#4CB293'),
    'TenRiff_SimpleSquare': ('Simple Square', '#D8E4F0', '#8CA7C2'),
    'TenRiff_Studio': ('Studio', '#E9C78E', '#C49B60'),
    'TenRiff_VelocityCircuit_DanceGrid_4K-16K': ('Velocity Circuit', '#F3B195', '#D28B72'),
    'LUNAR-FRACTURE-Void-Garden-7K': ('Void Garden', '#CFADF3', '#A786D1'),
    'SunShine-Light-er-Arrow-7K': ('SunShine', '#E9DC9C', '#B7AA66'),
    'TenRiff_NativeEditable': ('Native Editable', '#9DD8ED', '#6CA9CC'),
}
SCREENS = ('quick_setup', 'title', 'options', 'multiplayer', 'song_select',
           'session_mix', 'song_browser', 'settings', 'settings_audio',
           'settings_graphics', 'settings_skins', 'settings_input', 'settings_calibration',
           'mode_select', 'mode_mods', 'keymap', 'keymap_confirm', 'onnx_upscaler_confirm',
           'keymap_test', 'result')


def layer(x, y, width, height, color, mix=1.0, alpha=1.0, radius=0):
    return dict(x=x, y=y, width=width, height=height, color=color,
                mix=mix, alpha=alpha, radius=radius)


def sprites():
    """One geometry at 128x32 / 128x256, tinted with each lane's own color."""
    # The renderer reserves the first two note layers for optional outlines.
    # Keep the solid core after them so disabling borders cannot erase the note.
    note = [layer(0, 2, 128, 28, '#FFFFFF', .18, radius=3),
            layer(1, 3, 126, 26, '#111923', .28, radius=2),
            layer(1, 3, 126, 26, '#FFFFFF', .02, radius=2),
            layer(3, 3, 122, 2, '#FFFFFF', .48, .72)]
    return {
        'note': note,
        'hold_head': note,
        'hold_tail': [layer(0, 9, 128, 14, '#FFFFFF', .20, radius=2),
                      layer(1, 10, 126, 12, '#111923', .28, radius=1),
                      layer(1, 10, 126, 12, '#FFFFFF', .02, radius=1),
                      layer(3, 10, 122, 2, '#FFFFFF', .48, .62)],
        'key_idle': [layer(0, 0, 128, 256, '#05080E', radius=7),
                     layer(1, 1, 126, 251, '#344252', radius=6),
                     layer(2, 3, 124, 247, '#111923', radius=5),
                     layer(3, 4, 122, 2, '#9DADBE', alpha=.24),
                     layer(9, 220, 110, 1, '#8094A8', alpha=.18),
                     layer(10, 239, 108, 3, '#FFFFFF', .12, .54, 1)],
        'key_pressed': [layer(0, 0, 128, 256, '#FFFFFF', .10, radius=7),
                        layer(1, 1, 126, 251, '#FFFFFF', .14, radius=6),
                        layer(2, 3, 124, 247, '#0B1420', .72, radius=5),
                        layer(3, 4, 122, 3, '#FFFFFF', .16, .82),
                        layer(10, 239, 108, 4, '#FFFFFF', .16, .94, 1)],
    }


def lane_colors(count, accent, secondary):
    # Symmetric hand groups preserve contrast at 4K and dense DP/16K alike.
    white = '#EAF0F7'
    if count == 4:
        return [white, secondary, secondary, white]
    result = [white if min(i, count - 1 - i) % 2 == 0 else secondary for i in range(count)]
    if count % 2:
        result[count // 2] = accent
    if count in (8, 12, 16):
        result[0] = result[-1] = accent
    return result


def manifest(original, title, accent, secondary):
    metrics = {'key_min_height': 48, 'key_max_height': 126, 'key_inset': 2,
               'key_inset_ratio': .055, 'key_bottom_gap': 4, 'key_face_gap': 5,
               'judgement_line_width': 1.5, 'judgement_glow_height': 6,
               'lane_divider_opacity': .13, 'hold_edge_mix': .36, 'hold_core_mix': .48,
               'timing_half_width': 112, 'timing_height': 5, 'timing_range_ms': 80,
               'title_font_size': 28, 'score_font_size': 32, 'body_font_size': 18,
               'timing_font_size': 17, 'judgement_font_size': 46, 'combo_font_size': 40,
               'key_label_font_size': 14}
    # Gauge and grade colors are deliberately absent: they retain their meaning.
    colors = {'key_well': '#05080E', 'key_led': accent, 'chassis': '#0B111A',
              'judgement_line': accent, 'judgement_glow': accent + '18',
              'hold_edge': '#DDE7F3', 'hold_core': '#14202F', 'hold_shadow': '#05080E',
              'burst_core': '#F7FAFD', 'burst_trail': accent,
              'title': '#EDF3FA', 'score': '#EDF3FA', 'body': '#B5C1D0',
              'combo': accent, 'judgement': '#EDF3FA', 'timing': '#DCE6F2',
              'key_label': '#D0DCE9'}
    return {
        '$schema': '../../docs/tenriff-skin.schema.json', 'format': 'tenriff-skin',
        'version': 1, 'name': title + ' / Modern', 'author': original.get('author', 'TenRiff'),
        'lobby': {'renderer': 'native', 'background': 'modern/background.png', 'logo': '',
                  'background_opacity': .48,
                  'screen_backgrounds': {screen: 'modern/background.png' for screen in SCREENS},
                  'screen_opacities': {screen: (.42 if screen == 'title' else .28) for screen in SCREENS}},
        'theme': {'accent': accent, 'text': '#EDF3FA', 'muted': '#B5C1D0',
                  'card': '#101720F5', 'panel': '#0E151FEC', 'footer': '#080C13F5',
                  'button': '#15202DEB', 'button_selected': accent + '2A',
                  'border': '#344353', 'judgement_line': accent, 'lane_divider': '#C3CFDD',
                  'scene_primary': secondary, 'scene_secondary': accent, 'scene_background': '#080C13'},
        'native': {'metrics': {'icon.stroke_width': 2.0, 'options_grid.radius': 16,
                               'options_grid.tint': .008, 'options_grid.selected_tint': .045,
                               'options_grid.border_width': 1, 'options_grid.selected_border_width': 2,
                               'generic.line_height': 27},
                   'colors': {'palette.05090f': '#080C13', 'palette.101820': '#101720',
                              'palette.63e9f2': accent, 'palette.101d32': '#142130',
                              'palette.a4e8f3': accent,
                              'title.primary': '#142130', 'title.primary_text': '#EDF3FA',
                              'title.primary_detail': '#B5C1D0'},
                   'motion': {'intensity': .40, 'entry_seconds': .24}},
        'gameplay': {
            'description': 'Modern Common: one responsive instrument, typography and note system; theme colors vary.',
            'renderer': 'native', 'background': '', 'gear': '',
            # Explicit empty paths suppress conventional legacy bitmap discovery.
            'note': '', 'hold_head': '', 'hold_body': '', 'hold_tail': '',
            'key_idle': '', 'key_pressed': '',
            'note_shape': 'rect', 'note_aspect': 'stretch', 'note_border': False,
            'black_playfield': True, 'lane_background_opacity': .025,
            'show_lane_dividers': True, 'show_judgement_line': True,
            'show_gear_boundary_line': False, 'show_hold_tail': True, 'hold_tail_taper': False,
            'judgement_line_glow': True, 'key_pulse': True, 'key_pulse_brightness': .38,
            'key_label_position': 'bottom', 'hold_body_opacity': .64,
            'visual_opacity': 1, 'note_outline_opacity': .52,
            'native': {'metrics': metrics, 'colors': colors,
                       'motion': {'press_response': 110, 'release_response': 30, 'press_depth': 3,
                                  'pressed_light': .72, 'hit_light': .22, 'burst_rise': 42,
                                  'burst_height': 18, 'combo_duration_ms': 150,
                                  'combo_scale': 1.055, 'combo_lift': -2,
                                  'judgement_duration_ms': 500},
                       'fonts': {'title': 'Segoe UI', 'score': 'Bahnschrift', 'body': 'Segoe UI',
                                 'combo': 'Bahnschrift', 'judgement': 'Segoe UI',
                                 'timing': 'Segoe UI', 'key_label': 'Segoe UI'},
                       'sprites': sprites()},
            'modes': {f'{keys}k': {'lane_colors': lane_colors(keys, accent, secondary)} for keys in range(1, 17)},
        },
    }


def background(accent):
    """Original geometric backdrop; new artwork, with no legacy image editing."""
    rgb = tuple(int(accent[i:i + 2], 16) for i in (1, 3, 5))
    im = Image.new('RGB', (1920, 1080), '#080C13')
    draw = ImageDraw.Draw(im)
    # Low-contrast diagonal tracks keep the reading/controls area visually quiet.
    for offset in range(-1000, 1800, 96):
        draw.line([(offset, 1080), (offset + 680, 0)], fill='#0C131E', width=1)
    for x, y, size, alpha in [(1510, 180, 410, .075), (190, 940, 520, .045)]:
        for radius in range(size, 0, -4):
            strength = alpha * (1.0 - radius / size) ** 1.6
            color = tuple(round(base * (1 - strength) + value * strength)
                          for base, value in zip((8, 12, 19), rgb))
            draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=color)
    draw.line([(1400, 90), (1830, 90)], fill=tuple(round(v * .17) for v in rgb), width=1)
    draw.line([(90, 990), (520, 990)], fill=tuple(round(v * .13) for v in rgb), width=1)
    output = io.BytesIO();im.save(output, format='PNG', optimize=True)
    return output.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--write', action='store_true');group.add_argument('--check', action='store_true')
    args = parser.parse_args()
    for folder, (title, accent, secondary) in THEMES.items():
        directory = ROOT / 'skins' / folder
        backup = directory / 'skin.legacy.json'
        if not backup.exists():
            if args.check:
                raise SystemExit(f'Missing original manifest: {folder}')
            # Create once; regeneration must never rewrite the preserved design.
            backup.write_bytes((directory / 'skin.json').read_bytes())
        original = json.loads(backup.read_text(encoding='utf-8-sig'))
        outputs = {directory / 'skin.json': (json.dumps(manifest(original, title, accent, secondary),
                     ensure_ascii=False, indent=2) + '\n').encode('utf-8'),
                   directory / 'modern/background.png': background(accent)}
        for path, data in outputs.items():
            if args.check:
                if not path.is_file() or path.read_bytes() != data:
                    raise SystemExit(f'Stale generated file: {path.relative_to(ROOT)}')
            else:
                path.parent.mkdir(parents=True, exist_ok=True);path.write_bytes(data)
        print(('Checked ' if args.check else 'Generated ') + folder)


if __name__ == '__main__':
    main()
