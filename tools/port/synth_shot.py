#!/usr/bin/env python3
"""Coarse checks of a screenshot of the synthetic cartridge's race (docs/port/SYNTH.md 2).

    python3 tools/port/synth_shot.py SHOT.png VIEWS

The race draws the game's own display lists with the cartridge's models: the ground, a checker of greens and greys,
and the cars, coloured boxes. Exact pixels depend on the physics and the renderer, so this checks what doesn't: the
picture is the game's screen (320 x 220), and each of the VIEWS viewports the split screen should have (1: the
screen; 2: its top and bottom halves; 3 or 4: its quarters, the fourth empty with 3) shows drawing, a share of its
pixels that are not black, while the borders between them stay dark. Exit status 1 on a failed check.
"""
import struct
import sys
import zlib


def read_png(path):
    """(width, height, rows of (r, g, b)) of an 8-bit RGB or RGBA PNG, with the five filters."""
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"synth_shot: {path} is not a PNG")
    pos, idat, w = 8, b"", 0
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if kind == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", body[:10])
            if depth != 8 or ctype not in (2, 6):
                raise SystemExit(f"synth_shot: {path}: only 8-bit RGB and RGBA")
            bpp = 3 if ctype == 2 else 4
        elif kind == b"IDAT":
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    stride = w * bpp
    rows, prev, i = [], bytearray(stride), 0
    for _ in range(h):
        f, line = raw[i], bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                line[x] = (line[x] + a) & 0xFF
            elif f == 2:
                line[x] = (line[x] + b) & 0xFF
            elif f == 3:
                line[x] = (line[x] + (a + b) // 2) & 0xFF
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xFF
        rows.append([tuple(line[x:x + 3]) for x in range(0, stride, bpp)])
        prev = line
    return w, h, rows


def lit(px):
    return max(px) > 24


def share(rows, x0, y0, x1, y1):
    n = sum(lit(rows[y][x]) for y in range(y0, y1) for x in range(x0, x1))
    return n / max(1, (x1 - x0) * (y1 - y0))


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    path, views = sys.argv[1], int(sys.argv[2])
    w, h, rows = read_png(path)
    fails = []
    if (w, h) != (320, 220):
        fails.append(f"the picture is {w} x {h}, not the game's 320 x 220")
    else:
        hw, hh = w // 2, h // 2
        if views == 1:
            regions = {"the screen": (0, 0, w, h)}
            borders = {}
        elif views == 2:
            regions = {"the top view": (0, 0, w, hh - 2), "the bottom view": (0, hh + 2, w, h)}
            borders = {"the border between them": (0, hh - 1, w, hh + 1)}
        else:
            regions = {"the top left view": (0, 0, hw - 2, hh - 2), "the top right view": (hw + 2, 0, w, hh - 2),
                       "the bottom left view": (0, hh + 2, hw - 2, h)}
            if views == 4:
                regions["the bottom right view"] = (hw + 2, hh + 2, w, h)
            borders = {"the border between them": (hw - 1, 0, hw + 1, h)}
        for name, r in regions.items():
            s = share(rows, *r)
            print(f"synth_shot: {name}: {100 * s:.1f}% drawn")
            if s < 0.02:
                fails.append(f"{name} is black ({100 * s:.1f}% drawn)")
        for name, r in borders.items():
            s = share(rows, *r)
            print(f"synth_shot: {name}: {100 * s:.1f}% drawn")
            if s > 0.5:
                fails.append(f"{name} is drawn over ({100 * s:.1f}%): not {views} views")
    for f in fails:
        print(f"synth_shot: FAIL: {f}")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
