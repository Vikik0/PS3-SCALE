#!/usr/bin/env python3
"""Generate PS3 SCALE XMB artwork from the approved pearl-white staircase source and neon-glass icon."""
from __future__ import annotations
from pathlib import Path
import struct, zlib

ROOT = Path(__file__).resolve().parents[1]
PKG = ROOT / "pkgfiles"
PIC1_SOURCE = ROOT / "assets/xmb/pic1_source.jpg"
ICON0_SOURCE = ROOT / "assets/xmb/icon0_source.png"

class Canvas:
    """Small RGB raster canvas retained for the host-side staircase QA renderer."""
    def __init__(self, w, h, top, bottom):
        self.w, self.h = w, h
        self.p = bytearray(w * h * 3)
        for y in range(h):
            t = y / max(1, h - 1)
            c = tuple(int(top[k] * (1 - t) + bottom[k] * t) for k in range(3))
            row = bytes(c) * w
            self.p[y * w * 3:(y + 1) * w * 3] = row

    def rect(self, x, y, w, h, c):
        x0, y0 = max(0, int(x)), max(0, int(y))
        x1, y1 = min(self.w, int(x + w)), min(self.h, int(y + h))
        if x1 <= x0 or y1 <= y0:
            return
        row = bytes(c) * (x1 - x0)
        for yy in range(y0, y1):
            i = (yy * self.w + x0) * 3
            self.p[i:i + len(row)] = row

    def line(self, x0, y0, x1, y1, c, width=1):
        x0, y0, x1, y1 = map(int, (x0, y0, x1, y1))
        dx, sx = abs(x1 - x0), 1 if x0 < x1 else -1
        dy, sy = -abs(y1 - y0), 1 if y0 < y1 else -1
        err = dx + dy
        while True:
            self.rect(x0 - width // 2, y0 - width // 2, width, width, c)
            if x0 == x1 and y0 == y1:
                break
            e = 2 * err
            if e >= dy:
                err += dy
                x0 += sx
            if e <= dx:
                err += dx
                y0 += sy

    def poly(self, pts, c):
        ymin = max(0, int(min(y for x, y in pts)))
        ymax = min(self.h - 1, int(max(y for x, y in pts)))
        for y in range(ymin, ymax + 1):
            xs = []
            for i, (x1, y1) in enumerate(pts):
                x2, y2 = pts[(i + 1) % len(pts)]
                if y1 != y2 and min(y1, y2) <= y < max(y1, y2):
                    xs.append(int(x1 + (y - y1) * (x2 - x1) / (y2 - y1)))
            xs.sort()
            for i in range(0, len(xs) - 1, 2):
                self.rect(xs[i], y, xs[i + 1] - xs[i] + 1, 1, c)

    def save(self, path):
        raw = b"".join(
            b"\x00" + self.p[y * self.w * 3:(y + 1) * self.w * 3]
            for y in range(self.h)
        )
        def chunk(kind, data):
            return (struct.pack(">I", len(data)) + kind + data +
                    struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff))
        png = (
            b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", self.w, self.h, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) +
            chunk(b"IEND", b"")
        )
        Path(path).write_bytes(png)


def load_display_font(size, bold=False):
    from PIL import ImageFont
    candidates = (
        Path("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf") if bold else
        Path("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
        Path("/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf") if bold else
        Path("/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"),
    )
    for candidate in candidates:
        if candidate.is_file():
            try:
                return ImageFont.truetype(str(candidate), size=size)
            except OSError:
                pass
    return ImageFont.load_default()


def generate():
    from PIL import Image, ImageDraw, ImageOps

    PKG.mkdir(parents=True, exist_ok=True)
    if not PIC1_SOURCE.is_file():
        raise RuntimeError(f"Approved XMB staircase source is missing: {PIC1_SOURCE}")
    if not ICON0_SOURCE.is_file():
        raise RuntimeError(f"Approved square neon-glass icon is missing: {ICON0_SOURCE}")

    # PIC1 is the clean 16:9 pearl-white / lavender XMB wallpaper. It contains
    # one staircase; the right side remains deliberately quiet.
    with Image.open(PIC1_SOURCE) as source_file:
        source = source_file.convert("RGB")
    wallpaper = ImageOps.fit(source, (1280, 720), method=Image.LANCZOS)
    wallpaper.save(PKG / "PIC1.PNG", format="PNG", optimize=True)

    # Preserve the approved square icon without stretching it into the wide
    # XMB slot. The square is centered on the required 320x176 canvas.
    with Image.open(ICON0_SOURCE) as icon_file:
        square_icon = icon_file.convert("RGB")
    if square_icon.size != (176, 176):
        raise RuntimeError(
            f"Approved ICON0 source dimensions {square_icon.size}, expected (176, 176)"
        )
    icon_canvas = Image.new("RGB", (320, 176), (246, 243, 253))
    icon_canvas.paste(square_icon, (72, 0))
    icon_canvas.save(PKG / "ICON0.PNG", format="PNG", optimize=True)

    # PIC0 is the XMB preview tile: the same single-staircase scene with a
    # restrained PS3 SCALE identity panel on the right.
    pic0 = ImageOps.fit(source, (310, 176), method=Image.LANCZOS).convert("RGBA")
    overlay = Image.new("RGBA", pic0.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    draw.rectangle(
        (143, 13, 299, 163),
        fill=(251, 249, 255, 232),
        outline=(176, 150, 224, 255), width=2
    )
    draw.line((153, 19, 289, 19), fill=(204, 185, 239, 255), width=1)
    draw.line((158, 139, 285, 139), fill=(177, 145, 226, 255), width=2)
    draw.line((221, 28, 221, 47), fill=(48, 39, 76, 255), width=3)
    draw.line((221, 28, 234, 32), fill=(48, 39, 76, 255), width=3)
    draw.line((234, 32, 234, 46), fill=(48, 39, 76, 255), width=3)
    draw.line((221, 46, 237, 52), fill=(48, 39, 76, 255), width=2)

    font_ps3 = load_display_font(25, bold=True)
    font_scale = load_display_font(22, bold=True)
    font_small = load_display_font(8, bold=True)

    def centered_text(label, y, selected_font, color):
        try:
            bounds = draw.textbbox((0, 0), label, font=selected_font)
            width = bounds[2] - bounds[0]
        except AttributeError:
            width = draw.textsize(label, font=selected_font)[0]
        x = 221 - width // 2
        draw.text((x, y), label, font=selected_font, fill=color)

    centered_text("PS3", 62, font_ps3, (43, 35, 69, 255))
    centered_text("SCALE", 94, font_scale, (126, 75, 204, 255))
    centered_text("GAME ENHANCER", 145, font_small, (92, 82, 119, 255))
    pic0 = Image.alpha_composite(pic0, overlay).convert("RGB")
    pic0.save(PKG / "PIC0.PNG", format="PNG", optimize=True)

    for name, expected in (
        ("PIC1.PNG", (1280, 720)),
        ("ICON0.PNG", (320, 176)),
        ("PIC0.PNG", (310, 176)),
    ):
        path = PKG / name
        with Image.open(path) as image_file:
            if image_file.format != "PNG":
                raise RuntimeError(f"{name} is not PNG")
            if image_file.size != expected:
                raise RuntimeError(
                    f"{name} dimensions {image_file.size}, expected {expected}"
                )
        data = path.read_bytes()
        print(f"XMB asset ready: {name} {expected[0]}x{expected[1]}, {len(data):,} bytes")


if __name__ == "__main__":
    generate()
