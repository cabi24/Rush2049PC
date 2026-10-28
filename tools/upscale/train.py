#!/usr/bin/env python3
"""Train the 2x texture network on make_pairs.py's pairs, on the CPU (docs/port/UPSCALE.md).

    train.py --pairs pairs.npz --out net.pt [--channels 16 --blocks 4 --iters 40000 --batch 64 --threads 6]

L1 loss on RGB in [0, 1], Adam, cosine learning-rate decay; every 2000 steps the validation PSNR (model and
Catmull-Rom) is printed and the best model is kept. Seeded: the same pairs, options and thread count give the
same weights.
"""
import argparse
import math
import os
import sys
import time

import numpy as np
import torch
import torch.nn.functional as F

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model import Net, cubic2x  # noqa: E402


def to_t(a):
    return torch.from_numpy(a).permute(0, 3, 1, 2).float().div_(255)


def psnr(a, b):
    a = (a.clamp(0, 1) * 255).round()
    b = (b * 255).round()
    mse = ((a - b) ** 2).mean(dim=(1, 2, 3)).clamp_min(1e-3)
    return 10 * torch.log10(255 ** 2 / mse)   # per image


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--pairs", required=True)
    ap.add_argument("--val", help="validation pairs (default: val_<pairs> next to it)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--channels", type=int, default=16)
    ap.add_argument("--blocks", type=int, default=4)
    ap.add_argument("--iters", type=int, default=40000)
    ap.add_argument("--batch", type=int, default=64)
    ap.add_argument("--lr", type=float, default=2e-3)
    ap.add_argument("--threads", type=int, default=6)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()
    torch.set_num_threads(args.threads)
    torch.manual_seed(args.seed)
    rng = np.random.default_rng(args.seed)
    d = np.load(args.pairs)
    hr, lr = d["hr"], d["lr"]
    vpath = args.val or os.path.join(os.path.dirname(args.pairs), "val_" + os.path.basename(args.pairs))
    v = np.load(vpath)
    vhr, vlr = to_t(v["hr"]), to_t(v["lr"])
    net = Net(args.channels, args.blocks)
    print(f"{net.params()} parameters, {len(hr)} pairs, {len(vhr)} validation", file=sys.stderr)
    opt = torch.optim.Adam(net.parameters(), lr=args.lr, betas=(0.9, 0.99))
    sched = torch.optim.lr_scheduler.LambdaLR(
        opt, lambda i: 0.5 * (1 + math.cos(math.pi * min(i, args.iters) / args.iters)) * 0.995 + 0.005)
    with torch.no_grad():
        base = psnr(cubic2x(vlr), vhr).mean().item()
    best, t0 = -1.0, time.time()
    for it in range(1, args.iters + 1):
        idx = rng.integers(0, len(hr), args.batch)
        x, y = to_t(lr[idx]), to_t(hr[idx])
        loss = F.l1_loss(net(x), y)
        opt.zero_grad(set_to_none=True)
        loss.backward()
        opt.step()
        sched.step()
        if it % 2000 == 0 or it == args.iters:
            net.eval()
            with torch.no_grad():
                p = torch.cat([psnr(net(vlr[i:i + 512]), vhr[i:i + 512])
                               for i in range(0, len(vlr), 512)]).mean().item()
            net.train()
            tag = ""
            if p > best:
                best = p
                torch.save({"channels": args.channels, "blocks": args.blocks, "state": net.state_dict(),
                            "iters": it, "val_psnr": p, "val_cubic_psnr": base}, args.out)
                tag = " (saved)"
            print(f"{it:6d} loss {loss.item():.4f} val {p:.3f} dB (cubic {base:.3f}) lr {sched.get_last_lr()[0]:.2e}"
                  f" {time.time() - t0:.0f} s{tag}", file=sys.stderr, flush=True)


if __name__ == "__main__":
    main()
