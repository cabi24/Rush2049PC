#!/usr/bin/env python3
"""Training and validation pairs from the CC0 images that fetch_data.py downloaded (docs/port/UPSCALE.md).

    make_pairs.py --data DIR --out pairs.npz [--per-image 100] [--seed 1]

Train assets (split "train" in sources.json) give the pairs; one train asset in twenty is kept for validation
(val_*.npz). Each pair: a random square crop of a texture reduced to an N64-like size (64-256 px across the whole
crop), 64x64 cut out of it as HR, flipped or rotated, colour-jittered, sometimes with synthetic flat-colour art on
top or instead, then n64tex.degrade() for the 32x32 LR. Every image gets its own seed, so the output depends only
on the image list and --seed.
"""
import argparse
import hashlib
import json
import multiprocessing as mp
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import n64tex  # noqa: E402

HR = 64


def is_val(asset):
    return int(hashlib.sha1(asset.encode()).hexdigest()[:8], 16) % 20 == 1


def pairs_for(job):
    path, seed, n = job
    rng = np.random.default_rng(seed)
    img = Image.open(path).convert("RGB")
    if max(img.size) > 512:
        img = img.resize((512, 512 * img.size[1] // img.size[0]), Image.Resampling.LANCZOS)
    src = np.asarray(img, np.float32) / 255
    hs, ls, fs = [], [], []
    for _ in range(n):
        if rng.random() < 0.12:
            hr = n64tex.synthetic(HR, rng)
        else:
            side = min(src.shape[:2])
            c = int(side * rng.uniform(0.2, 1.0))
            y0, x0 = rng.integers(0, src.shape[0] - c + 1), rng.integers(0, src.shape[1] - c + 1)
            s = int(rng.integers(HR, 257))
            crop = Image.fromarray((src[y0:y0 + c, x0:x0 + c] * 255 + 0.5).astype(np.uint8))
            whole = np.asarray(crop.resize((s, s), Image.Resampling.LANCZOS), np.float32) / 255
            y1, x1 = rng.integers(0, s - HR + 1, 2)
            hr = whole[y1:y1 + HR, x1:x1 + HR]
            if rng.random() < 0.7:
                hr = n64tex.colour_jitter(hr, rng)
            if rng.random() < 0.1:
                hr = n64tex.synthetic(HR, rng, background=hr)
        hr = np.rot90(hr, int(rng.integers(4)))
        if rng.random() < 0.5:
            hr = hr[:, ::-1]
        hr = np.ascontiguousarray(hr)
        t, lr, desc = n64tex.degrade(hr, rng)
        hs.append(np.clip(np.floor(t * 255 + 0.5), 0, 255).astype(np.uint8))
        ls.append(np.clip(np.floor(lr * 255 + 0.5), 0, 255).astype(np.uint8))
        fs.append(n64tex.FORMATS.index(desc.split()[0]))
    return np.stack(hs), np.stack(ls), np.array(fs, np.uint8)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--data", required=True, help="fetch_data.py's --out directory")
    ap.add_argument("--out", required=True, help="output .npz (the validation set goes next to it as val_<name>)")
    ap.add_argument("--per-image", type=int, default=100)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    args = ap.parse_args()
    index = json.load(open(os.path.join(args.data, "index.json")))
    train = [e for e in index if e["split"] == "train"]
    jobs = {"train": [], "val": []}
    for e in train:
        part = "val" if is_val(e["asset"]) else "train"
        seed = int(hashlib.sha1(f"{args.seed}:{e['source']}:{e['asset']}".encode()).hexdigest()[:8], 16)
        n = args.per_image if part == "train" else max(1, args.per_image // 4)
        jobs[part].append((os.path.join(args.data, e["files"]["color"]), seed, n))
    for part, out in (("train", args.out), ("val", os.path.join(os.path.dirname(args.out) or ".",
                                                               "val_" + os.path.basename(args.out)))):
        with mp.Pool(args.jobs) as pool:
            res = pool.map(pairs_for, jobs[part], chunksize=4)
        hr = np.concatenate([r[0] for r in res])
        lr = np.concatenate([r[1] for r in res])
        fmt = np.concatenate([r[2] for r in res])
        np.savez(out, hr=hr, lr=lr, fmt=fmt)
        print(f"{part}: {len(hr)} pairs from {len(jobs[part])} images -> {out}", file=sys.stderr)


if __name__ == "__main__":
    main()
