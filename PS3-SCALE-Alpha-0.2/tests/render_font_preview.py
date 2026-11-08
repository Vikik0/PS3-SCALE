#!/usr/bin/env python3
"""Visual regression preview using the same Noto Arabic TTF advances as PS3 runtime."""
from __future__ import annotations

import re
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT_TTF = ROOT / "pkgfiles/USRDIR/NotoSansArabicUI.ttf"
FONT_ASCII = ROOT / "source/ps3/font8x12.h"
ARABIC_MAP = ROOT / "source/ps3/arabic_glyph_map.h"
OUT = ROOT / "build-host/arabic-font-preview.png"

FONT_CELL_W = 32
FONT_CELL_H = 32
FONT_FIRST = 32
UI_WIDTH = 14
UI_HEIGHT = 22
ZOOM = 4
BASELINE = 24

assert FONT_TTF.is_file(), f"missing verified runtime font: {FONT_TTF}"
ARABIC_FONT = ImageFont.truetype(str(FONT_TTF), 28)

ascii_text = FONT_ASCII.read_text(encoding="utf-8")
ascii_match = re.search(
    r"ps3scale_font8x12\[\]\s*=\s*\{(.*?)\};",
    ascii_text,
    re.S,
)
assert ascii_match, "ASCII bitmap declaration not found"
ASCII_FONT = bytes(
    int(value, 16)
    for value in re.findall(r"0x([0-9a-fA-F]{2})", ascii_match.group(1))
)
assert len(ASCII_FONT) >= (126 - 32 + 1) * 12

map_text = ARABIC_MAP.read_text(encoding="utf-8")
rows = re.findall(
    r"\{\s*0x([0-9a-fA-F]+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}",
    map_text,
)
FORMS = {
    int(cp, 16): tuple(int(value) for value in values)
    for cp, *values in rows
}
assert len(FORMS) >= 30, "Arabic contextual form table was not parsed"
ARABIC_GLYPHS = {
    glyph for form in FORMS.values() for glyph in form if glyph
}


def strong_direction(cp: int) -> int:
    if 0x0600 <= cp <= 0x06FF:
        return 1
    if (
        0x41 <= cp <= 0x5A
        or 0x61 <= cp <= 0x7A
        or 0x30 <= cp <= 0x39
        or 0x00C0 <= cp <= 0x02AF
    ):
        return 0
    return -1


def unicode_to_glyph(cp: int) -> int:
    form = FORMS.get(cp)
    if form:
        return form[0] or ord("?")
    if cp < 128:
        return cp
    if cp in (0x2018, 0x2019):
        return ord("'")
    if cp in (0x2013, 0x2014):
        return ord("-")
    return ord("?")


def shaped_arabic_glyph(cps: list[int], index: int, start: int, end: int) -> int:
    current = FORMS.get(cps[index])
    if not current:
        return unicode_to_glyph(cps[index])

    previous = FORMS.get(cps[index - 1]) if index > start else None
    following = FORMS.get(cps[index + 1]) if index + 1 < end else None
    isolated, final_form, initial, medial = current

    join_previous = bool(previous and previous[2] and (final_form or medial))
    join_next = bool(following and following[1] and (initial or medial))

    if join_previous and join_next and medial:
        return medial
    if join_previous and final_form:
        return final_form
    if join_next and initial:
        return initial
    return isolated or ord("?")


def shape_text(value: str) -> list[int]:
    cps = [ord(ch) for ch in value]
    base_direction = next(
        (direction for cp in cps if (direction := strong_direction(cp)) >= 0),
        0,
    )
    directions = []
    for index, cp in enumerate(cps):
        direction = strong_direction(cp)
        if direction < 0:
            direction = directions[index - 1] if index else base_direction
        directions.append(direction)

    runs = []
    index = 0
    while index < len(cps):
        start = index
        direction = directions[index]
        while index < len(cps) and directions[index] == direction:
            index += 1
        runs.append((start, index, direction))

    output = []
    run_order = reversed(runs) if base_direction == 1 else runs
    for start, end, direction in run_order:
        if direction == 1:
            for index in range(end - 1, start - 1, -1):
                output.append(shaped_arabic_glyph(cps, index, start, end))
        else:
            for index in range(start, end):
                output.append(unicode_to_glyph(cps[index]))
    return output


def is_arabic_glyph(glyph: int) -> bool:
    return glyph in ARABIC_GLYPHS


def presentation_codepoint(glyph: int) -> int:
    assert 128 <= glyph <= 244
    return 0xFE80 + glyph - 128


def glyphs_joined(left_glyph: int, right_glyph: int) -> bool:
    left_form = None
    right_form = None
    for form in FORMS.values():
        if (form[1] and form[1] == left_glyph) or (
            form[3] and form[3] == left_glyph
        ):
            left_form = form
        if (form[2] and form[2] == right_glyph) or (
            form[3] and form[3] == right_glyph
        ):
            right_form = form
    return left_form is not None and right_form is not None


# Verify that the runtime TTF contains every Arabic presentation-form glyph.
for glyph in ARABIC_GLYPHS:
    codepoint = presentation_codepoint(glyph)
    assert ARABIC_FONT.getmask(chr(codepoint)).getbbox() is not None, (
        f"missing Arabic presentation form U+{codepoint:04X}"
    )


WIDTH = 1024 * ZOOM
HEIGHT = 720 * ZOOM
BACKGROUND = (7, 20, 36)
INK = (226, 245, 255)
ACCENT = (139, 212, 80)
canvas = Image.new("RGB", (WIDTH, HEIGHT), BACKGROUND)
draw = ImageDraw.Draw(canvas)
arabic_cell_cache = {}


def draw_scaled_ascii_cell(glyph: int, x: float, y: float):
    if not 32 <= glyph <= 126:
        glyph = ord("?")
    if glyph == 32:
        return
    offset = (glyph - 32) * 12
    for dy in range(UI_HEIGHT):
        source_y = min(11, int(dy * 12 / UI_HEIGHT))
        row = ASCII_FONT[offset + source_y]
        for dx in range(UI_WIDTH):
            source_x = min(7, int(dx * 8 / UI_WIDTH))
            if row & (0x80 >> source_x):
                px = int(round((x + dx) * ZOOM))
                py = int(round((y + dy) * ZOOM))
                draw.rectangle(
                    (px, py, px + ZOOM - 1, py + ZOOM - 1),
                    fill=INK,
                )


def arabic_cell(glyph: int) -> Image.Image:
    if glyph in arabic_cell_cache:
        return arabic_cell_cache[glyph]

    codepoint = presentation_codepoint(glyph)
    cell = Image.new("L", (FONT_CELL_W, FONT_CELL_H), 0)
    cell_draw = ImageDraw.Draw(cell)
    cell_draw.text(
        (0, BASELINE),
        chr(codepoint),
        font=ARABIC_FONT,
        fill=255,
        anchor="ls",
    )
    cell = cell.resize(
        (UI_WIDTH * ZOOM, UI_HEIGHT * ZOOM),
        Image.Resampling.BILINEAR,
    )
    arabic_cell_cache[glyph] = cell
    return cell


def draw_ascii(value: str, x: float, y: float):
    cursor = x
    for ch in value:
        glyph = ord(ch)
        if glyph == 32:
            cursor += UI_WIDTH
        else:
            draw_scaled_ascii_cell(glyph, cursor, y)
            cursor += UI_WIDTH


def draw_runtime_text(value: str, x: float, y: float):
    glyphs = shape_text(value)
    contains_arabic = any(is_arabic_glyph(glyph) for glyph in glyphs)
    cursor = x

    for glyph in glyphs:
        if glyph == 32:
            cursor += UI_WIDTH * (0.5 if contains_arabic else 1.0)
            continue

        if is_arabic_glyph(glyph):
            cell = arabic_cell(glyph)
            position = (int(round(cursor * ZOOM)), int(round(y * ZOOM)))
            canvas.paste(INK, position, cell)
            codepoint = presentation_codepoint(glyph)
            advance = round(ARABIC_FONT.getlength(chr(codepoint)))
            cursor += UI_WIDTH * advance / FONT_CELL_W
        else:
            draw_scaled_ascii_cell(glyph, cursor, y)
            cursor += UI_WIDTH


def draw_rule(y: int):
    draw.line(
        (40 * ZOOM, y * ZOOM, 940 * ZOOM, y * ZOOM),
        fill=(36, 67, 92),
        width=ZOOM,
    )


draw_ascii("ENGLISH / LATIN FONT", 40, 18)
draw_runtime_text("PS3 SCALE 4.92", 40, 42)
draw_rule(78)

draw_ascii("ARABIC SAMPLE 1 / SALAM", 40, 92)
draw_runtime_text("سلام", 40, 116)
draw_rule(152)

draw_ascii("ARABIC SAMPLE 2 / SETTINGS", 40, 166)
draw_runtime_text("الإعدادات", 40, 190)
draw_rule(226)

draw_ascii("ARABIC SAMPLE 3 / GAME LIBRARY", 40, 240)
draw_runtime_text("مكتبة الألعاب", 40, 264)
draw_rule(300)

draw_ascii("ARABIC SAMPLE 4 / VOLUME", 40, 314)
draw_runtime_text("مستوى الصوت", 40, 338)
draw_rule(374)

draw_ascii("MIXED ARABIC / ENGLISH / NUMBERS", 40, 388)
draw_runtime_text("PS3: 3 | PS2: 4 | النتائج: 4", 40, 412)


# Runtime regression checks.
assert shape_text("سلام") == [225, 142, 224, 179]
assert glyphs_joined(142, 224)
assert glyphs_joined(224, 179)
assert not glyphs_joined(225, 142)
assert len(shape_text("الإعدادات")) == 9
assert all(is_arabic_glyph(g) for g in shape_text("الإعدادات"))
assert shape_text("PS3 SCALE 4.92") == list(map(ord, "PS3 SCALE 4.92"))
assert 32 in shape_text("مستوى الصوت")

OUT.parent.mkdir(parents=True, exist_ok=True)
canvas.save(OUT, format="PNG")
print("Arabic TTF font preview: PASS")
print("سلام:", shape_text("سلام"))
print("الإعدادات:", shape_text("الإعدادات"))
print("مكتبة الألعاب:", shape_text("مكتبة الألعاب"))
print("مستوى الصوت:", shape_text("مستوى الصوت"))
print("Mixed:", shape_text("PS3: 3 | PS2: 4 | النتائج: 4"))
print("PNG:", OUT)
