#!/usr/bin/env python3
"""Approximate PS3 SCALE staircase geometry snapshots (NOT physical PS3 captures).

No external dependencies. Uses the same geometry, camera-space perspective,
step/material colors and painter ordering as the Tiny3D PS3 renderer.
The preview is deliberately a simplified flat-color rasterization; the
actual RSX per-vertex interpolation, blending and timing must be checked on PS3.
"""
from __future__ import annotations
import math
from pathlib import Path
from generate_xmb_assets import Canvas

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build-host" / "staircase-preview"
W, H = 854, 512
STEPS, PER_TURN, PERIOD = 32, 18, 21600
TAU = math.pi * 2
ANGLE_STEP = TAU / PER_TURN
RISE = 0.72
CENTER_X, CENTER_Z = 7.2, 34.0
INNER, OUTER, FOCAL = 2.0, 14.0, 780.0
YAW = math.atan2(CENTER_X, CENTER_Z)
SIN_YAW, COS_YAW = math.sin(YAW), math.cos(YAW)


def blend(a, b, t):
    t = min(1.0, max(0.0, t))
    return tuple(int(x * (1 - t) + y * t) for x, y in zip(a, b))


def world(radius, angle, y):
    return CENTER_X + math.cos(angle) * radius, y, CENTER_Z + math.sin(angle) * radius


def project(p, camera_y):
    x, y, z = p
    x = x * COS_YAW - z * SIN_YAW
    z = p[0] * SIN_YAW + p[2] * COS_YAW
    if z <= 2.5:
        return None
    return 427 + x * FOCAL / z, 256 - (y - camera_y) * FOCAL / z, z


def frames_geometry(frame):
    camera_steps = (frame % PERIOD) * PER_TURN / PERIOD
    whole = int(camera_steps)
    camera_y = (camera_steps - whole) * RISE
    faces = []
    for i in range(STEPS):
        angle = (i + whole) * ANGLE_STEP
        following = angle + ANGLE_STEP * 0.92
        y = (i - (STEPS - 1) / 2) * RISE
        top = [world(INNER, angle, y + .14),
               world(OUTER, angle, y + .14),
               world(OUTER, following, y + .14),
               world(INNER, following, y + .14)]
        riser = [world(OUTER, angle, y - .20),
                 world(OUTER, following, y - .20), top[2], top[1]]
        side = [world(INNER, following, y - .20),
                riser[1], top[2], top[3]]
        lip = [top[1], top[2], world(OUTER, following, y + .025),
               world(OUTER, angle, y + .025)]
        shadow = [riser[0], riser[1],
                  world(OUTER, following, y - .29),
                  world(OUTER, angle, y - .29)]
        light = .68 + .25 * math.cos(angle + .42)
        palette = [
            (top, blend((227,221,236), (255,254,255), light), True),
            (riser, blend((183,172,202), (226,220,236), light), False),
            (side, blend((190,181,210), (230,224,239), light), False),
            (lip, blend((222,215,239), (255,254,255), light), True),
            (shadow, blend((146,138,174), (189,180,209), light), False),
        ]
        for vertices, color, outline in palette:
            points = [project(p, camera_y) for p in vertices]
            if any(p is None for p in points):
                continue
            depth = sum(p[2] for p in points) / 4
            faces.append((depth, [(p[0], p[1]) for p in points], color, outline))
    return camera_y, whole, sorted(faces, key=lambda f: f[0], reverse=True)


def draw_background(image, frame):
    phase = (frame % PERIOD) / PERIOD * TAU
    daylight = .82 + .12 * math.cos(phase)
    top = blend((232,229,240), (253,252,255), daylight)
    bottom = blend((216,210,231), (242,239,249), daylight)
    for y in range(H):
        image.rect(0, y, W, 1, blend(top, bottom, y / (H - 1)))
    for ring in (178, 164):
        image.circle(565, 250, ring, (230,221,242), 1)


def draw_rails(image, frame, camera_y, whole):
    previous = [None, None]
    for index in range(STEPS * 6 + 1):
        t = index / 6
        angle = (t + whole) * ANGLE_STEP
        y = (t - (STEPS - 1) * .5) * RISE
        pts = [project(world(OUTER + .62, angle, y + 1.15), camera_y),
               project(world(INNER + .42, angle, y + .92), camera_y)]
        for j, point in enumerate(pts):
            if previous[j] is not None and point is not None:
                color = (183,154,223) if j == 0 else (216,201,240)
                image.line(*previous[j][:2], *point[:2], color, 2)
            previous[j] = point
    for i in range(0, STEPS, 3):
        angle = (i + whole) * ANGLE_STEP
        y = (i - (STEPS - 1) * .5) * RISE
        base = project(world(OUTER, angle, y + .16), camera_y)
        rail = project(world(OUTER + .62, angle, y + 1.15), camera_y)
        if base and rail:
            image.line(*base[:2], *rail[:2], (185,165,221), 1)


def render(frame):
    image = Canvas(W, H, (252,252,255), (230,226,242))
    draw_background(image, frame)
    camera_y, whole, faces = frames_geometry(frame)
    for _, polygon, color, outline in faces:
        image.poly(polygon, color)
        if outline:
            for i in range(4):
                image.line(*polygon[i], *polygon[(i+1) % 4],
                           (248,244,255), 1)
    draw_rails(image, frame, camera_y, whole)
    return image


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    frame_numbers = (0, 5400, 10800, 16199, 21599)
    sheet = Canvas(2*W, 3*H, (254,252,255), (234,230,248))
    for ix, frame in enumerate(frame_numbers):
        c = render(frame)
        c.save(OUT / f"staircase_frame_{frame:05d}.png")
        left = (ix % 2) * W
        top = (ix // 2) * H
        for y in range(H):
            a = (y*W)*3
            b = ((top+y)*sheet.w+left)*3
            sheet.p[b:b+W*3] = c.p[a:a+W*3]
    sheet.save(OUT / "staircase_motion_contact_sheet.png")
    assert all((OUT / f"staircase_frame_{f:05d}.png").stat().st_size > 5000
               for f in frame_numbers)
    print("Generated approximate staircase motion snapshots (not RSX captures)")


if __name__ == "__main__":
    main()
