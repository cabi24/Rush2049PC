#!/usr/bin/env python3
"""Find frames that differ from both neighbours far more than the neighbours differ from each other.

    RUSH_GFX_DUMP=DIR:FIRST:COUNT build/pc/rush2049pc ...      (docs/port/GFX.md: every graphics task's frame)
    tools/port/frame_spikes.py DIR [--min 2.0]

A frame drawn wrongly once (a polygon culled, a texture missing, the sky through the road) stands out against the
frames before and after it, while the game's own motion changes neighbours gradually. For each frame k, with d the
mean absolute difference of two frames (0-255, on a grid of every 4th pixel),
    spike = (d(k-1, k) + d(k, k+1)) / 2 - d(k-1, k+1).
Frames whose spike is at least --min are printed, largest first. Legitimate spikes exist (a camera cut, a car that
blinks after a reset, a fast-moving fine pattern): look at the frames.

Reads the renderer's own PNGs (8-bit RGB, filter 0 rows: gfx_png.c) with the standard library only.
"""
import argparse
import os
import struct
import sys
import zlib


def read_png(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(path + ": not a PNG")
    pos, idat, w = 8, b"", 0
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if kind == b"IHDR":
            w, h, depth, color = struct.unpack(">IIBB", body[:10])
            if depth != 8 or color != 2:
                raise ValueError(path + ": not 8-bit RGB (written by gfx_png.c?)")
        elif kind == b"IDAT":
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    stride = 1 + 3 * w
    rows = []
    for y in range(0, h, 4):
        row = raw[y * stride:(y + 1) * stride]
        if row[0] != 0:
            raise ValueError(path + ": filtered rows (written by gfx_png.c?)")
        rows.append(row[1::12])   # every 4th pixel's red, then green, blue below
        rows.append(row[2::12])
        rows.append(row[3::12])
    return rows


def diff(a, b):
    total = count = 0
    for ra, rb in zip(a, b):
        total += sum(abs(x - y) for x, y in zip(ra, rb))
        count += len(ra)
    return total / count if count else 0.0


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("dir")
    ap.add_argument("--min", type=float, default=2.0, help="smallest spike printed (default 2.0)")
    args = ap.parse_args()
    names = sorted(n for n in os.listdir(args.dir) if n.endswith(".png"))
    if len(names) < 3:
        sys.exit("need at least three frames in " + args.dir)
    frames = [read_png(os.path.join(args.dir, n)) for n in names]
    found = []
    for k in range(1, len(frames) - 1):
        a, b, c = diff(frames[k - 1], frames[k]), diff(frames[k], frames[k + 1]), diff(frames[k - 1], frames[k + 1])
        spike = (a + b) / 2 - c
        if spike >= args.min:
            found.append((spike, names[k], a, b, c))
    for spike, name, a, b, c in sorted(found, reverse=True):
        print("%s  spike %.1f  (to the one before %.1f, after %.1f, between those %.1f)" % (name, spike, a, b, c))
    print("%d frames, %d spikes of at least %.1f" % (len(frames), len(found), args.min))


if __name__ == "__main__":
    main()
