#!/usr/bin/env python3
"""Audit Japanese coverage for TenRiff's explicit UI localization boundaries.

This is a source-level coverage check, not a claim that every raw diagnostic,
user-authored string, or text inside an image has been translated.
"""
from __future__ import annotations

import argparse
import ast
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STRING = re.compile(r'(?:u8|L)?"(?:\\.|[^"\\])*"', re.S)
CALL = re.compile(r'\b(ui_text|loc|wloc|result_loc|localized|ui::text)\s*\(')


def decode_literal(value: str) -> str:
    result = ast.literal_eval(re.sub(r'^(?:u8|L)', '', value))
    # C++ UTF-8 byte escapes (e.g. an arrow) must compare to their runtime
    # Unicode value, not the Latin-1 spelling produced by Python's parser.
    def decode_bytes(match: re.Match[str]) -> str:
        try:
            return match[0].encode('latin-1').decode('utf-8')
        except UnicodeError:
            return match[0]
    return re.sub(r'[\x80-\xff]+', decode_bytes, result)


def call_arguments(source: str, position: int) -> list[str]:
    result: list[str] = []
    start, depth, i = position, 0, position
    while i < len(source):
        string = STRING.match(source, i)
        if string:
            i = string.end()
            continue
        char = source[i]
        if char in '([{':
            depth += 1
        elif char in ')]}':
            if char == ')' and depth == 0:
                result.append(source[start:i].strip())
                return result
            depth -= 1
        elif char == ',' and depth == 0:
            result.append(source[start:i].strip())
            start = i + 1
        i += 1
    return []


def source_keys(root: Path = ROOT) -> tuple[dict[str, list[str]], list[str]]:
    keys: dict[str, list[str]] = {}
    dynamic: list[str] = []
    files = sorted((*root.joinpath('src/app').rglob('*'),
                    *root.joinpath('src/render').rglob('*')))
    for path in files:
        if path.suffix not in ('.h', '.cpp', '.inl'):
            continue
        source = path.read_text(encoding='utf-8-sig')
        for call in CALL.finditer(source):
            args = call_arguments(source, call.end())
            index = 1 if call[1] in ('localized', 'ui::text') else 0
            if len(args) <= index:
                continue
            expr = args[index]
            if expr == 'english' or 'std::string_view' in expr:
                continue
            location = f'{path.relative_to(root).as_posix()}:{source[:call.start()].count(chr(10)) + 1}'
            literals = STRING.findall(expr)
            if not literals:
                # These closed tables are enumerated below. Any new dynamic
                # key needs an explicit audit path instead of disappearing.
                if expr not in ('builtin->name_en', 'descriptor.english_title', 'preset.name_en.data()',
                                'bms_editor_.status()', 'gameplay_hud_.loading_stage'):
                    dynamic.append(f'{location}: {expr}')
                continue
            if re.fullmatch(r'\s*(?:(?:u8|L)?"(?:\\.|[^"\\])*"\s*)+', expr):
                values = [''.join(map(decode_literal, literals))]
            else:
                values = list(map(decode_literal, literals))
            for value in values:
                if value:
                    keys.setdefault(value, []).append(location)
    tables = (
        ('src/app/menu/MenuScreenDescriptor.cpp', r'\{Screen::\w+,\s*("(?:\\.|[^"\\])*")'),
        ('src/config/BuiltinDifficultyTables.h', r'\{("(?:\\.|[^"\\])*"),\s*u8"'),
    )
    for name, pattern in tables:
        source = root.joinpath(name).read_text(encoding='utf-8-sig')
        for match in re.finditer(pattern, source):
            keys.setdefault(decode_literal(match[1]), []).append(name)
    # These two subsystems expose stable internal English status phrases. UI
    # snapshots translate exact matches; unknown backend diagnostics pass through.
    for name, pattern in (
        ('src/app/BmsEditor.cpp', r'status_\s*=([^;]+);'),
        ('src/app/BmsEditor.cpp', r'set_error\(([^;]+?),err\)'),
        ('src/app/GameSessionTail.inl', r'report_loading_progress\([^,]+,\s*([^;]+)\);'),
        ('src/app/MenuAppTail.inl', r'gameplay_hud_\.loading_stage\s*=([^;]+);'),
    ):
        source = root.joinpath(name).read_text(encoding='utf-8-sig')
        for match in re.finditer(pattern, source):
            for value in STRING.findall(match[1]):
                keys.setdefault(decode_literal(value), []).append(name)
    return keys, dynamic


def read_catalog(root: Path = ROOT) -> tuple[dict[str, str], list[str]]:
    catalog: dict[str, str] = {}
    issues: list[str] = []
    path = root / 'src/ui/JapaneseStrings.inc'
    if not path.exists():
        return catalog, ['Japanese catalog is missing']
    for number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        if not line.strip() or line.lstrip().startswith('//'):
            continue
        values = STRING.findall(line)
        if len(values) != 2:
            issues.append(f'Catalog line {number}: expected one key and one translation')
            continue
        english, japanese = map(decode_literal, values)
        if english in catalog:
            issues.append(f'Catalog line {number}: duplicate key {english!r}')
        if not japanese:
            issues.append(f'Catalog line {number}: empty translation {english!r}')
        catalog[english] = japanese
    return catalog, issues


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json', action='store_true', help='print full JSON report')
    parser.add_argument('--write-keys', type=Path, help='write English keys for translation maintenance')
    args = parser.parse_args()
    keys, dynamic = source_keys()
    catalog, issues = read_catalog()
    missing = sorted(set(keys) - set(catalog))
    if args.write_keys:
        args.write_keys.write_text(json.dumps(sorted(keys), ensure_ascii=False, indent=2), encoding='utf-8')
    report = {
        'scope': 'explicit UI helpers and closed title/preset tables',
        'source_keys': len(keys), 'catalog_entries': len(catalog),
        'covered_source_keys': len(keys) - len(missing),
        'missing': [{'english': key, 'locations': keys[key]} for key in missing],
        'unresolved_dynamic_keys': dynamic, 'catalog_issues': issues,
        'out_of_scope': ['chart metadata', 'user content', 'third-party skin artwork',
                         'backend/server diagnostics', 'persisted historical result descriptions'],
    }
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        print(f'Japanese UI coverage: {report["covered_source_keys"]}/{len(keys)} explicit keys; '
              f'{len(catalog)} catalog entries; {len(dynamic)} unresolved dynamic keys')
        for key in missing:
            print(f'MISSING {key!r} ({keys[key][0]})')
        for issue in dynamic + issues:
            print(f'ISSUE {issue}')
    return int(bool(missing or dynamic or issues))


if __name__ == '__main__':
    raise SystemExit(main())
