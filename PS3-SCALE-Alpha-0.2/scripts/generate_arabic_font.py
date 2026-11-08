#!/usr/bin/env python3
"""Regenerate the embedded PS3 SCALE Arabic bitmap atlas.

Requires Pillow and a copy of NotoSansArabic-Regular.ttf:
  python3 scripts/generate_arabic_font.py --font /path/to/NotoSansArabic-Regular.ttf

The output is a 1bpp, 32x32-cell atlas compatible with libfont.
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import re
import statistics
import unicodedata
import zlib
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
MAP_PATH = ROOT / "source/ps3/arabic_glyph_map.h"
DEFAULT_OUTPUT = ROOT / "source/ps3/font_bitmap_z.h"

FONT_FIRST = 32
FONT_LAST = 255
FONT_WIDTH = 32
FONT_HEIGHT = 32
FONT_SIZE = 26
FONT_BASELINE = 25
INK_THRESHOLD = 96
RAW_SIZE = (FONT_LAST - FONT_FIRST + 1) * (FONT_WIDTH * FONT_HEIGHT // 8)


def read_arabic_map():
    text = MAP_PATH.read_text(encoding="utf-8")
    rows = re.findall(
        r"\\{\\s*0x([0-9a-fA-F]+)\\s*,\\s*(\\d+)\\s*,"
        r"\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*\\}",
        text,
    )
    forms = {
        int(cp, 16): tuple(int(value) for value in values)
        for cp, *values in rows
    }
    if len(forms) < 30:
        raise RuntimeError("Arabic contextual form map could not be parsed")
    return forms


def presentation_form_codepoints():
    result = {}
    tags = {"<isolated>", "<final>", "<initial>", "<medial>"}
    for codepoint in range(0xFE70, 0xFF00):
        parts = unicodedata.decomposition(chr(codepoint)).split()
        if len(parts) == 2 and parts[0] in tags:
            base = int(parts[1], 16)
            result[(base, parts[0][1:-1])] = codepoint
    return result


def build_glyph_map():
    forms = read_arabic_map()
    presentation = presentation_form_codepoints()
    glyphs = {}
    tags = ("isolated", "final", "initial", "medial")
    for base, values in forms.items():
        for tag, glyph in zip(tags, values):
            if not glyph:
                continue
            codepoint = presentation.get((base, tag))
            if codepoint is None:
                raise RuntimeError(
                    f"Missing Unicode presentation form for U+{base:04X} {tag}"
                )
            glyphs[glyph] = codepoint

    # Spanish accents and inverted punctuation used by the extended locale.
    glyphs.update({
        246: 0x00E1,
        247: 0x00E9,
        248: 0x00ED,
        249: 0x00F3,
        250: 0x00FA,
        251: 0x00F1,
        252: 0x00FC,
        253: 0x00BF,
        254: 0x00A1,
    })
    if len([glyph for glyph in glyphs if 128 <= glyph <= 244]) != 117:
        raise RuntimeError("Expected all 117 Arabic contextual glyphs")
    return glyphs


def render_atlas(font_path: Path) -> bytes:
    font = ImageFont.truetype(str(font_path), FONT_SIZE)
    glyph_map = build_glyph_map()
    atlas = bytearray(RAW_SIZE)
    bytes_per_glyph = FONT_WIDTH * FONT_HEIGHT // 8

    for glyph, codepoint in glyph_map.items():
        image = Image.new("L", (FONT_WIDTH, FONT_HEIGHT), 0)
        draw = ImageDraw.Draw(image)
        # Presentation forms are already shaped; render each one as one glyph.
        draw.text(
            (1, FONT_BASELINE),
            chr(codepoint),
            font=font,
            fill=255,
            anchor="ls",
        )
        pixels = image.load()
        offset = (glyph - FONT_FIRST) * bytes_per_glyph
        for y in range(FONT_HEIGHT):
            for x in range(FONT_WIDTH):
                if pixels[x, y] >= INK_THRESHOLD:
                    atlas[offset + y * 4 + (x >> 3)] |= 0x80 >> (x & 7)

    if len(atlas) != RAW_SIZE:
        raise RuntimeError("Unexpected bitmap atlas size")
    return bytes(atlas)


def make_header(atlas: bytes) -> str:
    encoded = base64.b64encode(zlib.compress(atlas, 9)).decode("ascii")
    lines = [encoded[i:i + 70] for i in range(0, len(encoded), 70)]
    body = "\\n".join(f'    "{line}"' for line in lines)
    return f"""#ifndef PS3SCALE_FONT_BITMAP_Z_H
#define PS3SCALE_FONT_BITMAP_Z_H
#define PS3SCALE_FONT_RAW_SIZE {RAW_SIZE}u
#define PS3SCALE_FONT_WIDTH {FONT_WIDTH}
#define PS3SCALE_FONT_HEIGHT {FONT_HEIGHT}
#define PS3SCALE_FONT_FIRST {FONT_FIRST}
#define PS3SCALE_FONT_LAST {FONT_LAST}
#define PS3SCALE_FONT_EXTENDED_FIRST 128
/*
 * Noto Sans Arabic Regular bitmap atlas.
 * Rasterized at {FONT_SIZE} px with a shared baseline into 32x32 cells.
 * Arabic presentation forms are mapped in arabic_glyph_map.h.
 * Slots 128..244 contain the 117 Arabic forms; 246..254 contain
 * Spanish accents and punctuation. 1 bit per pixel, MSB first.
 * Copyright 2010, 2012-2020 Google Inc.; 2015-2020 Google LLC.
 * Licensed under the SIL Open Font License, Version 1.1.
 */
static const char ps3scale_font_z64[] =
{body};
#endif
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--font", type=Path, help="NotoSansArabic-Regular.ttf")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    font_path = args.font or Path(
        "/usr/share/fonts/truetype/noto/NotoSansArabic-Regular.ttf"
    )
    if not font_path.is_file():
        parser.error("Pass --font with the path to NotoSansArabic-Regular.ttf")

    atlas = render_atlas(font_path)
    output = args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(make_header(atlas), encoding="utf-8")
    print(f"Generated {output}")
    print(f"Raw atlas SHA-256: {hashlib.sha256(atlas).hexdigest()}")
    print(f"Arabic glyph count: {len([g for g in build_glyph_map() if 128 <= g <= 244])}")


if __name__ == "__main__":
    main()
