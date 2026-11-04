#!/usr/bin/env python3
"""Mods made from your own ROM for the retail checks (docs/port/MODS.md 8): run on your machine, written under build/.

    python3 tools/port/mods_retail.py --rom baserom.us.z64 --out build/scratch/retail/mods

Writes OUT/<set>/<mod>/..., each set a mods folder for --mods-dir:

    identity/all     every file the game loads, and the sample data, as rushmods.py extract writes them: the
                     cartridge's files from the mods' region, so a run is the run without it
    magenta/cars     every car file with its palettes' entries and its RGBA16 textures' texels magenta
    big/cars         every car file 3 MB larger (zeros after its end)
    silent/samples   the sample data with every byte past each sample's codebook zero: the sounds play silence

What it writes is the ROM's (changed): keep it under build/, never share or commit it. Stdlib only.
"""
import argparse
import shutil
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mods"))
sys.path.insert(0, str(ROOT / "tools/port"))
import rushmods  # noqa: E402
from mods_synth import mod, put, CARS  # noqa: E402

MAGENTA = 0xF83E   # RGBA16 5:5:5:1, the alpha bit kept from the texel


def magenta(b):
    """The palettes' entries and the RGBA16 texel textures' texels of a WAD, magenta."""
    b = bytearray(b)
    w = rushmods.Wad(bytes(b))
    im, tx, pl = w.chunk("IMAG"), w.chunk("TXHD"), w.chunk("PLHD")
    if not im:
        return bytes(b)
    spans = []
    for i in range(pl[1] if pl else 0):
        r = pl[0] + 0x18 * i
        s, e = b[r + 0x10], b[r + 0x11]
        spans.append(((im[0] + rushmods.be32(b, r + 0x14)) & 0xFFFFFFFF, e - s + 1))
    for i in range(tx[1] if tx else 0):
        r = tx[0] + 0x24 * i
        if rushmods.be32(b, r + 0x1C) & 0x08000000 and b[r + 0x15] == 0 and b[r + 0x14] == 2:   # RGBA16 texels
            spans.append(((im[0] + rushmods.be32(b, r + 0x18)) & 0xFFFFFFFF,
                          rushmods.be16(b, r + 0x10) * rushmods.be16(b, r + 0x12)))
    for at, n in spans:
        for k in range(n):
            p = at + 2 * k
            if p + 2 <= len(b):
                struct.pack_into(">H", b, p, MAGENTA | (rushmods.be16(b, p) & 1))
    return bytes(b)


def silent(samples, sdir):
    """The sample data with each sample's bytes after its 0x100-byte codebook zero (the directory's offsets at +4,
    records of 0x1C until id 0xFFFF; MUSYX_DATA.md)."""
    out = bytearray(samples)
    starts = []
    for r in range(0, len(sdir) - 0x1B, 0x1C):
        if rushmods.be16(sdir, r) == 0xFFFF:
            break
        starts.append(rushmods.be32(sdir, r + 4))
    starts = sorted(set(s for s in starts if s < len(out)))
    for i, s in enumerate(starts):
        end = starts[i + 1] if i + 1 < len(starts) else len(out)
        out[min(s + 0x100, end):end] = bytes(max(0, end - min(s + 0x100, end)))
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--rom", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    cart = rushmods.Cart(rushmods.read_rom(a.rom))
    out = Path(a.out)
    shutil.rmtree(out, ignore_errors=True)
    d = mod(out / "identity", "all", "The cartridge's own files", author="mods_retail.py")
    for w in range(rushmods.WAVES):
        if rushmods.loaded(w):
            put(d, w, cart.file(w))
    d = mod(out / "magenta", "cars", "Magenta cars", author="mods_retail.py")
    for w in CARS:
        put(d, w, magenta(cart.file(w)))
    d = mod(out / "big", "cars", "Big cars", author="mods_retail.py")
    for w in CARS:
        put(d, w, cart.file(w) + bytes(3 << 20))
    d = mod(out / "silent", "samples", "Silent samples", author="mods_retail.py")
    put(d, 9, silent(cart.file(9), cart.file(8)))
    print(f"mods_retail: identity, magenta, big and silent in {out} (your ROM's: keep them to yourself)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
