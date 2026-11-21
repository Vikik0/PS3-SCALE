#!/usr/bin/env python3
"""Generate PS3 SCALE XMB artwork from the approved pearl-white staircase source and neon-glass icon."""
from __future__ import annotations
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PKG = ROOT / "pkgfiles"
PIC1_SOURCE = ROOT / "assets/xmb/pic1_source.jpg"
ICON0_SOURCE = ROOT / "assets/xmb/icon0_source.png"


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
