#!/usr/bin/env python3
"""The program's icons from dist/rush2049pc.svg (docs/port/WINDOWS.md, "Icon and version"):

  dist/rush2049pc.ico     the Windows icon (port/app/rush2049pc.rc): 16 to 256 px, each size drawn at that size
  port/app/icon_data.h    a 64 x 64 RGBA image, zlib-compressed, for SDL_SetWindowIcon (port/app/icon.c)

Both are committed, so a build needs neither this script nor what it uses (pycairo). Run it again after changing
the SVG:

    python3 tools/port/make_icon.py

It draws the SVG with cairo, and understands only what that file uses: <rect> (with rx) and <path> with absolute
M, L, H, V and Z, filled or stroked (stroke-width, stroke-linecap). Anything else is an error, so a new element is
noticed rather than left out.
"""
import argparse
import io
import re
import struct
import sys
import xml.etree.ElementTree as ET
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ICO_SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)   # 100% to 250% scaling of the small and large icons, and 256
PNG_FROM = 64     # sizes from this up are stored as PNG (Windows Vista and later), smaller ones as BMP
SDL_SIZE = 64
NS = "{http://www.w3.org/2000/svg}"


def colour(text):
    m = re.fullmatch(r"#([0-9a-fA-F]{6})", text.strip())
    if not m:
        raise SystemExit(f"make_icon: colour {text!r}: only #rrggbb is understood")
    v = int(m.group(1), 16)
    return ((v >> 16) & 255) / 255, ((v >> 8) & 255) / 255, (v & 255) / 255


def path_ops(d):
    """The path as (op, x, y) with op in M, L, Z, absolute coordinates."""
    tokens = re.findall(r"[MLHVZmlhvz]|-?\d*\.?\d+(?:[eE][-+]?\d+)?", d)
    ops, i, cmd, x, y, sx, sy = [], 0, None, 0.0, 0.0, 0.0, 0.0
    while i < len(tokens):
        t = tokens[i]
        if t.isalpha():
            if t.islower():
                raise SystemExit(f"make_icon: path command {t!r}: only absolute commands are understood")
            cmd, i = t, i + 1
            if cmd == "Z":
                ops.append(("Z", sx, sy))
                x, y = sx, sy
            continue
        if cmd in ("M", "L"):
            x, y = float(tokens[i]), float(tokens[i + 1])
            i += 2
            ops.append((cmd, x, y))
            if cmd == "M":
                sx, sy, cmd = x, y, "L"   # pairs after M are lines
        elif cmd == "H":
            x, i = float(t), i + 1
            ops.append(("L", x, y))
        elif cmd == "V":
            y, i = float(t), i + 1
            ops.append(("L", x, y))
        else:
            raise SystemExit(f"make_icon: path data {d!r}: unexpected {t!r}")
    return ops


def render(svg, size):
    """The SVG drawn at size x size: cairo's ARGB32 surface."""
    import cairo
    root = ET.parse(svg).getroot()
    vb = [float(v) for v in root.get("viewBox").split()]
    surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, size, size)
    cr = cairo.Context(surface)
    cr.scale(size / vb[2], size / vb[3])
    cr.translate(-vb[0], -vb[1])
    for el in root:
        tag = el.tag.replace(NS, "")
        if tag == "rect":
            x, y, w, h = (float(el.get(k)) for k in ("x", "y", "width", "height"))
            r = min(float(el.get("rx", el.get("ry", "0"))), w / 2, h / 2)
            cr.new_sub_path()
            cr.arc(x + w - r, y + r, r, -3.14159265 / 2, 0)
            cr.arc(x + w - r, y + h - r, r, 0, 3.14159265 / 2)
            cr.arc(x + r, y + h - r, r, 3.14159265 / 2, 3.14159265)
            cr.arc(x + r, y + r, r, 3.14159265, 3 * 3.14159265 / 2)
            cr.close_path()
        elif tag == "path":
            for op, x, y in path_ops(el.get("d")):
                if op == "M":
                    cr.move_to(x, y)
                elif op == "L":
                    cr.line_to(x, y)
                else:
                    cr.close_path()
        else:
            raise SystemExit(f"make_icon: <{tag}>: only <rect> and <path> are understood")
        fill, stroke = el.get("fill", "#000000" if tag == "rect" else None), el.get("stroke")
        if fill and fill != "none":
            cr.set_source_rgb(*colour(fill))
            cr.fill_preserve() if stroke else cr.fill()
        if stroke and stroke != "none":
            cr.set_source_rgb(*colour(stroke))
            cr.set_line_width(float(el.get("stroke-width", "1")))
            cr.set_line_cap({"round": cairo.LINE_CAP_ROUND, "square": cairo.LINE_CAP_SQUARE}.get(
                el.get("stroke-linecap"), cairo.LINE_CAP_BUTT))
            cr.stroke()
        cr.new_path()
    surface.flush()
    return surface


def rgba(surface):
    """Straight-alpha RGBA rows, top to bottom, from cairo's premultiplied native-endian ARGB32."""
    w, h, stride, data = surface.get_width(), surface.get_height(), surface.get_stride(), bytes(surface.get_data())
    out = bytearray()
    little = sys.byteorder == "little"
    for row in range(h):
        for col in range(w):
            px = data[row * stride + col * 4: row * stride + col * 4 + 4]
            b, g, r, a = px if little else px[::-1]
            if a:
                r, g, b = (min(255, (c * 255 + a // 2) // a) for c in (r, g, b))
            out += bytes((r, g, b, a))
    return bytes(out)


def bmp_entry(size, pixels):
    """An icon image as a DIB: BITMAPINFOHEADER (height doubled for the mask), BGRA bottom-up, a 1-bit AND mask."""
    header = struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    rows = []
    for row in range(size - 1, -1, -1):
        line = pixels[row * size * 4:(row + 1) * size * 4]
        rows.append(b"".join(bytes((line[i + 2], line[i + 1], line[i], line[i + 3])) for i in range(0, len(line), 4)))
    mask_stride = ((size + 31) // 32) * 4
    mask = bytearray()
    for row in range(size - 1, -1, -1):
        bits = bytearray(mask_stride)
        for col in range(size):
            if pixels[(row * size + col) * 4 + 3] == 0:
                bits[col // 8] |= 0x80 >> (col % 8)
        mask += bits
    return header + b"".join(rows) + bytes(mask)


def ico(images):
    """An .ico from (size, data) pairs."""
    out = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    body = b""
    for size, data in images:
        dim = 0 if size >= 256 else size
        out += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset + len(body))
        body += data
    return out + body


def c_header(pixels, size):
    packed = zlib.compress(pixels, 9)
    lines = ["/* port/app/icon_data.h: the window icon, made by tools/port/make_icon.py from dist/rush2049pc.svg (do not",
             " * edit; run the script again). RGBA, %d x %d, zlib-compressed. */" % (size, size),
             "#define PC_ICON_SIZE %d" % size,
             "#define PC_ICON_RGBA_BYTES %d" % len(pixels),
             "static const unsigned char pc_icon_rgba_z[%d] = {" % len(packed)]
    for i in range(0, len(packed), 16):
        lines.append("    " + ", ".join("0x%02x" % b for b in packed[i:i + 16]) + ",")
    lines.append("};")
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--svg", default=str(ROOT / "dist/rush2049pc.svg"))
    ap.add_argument("--ico", default=str(ROOT / "dist/rush2049pc.ico"))
    ap.add_argument("--header", default=str(ROOT / "port/app/icon_data.h"))
    a = ap.parse_args()
    try:
        import cairo   # noqa: F401
    except ImportError:
        raise SystemExit("make_icon: needs pycairo (Debian: python3-cairo); the committed icons are unchanged")
    images = []
    for size in ICO_SIZES:
        surface = render(a.svg, size)
        if size >= PNG_FROM:
            png = io.BytesIO()
            surface.write_to_png(png)
            images.append((size, png.getvalue()))
        else:
            images.append((size, bmp_entry(size, rgba(surface))))
    Path(a.ico).write_bytes(ico(images))
    Path(a.header).write_text(c_header(rgba(render(a.svg, SDL_SIZE)), SDL_SIZE))
    print(f"make_icon: {a.ico} ({', '.join(str(s) for s in ICO_SIZES)} px), {a.header} ({SDL_SIZE} px)")


if __name__ == "__main__":
    main()
