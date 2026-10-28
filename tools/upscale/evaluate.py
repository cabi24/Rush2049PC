#!/usr/bin/env python3
"""The upscaler on the held-out CC0 images: PSNR / SSIM against classical 2x scalers, alpha, contact sheets.

    evaluate.py --data DIR --tool build/upscale/upscale_tool [--out DIR] [--sheets]

Every asset fetch_data.py put in the "test" split (never trained on) is reduced whole to 64x64 and 128x128 (the
"ground truth", 2x the size of an N64 texture: they are tileable materials, so everything wraps), then degraded to
every texel format of n64tex.FORMATS with a seed from its name; each method scales the 32x32 / 64x64 input back
to 2x. The network is port/upscale's C code (upscale_tool), so the numbers are the shipped code's. Classical
methods are n64tex's numpy versions (the C Catmull-Rom and bilinear are checked against them). PSNR: RGB, 8 bits;
SSIM: luma, Gaussian 11x11, sigma 1.5.

Alpha: the leaf and foliage atlases' opacity maps, reduced to 128x128 and thresholded as a cut-out's ground
truth, at 64x64 as the input: intersection over union of the 2x alpha with the ground truth for the C code's
alpha and for nearest and bilinear-then-threshold.
"""
import argparse
import hashlib
import json
import os
import struct
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import n64tex  # noqa: E402

CLASSICAL = {"bilinear": n64tex.bilinear2x, "catmull-rom": n64tex.cubic2x, "lanczos3": n64tex.lanczos2x}


def seed_of(*parts):
    return int(hashlib.sha1(":".join(map(str, parts)).encode()).hexdigest()[:8], 16)


def write_batch(path, items):
    with open(path, "wb") as f:
        f.write(b"UPS1" + struct.pack("<I", len(items)))
        for rgba, ws, wt in items:
            h, w, _ = rgba.shape
            f.write(struct.pack("<4i", w, h, ws, wt))
            f.write(np.ascontiguousarray(rgba, np.uint8).tobytes())


def read_batch(path):
    out = []
    with open(path, "rb") as f:
        assert f.read(4) == b"UPS1"
        (n,) = struct.unpack("<I", f.read(4))
        for _ in range(n):
            w, h, _, _ = struct.unpack("<4i", f.read(16))
            out.append(np.frombuffer(f.read(w * h * 4), np.uint8).reshape(h, w, 4))
    return out


def run_tool(tool, method, items):
    with tempfile.TemporaryDirectory() as d:
        write_batch(os.path.join(d, "in.bin"), items)
        r = subprocess.run([tool, "run", method, os.path.join(d, "in.bin"), os.path.join(d, "out.bin")],
                           capture_output=True, text=True, check=True)
        return read_batch(os.path.join(d, "out.bin")), r.stderr.strip()


def to8(x):
    return np.clip(np.floor(x * 255 + 0.5), 0, 255).astype(np.uint8)


def load_rgb(path, size):
    img = Image.open(path).convert("RGB")
    return np.asarray(img.resize((size, size), Image.Resampling.LANCZOS), np.float32) / 255


def sheet(rows, labels, path, scale):
    """rows: lists of uint8 RGB(A) images (same size per column); each shown nearest-magnified to a fixed size."""
    cell = max(im.shape[1] for im in rows[0]) * scale // 2
    pad, top = 4, 16
    W = len(labels) * (cell + pad) + pad
    H = len(rows) * (cell + pad) + pad + top
    canvas = Image.new("RGB", (W, H), (40, 40, 40))
    d = ImageDraw.Draw(canvas)
    for i, lab in enumerate(labels):
        d.text((pad + i * (cell + pad), 2), lab, fill=(230, 230, 230))
    for r, row in enumerate(rows):
        for c, im in enumerate(row):
            if im.shape[2] == 4:   # on a checkerboard
                yy, xx = np.mgrid[0:im.shape[0], 0:im.shape[1]]
                bg = np.where(((yy // 4 + xx // 4) % 2)[..., None] == 0, 90, 150).astype(np.float32)
                a = im[..., 3:4].astype(np.float32) / 255
                im = (im[..., :3] * a + bg * (1 - a)).astype(np.uint8)
            tile = Image.fromarray(im).resize((cell, cell), Image.Resampling.NEAREST)
            canvas.paste(tile, (pad + c * (cell + pad), top + pad + r * (cell + pad)))
    canvas.save(path)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--data", required=True)
    ap.add_argument("--tool", required=True)
    ap.add_argument("--out", default="upscale-eval")
    ap.add_argument("--sheets", action="store_true")
    ap.add_argument("--extra", action="append", default=[], metavar="NAME=TOOL",
                    help="another upscale_tool build (another upscale.c) as a method named NAME")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    index = json.load(open(os.path.join(args.data, "index.json")))
    test = [e for e in index if e["split"] == "test"]
    mats = [e for e in test if e.get("kind") != "atlas"]
    atlases = [e for e in test if e.get("kind") == "atlas" and "opacity" in e["files"]]

    # ---------------------------------------------------------------- colour
    cases = []   # (asset, size, fmt, hr, lr)
    for e in mats:
        for size in (64, 128):
            hr0 = load_rgb(os.path.join(args.data, e["files"]["color"]), size)
            for fmt in n64tex.FORMATS:
                rng = np.random.default_rng(seed_of(e["asset"], size, fmt))
                hr, lr, desc = n64tex.degrade(hr0, rng, fmt=fmt)
                cases.append((e["asset"], size, fmt, hr, lr, desc))
    items = [(np.concatenate([to8(c[4]), np.full(c[4].shape[:2] + (1,), 255, np.uint8)], 2), 1, 1) for c in cases]
    outs = {}
    for m in ("net", "cubic", "bilinear"):
        res, log = run_tool(args.tool, m, items)
        outs["c-" + m] = [r[..., :3].astype(np.float32) / 255 for r in res]
        print(f"C {m}: {log}", file=sys.stderr)
    extra = []
    for spec in args.extra:
        name, tool = spec.split("=", 1)
        res, log = run_tool(tool, "net", items)
        outs[name] = [r[..., :3].astype(np.float32) / 255 for r in res]
        extra.append(name)
        print(f"{name}: {log}", file=sys.stderr)
    for name, fn in CLASSICAL.items():
        outs[name] = [np.clip(fn(c[4].astype(np.float32), 1, 1), 0, 1) for c in cases]
    # the C classical scalers agree with the numpy ones
    for cm, pm in (("c-cubic", "catmull-rom"), ("c-bilinear", "bilinear")):
        diff = max(np.abs(to8(a).astype(int) - to8(b).astype(int)).max() for a, b in zip(outs[cm], outs[pm]))
        print(f"{cm} vs numpy {pm}: max difference {diff} levels", file=sys.stderr)
    methods = ["bilinear", "catmull-rom", "lanczos3"] + extra + ["c-net"]
    table = {}
    for i, c in enumerate(cases):
        for m in methods:
            key = (c[1], c[2], m)
            table.setdefault(key, []).append((n64tex.psnr(outs[m][i], c[3]), n64tex.ssim(outs[m][i], c[3])))
    lines = ["| input | format | " + " | ".join(m if m != "c-net" else "network" for m in methods) + " |",
             "|---|---|" + "---|" * len(methods)]
    summary = {}
    for size in (64, 128):
        for fmt in n64tex.FORMATS + ["all"]:
            cells = []
            for m in methods:
                v = [x for (s, f, mm), l in table.items() if s == size and (f == fmt or fmt == "all") and mm == m
                     for x in l]
                p, q = np.mean([a for a, _ in v]), np.mean([b for _, b in v])
                summary[(size, fmt, m)] = (p, q)
                cells.append(f"{p:.2f} / {q:.4f}")
            lines.append(f"| {size // 2}x{size // 2} | {fmt} | " + " | ".join(cells) + " |")
    report = [f"{len(mats)} held-out materials x 2 sizes x {len(n64tex.FORMATS)} formats = {len(cases)} cases; "
              "PSNR dB / SSIM (mean)", ""] + lines
    # how often the network beats the best classical method, per case
    others = list(CLASSICAL) + extra
    wins = sum(n64tex.psnr(outs["c-net"][i], c[3]) > max(n64tex.psnr(outs[m][i], c[3]) for m in others)
               for i, c in enumerate(cases))
    report.append("")
    report.append(f"network PSNR above the best of {', '.join(others)} in {wins} of {len(cases)} cases")
    pn = [n64tex.psnr(outs["c-net"][i], c[3]) for i, c in enumerate(cases)]
    for m in others:
        pm = [n64tex.psnr(outs[m][i], c[3]) for i, c in enumerate(cases)]
        report.append(f"network PSNR above {m} in {sum(a > b for a, b in zip(pn, pm))} of {len(cases)} cases, "
                      f"mean gain {np.mean(np.array(pn) - np.array(pm)):+.2f} dB")

    # ---------------------------------------------------------------- alpha
    # Held-out atlases, and all of them (EPX was chosen over the alternatives on the train ones).
    every = [e for e in index if e.get("kind") == "atlas" and "opacity" in e["files"]]
    alpha_items, alpha_hr, res = [], [], []
    for label, group in (("held-out", atlases), ("all", every)):
        for size in (64, 32):
            ious = {"nearest": [], "bilinear>0.5": [], "port": []}
            items, hrs = [], []
            for e in group:
                col = load_rgb(os.path.join(args.data, e["files"]["color"]), 2 * size)
                op = Image.open(os.path.join(args.data, e["files"]["opacity"])).convert("L")
                ahi = np.asarray(op.resize((2 * size, 2 * size), Image.Resampling.LANCZOS), np.float32) / 255 >= 0.5
                alo = np.asarray(op.resize((size, size), Image.Resampling.LANCZOS), np.float32) / 255 >= 0.5
                lr = to8(n64tex.downscale2(col, "box"))
                lr = np.concatenate([lr, (alo * 255).astype(np.uint8)[..., None]], 2)
                lr[~alo, :3] = 0   # black under transparent texels, as many N64 cut-outs have
                items.append((lr, 0, 0))
                hrs.append((col, ahi, lr))
            if not items:
                continue
            out, _ = run_tool(args.tool, "net", items)
            for (col, ahi, lr), o in zip(hrs, out):
                a = lr[..., 3:4].astype(np.float32) / 255
                cand = {"nearest": n64tex.nearest2x(a)[..., 0] >= 0.5,
                        "bilinear>0.5": n64tex.bilinear2x(a)[..., 0] >= 0.5, "port": o[..., 3] >= 128}
                for k, v in cand.items():
                    ious[k].append((v & ahi).sum() / max(1, (v | ahi).sum()))
            if label == "held-out" and size == 64:
                alpha_items, alpha_hr, res = items, hrs, out
            report.append("")
            report.append(f"cut-out alpha, {len(items)} {label} leaf/foliage/graffiti atlases {size}x{size} -> "
                          f"{2 * size}x{2 * size}, IoU with the real cut-out at that size: "
                          + ", ".join(f"{k} {np.mean(v):.4f}" for k, v in ious.items()))
    text = "\n".join(report)
    print(text)
    with open(os.path.join(args.out, "report.md"), "w") as f:
        f.write(text + "\n")

    # ---------------------------------------------------------------- contact sheets
    if args.sheets:
        for size in (64,):
            for fmts, name in ((["ci4", "rgba16"], "colour"), (["i4", "ci8"], "more")):
                rows = []
                picks = [i for i, c in enumerate(cases) if c[1] == size and c[2] in fmts]
                for i in picks[:: max(1, len(picks) // 12)][:12]:
                    c = cases[i]
                    rows.append([to8(c[4]), to8(outs["bilinear"][i]), to8(outs["catmull-rom"][i]),
                                 to8(outs["c-net"][i]), to8(c[3])])
                sheet(rows, ["input (" + "/".join(fmts) + ")", "bilinear", "Catmull-Rom", "network", "truth"],
                      os.path.join(args.out, f"sheet_{name}_{size}.png"), 6)
        if alpha_items:
            rows = []
            for (col, a128, lr), out in list(zip(alpha_hr, res))[:6]:
                a = lr[..., 3:4].astype(np.float32) / 255
                bil = np.concatenate([to8(n64tex.bilinear2x(lr[..., :3].astype(np.float32) / 255)),
                                      ((n64tex.bilinear2x(a)[..., 0] >= 0.5) * 255).astype(np.uint8)[..., None]], 2)
                truth = np.concatenate([to8(col), (a128 * 255).astype(np.uint8)[..., None]], 2)
                rows.append([lr, bil, out, truth])
            sheet(rows, ["input", "bilinear, a>0.5", "port", "truth"], os.path.join(args.out, "sheet_alpha.png"), 4)
    json.dump({f"{k[0]}:{k[1]}:{k[2]}": [float(x) for x in v] for k, v in summary.items()},
              open(os.path.join(args.out, "summary.json"), "w"), indent=0)


if __name__ == "__main__":
    main()
