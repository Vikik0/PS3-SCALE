#!/usr/bin/env python3
"""Render the real embedded PS3 SCALE bitmap atlas with the runtime Arabic spacing rules.

This is a host-side visual regression aid. It uses only Python's standard library.
"""
from __future__ import annotations

import base64
import binascii
import re
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FONT_Z = ROOT / "source/ps3/font_bitmap_z.h"
FONT_ASCII = ROOT / "source/ps3/font8x12.h"
ARABIC_MAP = ROOT / "source/ps3/arabic_glyph_map.h"
OUT = ROOT / "build-host/arabic-font-preview.png"

FONT_WIDTH = 32
FONT_HEIGHT = 32
FONT_FIRST = 32
FONT_EXTENDED_FIRST = 128
UI_WIDTH = 14
UI_HEIGHT = 22
ZOOM = 4


def read_assets():
    z_text = FONT_Z.read_text(encoding="utf-8")
    z_match = re.search(
        r"static const char ps3scale_font_z64\[\]\s*=\s*(.*?);",
        z_text,
        re.S,
    )
    assert z_match, "compressed font atlas declaration not found"
    encoded = "".join(re.findall(r'"([A-Za-z0-9+/=]+)"', z_match.group(1)))
    atlas = zlib.decompress(base64.b64decode(encoded))
    assert len(atlas) == 224 * 128, f"unexpected atlas size: {len(atlas)}"

    ascii_text = FONT_ASCII.read_text(encoding="utf-8")
    ascii_match = re.search(
        r"ps3scale_font8x12\[\]\s*=\s*\{(.*?)\};",
        ascii_text,
        re.S,
    )
    assert ascii_match, "ASCII bitmap declaration not found"
    ascii_font = bytes(
        int(value, 16)
        for value in re.findall(r"0x([0-9a-fA-F]{2})", ascii_match.group(1))
    )
    assert len(ascii_font) >= (126 - 32 + 1) * 12

    map_text = ARABIC_MAP.read_text(encoding="utf-8")
    rows = re.findall(
        r"\{\s*0x([0-9a-fA-F]+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}",
        map_text,
    )
    forms = {
        int(cp, 16): tuple(int(value) for value in values)
        for cp, *values in rows
    }
    assert len(forms) >= 30, "Arabic contextual form table was not parsed"
    return atlas, ascii_font, forms


ATLAS, ASCII_FONT, FORMS = read_assets()
ARABIC_GLYPHS = {
    glyph
    for form in FORMS.values()
    for glyph in form
    if glyph
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


def glyph_pixel(glyph: int, x: int, y: int) -> bool:
    offset = (glyph - FONT_FIRST) * 128
    byte_index = offset + y * 4 + (x >> 3)
    return bool(ATLAS[byte_index] & (0x80 >> (x & 7)))


def glyph_bounds(glyph: int) -> tuple[int, int]:
    xs = [
        x
        for y in range(FONT_HEIGHT)
        for x in range(FONT_WIDTH)
        if glyph_pixel(glyph, x, y)
    ]
    if not xs:
        return 0, FONT_WIDTH
    return min(xs), max(xs) - min(xs) + 1


WIDTH = 1024
HEIGHT = 390
BACKGROUND = (7, 20, 36)
INK = (226, 245, 255)
ACCENT = (139, 212, 80)
pixels = bytearray(BACKGROUND * (WIDTH * HEIGHT))


def put_pixel(x: int, y: int, color=INK):
    if 0 <= x < WIDTH and 0 <= y < HEIGHT:
        offset = (y * WIDTH + x) * 3
        pixels[offset : offset + 3] = bytes(color)


def fill_rect(x: int, y: int, width: int, height: int, color):
    for py in range(max(0, y), min(HEIGHT, y + height)):
        for px in range(max(0, x), min(WIDTH, x + width)):
            put_pixel(px, py, color)


def draw_scaled_arabic_cell(glyph: int, x: float, y: float):
    for dy in range(UI_HEIGHT):
        source_y = min(FONT_HEIGHT - 1, int(dy * FONT_HEIGHT / UI_HEIGHT))
        for dx in range(UI_WIDTH):
            source_x = min(FONT_WIDTH - 1, int(dx * FONT_WIDTH / UI_WIDTH))
            if glyph_pixel(glyph, source_x, source_y):
                px = int(round((x + dx) * ZOOM))
                py = int(round((y + dy) * ZOOM))
                fill_rect(px, py, ZOOM, ZOOM, INK)


def draw_scaled_ascii_cell(glyph: int, x: float, y: float):
    if not 32 <= glyph <= 126:
        glyph = ord("?")
    offset = (glyph - 32) * 12
    for dy in range(UI_HEIGHT):
        source_y = min(11, int(dy * 12 / UI_HEIGHT))
        row = ASCII_FONT[offset + source_y]
        for dx in range(UI_WIDTH):
            source_x = min(7, int(dx * 8 / UI_WIDTH))
            if row & (0x80 >> source_x):
                px = int(round((x + dx) * ZOOM))
                py = int(round((y + dy) * ZOOM))
                fill_rect(px, py, ZOOM, ZOOM, INK)


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
    scale_x = UI_WIDTH / FONT_WIDTH

    for index, glyph in enumerate(glyphs):
        next_glyph = glyphs[index + 1] if index + 1 < len(glyphs) else 0
        if glyph == 32:
            cursor += UI_WIDTH * (0.5 if contains_arabic else 1.0)
            continue

        if is_arabic_glyph(glyph):
            min_x, ink_width = glyph_bounds(glyph)
            draw_runtime_x = cursor - min_x * scale_x
            draw_scaled_arabic_cell(glyph, draw_runtime_x, y)
            cursor += ink_width * scale_x

            if is_arabic_glyph(next_glyph):
                if glyphs_joined(glyph, next_glyph):
                    cursor -= scale_x * 2.0
                else:
                    cursor += scale_x * 2.0
            elif next_glyph not in (0, 32):
                cursor += scale_x
        else:
            draw_scaled_ascii_cell(glyph, cursor, y)
            cursor += UI_WIDTH


def draw_rule(y: int):
    fill_rect(40 * ZOOM, y * ZOOM, 900 * ZOOM, 1 * ZOOM, (36, 67, 92))


# Visual labels are English so the sample Arabic words remain isolated for review.
draw_ascii("ENGLISH / LATIN FONT", 40, 18)
draw_runtime_text("PS3 SCALE 4.92", 40, 42)
draw_rule(78)

draw_ascii("ARABIC SAMPLE 1", 40, 92)
draw_runtime_text("سلام", 40, 116)
draw_rule(152)

draw_ascii("ARABIC SAMPLE 2", 40, 166)
draw_runtime_text("الإعدادات", 40, 190)
draw_rule(226)

draw_ascii("ARABIC SAMPLE 3", 40, 240)
draw_runtime_text("مكتبة الألعاب", 40, 264)
draw_rule(300)

draw_ascii("ARABIC SAMPLE 4", 40, 314)
draw_runtime_text("مستوى الصوت", 40, 338)


def png_chunk(kind: bytes, data: bytes) -> bytes:
    return (
        struct.pack(">I", len(data))
        + kind
        + data
        + struct.pack(">I", binascii.crc32(kind + data) & 0xFFFFFFFF)
    )


def write_png(path: Path):
    path.parent.mkdir(parents=True, exist_ok=True)
    scanlines = bytearray()
    stride = WIDTH * 3
    for row in range(HEIGHT):
        scanlines.append(0)
        start = row * stride
        scanlines.extend(pixels[start : start + stride])

    header = struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 2, 0, 0, 0)
    payload = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", zlib.compress(bytes(scanlines), 9))
        + png_chunk(b"IEND", b"")
    )
    path.write_bytes(payload)


# Hard regression checks against the current C locale implementation.
assert shape_text("سلام") == [225, 142, 224, 179]
assert glyphs_joined(142, 224)
assert glyphs_joined(224, 179)
assert not glyphs_joined(225, 142)
assert len(shape_text("الإعدادات")) == 9
assert all(is_arabic_glyph(g) for g in shape_text("الإعدادات"))
assert shape_text("PS3 SCALE 4.92") == list(map(ord, "PS3 SCALE 4.92"))
assert 32 in shape_text("مستوى الصوت")

missing = [glyph for glyph in ARABIC_GLYPHS if glyph_bounds(glyph)[1] <= 0]
assert not missing, f"Arabic glyphs missing from bitmap atlas: {missing}"

write_png(OUT)
print("Arabic font preview: PASS")
print("سلام:", shape_text("سلام"))
print("الإعدادات:", shape_text("الإعدادات"))
print("مكتبة الألعاب:", shape_text("مكتبة الألعاب"))
print("مستوى الصوت:", shape_text("مستوى الصوت"))
print("PNG:", OUT)
