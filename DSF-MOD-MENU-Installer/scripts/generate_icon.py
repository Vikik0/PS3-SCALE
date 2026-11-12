#!/usr/bin/env python3
"""Generate a compact 320x176 RGB PNG icon using only the Python stdlib."""
import struct
import zlib

W, H = 320, 176
pixels = [[(7, 11, 17) for _ in range(W)] for _ in range(H)]

for y in range(H):
    for x in range(W):
        lift = (x * 9 + y * 5) // 180
        pixels[y][x] = (min(26, 7 + lift), min(34, 11 + lift), min(48, 17 + lift))
for y in range(H):
    for x in range(7):
        pixels[y][x] = (210, 66, 50) if x < 3 else (90, 34, 36)

FONT = {
    "D": ["11110","10001","10001","10001","10001","10001","11110"],
    "E": ["11111","10000","10000","11110","10000","10000","11111"],
    "F": ["11111","10000","10000","11110","10000","10000","10000"],
    "M": ["10001","11011","10101","10101","10001","10001","10001"],
    "N": ["10001","11001","10101","10011","10001","10001","10001"],
    "O": ["01110","10001","10001","10001","10001","10001","01110"],
    "S": ["01111","10000","10000","01110","00001","00001","11110"],
    "U": ["10001","10001","10001","10001","10001","10001","01110"],
    " ": ["00000"] * 7,
}

def draw_text(text, x, y, scale, color):
    cursor = x
    for char in text:
        glyph = FONT.get(char, FONT[" "])
        for row, bits in enumerate(glyph):
            for col, bit in enumerate(bits):
                if bit == "1":
                    for dy in range(scale):
                        for dx in range(scale):
                            px, py = cursor + col * scale + dx, y + row * scale + dy
                            if 0 <= px < W and 0 <= py < H:
                                pixels[py][px] = color
        cursor += 6 * scale

for y in range(28, 150):
    for x in range(18, 21):
        pixels[y][x] = (222, 78, 57)

draw_text("DSF", 58, 34, 8, (244, 248, 252))
draw_text("MOD MENU", 60, 112, 4, (191, 211, 228))
for x in range(58, 284):
    pixels[155][x] = (70, 105, 129)

raw = bytearray()
for row in pixels:
    raw.append(0)
    for r, g, b in row:
        raw.extend((r, g, b))

def chunk(kind, data):
    payload = kind + data
    return struct.pack(">I", len(data)) + payload + struct.pack(">I", zlib.crc32(payload) & 0xffffffff)

png = b"\x89PNG\r\n\x1a\n"
png += chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
png += chunk(b"IEND", b"")

with open("pkgfiles/ICON0.PNG", "wb") as output:
    output.write(png)
print("Generated pkgfiles/ICON0.PNG")
