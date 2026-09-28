"""Export the actual native renderer's editable slots for the offline skin editor.

--expose is a developer migration for new unwrapped rectangles/color constants.
Normal runs only read C++ and rebuild the catalog; --check detects stale output.
Existing identifiers stay stable across line moves and translations.
"""
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILES = sorted((ROOT / 'src/render').glob('MenuWindow_draw_*.inl'))

def blank(source):
    """Hide unreachable code without shifting source-reference line numbers."""
    return re.sub(r'[^\n]', ' ', source)

def balanced_end(source, start, opening='(', closing=')'):
    depth, quote, escape, comment = 1, None, False, None
    index = start
    while index < len(source):
        char = source[index]
        pair = source[index:index + 2]
        if comment == '//':
            if char == '\n': comment = None
        elif comment == '/*':
            if pair == '*/':
                comment = None
                index += 1
        elif quote:
            if escape: escape = False
            elif char == '\\': escape = True
            elif char == quote: quote = None
        elif pair in ('//', '/*'):
            comment = pair
            index += 1
        elif char in ('"', "'"): quote = char
        elif char == opening: depth += 1
        elif char == closing:
            depth -= 1
            if depth == 0: return index + 1
        index += 1
    raise ValueError(f'Unbalanced C++ {opening}{closing}')

def native_source(source, filename):
    if filename == 'MenuWindow_draw_title_body.inl': return ''
    if filename == 'MenuWindow_draw_result_single_body.inl':
        # The native summary replaces the old floating prism. Keep line numbers
        # but omit its unreachable style controls from the editor catalog.
        marker = '#include "MenuWindow_draw_result_summary.inl"'
        start = source.index('} else {', source.index(marker)) + len('} else ')
        end = balanced_end(source, start + 1, '{', '}')
        source = source[:start] + blank(source[start:end]) + source[end:]
    modern_flag = {
        'MenuWindow_draw_generic_body.inl': 'modern_settings_screen',
        'MenuWindow_draw_songselect_body.inl': 'modern_library_screen',
        'MenuWindow_draw_result_single_body.inl': 'modern_library_screen',
    }.get(filename)
    if modern_flag:
        # These flags are true on the native renderer's corresponding screens.
        # Only omit explicit false conjuncts; unknown expressions remain visible.
        for match in list(re.finditer(r'\bif\s*\(', source)):
            if source[match.start():match.end()].isspace(): continue
            end = call_end(source, match.end())
            condition = ' '.join(source[match.end():end - 1].split())
            legacy_only = '||' not in condition and (
                '!' + modern_flag in [part.strip() for part in condition.split('&&')])
            if filename == 'MenuWindow_draw_generic_body.inl':
                # These locals are forced to zero/false in the native branch.
                legacy_only |= condition in ('displayed_note_count > 0', 'has_footer_notes')
            body = end
            while body < len(source) and source[body].isspace(): body += 1
            if legacy_only and body < len(source) and source[body] == '{':
                end = balanced_end(source, body + 1, '{', '}')
                source = source[:body] + blank(source[body:end]) + source[end:]
    return source

def call_end(source, start):
    return balanced_end(source, start)

def expose(source, screen):
    count = max([int(x) for x in re.findall(rf'"{screen}\.rect\.(\d+)"', source)] or [0])
    edits = []
    for match in re.finditer(r'D2D1::RectF\(', source):
        # Do not wrap an existing slot a second time.
        if re.search(r'native_rect\("[^"\n]+",\s*$', source[max(0, match.start()-160):match.start()]):
            continue
        count += 1
        end = call_end(source, match.end())
        edits.append((match.start(), end, f'native_rect("{screen}.rect.{count:03d}", ' + source[match.start():end] + ')'))
    for start, end, replacement in reversed(edits): source = source[:start] + replacement + source[end:]
    edits = []
    for match in re.finditer(r'D2D1::ColorF\((0x[0-9A-Fa-f]{6})(?=[,)])', source):
        if re.search(r'native_palette\("[^"\n]+",\s*$', source[max(0, match.start()-160):match.start()]):
            continue
        end = call_end(source, match.start() + len('D2D1::ColorF('))
        key = 'palette.' + match[1][2:].lower()
        edits.append((match.start(), end, f'native_palette("{key}", ' + source[match.start():end] + ')'))
    for start, end, replacement in reversed(edits): source = source[:start] + replacement + source[end:]
    def metric(match):
        qualifier, name, number = match.groups()
        return f'const float {name} = native_metric("{screen}.{name.lower()}", {number});'
    # Only local named scalar dimensions, never indices, enum values or sample times.
    source = re.sub(r'(?<!static )(const|constexpr) float (\w+) = (-?\d+\.\d+f);', metric, source)
    return source

def catalog():
    data = dict(metrics={}, colors={}, rects={}, assets={}, motion={
        'enabled': 1, 'speed': 1, 'intensity': 1, 'entry_seconds': 0.42}, fonts={}, references={})
    for path in FILES:
        source = native_source(path.read_text(encoding='utf-8-sig'), path.name)
        for match in re.finditer(r'native_rect\("([^"\n]+)",\s*D2D1::RectF\(', source):
            # The legacy ternary arm and legacy-only header are declared outside
            # the unreachable blocks above. Keep the IDs stable in C++.
            if path.name == 'MenuWindow_draw_generic_body.inl' and match[1] in (
                    'generic.rect.002', 'generic.rect.004'): continue
            data['rects'][match[1]] = [0, 0, 0, 0]
            end = call_end(source, match.end())
            expression = ' '.join(source[match.end():end-1].split())
            line = source[:match.start()].count('\n') + 1
            data['references']['rects.' + match[1]] = {'file': path.name, 'line': line, 'expression': expression}
        for match in re.finditer(r'native_metric\("([^"\n]+)",\s*(-?\d+(?:\.\d+)?)[f]?\)', source):
            # Declared before the modern/legacy split, used only by the old prism.
            if match[1] == 'result_single.prism_bottom': continue
            data['metrics'][match[1]] = float(match[2])
        for match in re.finditer(r'native_palette\("([^"\n]+)",\s*D2D1::ColorF\((0x[0-9A-Fa-f]{6})', source):
            data['colors'][match[1]] = '#' + match[2][2:].upper() + 'FF'
        for match in re.finditer(r'native_color\(', source):
            end = call_end(source, match.end())
            for rgb in re.findall(r'0x([A-Fa-f0-9]{6})\b', source[match.end():end]):
                data['colors']['palette.' + rgb.lower()] = '#' + rgb.upper() + 'FF'
    cpp = (ROOT / 'src/render/MenuWindow.cpp').read_text(encoding='utf-8-sig')
    for match in re.finditer(r'create_text_format\((ui_family|L"[^"]+"),\s*DWRITE_FONT_WEIGHT_\w+,\s*(\d+\.\d+)f,\s*&d2d_->(\w+)_format\)', cpp):
        role = match[3]
        # Native title/nav use header/song_title; rank is still used by song stats.
        if role in ('gameplay_combo', 'logo', 'song_logo', 'song_nav'): continue
        data['fonts'][role] = 'Segoe UI' if match[1] == 'ui_family' else match[1][2:-1]
        data['metrics']['font.' + role + '.size'] = float(match[2])
    for name in ('mark', 'prism', 'chevron', 'spark', 'wave', 'audio', 'display', 'input', 'network', 'sliders', 'folder', 'exit'):
        data['assets'][name] = ''
    # Atlas colors can be overridden even when the compiled vector is in use.
    for color in ('63E9F2', 'A499FF', 'ECF6FF'):
        data['colors']['palette.' + color.lower()] = '#' + color + 'FF'
    return data

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expose', action='store_true')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if args.expose and args.check: parser.error('--expose and --check are exclusive')
    if args.expose:
        for path in FILES:
            # Gameplay geometry/animation stays untouched. Its existing lane,
            # note, hold, receptor and feedback fields remain in the skin format.
            if 'gameplay' in path.name or 'bms_editor' in path.name or path.name == 'MenuWindow_draw_title_body.inl': continue
            source = path.read_text(encoding='utf-8-sig')
            screen = path.stem.removeprefix('MenuWindow_draw_').removesuffix('_body')
            changed = expose(source, screen)
            if changed != source: path.write_text(changed, encoding='utf-8', newline='\n')
    data = catalog()
    serialized = json.dumps(data, ensure_ascii=False, indent=2) + '\n'
    files = {
        ROOT / 'tools/skin_editor/native-catalog.json': serialized,
        ROOT / 'tools/skin_editor/native-catalog.js': 'window.TENRIFF_NATIVE_CATALOG = ' + serialized.rstrip() + ';\n',
    }
    for path, content in files.items():
        if args.check:
            if not path.exists() or path.read_text(encoding='utf-8') != content:
                raise SystemExit(f'Stale native catalog: {path.name}')
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding='utf-8', newline='\n')
    print('Native slots:', ', '.join(f'{key}={len(value)}' for key, value in data.items() if key != 'references'))

if __name__ == '__main__': main()
