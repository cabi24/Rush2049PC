#!/usr/bin/env python3
"""Making mods for Rush2049PC (docs/port/MODS.md): your ROM's files, a mod's skeleton, and the game's checks.

    python3 tools/mods/rushmods.py list ROM                       every file the game loads: wave, name, kind, size
    python3 tools/mods/rushmods.py extract ROM OUT [FILE ...]     unpack them into OUT as mods use them
                                   [--stored] [--text]            (FILE: a wave number, a name, or FIRST-LAST)
    python3 tools/mods/rushmods.py check MOD [--rom ROM]          a mod folder (or .zip) against the game's rules
    python3 tools/mods/rushmods.py new NAME [--dir DIR] [--author A] [--version V] [--description D] [--priority P]
    python3 tools/mods/rushmods.py names                          the waves' names, as the game knows them

extract writes OUT/<folder>/<name>.bin, the file as the game reads it (decompressed), which is the form a mod's file
takes; --stored writes the cartridge's stored form instead (.lzs for LZSS, .deflate for raw DEFLATE, .bin for a raw
file), which a mod may also give. --text writes each language's strings as OUT/text/<language>.txt in the form of a
mod's text overrides: copy the lines you change into your mod's text/<language>.txt.

check applies the rules port/assets/check.c applies at start (with --rom, also those against the cartridge's own
file: the names and counts the game looks up), and the folder's own: mod.ini, the files' names, text overrides and
the texture pack's file names. It exits 1 if the game would refuse anything.

Everything runs on your own ROM, on your machine: extracted files are the ROM's, so never share or commit them; a
mod you share should hold only your own work. Stdlib only, Python 3.8+.
"""
import argparse
import os
import re
import struct
import sys
import zipfile
import zlib
from pathlib import Path

WAVES = 183

# The file table (docs/port/ASSETS.md 2.2): gWaveSizes and the first text bank in the boot image (raw), the ROM offsets
# and storage kinds in the game image (raw DEFLATE): port/data's image table, as port/mods/mods.c reads it.
BOOT_ROM, BOOT_BASE, BOOT_SIZE = 0x283D0, 0x800277D0, 28944
GAME_ROM, GAME_BASE, GAME_SIZE = 0xB0CB10, 0x80086A50, 647072
SYM_SIZES, SYM_TEXT_INITIAL, SYM_OFFSETS, SYM_KINDS = 0x8002E580, 0x8002E860, 0x8011B5BC, 0x80123564
KINDS = {0: "raw", 1: "LZSS", 2: "DEFLATE"}
STORED_EXT = {0: ".bin", 1: ".lzs", 2: ".deflate"}

# ---------------------------------------------------------------- the waves' names (port/mods/mods.c)

TEXT = ["text_en", "text_es", "text_it", "text_de", "text_nl", "text_fr"]
LANG = ["en", "es", "it", "de", "nl", "fr"]
FRONTEND = ["attract", "interface", "selcar", "selcirc", "selcom", "selopt", "seltrk"]
RACE = ["raceextra", "hud", "hudbattle", "hudcom", "hudlogo", "hudmulti", "hudstunt", "targetsnopak",
        None, None, None, None, None, None, None, "weapons", "wings", "vehicles", "vehiclesnopak", "wheels",
        "wheelsnopak"]


def track_name(t):
    if t < 6:
        return f"track{t + 1}"
    if t < 14:
        return f"battle{t - 5}"
    if t < 18:
        return f"stunt{t - 13}"
    return "obstacle"


def wave_name(w):
    if w <= 5:
        return TEXT[w]
    if w <= 9:
        return ["project", "pool", "sampledir", "samples"][w - 6]
    if w <= 21:
        return f"song{w - 9:02d}"
    if w <= 37:
        return f"fonttex{w - 22:02d}"
    if w <= 53:
        return f"fontdat{w - 38:02d}"
    if w <= 60:
        return FRONTEND[w - 54]
    if w <= 81:
        return RACE[w - 61]
    if w <= 87:
        return f"targetst{w - 81}"
    if w <= 100:
        return f"car{w - 87:02d}"
    if w <= 119:
        return track_name(w - 101)
    if w <= 157:
        return f"{track_name((w - 120) % 19)}_{'world' if w <= 138 else 'road'}"
    if w <= 176:
        return f"{track_name(w - 158)}_path"
    return f"{track_name(w - 177)}_pathback"


def wave_folder(w):
    return ("text" if w <= 5 else "sound" if w <= 21 else "fonts" if w <= 53 else "frontend" if w <= 60
            else "race" if w <= 87 else "cars" if w <= 100 else "tracks")


def wave_format(w):
    return ("text" if w <= 5 else "samples" if w == 9 else "sound" if w <= 21 else "font WAD" if w <= 37
            else "font" if w <= 53 else "WAD" if w <= 119 else "world" if w <= 138 else "road" if w <= 157
            else "path")


def wave_by_name(name):
    n = name.lower()
    m = re.fullmatch(r"wave_?(\d+)", n)
    if m:
        w = int(m.group(1))
        return w if w < WAVES else -1
    for w in range(WAVES):
        if wave_name(w) == n:
            return w
    return -1


def loaded(w):
    return not 69 <= w <= 75


# ---------------------------------------------------------------- the ROM

def read_rom(path):
    b = bytearray(Path(path).read_bytes())
    head = bytes(b[:4])
    if head == b"\x37\x80\x40\x12":          # .v64: 16-bit words swapped
        b[0::2], b[1::2] = b[1::2], b[0::2]
    elif head == b"\x40\x12\x37\x80":        # .n64: 32-bit words little-endian
        for i in range(0, len(b) - 3, 4):
            b[i:i + 4] = b[i:i + 4][::-1]
    elif head != b"\x80\x37\x12\x40":
        raise SystemExit(f"rushmods: {path} is not an N64 ROM")
    return bytes(b)


def be32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def be16(b, o):
    return struct.unpack_from(">H", b, o)[0]


def lzss(data, cap, used=None):
    """The boot LZSS (port/ultra/pi.c): None for a bad stream or one that makes more than cap bytes. used, a list,
    gets the stream's length."""
    out = bytearray()
    i, n, flags, bits = 0, len(data), 0, 0
    while True:
        if not bits:
            if i >= n:
                break
            flags, bits, i = data[i], 8, i + 1
        literal = flags & 1
        flags >>= 1
        bits -= 1
        if literal:
            if i >= n:
                break
            if len(out) >= cap:
                return None
            out.append(data[i])
            i += 1
            continue
        if i + 2 > n:
            break
        b0, b1 = data[i], data[i + 1]
        i += 2
        dist, length = ((b0 & 0xF0) << 4 | b1) & 0xFFF, (b0 & 0xF) + 2
        if dist == 0 and (b0 & 0xF) == 0:
            break
        if dist == 0 or dist > len(out) or len(out) + length > cap:
            return None
        for _ in range(length):
            out.append(out[-dist])
    if used is not None:
        used.append(i)
    return bytes(out)


def inflate(data, cap):
    d = zlib.decompressobj(-15)
    try:
        out = d.decompress(data, cap + 1)
    except zlib.error:
        return None
    return out if d.eof and len(out) <= cap else None


class Cart:
    """The cartridge's file table and files."""

    def __init__(self, rom):
        self.rom = rom
        game = inflate(rom[GAME_ROM:], GAME_SIZE)
        if game is None or len(game) != GAME_SIZE:
            raise SystemExit("rushmods: the game image doesn't inflate: not the Rush 2049 (USA) layout")
        boot = rom[BOOT_ROM:BOOT_ROM + BOOT_SIZE]
        self.size = [be32(boot, SYM_SIZES - BOOT_BASE + 4 * w) for w in range(WAVES)]
        self.off = [be32(game, SYM_OFFSETS - GAME_BASE + 4 * w) & 0x0FFFFFFF for w in range(WAVES)]
        self.kind = [be32(game, SYM_KINDS - GAME_BASE + 4 * w) for w in range(WAVES)]
        self.text_initial = boot[SYM_TEXT_INITIAL - BOOT_BASE]

    def stored(self, w):
        """The file's bytes as the cartridge stores them (a compressed one to its stream's end)."""
        o, k = self.off[w], self.kind[w]
        if k == 0:
            return self.rom[o:o + self.size[w]]
        end = len(self.rom)   # the stream's own end: decompress it, then cut what the stream used
        if k == 1:
            used = []
            lzss(self.rom[o:], self.size[w], used)
            return self.rom[o:o + used[0]] if used else self.rom[o:o + self.size[w]]
        d = zlib.decompressobj(-15)
        d.decompress(self.rom[o:], self.size[w] + 1)
        return self.rom[o:end - len(d.unused_data)] if d.eof else self.rom[o:o + self.size[w]]

    def file(self, w):
        o, k, n = self.off[w], self.kind[w], self.size[w]
        if k == 0:
            return self.rom[o:o + n]
        return (lzss if k == 1 else inflate)(self.rom[o:], n)


# ---------------------------------------------------------------- the checks (port/assets/check.c)

class Refused(Exception):
    pass


def T4(s):
    return struct.unpack(">I", s.encode())[0]


TAGS = {t: T4(t) for t in ("OBHD", "TXHD", "PLHD", "PTHD", "TXLD", "IMAG", "WHDR", "WOBJ", "GDAT", "GTLD")}
MAX_TOC, MAX_DL_DEPTH, ROAD_LIST_MAX = 4096, 16, 35
DL_STEPS = 1 << 26   # commands the display-list passes visit in one file, all its lists' together (check.c's)
dl_steps = [0]       # check_file's count so far


def dl_step(what, tail=""):
    dl_steps[0] += 1
    if dl_steps[0] > DL_STEPS:
        raise Refused(f"{what}: the display lists take more than {DL_STEPS} steps to walk{tail}")


def cstr(b, o, n=16):
    raw = b[o:o + n]
    return raw.split(b"\0", 1)[0].decode("latin-1")


class Wad:
    def __init__(self, b):
        self.b, self.size = b, len(b)
        if self.size < 8:
            raise Refused(f"it is {self.size} bytes: too short for a WAD's header")
        self.toc, self.n = be32(b, 0), be32(b, 4)
        if self.toc & 0x80000000:
            raise Refused(f"its table of contents is already relocated (0x{self.toc:08X}): not a file as the "
                          "cartridge holds it")
        if self.toc < 8 or self.toc & 3 or self.toc > self.size or self.n > MAX_TOC or \
                self.n > (self.size - self.toc) // 12:
            raise Refused(f"no WAD table of contents (offset 0x{self.toc:X}, {self.n} entries, file "
                          f"0x{self.size:X} bytes)")
        self.first, self.offs = {}, []
        for i in range(self.n):
            tag, off, count = struct.unpack_from(">III", b, self.toc + 12 * i)
            self.offs.append(off)
            if off > self.size:
                raise Refused(f"chunk {tag_text(tag)} starts at 0x{off:X}, past the file's end (0x{self.size:X})")
            for name, t in TAGS.items():
                if t == tag and name not in self.first:
                    self.first[name] = (off, count)

    def chunk(self, name):
        return self.first.get(name)

    def room(self, off):
        end = self.size
        for o in self.offs:
            if off < o < end:
                end = o
        if off < self.toc < end:
            end = self.toc
        return end - off

    def records_fit(self, name, rec):
        c = self.chunk(name)
        if not c or not c[1]:
            return
        off, count = c
        if off & 3:
            raise Refused(f"chunk {name} at 0x{off:X} is not 4-aligned")
        if count * rec > self.room(off):
            raise Refused(f"chunk {name}'s {count} records of 0x{rec:X} bytes run past it (0x{self.room(off):X} "
                          "bytes there)")

    def names_sorted(self, what, first, count, stride):
        prev = None
        for i in range(count):
            raw = self.b[first + i * stride:first + i * stride + 16]
            if b"\0" not in raw:
                raise Refused(f"{what} {i}'s name doesn't end within its 16 bytes")
            name = raw.split(b"\0", 1)[0]
            if prev is not None and prev > name:   # the cartridge has a name twice: bsearch finds one
                raise Refused(f"{what} {i}, '{name.decode('latin-1')}', is out of order after "
                              f"'{prev.decode('latin-1')}' (the game looks them up by name, in order)")
            prev = name


def tag_text(tag):
    return "".join(chr(c) if 0x20 <= c < 0x7F else "?" for c in struct.pack(">I", tag))


def inside(size, off, n):
    return off <= size and n <= size - off


def dl_relocation(b, start, what):
    size, seen, pos, latched, first = len(b), set(), start, 0, True
    while True:
        if not inside(size, pos, 8):
            raise Refused(f"{what} runs past the file's end (at 0x{pos:X}) before its G_ENDDL")
        if pos // 8 in seen:
            raise Refused(f"{what} comes back to 0x{pos:X}: the relocation would never end")
        seen.add(pos // 8)
        dl_step(what)
        w0, w1 = be32(b, pos), be32(b, pos + 4)
        op = w0 >> 24
        if op & 0xC0 in (0x40, 0x80) or 9 <= op < 0x40 or 0xC0 <= op < 0xD6:
            pos += 8
            continue
        if op == 0x01 and first:
            first = False
            if w1 & 0xFF000000:
                raise Refused(f"{what}'s first G_VTX (at 0x{pos:X}) names 0x{w1:08X}, not a place in the file")
            v = w1
            while True:
                if not inside(size, v, 10):
                    raise Refused(f"{what}'s vertices from 0x{w1:X} run past the file's end")
                if not be16(b, v + 6) & 0x8000:
                    break
                v += 16
        elif op == 0xE1:
            latched = w1
        elif op == 0x04:
            if (latched & 0xFFFFFF) + 8 >= size or latched & 0xF0000000:
                raise Refused(f"{what} branches (E1 04) to 0x{latched:08X}, outside the file")
            pos = (latched & 0xFFFFFF) + 8
            continue
        elif op == 0xDF:
            return
        pos += 8


def dl_traverse(b, start, depth, stack, what):
    size = len(b)
    if depth >= MAX_DL_DEPTH:
        raise Refused(f"{what} calls display lists more than {MAX_DL_DEPTH} deep")
    if start in stack[:depth]:
        raise Refused(f"{what} calls the list at 0x{start:X} from inside itself")
    stack[depth:] = [start]
    seen, pos = set(), start
    while True:
        if not inside(size, pos, 8):
            raise Refused(f"{what} (a list at 0x{start:X}) runs past the file's end before its G_ENDDL")
        if pos // 8 in seen:
            raise Refused(f"{what} (a list at 0x{start:X}) comes back to 0x{pos:X} and never ends")
        seen.add(pos // 8)
        dl_step(what, " (a list called many times, nested)")
        w0, w1 = be32(b, pos), be32(b, pos + 4)
        if w0 == 0xDF000000:
            return
        if w0 >> 24 in (0xDE, 0xE1):
            is_e1 = w0 >> 24 == 0xE1
            to = w1 & 0xFFFFFF
            is_04 = is_e1 and inside(size, pos + 8, 8) and b[pos + 8] == 0x04
            if is_e1 and not is_04:
                pos += 16
                continue
            if to >= size or to & 7 or w1 & 0xF0000000:
                raise Refused(f"{what} (a list at 0x{start:X}) goes to 0x{w1:08X} at 0x{pos:X}, outside the file")
            if not is_e1 and not w0 & 0x00FF0000:
                dl_traverse(b, to, depth + 1, stack, what)
                pos += 8
                continue
            pos = to
            continue
        pos += 8


def wad_models(w):
    c = w.chunk("OBHD")
    if not c or not c[1]:
        return
    off, count = c
    w.names_sorted("model", off, count, 0x58)
    for i in range(count):
        r = off + 0x58 * i
        name = cstr(w.b, r)
        lods = struct.unpack_from(">h", w.b, r + 0x16)[0]
        if not 0 <= lods <= 4:
            raise Refused(f"model '{name}' has {lods} levels of detail (0-4)")
        for lv in range(lods):
            dl = be32(w.b, r + 0x18 + 0x10 * lv + 8)
            what = f"model '{name}' LOD {lv}"
            if dl >= w.size or dl & 7:
                raise Refused(f"{what}'s display list 0x{dl:X} is not an 8-aligned place in the file")
            dl_relocation(w.b, dl, what)
            dl_traverse(w.b, dl, 0, [], what)


def wad_textures(w):
    b, size = w.b, w.size
    tx, pl, im, tl = w.chunk("TXHD"), w.chunk("PLHD"), w.chunk("IMAG"), w.chunk("TXLD")
    imag = im[0] if im else 0
    if tl and tl[1]:
        off, n = tl
        if off & 7 or not inside(size, off, n):
            raise Refused(f"the texture-load lists (TXLD, 0x{n:X} bytes at 0x{off:X}) are not inside the file, "
                          "8-aligned")
        for p in range(off, off + n - 7, 8):   # rebased in 24 bits
            if b[p] == 0xFD and not inside(size, (imag + (be32(b, p + 4) & 0xFFFFFF)) & 0xFFFFFF, 1):
                raise Refused(f"a texture-load list's G_SETTIMG at 0x{p:X} names 0x{be32(b, p + 4):X} past IMAG, "
                              "outside the file")
    if tx and tx[1]:
        off, count = tx
        w.names_sorted("texture", off, count, 0x24)
        if not im:
            raise Refused("it has textures (TXHD) but no texels (IMAG)")
        for i in range(count):
            r = off + 0x24 * i
            name = cstr(b, r)
            wd, ht, sz = be16(b, r + 0x10), be16(b, r + 0x12), b[r + 0x14]
            base, flags = be32(b, r + 0x18), be32(b, r + 0x1C)
            if not wd or not ht or wd > 1024 or ht > 1024 or sz > 3:
                raise Refused(f"texture '{name}' is {wd} x {ht} at size {sz} (1-1024 a side, sizes 0-3)")
            texels = flags & 0x08000000
            if not inside(size, (imag + base) & 0xFFFFFFFF, 1 if texels else 8):   # the game's 32-bit sum
                raise Refused(f"texture '{name}''s {'texels' if texels else 'load list'} (0x{base:X} past IMAG) "
                              f"{'are' if texels else 'is'} outside the file")
            if not texels and (imag + base) & 7:   # a list's commands are 8 bytes, 8-aligned (wad.c builds it)
                raise Refused(f"texture '{name}''s load list (0x{base:X} past IMAG) is not 8-aligned in the file")
    if pl and pl[1]:
        off, count = pl
        w.names_sorted("palette", off, count, 0x18)
        if not im:
            raise Refused("it has palettes (PLHD) but no texels (IMAG)")
        for i in range(count):
            r = off + 0x18 * i
            s, e = b[r + 0x10], b[r + 0x11]
            if s > e or not inside(size, (imag + be32(b, r + 0x14)) & 0xFFFFFFFF, 2 * (e - s + 1)):
                raise Refused(f"palette '{cstr(b, r)}' (entries {s}-{e} at 0x{be32(b, r + 0x14):X} past IMAG) is "
                              "outside the file")


def wad_paths(w, pthd):
    c = w.chunk("PTHD")
    if not c or not c[1]:
        return
    off, count = c
    for i in range(count):
        r = off + 0x24 * i
        n = struct.unpack_from(">h", w.b, r + 0x14)[0]
        pts, trig = be32(w.b, r + 0x1C), be32(w.b, r + 0x18)
        if n > 0 and (pts & 3 or not inside(w.size, pts, 0x44 * n)):
            raise Refused(f"path '{cstr(w.b, r)}''s {n} points at 0x{pts:X} are outside the file")
        if pthd and trig and (trig < off or (trig - off) % 0x24 or (trig - off) // 0x24 >= count):
            raise Refused(f"path '{cstr(w.b, r)}' links to 0x{trig:X}, which is no path record")


def names_of(w, name, rec):
    c = w.chunk(name)
    return [w.b[c[0] + rec * i:c[0] + rec * i + 16].split(b"\0", 1)[0] for i in range(c[1])] if c else []


def check_wad(b, cart, pthd):
    w = Wad(b)   # pthd: a track's WAD, whose paths the game reaches by number; wad.c converts any WAD's paths
    for name, rec in (("OBHD", 0x58), ("TXHD", 0x24), ("PLHD", 0x18), ("PTHD", 0x24)):
        w.records_fit(name, rec)
    wad_textures(w)
    wad_models(w)
    wad_paths(w, pthd)
    if cart is None:
        return w
    try:
        cw = Wad(cart)
    except Refused:
        return w
    for name, rec, what in (("OBHD", 0x58, "model"), ("TXHD", 0x24, "texture"), ("PLHD", 0x18, "palette")):
        have = set(names_of(w, name, rec))
        for n in names_of(cw, name, rec):
            if n not in have:
                raise Refused(f"the cartridge's file has {what} '{n.decode('latin-1')}', which this one lacks (the "
                              "game looks it up)")
    k, ck = w.chunk("PTHD"), cw.chunk("PTHD")
    if pthd and ck and ck[1] > (k[1] if k else 0):
        raise Refused(f"it has {k[1] if k else 0} paths (PTHD), the cartridge's file {ck[1]}: the game reaches them "
                      "by number")
    return w


def check_world(b, cart):
    w = check_wad(b, None, False)
    for name, rec in (("WOBJ", 0x68), ("GDAT", 0x1C), ("GTLD", 4)):
        w.records_fit(name, rec)
    wh, wo, gd, gt = w.chunk("WHDR"), w.chunk("WOBJ"), w.chunk("GDAT"), w.chunk("GTLD")
    size = len(b)
    if not wh:
        raise Refused("it has no world table (WHDR)")
    if wh[0] & 3 or not inside(size, wh[0], 4):
        raise Refused(f"its world table (WHDR) at 0x{wh[0]:X} is not 4-aligned in the file")
    worlds = be32(b, wh[0])
    if 4 + 0x14 * worlds > w.room(wh[0]):
        raise Refused(f"its world table's {worlds} entries run past it")
    w.names_sorted("world", wh[0] + 8, worlds, 0x14)
    seen = set()
    for i in range(worlds):
        base = be32(b, wh[0] + 4 + 0x14 * i)
        if base & 3 or not inside(size, base, 0x68):
            raise Refused(f"world '{cstr(b, wh[0] + 8 + 0x14 * i)}' starts at 0x{base:X}, not a node in the file")
        if base in seen:
            continue
        seen.add(base)
        stack = [base]
        while stack:
            at = stack.pop()
            if wo and wo[1] and (at < wo[0] or (at - wo[0]) % 0x68 or (at - wo[0]) // 0x68 >= wo[1]):
                raise Refused(f"a node of world {i} (at 0x{at:X}) is not one of the world nodes (WOBJ)")
            for k in range(2):
                link = struct.unpack_from(">h", b, at + 0x44 + 2 * k)[0]
                if link < 0:
                    continue
                to = base + 0x68 * link
                if not inside(size, to, 0x68):
                    raise Refused(f"a node of world {i} (at 0x{at:X}) links to node {link}, outside the file")
                if to not in seen:
                    seen.add(to)
                    stack.append(to)
    if gd:
        for i in range(gd[1]):
            g = gd[0] + 0x1C * i
            if b[g + 3] & 1:
                for k in range(4):
                    if be16(b, g + 0x14 + 2 * k) >= gd[1]:
                        raise Refused(f"quadtree node {i}'s child {be16(b, g + 0x14 + 2 * k)} is past the {gd[1]} "
                                      "nodes (GDAT)")
            elif be16(b, g + 0x14) + be16(b, g + 0x16) > (gt[1] if gt else 0):
                raise Refused(f"quadtree leaf {i}'s targets run past the target list (GTLD)")
    if cart is not None:
        try:
            cw = Wad(cart)
        except Refused:
            return
        cwh = cw.chunk("WHDR")
        if cwh and inside(len(cart), cwh[0], 4):
            mine = {b[wh[0] + 8 + 0x14 * j:wh[0] + 24 + 0x14 * j].split(b"\0", 1)[0] for j in range(worlds)}
            for i in range(be32(cart, cwh[0])):
                if not inside(len(cart), cwh[0] + 8 + 0x14 * i, 16):
                    break
                n = cart[cwh[0] + 8 + 0x14 * i:cwh[0] + 24 + 0x14 * i].split(b"\0", 1)[0]
                if n not in mine:
                    raise Refused(f"the cartridge's file has world '{n.decode('latin-1')}', which this one lacks "
                                  "(the game looks it up)")


TEXT_OFFSETS = [1, 0, 1, 0, 0, 1]


def text_sections(b):
    pos, n, at = 0, [], []
    for s in range(6):
        if not inside(len(b), pos, 4):
            raise Refused(f"text section {s} runs past the file's end")
        c = be32(b, pos)
        nbytes = 4 * c if TEXT_OFFSETS[s] else 2 * ((c + 1) & ~1)
        if not inside(len(b), pos + 4, nbytes):
            raise Refused(f"text section {s}'s {c} items run past the file's end")
        n.append(c)
        at.append(pos + 4)
        pos += 4 + nbytes
    return n, at, pos


def conversions(raw):
    """A string's printf conversions in order (%d, %s, ...; %% is none), as check.c reads them."""
    out, i = [], 0
    while i < len(raw):
        if raw[i] != 0x25:
            i += 1
            continue
        p = i + 1
        while p < len(raw) and chr(raw[p]) in "-+ #0123456789.*hlLqjzt":
            p += 1
        if p < len(raw) and chr(raw[p]) in "diouxXcsfeEgGpn":
            out.append(chr(raw[p]))
        i = p + 1 if p < len(raw) and raw[p] == 0x25 else i + 1
    return "".join(out)[:31]


def cstring(b, o):
    return b[o:b.index(b"\0", o)]


def check_text(b, cart):
    n, at, end = text_sections(b)
    for s in range(6):
        if not TEXT_OFFSETS[s]:
            continue
        for i in range(n[s]):
            o = be32(b, at[s] + 4 * i)
            if o < end or o >= len(b) or b.find(b"\0", o) < 0:
                raise Refused(f"text section {s}'s string {i} (at 0x{o:X}) is not a string inside the file")
    if cart is not None:
        try:
            cn, cat, _ = text_sections(cart)
        except Refused:
            return
        for s in range(6):
            if n[s] < cn[s]:
                raise Refused(f"text section {s} has {n[s]} items, the cartridge's {cn[s]}: the game reads them by "
                              "number")
            for i in range(cn[s] if TEXT_OFFSETS[s] else 0):
                co = be32(cart, cat[s] + 4 * i)
                if co >= len(cart) or cart.find(b"\0", co) < 0:
                    continue
                a = conversions(cstring(b, be32(b, at[s] + 4 * i)))
                c = conversions(cstring(cart, co))
                if a != c:
                    raise Refused(f"text section {s}'s string {i} has the conversions '{a}' where the cartridge's has "
                                  f"'{c}' (the game formats it with those arguments)")


def check_font(b, cart):
    if len(b) < 16:
        raise Refused(f"it is {len(b)} bytes: a font's header is 16")
    chars = b[12]
    need = 16 + chars * 12 + (chars * chars if b[10] else 0)
    if need > len(b):
        raise Refused(f"its {chars} letters{' and kerning' if b[10] else ''} need 0x{need:X} bytes; the file has "
                      f"0x{len(b):X}")
    for i in range(chars):
        if b[16 + 12 * i + 5] >= b[11]:
            raise Refused(f"letter {i} is on texture {b[16 + 12 * i + 5]}; the font has {b[11]}")
    if cart is not None and len(cart) >= 16 and chars < cart[12]:
        raise Refused(f"it has {chars} letters, the cartridge's font {cart[12]}")
    if cart is not None and len(cart) >= 16 and b[11] > cart[11]:
        raise Refused(f"it has {b[11]} textures, the cartridge's font {cart[11]} (the font WAD's)")


def check_road(b):
    rec = [0x84, 0x14, 0x18, 8, 0x20]
    size = len(b)
    if size < 0x10:
        raise Refused(f"it is {size} bytes: a road's header is 16")
    pos, at = 0x10, []
    for s in range(5):
        at.append(pos)
        if not inside(size, pos, be16(b, 2 * s) * rec[s]):
            raise Refused(f"road section {s}'s {be16(b, 2 * s)} records run past the file's end")
        pos += be16(b, 2 * s) * rec[s]
    tnum, snum, vlnum, slnum = be16(b, 2), be16(b, 4), be16(b, 10), be32(b, 12)
    slist = pos + vlnum
    if not inside(size, pos, vlnum + slnum):
        raise Refused(f"the road's vertex and surface lists (0x{vlnum:X} and 0x{slnum:X} bytes) run past the file's "
                      "end")
    if not tnum:
        raise Refused("the road has no quadtree")
    for i in range(tnum):
        t = at[1] + 0x14 * i
        parent, mask = struct.unpack_from(">h", b, t)[0], b[t + 3]
        if parent < -1 or parent >= tnum:
            raise Refused(f"road node {i}'s parent {parent} is not a node")
        for q in range(4):
            child = be16(b, t + 0xC + 2 * q)
            if mask & (1 << q):
                if child >= tnum:
                    raise Refused(f"road node {i}'s quarter {q} is node {child}, past the {tnum} nodes")
                continue
            if mask & (16 << q):
                child |= 0x10000
            if not child:
                continue
            if child >= slnum:
                raise Refused(f"road node {i}'s quarter {q} lists its surfaces at 0x{child:X}, past the surface list")
            left, p = b[slist + child], child + 1
            if left > ROAD_LIST_MAX:
                raise Refused(f"road node {i}'s quarter {q} lists {left} surfaces; the game's readers take "
                              f"{ROAD_LIST_MAX}")
            while left > 0:
                if p + 2 > slnum:
                    raise Refused(f"road node {i}'s quarter {q}'s surface list runs past its end")
                v = b[slist + p] << 8 | b[slist + p + 1]
                run = (v >> 13) + 1
                p += 2
                if run > left:
                    raise Refused(f"road node {i}'s quarter {q}'s surface list has more surfaces than its count")
                if (v & 0x1FFF) + run > snum:
                    raise Refused(f"road node {i}'s quarter {q} names surface {(v & 0x1FFF) + run - 1}, past the "
                                  f"{snum} surfaces")
                left -= run
    for i in range(snum):
        if be16(b, at[2] + 0x18 * i + 0x16) >= vlnum:
            raise Refused(f"road surface {i}'s vertices start past the vertex list (0x{vlnum:X} bytes)")


def path_walk(b):
    size = len(b)
    if not inside(size, 0, 0x32C + 0x10):
        raise Refused(f"it is {size} bytes: a path's checkpoints and header take 0x33C")
    checkpoints = struct.unpack_from(">h", b, 8)[0]
    if not 0 <= checkpoints <= 10:
        raise Refused(f"it has {checkpoints} checkpoints (0-10)")
    pos = 0x32C
    primary, alts = be16(b, pos), b[pos + 8]
    pos += 0x10
    alt_at = pos
    if not inside(size, pos, 0x10 * alts):
        raise Refused(f"its {alts} alternative paths run past the file's end")
    pos += 0x10 * alts
    if not inside(size, pos, 2 + 6 * primary):
        raise Refused(f"its resurrection path's {primary} points run past the file's end")
    pos += 2 + 6 * primary
    for i in range(alts):
        n = be16(b, alt_at + 0x10 * i + 0xA)
        if not inside(size, pos, 6 * n):
            raise Refused(f"alternative path {i}'s {n} points run past the file's end")
        pos += 6 * n
    drone = []
    for p in range(4):
        if not inside(size, pos, 8):
            raise Refused(f"drone path {p} runs past the file's end")
        n = be16(b, pos)
        if not inside(size, pos + 8, 8 * n):
            raise Refused(f"drone path {p}'s {n} points run past the file's end")
        drone.append(n)
        pos += 8 + 8 * n
    return checkpoints, primary, drone


def check_path(b, cart):
    checkpoints, primary, drone = path_walk(b)
    if cart is None:
        return
    try:
        cc, cp, cd = path_walk(cart)
    except Refused:
        return
    if cp and not primary:
        raise Refused(f"its resurrection path is empty; the cartridge's has {cp} points")
    for p in range(4):
        if cd[p] and not drone[p]:
            raise Refused(f"drone path {p} is empty; the cartridge's has {cd[p]} points")
    if cc and not checkpoints:
        raise Refused(f"it has no checkpoints; the cartridge's has {cc}")


# MusyX (port/assets/musyx.c's walk): every field the sound library reads, inside the file and read at one width.

M_NONE, M_BYTE, M_16, M_32, M_INSIDE = range(5)


class Mx:
    def __init__(self, b):
        self.b, self.size, self.map, self.adsr, self.errors = b, len(b), bytearray(len(b)), set(), 0

    def bad(self):
        self.errors += 1

    def field(self, off, width):
        mark = M_16 if width == 2 else M_32
        if not inside(self.size, off, width):
            self.bad()
            return None
        value = be16(self.b, off) if width == 2 else be32(self.b, off)
        if self.map[off] == mark:
            return value
        if any(self.map[off + i] != M_NONE for i in range(width)):
            self.bad()
            return None
        self.map[off] = mark
        for i in range(1, width):
            self.map[off + i] = M_INSIDE
        return value

    def u16(self, off):
        return self.field(off, 2)

    def u32(self, off):
        return self.field(off, 4)

    def u8(self, off):
        if not inside(self.size, off, 1) or self.map[off] not in (M_NONE, M_BYTE):
            self.bad()
            return None
        self.map[off] = M_BYTE
        return self.b[off]

    def ids(self, off):
        while True:
            v = self.u16(off)
            if v is None or v == 0xFFFF:
                return
            off += 2

    def sfx(self, off):
        count = self.u16(off)
        if count is None or self.u16(off + 2) is None:
            return
        for i in range(count):
            e = off + 4 + 12 * i
            if self.u16(e) is None or self.u16(e + 2) is None:
                return

    def page(self, off):
        while True:
            program = self.u8(off + 5)
            if program is None or program == 0xFF or self.u16(off) is None:
                return
            off += 8

    def setups(self, off):
        while True:
            v = self.u16(off)
            if v is None or v == 0xFFFF:
                return
            if self.u16(off + 2) is None:
                return
            if not inside(self.size, off, 132):
                self.bad()
                return
            off += 132

    def project(self):
        off = 0
        while True:
            size = self.u32(off)
            if size is None or size == 0xFFFFFFFF:
                return
            gid, gtype = self.u16(off + 4), self.u16(off + 6)
            if gid is None or gtype is None:
                return
            count = 8 if gtype == 0 else 6 if gtype == 1 else 5
            if size < 8 + 4 * count or not inside(self.size, off, size):
                self.bad()
                return
            offs = []
            for k in range(count):
                v = self.u32(off + 8 + 4 * k)
                if v is None:
                    return
                offs.append(v)
            for k in range(5):
                self.ids(off + 8 + offs[k])
            if gtype == 1:
                self.sfx(off + 8 + offs[5])
            if gtype == 0:
                self.page(off + 8 + offs[5])
                self.page(off + 8 + offs[6])
                self.setups(off + 8 + offs[7])
            off += size

    def pool_list(self, e, kind):
        while True:
            size = self.u32(e)
            if size is None or size == 0xFFFFFFFF:
                return
            header = 12 if kind == 3 else 8
            if size < header or not inside(self.size, e, size):
                self.bad()
                return
            tid = self.u16(e + 4)
            if tid is None or self.u16(e + 6) is None:
                return
            if kind == 0:
                p = e + 8
                while p + 8 <= e + size:
                    w0 = self.u32(p)
                    if w0 is None or self.u32(p + 4) is None:
                        return
                    if w0 & 0x7F == 0x0C:
                        self.adsr.add((w0 >> 8) & 0xFFFF)
                    p += 8
            elif kind == 1:
                if tid in self.adsr:
                    if size < 16:
                        self.bad()
                        return
                    for k in range(4):
                        if self.u16(e + 8 + 2 * k) is None:
                            return
            elif kind == 2:
                p = e + 8
                while p + 8 <= e + size:
                    if self.u16(p) is None or self.u16(p + 4) is None:
                        return
                    p += 8
            else:
                count = self.u16(e + 10) if self.u16(e + 8) is not None else None
                if count is None:
                    return
                if 12 + 12 * count > size:
                    self.bad()
                    return
                for p in range(e + 12, e + 12 + 12 * count, 12):
                    if self.u16(p) is None or self.u16(p + 6) is None:
                        return
            e += size

    def pool(self):
        offs = []
        for k in range(4):
            v = self.u32(4 * k)
            if v is None:
                return
            offs.append(v)
        for k in range(4):
            self.pool_list(offs[k], k)

    def sdir(self):
        off = 0
        while True:
            sid = self.u16(off)
            if sid is None or sid == 0xFFFF:
                return
            if not inside(self.size, off, 0x1C):
                self.bad()
                return
            if self.u16(off + 2) is None:
                return
            for k in range(4, 0x1C, 4):
                if self.u32(off + k) is None:
                    return
            off += 0x1C

    @staticmethod
    def note_size(key, vel):
        if key & 0x80 and (vel in (0, 1) or vel & 0x80):
            return 4
        return 6 if key or vel else 4

    def pattern(self, table, index):
        off = self.u32(table + 4 * index)
        if off is None:
            return
        if inside(self.size, off, 4) and self.map[off] == M_32:
            return
        for k in range(3):
            if self.u32(off + 4 * k) is None:
                return
        n = off + 12
        while True:
            if self.u16(n) is None:
                return
            key = self.u8(n + 2)
            vel = self.u8(n + 3) if key is not None else None
            if key is None or vel is None:
                return
            if key == 0xFF and vel == 0xFF:
                return
            if self.note_size(key, vel) == 6 and self.u16(n + 4) is None:
                return
            n += self.note_size(key, vel)

    def track(self, off, patterns):
        while True:
            if self.u32(off) is None or self.u16(off + 6) is None:
                return
            code = self.u16(off + 8)
            if code is None or code == 0xFFFF:
                return
            if code == 0xFFFE:
                self.u16(off + 10)
                return
            self.pattern(patterns, code)
            off += 12

    def song(self):
        h = []
        for k in range(6):
            v = self.u32(4 * k)
            if v is None:
                return
            h.append(v)
        for j in range(64):
            track = self.u32(h[0] + 4 * j)
            if track is None:
                return
            if track and not (inside(self.size, track, 4) and self.map[track] == M_32):
                self.track(track, h[1])
        if h[3]:
            off = h[3]
            while True:
                t = self.u32(off)
                if t is None or t == 0xFFFFFFFF or self.u32(off + 4) is None:
                    return
                off += 8


def check_musyx(w, b):
    if len(b) < 4 or not any(b):
        raise Refused("it is all zero, or too short for a sound file")
    m = Mx(b)
    {6: m.project, 7: m.pool, 8: m.sdir}.get(w, m.song)()
    if m.errors:
        raise Refused(f"{m.errors} of its sound records don't parse (port/assets/musyx.c's walk)")


def check_file(w, b, cart=None):
    """None if the game takes b (plain) for wave w, else why not (port/assets/check.c)."""
    dl_steps[0] = 0
    try:
        if not 0 <= w < WAVES:
            raise Refused(f"there is no wave {w} (0-{WAVES - 1})")
        if not b:
            raise Refused("it is empty")
        if w == 9:
            return None
        if 6 <= w <= 21:
            check_musyx(w, b)
        elif w <= 5:
            check_text(b, cart)
        elif w <= 37:
            check_wad(b, cart, False)
        elif w <= 53:
            check_font(b, cart)
        elif w <= 119:
            check_wad(b, cart, w >= 101)
        elif w <= 138:
            check_world(b, cart)
        elif w <= 157:
            check_road(b)
        else:
            check_path(b, cart)
    except (Refused, struct.error, IndexError) as e:
        return str(e) if isinstance(e, Refused) else f"it runs past its end ({e})"
    return None


# ---------------------------------------------------------------- a mod folder (port/mods/mods.c's reading)

def mod_entries(path):
    """(relative name, bytes) of every file of a mod folder or .zip, with '/' between folders."""
    p = Path(path)
    if p.is_file() and p.suffix.lower() == ".zip":
        with zipfile.ZipFile(p) as z:
            names = [n for n in z.namelist() if not n.endswith("/")]
            top = names[0].split("/", 1)[0] + "/" if names and "/" in names[0] else ""
            if not all(n.startswith(top) for n in names):
                top = ""
            return [(n[len(top):], z.read(n)) for n in sorted(names)]
    out = []
    for f in sorted(p.rglob("*")):
        rel = f.relative_to(p).as_posix()
        if f.is_file() and not any(part.startswith(".") for part in rel.split("/")):
            out.append((rel, f.read_bytes()))
    return out


def ini_lines(text):
    """[(line, section, key, value)] by port/config's INI rules (lower-case names, quotes, inline comments)."""
    section, out = "", []
    if text.startswith("﻿"):
        text = text[1:]
    for i, raw in enumerate(text.splitlines(), 1):
        s = raw.strip()
        if not s or s[0] in ";#":
            continue
        if s[0] == "[":
            section = s[1:s.index("]")].strip().lower() if "]" in s else section
            continue
        if "=" not in s:
            out.append((i, section, None, "expected key = value"))
            continue
        key, value = s.split("=", 1)
        key, value = key.strip().lower(), value.strip()
        if value.startswith('"'):
            end = value.find('"', 1)
            value = value[1:end] if end > 0 else value
        else:
            value = re.split(r"[ \t][;#]", value, 1)[0].strip()
        out.append((i, section, key, value))
    return out


PNG_NAME = re.compile(r"(rgba|yuv|ci|ia|i)(4|8|16|32)_\d+x\d+_[0-9a-f]{16}(_[0-9a-f]{8})?\.png", re.I)


def check_mod(path, cart=None, out=print):
    """Each problem the game would report, as lines; returns how many would be refused."""
    refused = 0
    entries = mod_entries(path)
    names = {n.lower(): (n, b) for n, b in entries}
    if "mod.ini" not in names:
        out("REFUSED: no mod.ini (rushmods.py new makes one): the game doesn't use the mod")
        return 1
    meta = {}
    for line, section, key, value in ini_lines(names["mod.ini"][1].decode("utf-8", "replace")):
        if key is None:
            out(f"mod.ini:{line}: {value}; the game ignores it")
        elif section != "mod":
            out(f"mod.ini:{line}: [{section}] is not [mod]; the game ignores it")
        elif key not in ("name", "author", "version", "description", "game", "priority"):
            out(f"mod.ini:{line}: no setting '{key}'; the game ignores it")
        else:
            meta[key] = value
    game = meta.get("game", "")
    if not game:
        out("mod.ini doesn't say which game it is for (game = us); the game takes it as the USA cartridge's")
    elif game.lower() not in ("us", "usa", "nrue"):
        out(f"REFUSED: it is for '{game}' (mod.ini's game); this port runs the USA cartridge's files (game = us)")
        return 1
    if "priority" in meta and not re.fullmatch(r"-?\d+", meta["priority"] or "x"):
        out(f"mod.ini: priority '{meta['priority']}' is not a number from -1000 to 1000; the game takes 0")
    waves = {}
    for rel, data in entries:
        base = rel.split("/")[-1]
        low = rel.lower()
        if low == "mod.ini" or base.startswith("."):
            continue
        if low.startswith("textures/"):
            if low.endswith(".png") and not PNG_NAME.fullmatch(base):
                out(f"{rel}: not a texture's name as the dump names them (rgba16_32x32_<16 hex digits>.png); the "
                    "texture code never asks for it")
            continue
        if low.startswith("text/") and low.endswith(".txt"):
            refused += check_text_overrides(rel, data, cart, out)
            continue
        if base.lower().startswith("readme") or low.endswith((".txt", ".md", ".ini", ".png", ".jpg", ".html")):
            continue
        stem, dot, ext = base.rpartition(".")
        if not dot:
            stem, ext = base, ""
        ext = "." + ext.lower() if dot else ""
        if ext not in ("", ".bin", ".lzs", ".deflate"):
            out(f"{rel}: not a kind of file mods use (.bin, .lzs, .deflate); the game ignores it")
            continue
        w = wave_by_name(stem)
        if w < 0:
            out(f"{rel}: '{stem}' names no game file (wave_NNN, or a name: rushmods.py names); the game ignores it")
            continue
        if not loaded(w):
            out(f"{rel}: no game code loads wave {w}; the game ignores it")
            continue
        if w in waves:
            out(f"{rel}: wave {w} again; the game uses {waves[w]}")
            continue
        waves[w] = rel
        plain = data if ext in ("", ".bin") else (lzss if ext == ".lzs" else inflate)(data, 0xBC00000)
        label = f"wave {w}, {wave_name(w)}" if wave_name(w) else f"wave {w}"
        if plain is None:
            out(f"REFUSED: {rel} ({label}): its {'LZSS' if ext == '.lzs' else 'DEFLATE'} stream doesn't decompress")
            refused += 1
            continue
        why = check_file(w, plain, cart.file(w) if cart is not None and w != 9 else None)
        grow = f", {len(plain) - cart.size[w]:+d} bytes against the cartridge's" if cart is not None else ""
        if why:
            out(f"REFUSED: {rel} ({label}): {why}")
            refused += 1
        else:
            out(f"ok: {rel} ({label}), {len(plain)} bytes{grow}")
    if not waves:
        out("it replaces no file")
    return refused


def text_escape(raw):
    out = []
    for c in raw:
        if c == 0x5C:
            out.append("\\\\")
        elif c == 0x0A:
            out.append("\\n")
        elif c == 0x09:
            out.append("\\t")
        elif c == 0x22 or c < 0x20 or c >= 0x7F:
            out.append(f"\\x{c:02X}")
        else:
            out.append(chr(c))
    return "".join(out)


def text_unescape(s):
    out, i = bytearray(), 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            e = s[i + 1]
            if e == "x" and re.fullmatch(r"[0-9A-Fa-f]{2}", s[i + 2:i + 4]):
                out.append(int(s[i + 2:i + 4], 16))
                i += 4
                continue
            out += {"n": b"\n", "t": b"\t"}.get(e, e.encode("latin-1", "replace"))
            i += 2
            continue
        out += c.encode("latin-1", "replace")
        i += 1
    return bytes(out)


SECTIONS = {"single": 0, "0": 0, "taba": 2, "2": 2, "tab2b": 5, "5": 5}


def check_text_overrides(rel, data, cart, out):
    base = rel.split("/")[-1].lower()
    if len(base) != 6 or base[:2] not in LANG:
        out(f"{rel}: text overrides are text/en.txt, es, it, de, nl or fr; the game ignores it")
        return 0
    w = LANG.index(base[:2])
    counts = None
    if cart is not None:
        try:
            counts = text_sections(cart.file(w))[0]
        except Refused:
            counts = None
    n = 0
    for line, section, key, value in ini_lines(data.decode("utf-8", "replace")):
        if key is None or section not in SECTIONS or not re.fullmatch(r"\d+", key):
            out(f"{rel}:{line}: not [single], [taba] or [tab2b] then a string's number = its text; the game ignores it")
            continue
        if 0 in text_unescape(value):
            out(f"{rel}:{line}: a string can't hold \\x00; the game ignores it")
            continue
        if counts is not None and int(key) >= counts[SECTIONS[section]]:
            out(f"{rel}:{line}: the text bank has no string {key} in [{section}]; the game ignores it")
            continue
        n += 1
    out(f"ok: {rel}: {n} strings")
    return 0


# ---------------------------------------------------------------- the commands

def parse_files(specs):
    if not specs:
        return [w for w in range(WAVES) if loaded(w)]
    out = []
    for s in specs:
        m = re.fullmatch(r"(\d+)-(\d+)", s)
        if m:
            out += [w for w in range(int(m.group(1)), int(m.group(2)) + 1) if w < WAVES and loaded(w)]
        elif s.isdigit():
            out.append(int(s))
        elif wave_by_name(s) >= 0:
            out.append(wave_by_name(s))
        elif "/" in s and wave_by_name(s.rsplit("/", 1)[1]) >= 0 and \
                wave_folder(wave_by_name(s.rsplit("/", 1)[1])) == s.rsplit("/", 1)[0].lower():
            out.append(wave_by_name(s.rsplit("/", 1)[1]))   # as names lists it: cars/car01
        else:
            raise SystemExit(f"rushmods: '{s}' is no wave number or name (rushmods.py names)")
    return out


def cmd_names(a):
    for w in range(WAVES):
        n = wave_name(w)
        print(f"{w:3d}  {wave_folder(w) + '/' + n if n else '(no game code loads it)':28} {wave_format(w)}")


def cmd_list(a):
    cart = Cart(read_rom(a.rom))
    print(f"{'wave':>4}  {'name':28} {'format':9} {'kind':8} {'size':>9}  stored")
    for w in range(WAVES):
        n = wave_name(w)
        stored = len(cart.stored(w)) if cart.kind[w] else cart.size[w]
        print(f"{w:4d}  {(wave_folder(w) + '/' + n) if n else '-':28} {wave_format(w):9} {KINDS.get(cart.kind[w], '?'):8} "
              f"{cart.size[w]:9d}  {stored}")


def cmd_extract(a):
    cart = Cart(read_rom(a.rom))
    out = Path(a.out)
    n = 0
    for w in parse_files(a.files):
        name = wave_name(w) or f"wave_{w:03d}"
        if a.stored:
            data, ext = cart.stored(w), STORED_EXT.get(cart.kind[w], ".bin")
        else:
            data, ext = cart.file(w), ".bin"
        if data is None:
            print(f"rushmods: wave {w} ({name}) doesn't read from the ROM; skipped", file=sys.stderr)
            continue
        f = out / wave_folder(w) / (name + ext)
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_bytes(data)
        n += 1
    if a.text:
        for w in range(6):
            bank = cart.file(w)
            nn, at, _ = text_sections(bank)
            lines = [f"; {LANG[w]}: every string of the text bank (wave {w}). Copy the lines you change into your mod's",
                     f"; text/{LANG[w]}.txt; \\n is a new line, \\xHH a byte."]
            for s, sec in ((0, "single"), (2, "taba"), (5, "tab2b")):
                lines.append(f"\n[{sec}]")
                for i in range(nn[s]):
                    o = be32(bank, at[s] + 4 * i)
                    raw = bank[o:bank.index(b"\0", o)] if 0 <= o < len(bank) and b"\0" in bank[o:] else b""
                    lines.append(f'{i} = "{text_escape(raw)}"')
            f = out / "text" / f"{LANG[w]}.txt"
            f.parent.mkdir(parents=True, exist_ok=True)
            f.write_text("\n".join(lines) + "\n", encoding="utf-8")
            n += 1
    print(f"rushmods: {n} files in {out} (your ROM's: keep them to yourself)")


def cmd_check(a):
    cart = Cart(read_rom(a.rom)) if a.rom else None
    if cart is None:
        print("rushmods: without --rom, the checks against the cartridge's own files (names, counts) are skipped")
    refused = check_mod(a.mod, cart)
    print(f"rushmods: {a.mod}: {'the game takes it' if not refused else f'{refused} refused'}")
    return 1 if refused else 0


def cmd_new(a):
    if not re.fullmatch(r"[^=;#\[\],\"\s.][^=;#\[\],\"]{0,62}", a.name) or a.name != a.name.strip():
        raise SystemExit("rushmods: a mod's folder name: up to 63 characters, without = ; # [ ] , or quotes")
    d = Path(a.dir) / a.name
    if d.exists():
        raise SystemExit(f"rushmods: {d} exists")
    d.mkdir(parents=True)
    (d / "mod.ini").write_text(
        "; A mod for Rush2049PC (docs/port/MODS.md).\n[mod]\n"
        f"name = {a.title or a.name}\nauthor = {a.author}\nversion = {a.version}\n"
        f"description = {a.description}\n"
        "; the game version it is for: us, San Francisco Rush 2049 (USA), the cartridge this port runs\n"
        "game = us\n"
        "; of two mods with a file for the same wave, the higher priority's is used (the settings overlay's Mods page,\n"
        "; or game.mods_order in rush2049pc.ini, can order them otherwise)\n"
        f"priority = {a.priority}\n", encoding="utf-8")
    for sub in ("textures", "text"):
        (d / sub).mkdir()
    (d / "README.txt").write_text(
        "Files go anywhere in this folder, named by their wave or name: cars/car01.bin, wave_088.bin,\n"
        "tracks/track1_road.bin (python3 tools/mods/rushmods.py names). textures/ is a texture pack (PNG files\n"
        "named as video.texture_dump names them); text/en.txt overrides strings ([single] 12 = text).\n"
        "Check it with: python3 tools/mods/rushmods.py check THIS_FOLDER --rom YOUR_ROM\n", encoding="utf-8")
    print(f"rushmods: made {d}")


def cmd_selftest(a):
    """ctest mods_tool: the names and the rules against the game's own (port/assets/tests/test_mods.c's program), on
    its good files of each kind and 600 changed copies of each; then a mod folder and a skeleton."""
    import random
    import shutil
    import subprocess
    work = Path(a.work)
    shutil.rmtree(work, ignore_errors=True)
    (work / "good").mkdir(parents=True)
    fails = 0
    c_names = subprocess.run([a.program, "names"], stdout=subprocess.PIPE, check=True).stdout.decode().splitlines()
    py_names = [f"{w} {wave_folder(w)}/{wave_name(w)}" for w in range(WAVES) if wave_name(w)]
    if [l for l in c_names if l] != py_names:
        print("rushmods selftest: the names differ from port/mods/mods.c's")
        fails += 1
    for w in range(WAVES):   # extract's arguments: a wave by its number, its name and as names lists it
        if wave_name(w) and loaded(w) and parse_files([str(w), wave_name(w), f"{wave_folder(w)}/{wave_name(w)}"]) != [w] * 3:
            print(f"rushmods selftest: wave {w} isn't found by its number, name and {wave_folder(w)}/{wave_name(w)}")
            fails += 1
    try:
        parse_files(["tracks/car01"])
        print("rushmods selftest: tracks/car01 (another wave's folder) was taken")
        fails += 1
    except SystemExit:
        pass
    subprocess.run([a.program, "samples", str(work / "good")], check=True)
    rnd = random.Random(2049)
    files = []
    for good in sorted((work / "good").iterdir(), key=lambda p: int(p.name)):
        w, b = int(good.name), good.read_bytes()
        variants = [b, b + bytes(65536), b + bytes(rnd.randrange(256) for _ in range(4096)), b""]
        variants += [b[:n] for n in sorted({rnd.randrange(len(b)) for _ in range(40)})]
        variants += [bytes(rnd.randrange(256) for _ in range(len(b))) for _ in range(60)]
        for _ in range(500):
            m = bytearray(b)
            for _ in range(rnd.randrange(1, 5)):
                m[rnd.randrange(len(m))] = rnd.randrange(256)
            variants.append(bytes(m))
        for i, v in enumerate(variants):
            f = work / "corpus" / f"{w}_{i}"
            f.parent.mkdir(exist_ok=True)
            f.write_bytes(v)
            files.append((f, w, v, b))
    ok = refused = 0
    for start in range(0, len(files), 400):
        batch = files[start:start + 400]
        out = subprocess.run([a.program, "verdicts", "-c", str(work / "good")] + [str(f) for f, _, _, _ in batch],
                             stdout=subprocess.PIPE, check=True).stdout.decode().splitlines()
        for (f, w, v, b), line in zip(batch, out):
            mine = "refused" if check_file(w, v, b) else "ok"
            theirs = line.split(": ", 1)[1]
            ok += mine == "ok"
            refused += mine == "refused"
            if mine != theirs:
                fails += 1
                if fails < 20:
                    print(f"rushmods selftest: {f.name}: the game says {theirs}, this tool {mine}: "
                          f"{check_file(w, v, b)}")
    print(f"rushmods selftest: {len(files)} files, the same verdict as the game's: {ok} taken, {refused} refused")
    mod = work / "mods" / "test"
    a2 = argparse.Namespace(name="test", dir=str(work / "mods"), title="Test", author="me", version="0.1",
                            description="a test", priority=3)
    cmd_new(a2)
    good = sorted((work / "good").iterdir(), key=lambda p: int(p.name))
    for g in good:
        w = int(g.name)
        dst = mod / wave_folder(w) / f"{wave_name(w)}.bin"
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(g.read_bytes())
    (mod / "tracks" / "wave_158.bin").write_bytes(good[-1].read_bytes()[:100])   # a second path file, cut short
    (mod / "text" / "en.txt").write_text("[single]\n0 = \"Hello\"\n[nope]\n1 = x\n", encoding="utf-8")
    lines = []
    n = check_mod(mod, None, lines.append)
    if n != 0 or not any("wave 158 again" in l for l in lines) or not any("not [single]" in l for l in lines):
        print("rushmods selftest: the mod folder's report is wrong:\n  " + "\n  ".join(lines))
        fails += 1
    (mod / "tracks" / f"{wave_name(158)}.bin").write_bytes(good[-1].read_bytes()[:100])
    lines = []
    if check_mod(mod, None, lines.append) != 1 or not any(l.startswith("REFUSED: tracks/track1_path.bin") for l in lines):
        print("rushmods selftest: a cut path file isn't refused:\n  " + "\n  ".join(lines))
        fails += 1
    print(f"rushmods selftest: {'ok' if not fails else f'{fails} failed'}")
    return 1 if fails else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("list", help="the ROM's files")
    p.add_argument("rom")
    p = sub.add_parser("extract", help="unpack the ROM's files as mods use them")
    p.add_argument("rom")
    p.add_argument("out")
    p.add_argument("files", nargs="*")
    p.add_argument("--stored", action="store_true", help="the cartridge's stored form (.lzs, .deflate)")
    p.add_argument("--text", action="store_true", help="also the strings, as text overrides")
    p = sub.add_parser("check", help="a mod against the game's rules")
    p.add_argument("mod")
    p.add_argument("--rom")
    p = sub.add_parser("new", help="a mod's skeleton")
    p.add_argument("name")
    p.add_argument("--dir", default="mods")
    p.add_argument("--title", default="")
    p.add_argument("--author", default="")
    p.add_argument("--version", default="1.0")
    p.add_argument("--description", default="")
    p.add_argument("--priority", type=int, default=0)
    sub.add_parser("names", help="the waves' names")
    p = sub.add_parser("selftest", help="(ctest) the rules against the game's")
    p.add_argument("--program", required=True)
    p.add_argument("--work", required=True)
    a = ap.parse_args(argv)
    return {"list": cmd_list, "extract": cmd_extract, "check": cmd_check, "new": cmd_new,
            "names": cmd_names, "selftest": cmd_selftest}[a.cmd](a) or 0


if __name__ == "__main__":
    sys.exit(main())
