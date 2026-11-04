#!/usr/bin/env python3
"""Synthetic mods for the synthetic cartridge's tests (docs/port/MODS.md 8): made from its files only.

    python3 tools/port/mods_synth.py --rom build/synth/rush2049_synth.z64 --out DIR

Writes DIR/<set>/<mod>/..., each set a mods folder a test points --mods-dir at (tools/port/synth/mods_*.press):

    colour/ground    track1's ground (the TRACK model of wave 101) magenta: the drive screenshot shows it
    conflict/magenta track1's ground magenta, priority 1
    conflict/green   track1's ground pure green, priority 0: the magenta one wins, unless game.mods_order says
    big/cars         every car file 3 MB larger (zeros after its end): game.memory goes up; the picture is drive's
    bad/broken       a file of every kind the game refuses (cut short, garbage, the wrong kind): all refused, and
                     the race is drive's
    text/long        every English [single] string 24 letters long (text overrides): the front end's text grows
    texture/pack     the texture pack (textures/) of tools/port/synth/mods_texture's dump: every texture of the
                     drive screenshot red, by name (made by --pack-from DUMP)
    fuzz/<variant>/  for the fuzz runs: every kind's file cut at a quarter, a half, three quarters, one byte short,
                     grown by 64 KiB of zeros or of garbage, or garbage of its size

Stdlib only; tools/mods/rushmods.py reads the cartridge.
"""
import argparse
import random
import shutil
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/mods"))
import rushmods  # noqa: E402

TRACK1, TRACK1_WORLD, TRACK1_ROAD, TRACK1_PATH = 101, 120, 139, 158
CARS = range(88, 101)
# a file of every kind, for the broken mod and the fuzz runs: text, sound project, a font WAD, font data, the cars
# (all 13: a race's drones take types at random), track 1's geometry, world, road and path
KINDS = [0, 6, 22, 38] + list(CARS) + [TRACK1, TRACK1_WORLD, TRACK1_ROAD, TRACK1_PATH]


def mod(root, folder, name, priority=0, author="mods_synth.py"):
    d = Path(root) / folder
    d.mkdir(parents=True, exist_ok=True)
    (d / "mod.ini").write_text(f"[mod]\nname = {name}\nauthor = {author}\nversion = 1.0\n"
                               f"description = a synthetic test mod\ngame = us\npriority = {priority}\n")
    return d


def put(d, wave, data, ext=".bin"):
    f = d / rushmods.wave_folder(wave) / (rushmods.wave_name(wave) + ext)
    f.parent.mkdir(parents=True, exist_ok=True)
    f.write_bytes(data)


def models(b):
    """(name, [display list offsets]) of each OBHD record."""
    w = rushmods.Wad(b)
    c = w.chunk("OBHD")
    out = []
    for i in range(c[1] if c else 0):
        r = c[0] + 0x58 * i
        lods = struct.unpack_from(">h", b, r + 0x16)[0]
        out.append((rushmods.cstr(b, r), [rushmods.be32(b, r + 0x18 + 0x10 * k + 8) for k in range(lods)]))
    return out


def recolour(b, rgb, only=None):
    """The vertices every G_VTX of the models' lists loads (and the lists they call) get the colour rgb."""
    b = bytearray(b)
    done, todo = set(), []
    for name, lists in models(bytes(b)):
        if only is None or name == only:
            todo += lists
    while todo:
        pos = todo.pop()
        while pos not in done and pos + 8 <= len(b):
            done.add(pos)
            w0, w1 = struct.unpack_from(">II", b, pos)
            op = w0 >> 24
            if op == 0xDF:
                break
            if op == 0x01:
                n = (w0 >> 12) & 0xFF
                for v in range(n):
                    at = (w1 & 0xFFFFFF) + 16 * v + 12
                    if at + 3 <= len(b):
                        b[at:at + 3] = bytes(rgb)
            elif op == 0xDE:
                todo.append(w1 & 0xFFFFFF)
                if w0 & 0x00FF0000:
                    break
            pos += 8
    return bytes(b)


def write_sets(cart, out):
    out = Path(out)
    shutil.rmtree(out, ignore_errors=True)
    track = cart.file(TRACK1)
    put(mod(out / "colour", "ground", "Magenta ground", 1), TRACK1, recolour(track, (255, 0, 255), "TRACK"))
    put(mod(out / "conflict", "magenta", "Magenta ground", 1), TRACK1, recolour(track, (255, 0, 255), "TRACK"))
    put(mod(out / "conflict", "green", "Green ground", 0), TRACK1, recolour(track, (0, 255, 0), "TRACK"))
    d = mod(out / "big", "cars", "Big cars")
    for w in CARS:
        put(d, w, cart.file(w) + bytes(3 << 20))
    d = mod(out / "bad", "broken", "Broken files")
    rnd = random.Random(2049)
    for w in KINDS:
        b = cart.file(w)
        if w == 89:
            put(d, w, cart.file(TRACK1_ROAD))                      # the wrong kind: a road for a car
        elif w == 90:
            put(d, w, zlib.compress(cart.file(90))[:40], ".lzs")   # a stored file that doesn't decompress
        else:
            put(d, w, b[:len(b) // 2] if w % 2 else bytes(rnd.randrange(256) for _ in range(len(b))))
    d = mod(out / "text", "long", "Long text")
    bank = cart.file(0)
    n = rushmods.text_sections(bank)[0][0]
    (d / "text").mkdir(exist_ok=True)
    (d / "text" / "en.txt").write_text("[single]\n" + "".join(f"{i} = WWWWWWWWWWWWWWWWWWWWWWWW\n" for i in range(n)))
    for name, cut in (("quarter", 0.25), ("half", 0.5), ("three", 0.75)):
        d = mod(out / "fuzz" / name, name, f"Cut at {cut}")
        for w in KINDS:
            b = cart.file(w)
            put(d, w, b[:int(len(b) * cut)])
    d = mod(out / "fuzz" / "short", "short", "One byte short")
    for w in KINDS:
        put(d, w, cart.file(w)[:-1])
    d = mod(out / "fuzz" / "zeros", "zeros", "64 KiB of zeros more")
    for w in KINDS:
        put(d, w, cart.file(w) + bytes(65536))
    d = mod(out / "fuzz" / "grown", "grown", "64 KiB of garbage more")
    for w in KINDS:
        put(d, w, cart.file(w) + bytes(rnd.randrange(256) for _ in range(65536)))
    d = mod(out / "fuzz" / "garbage", "garbage", "Garbage")
    for w in KINDS:
        put(d, w, bytes(rnd.randrange(256) for _ in range(len(cart.file(w)))))


def write_pack(dump, out, rgba=(255, 0, 0, 0)):
    """Every texture of a dump replaced by one of its size of one colour (PNG, RGBA): red and clear, so the text
    the synthetic HUD draws with the font's texture (black, its texels' alpha the coverage) vanishes."""
    d = mod(Path(out) / "texture", "pack", "Clear textures")
    (d / "textures").mkdir(exist_ok=True)
    n = 0
    for f in sorted(Path(dump).glob("*.png")):
        w, h = struct.unpack(">II", f.read_bytes()[16:24])
        raw = b"".join(b"\0" + bytes(rgba) * w for _ in range(h))
        png = b"\x89PNG\r\n\x1a\n"
        for tag, body in ((b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)), (b"IDAT", zlib.compress(raw)),
                          (b"IEND", b"")):
            png += struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body))
        (d / "textures" / f.name).write_bytes(png)
        n += 1
    print(f"mods_synth: {n} textures in {d / 'textures'}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--rom")
    ap.add_argument("--out", required=True)
    ap.add_argument("--pack-from", help="a texture dump: make texture/pack from it (and nothing else)")
    a = ap.parse_args()
    if a.pack_from:
        write_pack(a.pack_from, a.out)
        return 0
    if not a.rom:
        ap.error("--rom is needed (or --pack-from)")
    write_sets(rushmods.Cart(rushmods.read_rom(a.rom)), a.out)
    print(f"mods_synth: the synthetic mods in {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
