#!/usr/bin/env python3
"""A synthetic test cartridge for the PC port (docs/port/SYNTH.md).

    python3 tools/port/synth_rom.py [--out build/synth/rush2049_synth.z64]
                                    [--data-symbols build/pc/pc-data/data_symbols.txt]
    build/pc/rush2049pc --rom build/synth/rush2049_synth.z64 --no-rom-check ...

Writes a 12 MiB cartridge image of original, minimal data, where the game and the PC port read it: the boot
segment's data at ROM 0x283D0, the game image and the two runtime images as raw DEFLATE streams in their slots,
and the file table's files from ROM 0x100000. Every value is made up here, from the decompilation's own knowledge
of the layout (the image table, the symbol files, layout/*.json, the build's data report) and of the formats
(docs/port/ASSETS.md, LOADERS.md, MUSYX_DATA.md). No ROM is read, and none of the output is ROM data: the images
are zero except the values make() sets, each with the reason the game needs it, and the files are as small as
their formats allow. The output is a build artifact (build/ is ignored by version control); never commit it.

Values are written big-endian at their N64 addresses, as the ROM holds data, and the port's data loader swaps them
by the element kinds the build's DWARF gives (port/data). The data report (build/pc/pc-data/data_symbols.txt, so
build the port first) gives every symbol's address and kinds, and each value is checked against the kinds of the
symbol it lands in: one written at the wrong width fails here instead of reading back swapped. Strings and the few
tables that pointers in the data reach (make()) go in game-image data bytes no symbol covers, which the loader
copies as they are: bytes, or values whose byte order doesn't matter.

Next to OUT, OUT with .txt lists every value and file, and why.
"""
import argparse
import math
import struct
import sys
import zlib
from fractions import Fraction
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import n64syms  # noqa: E402

ROOT = n64syms.ROOT
ROM_SIZE = 0xC00000
FILES_ROM = 0x100000        # the file table's files go from here (below the game image's slot at 0xB0CB10)
N_WAVES = 183               # docs/port/ASSETS.md 2.2
SQUARE = 2000               # the tracks: a square loop this many feet a side, at y = 0 (paths, road, ground)

KIND_WIDTH = {"u8": 1, "u16": 2, "u32": 4, "f32": 4, "u64": 8, "f64": 8, "ptr": 4}


def be16(v):
    return struct.pack(">H", v & 0xFFFF)


def be32(v):
    return struct.pack(">I", v & 0xFFFFFFFF)


def bef32(v):
    return struct.pack(">f", v)


# ---------------------------------------------------------------- symbols


class Symbols:
    """Data symbols by name (address, storage, size, element kinds) from the build's data report, data_symbols.txt;
    functions by name from layout/*.json, the image symbol tables and the symbol file."""

    def __init__(self, data_symbols):
        self.data = {}      # name -> (address, storage, n64 size, kinds)
        self.by_addr = []   # (address, n64 size, kinds, name)
        if data_symbols and Path(data_symbols).exists():
            for line in Path(data_symbols).read_text().splitlines():
                if line.startswith("#"):
                    continue
                f = line.split("\t")
                if len(f) < 7:
                    continue
                addr, storage, size, kinds = int(f[0], 16), f[1], int(f[2]), f[4].split(",")
                for name in f[6].split():
                    self.data.setdefault(name, (addr, storage, size, kinds))
                self.by_addr.append((addr, size, kinds, f[6].split()[0]))
        self.checked = bool(self.data)
        self.file_syms = n64syms.load_symbols()
        self.funcs = {}     # (image, name) -> address
        for image in ("game", "ovl_a", "ovl_b"):
            for addr, name in n64syms.load_layout_functions(image):
                self.funcs.setdefault((image, name), addr)
            for name, addr in n64syms.load_image_symbols(image).items():
                self.funcs.setdefault((image, name), addr)

    def addr(self, name):
        if name in self.data:
            return self.data[name][0]
        a = n64syms.address_of(name, self.file_syms)
        if a is None:
            raise SystemExit(f"synth_rom: no address for {name}")
        return a

    def func(self, name, image="game"):
        a = self.funcs.get((image, name))
        if a is None:
            a = n64syms.address_of(name, self.file_syms)
        if a is None:
            raise SystemExit(f"synth_rom: no function {name} in image {image}")
        return a

    def check(self, addr, kind, what):
        """The value written at addr must be an element of that kind in the symbol that holds it."""
        if not self.checked:
            return
        best = None
        for a, size, kinds, name in self.by_addr:
            if a <= addr < a + max(size, 1) and (best is None or a > best[0]):
                best = (a, size, kinds, name)
        if best is None:
            raise SystemExit(f"synth_rom: {what}: 0x{addr:08X} is in no data symbol")
        kinds = best[2]
        if kind not in kinds and not (kind == "u32" and {"s32>s64", "u32>u64"} & set(kinds)):
            raise SystemExit(f"synth_rom: {what}: {best[3]} holds {','.join(kinds)}, not {kind}")


# ---------------------------------------------------------------- images


class Image:
    def __init__(self, name, storage, rom_off, rom_len, size, base):
        self.name, self.storage, self.rom_off, self.rom_len = name, storage, rom_off, rom_len
        self.size, self.base = size, base
        self.b = bytearray(size)

    def has(self, addr, n=1):
        return self.base <= addr and addr + n <= self.base + self.size


class Cart:
    def __init__(self, syms):
        self.syms = syms
        self.images = [Image(*im) for im in n64syms.IMAGES]
        self.rom = bytearray(ROM_SIZE)
        self.log = []
        self.files = {}         # wave -> (bytes, what)
        self.model_waves = set()
        self.strings = {}       # string -> N64 address

    def image(self, name):
        return next(im for im in self.images if im.name == name)

    def _put(self, image, addr, data, kind, why):
        im = self.image(image)
        if not im.has(addr, len(data)):
            raise SystemExit(f"synth_rom: 0x{addr:08X} is outside image {image}")
        self.syms.check(addr, kind, why)
        im.b[addr - im.base:addr - im.base + len(data)] = data

    def set(self, image, sym, kind, value, why, index=0, offset=0, elem=None):
        """sym[index] (an element of `kind`), or the element at sym + offset, is value. elem: the element kind the
        data report must give that place, when it is not `kind` (a u32 in an f32 element swaps the same)."""
        w = KIND_WIDTH[kind]
        addr = self.syms.addr(sym) + offset + index * w
        if kind == "f32":
            data = bef32(value)
        elif kind == "f64":
            data = struct.pack(">d", value)
        elif w == 1:
            data = bytes([value & 0xFF])
        elif w == 2:
            data = be16(value)
        else:
            data = be32(value)
        self._put(image, addr, data, elem or kind, f"{sym}[{index}]+{offset}")
        self.log.append(f"{image:5} 0x{addr:08X} {sym}[{index}]{'+%d' % offset if offset else ''} {kind} = "
                        f"{value if kind in ('f32', 'f64') else hex(value)}: {why}")

    def set_bytes(self, image, addr, data, why):
        """Bytes at an address no declaration covers (the arena copies them as they are): it must be in an arena's
        initialized data and in no symbol with storage of its own."""
        if self.syms.checked:
            for a, size, kinds, name in self.syms.by_addr:
                if a <= addr < a + max(size, 1) or addr <= a < addr + len(data):
                    if self.syms.data[name][1].startswith("own"):
                        raise SystemExit(f"synth_rom: {why}: 0x{addr:08X} is in {name}'s own storage")
        im = self.image(image)
        if not im.has(addr, len(data)):
            raise SystemExit(f"synth_rom: 0x{addr:08X} is outside image {image}")
        im.b[addr - im.base:addr - im.base + len(data)] = data
        self.log.append(f"{image:5} 0x{addr:08X} {len(data)} bytes = {bytes(data).hex()}: {why}")

    def string(self, s):
        """The N64 address of the NUL-terminated string s in the game image's initialized data, in bytes that no
        symbol covers: the loader copies those as they are, and a char * to them resolves to the arena there."""
        if s in self.strings:
            return self.strings[s]
        self.strings[s] = self.blob(s.encode() + b"\0", f"string {s!r}")
        return self.strings[s]

    def strings_after(self, sym, size, texts, why, image="game"):
        """The N64 addresses of the strings `texts`, placed one after another (8-aligned) right after sym's first
        `size` bytes, where the images keep strings after some tables. No symbol may cover those bytes: a declaration
        sized by the gap to the next known address would, and the loader would swap the strings with its elements
        (BRINGUP.md 2)."""
        addr, out = self.syms.addr(sym) + size, []
        for text in texts:
            data = text.encode() + b"\0"
            for a, n, kinds, name in self.syms.by_addr:
                if a < addr + len(data) and addr < a + max(n, 1):
                    raise SystemExit(f"synth_rom: {why}: 0x{addr:08X} is inside {name} ({n} bytes from 0x{a:08X}), "
                                     f"which should end at 0x{self.syms.addr(sym) + size:08X}")
            self.set_bytes(image, addr, data, f"{why}: {text!r}")
            out.append(addr)
            addr = (addr + len(data) + 7) & ~7
        return out

    def blob(self, data, why):
        """The N64 address of bytes (big-endian data, 4-aligned) in the same free bytes of the game image's data."""
        if not hasattr(self, "pool"):
            lo, hi = n64syms.ARENAS[1][1], n64syms.ARENAS[1][2]
            spans = sorted((a, a + max(size, 1)) for a, size, _, _ in self.syms.by_addr)
            gaps, end = [], lo
            for a, b in spans:
                if a > end and end >= lo and a <= hi:
                    gaps.append((end, a))
                end = max(end, b)
            gaps = [((a + 3) & ~3, b) for a, b in gaps if b - ((a + 3) & ~3) >= 256]
            if not gaps:
                raise SystemExit("synth_rom: no free bytes in the game image's data for strings")
            self.pool = list(max(gaps, key=lambda g: g[1] - g[0]))
        addr = self.pool[0]
        if addr + len(data) > self.pool[1]:
            raise SystemExit("synth_rom: the free bytes for strings and tables are used up")
        self.set_bytes("game", addr, data, why)
        self.pool[0] = (addr + len(data) + 3) & ~3
        return addr

    def set_all(self, image, sym, kind, values, why):
        for i, v in enumerate(values):
            self.set(image, sym, kind, v, why if i == 0 else "(same)", index=i)

    def func_ptr(self, image, sym, index, func, why, func_image="game"):
        a = self.syms.func(func, func_image)
        self.set(image, sym, "ptr", a, f"{func}: {why}", index=index)

    def file(self, wave, data, what, model=False):
        self.files[wave] = (bytes(data), what)
        if model:
            self.model_waves.add(wave)

    def build(self):
        game = "game"
        # The file table (ASSETS.md 2.2): ROM offset (game data), size (boot data), storage kind 0 = raw (game
        # data), model flag (game data). Every wave gets a file: a zero size would make its heap block alias the
        # next block's header.
        pos = FILES_ROM
        place = {}
        for wave in range(N_WAVES):
            data, what = self.files.get(wave, (bytes(0x20), "unused: 32 zero bytes"))
            data = data or bytes(0x20)
            place[wave] = (pos, len(data), what)
            self.rom[pos:pos + len(data)] = data
            pos = (pos + len(data) + 15) & ~15
        if pos > self.image("game").rom_off:
            raise SystemExit("synth_rom: the files run into the game image's slot")
        for wave, (off, size, what) in place.items():
            self._put(game, self.syms.addr("D_8011B5BC") + 4 * wave, be32(off), "u32", "file table offset")
            self._put("boot", self.syms.addr("D_8002E580") + 4 * wave, be32(size), "u32", "file table size")
            self._put(game, self.syms.addr("D_80123564") + 4 * wave, be32(0), "u32", "file table kind")
            self._put(game, self.syms.addr("D_801234AC") + wave, bytes([1 if wave in self.model_waves else 0]),
                      "u8", "file table model flag")
        self.place = place

        self.rom[0:4] = be32(0x80371240)        # the .z64 byte order mark the loader checks
        self.rom[0x20:0x34] = b"RUSH2049 SYNTHETIC  "
        for im in self.images:
            if im.storage == "raw":
                self.rom[im.rom_off:im.rom_off + im.size] = im.b
                continue
            z = zlib.compressobj(9, zlib.DEFLATED, -15)
            stream = z.compress(bytes(im.b)) + z.flush()
            if len(stream) > im.rom_len:
                raise SystemExit(f"synth_rom: image {im.name}'s stream ({len(stream)} B) is longer than its slot")
            self.rom[im.rom_off:im.rom_off + len(stream)] = stream
        return self.rom

    def report(self):
        out = ["# synthetic cartridge (tools/port/synth_rom.py): every value set, and why", ""]
        out += self.log
        out += ["", "# files: wave, ROM offset, size, model flag, what"]
        for wave, (off, size, what) in sorted(self.place.items()):
            if wave in self.files:
                out.append(f"wave {wave:3} 0x{off:06X} {size:6} {'M' if wave in self.model_waves else '-'} {what}")
        return "\n".join(out) + "\n"


# ---------------------------------------------------------------- files


class Buf:
    """A big-endian file being built."""

    def __init__(self):
        self.b = bytearray()

    def __len__(self):
        return len(self.b)

    def u8(self, v):
        self.b.append(v & 0xFF)
        return len(self.b) - 1

    def u16(self, v):
        self.b += be16(v)
        return len(self.b) - 2

    def u32(self, v):
        self.b += be32(v)
        return len(self.b) - 4

    def f32(self, v):
        self.b += bef32(v)
        return len(self.b) - 4

    def raw(self, data):
        self.b += data
        return len(self.b) - len(data)

    def align(self, a):
        while len(self.b) % a:
            self.b.append(0)
        return len(self.b)

    def at32(self, off, v):
        self.b[off:off + 4] = be32(v)

    def at16(self, off, v):
        self.b[off:off + 2] = be16(v)


def text_bank(n_single=1024, n_tab=256, lang="EN"):
    """A text bank (ASSETS.md 3.3): six sections (u32 count, then u32 offsets or u16 items padded to an even
    count), then the strings. Every offset slot points at a short string of its own; the u16 tables are 0 (each
    index then names the first entry of the table it indexes)."""
    b = Buf()
    strings = []
    slots = []
    for section, n in enumerate((n_single, n_tab, n_tab, n_tab, n_tab, n_tab)):
        b.u32(n)
        if section in (0, 2, 5):
            for i in range(n):
                slots.append((b.u32(0), f"{lang}{section}.{i}"))
        else:
            for i in range((n + 1) & ~1):
                b.u16(0)
    for slot, s in slots:
        strings.append((slot, b.raw(s.encode() + b"\0")))
    for slot, off in strings:
        b.at32(slot, off)
    b.align(4)
    return b.b


def musyx_project(sfx_groups, song_groups):
    """A MusyX project (MUSYX_DATA.md 3.1): one group per id, with empty id lists; sfx groups (type 1) have an
    empty sfx table, song groups (type 0) an empty normal and drum page and no MIDI setups."""
    b = Buf()
    for gid, gtype in [(g, 1) for g in sfx_groups] + [(g, 0) for g in song_groups]:
        g = b.u32(0)
        b.u16(gid)
        b.u16(gtype)
        n_off = 6 if gtype == 1 else 8
        offs = [b.u32(0) for _ in range(n_off)]
        empty = b.u16(0xFFFF)
        for k in range(5):
            b.at32(offs[k], empty - g - 8)
        b.align(4)
        if gtype == 1:
            b.at32(offs[5], len(b) - g - 8)
            b.u16(0)        # sfx count
            b.u16(0)
        else:
            page = len(b)
            b.at32(offs[5], page - g - 8)
            b.at32(offs[6], page - g - 8)
            b.raw(bytes([0, 0, 0, 0, 0, 0xFF, 0, 0]))   # page end: program 0xFF
            b.at32(offs[7], len(b) - g - 8)
            b.u16(0xFFFF)   # no MIDI setups
            b.u16(0)
        b.at32(g, b.align(4) - g)
    b.u32(0xFFFFFFFF)
    return b.b


def musyx_pool():
    """A MusyX pool (MUSYX_DATA.md 3.2): four lists, each only its end entry."""
    b = Buf()
    offs = [b.u32(0) for _ in range(4)]
    for k in range(4):
        b.at32(offs[k], b.u32(0xFFFFFFFF))
    return b.b


def musyx_sdir():
    """A MusyX sample directory (MUSYX_DATA.md 3.3): only the end record."""
    b = Buf()
    b.u16(0xFFFF)
    b.raw(bytes(0x1A))
    return b.b


def musyx_song():
    """A song (MUSYX_DATA.md 3.4): no tracks, no patterns, a silent channel map, no tempo track."""
    b = Buf()
    hdr = [b.u32(0) for _ in range(6)]
    b.at32(hdr[0], b.align(4))
    for _ in range(64):
        b.u32(0)            # track table: no tracks
    b.at32(hdr[1], len(b))
    b.u32(0)                # pattern table (unused)
    b.at32(hdr[2], len(b))
    b.raw(bytes(64))        # channel map
    b.at32(hdr[3], 0)       # tempo track: none
    b.at32(hdr[4], 120 << 10)
    b.at32(hdr[5], 0)
    return b.b


def tag(s):
    return struct.unpack(">I", s.encode())[0]


def name16(s):
    return s.encode()[:15].ljust(16, b"\0")


def wad(chunks):
    """A WAD (ASSETS.md 3.1): an 8-byte header {u32 TOC offset, u32 count}, the chunks (8-aligned, as display
    lists are), then the TOC {u32 tag, u32 offset, u32 count or bytes} per chunk. chunks: [(tag, bytes, count)],
    count None meaning the chunk's length in bytes."""
    b = Buf()
    b.u32(0)
    b.u32(len(chunks))
    toc = []
    for t, data, count in chunks:
        off = b.align(8)
        b.raw(data)
        toc.append((tag(t), off, len(data) if count is None else count))
    b.at32(0, b.align(8))
    for t, off, n in toc:
        b.u32(t)
        b.u32(off)
        b.u32(n)
    return b.b


def txhd(name, width, height, size, fmt, tex_base, pal=-1, flags=0x08000000):
    """A TXHD texture record, 0x24 bytes: texBase is IMAG-relative; flags 0x08000000 = texels in IMAG."""
    b = Buf()
    b.raw(name16(name))
    b.u16(width)
    b.u16(height)
    b.u8(size)
    b.u8(fmt)
    b.u16(pal)
    b.u32(tex_base)
    b.u32(flags)
    b.u32(0)
    return b.b


FONT_W, FONT_H = 32, 16      # the font page: I4, solid
FONT_CHARS = range(32, 127)


def font_wad():
    """A font texture WAD (waves 22-37): one I4 page of solid white texels."""
    page = bytes([0xFF]) * (FONT_W * FONT_H // 2)
    return wad([("TXHD", txhd("font0", FONT_W, FONT_H, 0, 4, 0), 1), ("IMAG", page, None)])


def font_data():
    """Font metrics (ASSETS.md 3.7, waves 38-53): a 16-byte header of bytes, then a 12-byte letter per character
    {u32 kern slot; u8 value, texture, uls, ult, lrs, lrt; s16}. Every letter is the same 6 x 8 box of the page; no
    kerning. Type 1: an I4 page (_SetupFontData picks format I, 4-bit)."""
    b = Buf()
    chars = list(FONT_CHARS)
    #     version type ascent descent leading totalHeight maxWidth spaceWidth spacing nonProp kern textures chars
    for v in (1, 1, 8, 2, 1, 10, 6, 4, 1, 0, 0, 1, len(chars), 0, 0, 0):
        b.u8(v)
    for ch in chars:
        b.u32(0)
        for v in (ch, 0, 0, 0, 5, 7):
            b.u8(v)
        b.u16(0)
    return b.b


# Texture names the game's C looks up with MBOX_FindTexture (func_800B24EC): string literals in the code, so each
# must be in some loaded WAD or the caller gets NULL. "" stands for every name that comes from initialized data
# (MultiBlit tables, rodata strings), which is zero here.
CODE_TEXTURES = ["", "A1", "B1", "BACKGROUND_SFX", "BUTTON", "BUTTON_SELECT", "BUTTON_TITLE", "CNTDWN3",
                 "CURSOR", "CURSOR_B", "HIT_WALL"]
TEX_W, TEX_H = 16, 16          # every such texture: RGBA16, one solid colour


# Model names the C looks up with MBOX_FindModel (string_copy_format): "AAANULLOBJ" is what it looks up for a NULL
# or empty name, which every name from initialized data is here; the rest are string literals in the code.
CODE_MODELS = ["AAANULLOBJ", "MINEG1"]
WAD_OBJS = 8                   # the OBJS chunk comes first in these WADs, right after the header


def obhd(name, dl, vtx, radius=100.0):
    """An OBHD model record, 0x58 bytes: one LOD whose display list and vertices are file-relative (SetupWaveModel
    adds the file's address); the other three LODs zero."""
    b = Buf()
    b.raw(name16(name))
    b.f32(radius)               # radius
    b.u16(0)                    # kind
    b.u16(1)                    # lodCount
    b.u16(0)                    # lod 0: texidx
    b.u16(0)                    #        flags: no texture of its own (0x8000), no lighting (0x10)
    b.f32(1e6)                  #        lodRad
    b.u32(dl)                   #        displayList
    b.u32(vtx)                  #        vtxList
    b.raw(bytes(0x30))          # lods 1-3
    return b.b


# ---------------------------------------------------------------- display lists (F3DEX2, port/gfx/gfx_gbi.h)

def gfx(w0, w1=0):
    return be32(w0) + be32(w1)


# Each model's list sets the state it draws with, so it doesn't depend on what the game set before it: one-cycle
# mode, the shade colour (G_CC_SHADE in both cycles), an opaque z-buffered surface without blending, no texture, and
# smooth shading with back faces culled, without fog, lighting or texture generation. Then its vertices (G_VTX, w1
# file-relative: the WAD relocator, entity_lod_select, adds the file's address), its triangles (G_TRI2), G_ENDDL. The
# faces wind clockwise seen from outside in the world's axes: the scene's modelview negates x (func_8009F058's
# identity has m[0][0] = -1), which makes them counter-clockwise, front faces, on the screen. So the ground is seen
# from above only, and a box not from inside (the in-car camera's own car).
RM_ZB_OPA = 0x10 | 0x20 | 0x4000 | (3 << 26) | (2 << 18) | (3 << 24) | (2 << 16)   # Z_CMP Z_UPD FORCE_BL, CLR_IN x 1
DL_STATE = (gfx(0xE7000000) +                                   # G_RDPPIPESYNC
            gfx(0xE3000A01, 0) +                                # G_SETOTHERMODE_H cycle type: G_CYC_1CYCLE
            gfx(0xFCFFFFFF, 0xFFFE793C) +                       # G_SETCOMBINE G_CC_SHADE, G_CC_SHADE
            gfx(0xE200001C, RM_ZB_OPA) +                        # G_SETOTHERMODE_L render mode
            gfx(0xD7000000, 0xFFFFFFFF) +                       # G_TEXTURE off
            gfx(0xD9000000 | (~(0x00010000 | 0x00020000 | 0x00040000 | 0x00080000 | 0x600) & 0xFFFFFF),
                0x1 | 0x4 | 0x00200000 | 0x400))               # G_GEOMETRYMODE: - fog, lighting, texgen, culling;
                                                                # + z-buffer, shade, smooth, cull back


def vtx(x, y, z, rgba, last):
    """A Vtx (16 bytes): s16 position, the u16 flags at +6 (bit 15: another vertex follows, which func_800966D8's walk
    of the vertex chain reads), s16 texture coordinates, the colour."""
    b = Buf()
    for v in (x, y, z):
        b.u16(int(v))
    b.u16(0 if last else 0x8000)
    b.u16(0)
    b.u16(0)
    b.raw(bytes(rgba))
    return b.b


def mesh(quads):
    """Vertices and a display list for quads [(four (x, y, z), rgba)], eight quads (32 vertices) a G_VTX: returns
    (vertex bytes, a function of the vertices' file offset giving the list's bytes)."""
    verts = b""
    count = 4 * len(quads)
    for qi, (corners, rgba) in enumerate(quads):
        for k, (x, y, z) in enumerate(reversed(corners)):      # clockwise from outside (above)
            verts += vtx(x, y, z, rgba, 4 * qi + k == count - 1)

    def dlist(vtx_off):
        out = DL_STATE
        for first in range(0, len(quads), 8):
            n = 4 * min(8, len(quads) - first)
            out += gfx((0x01 << 24) | (n << 12) | (n << 1), vtx_off + 16 * 4 * first)   # G_VTX n, v0 = 0
            for q in range(n // 4):
                a = 4 * q
                out += gfx((0x06 << 24) | (2 * a << 16) | (2 * (a + 1) << 8) | 2 * (a + 2),
                           (2 * a << 16) | (2 * (a + 2) << 8) | 2 * (a + 3))           # G_TRI2: two halves
        return out + gfx(0xDF000000)                                                  # G_ENDDL
    return verts, dlist


def box_quads(dx, dy, dz):
    """A box from -dx..dx, 0..dy, -dz..dz with a colour per face: +x (the front) red, -x blue, the top yellow, the
    bottom grey, the sides green and cyan. Each face counter-clockwise seen from outside (mesh reverses them)."""
    x0, x1, y0, y1, z0, z1 = -dx, dx, 0, dy, -dz, dz
    return [(((x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)), (230, 40, 40, 255)),
            (((x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (x0, y0, z0)), (40, 60, 230, 255)),
            (((x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)), (240, 220, 40, 255)),
            (((x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)), (110, 110, 110, 255)),
            (((x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)), (40, 200, 60, 255)),
            (((x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)), (40, 200, 200, 255))]


# Model vertices are in sixteenths of a foot (the scene's matrices and the projection's near and far, x16; a world
# position is in feet). The ground is a 16 x 16 grid of 250-foot tiles, 4000 feet a side, centred on the TRACK
# node, which the world puts at the middle of the 2000-foot square loop.
UNITS = 16
GROUND_TILE = 250
GROUND_CENTRE = (SQUARE / 2, 0.0, SQUARE / 2)


def ground_quads():
    """The track's ground at y = 0 round its node: a checker of two greens, with the tiles under the square loop
    (the paths' and the road's) grey."""
    quads = []
    half = 8 * GROUND_TILE
    for i in range(16):
        for j in range(16):
            x, z = -half + GROUND_TILE * i, -half + GROUND_TILE * j      # the tile's corner, from the node
            wx, wz = x + GROUND_CENTRE[0], z + GROUND_CENTRE[2]          # in the world
            on_loop = ((wx <= 0 < wx + GROUND_TILE or wx <= SQUARE < wx + GROUND_TILE) and 0 <= wz + 1 <= SQUARE) or \
                      ((wz <= 0 < wz + GROUND_TILE or wz <= SQUARE < wz + GROUND_TILE) and 0 <= wx + 1 <= SQUARE)
            rgba = (120, 120, 130, 255) if on_loop else (60, 150, 60, 255) if (i + j) % 2 else (40, 110, 40, 255)
            t, x, z = GROUND_TILE * UNITS, x * UNITS, z * UNITS
            quads.append((((x, 0, z), (x, 0, z + t), (x + t, 0, z + t), (x + t, 0, z)), rgba))
    return quads


# The models: a car-sized box for the models the game finds by an empty or no name (AAANULLOBJ: the cars, the
# front end's buttons, the weapon icons) and the code's others, and the ground for the world node's TRACK.
BOX = (4.5 * UNITS, 3.0 * UNITS, 2.5 * UNITS)


def model_objs(models):
    """OBJS for the models (names): each model's vertices and display list, in the chunk that starts at file offset
    WAD_OBJS. Returns (the chunk, {name: (list offset, vertex offset, radius)}), offsets file-relative. Each model has
    lists of its own: SetupWaveModel relocates every model's lists in place (entity_lod_select adds the file's
    address to each address), so a list two models shared would be relocated twice."""
    out, where = b"", {}
    for m in models:
        ground = m == WORLD_NODE
        verts, dlist = mesh(ground_quads() if ground else box_quads(*BOX))
        vtx_off = WAD_OBJS + len(out)
        out += verts
        dl_off = WAD_OBJS + len(out)
        out += dlist(vtx_off)
        where[m] = (dl_off, vtx_off, 3000.0 if ground else 10.0)
    if not out:                                   # no models: one list that ends at once
        out = gfx(0xDF000000)
    return out, where


def plhd(name, pal_base):
    """A PLHD palette record, 0x18 bytes: 256 RGBA16 entries at palBase (IMAG-relative)."""
    b = Buf()
    b.raw(name16(name))
    b.u8(0)
    b.u8(255)                   # start, end
    b.u16(0)                    # format
    b.u32(pal_base)
    return b.b


def texture_wad(names, models=(), palettes=("",)):
    """A WAD of textures (TXHD sorted by name: MBOX_FindTexture bsearches it with strcmp), all sharing one solid
    RGBA16 image in IMAG, models (OBHD, sorted the same way for MBOX_FindModel: boxes, and the ground), and palettes
    (PLHD: the car paint looks its palette up by a name from a format string, which is "" here) of 256 greys."""
    image = be16(0x7BDF) * (TEX_W * TEX_H)      # a light grey, opaque (5-5-5-1)
    palette = be16(0x7BDF) * 256
    names = sorted(set(names), key=lambda n: n.encode())
    models = sorted(set(models), key=lambda n: n.encode())
    palettes = sorted(set(palettes), key=lambda n: n.encode())
    recs = b"".join(txhd(n, TEX_W, TEX_H, 2, 0, 0) for n in names)
    objs, where = model_objs(models)
    chunks = [("OBJS", objs, None)]
    if models:
        chunks.append(("OBHD", b"".join(obhd(m, *where[m]) for m in models), len(models)))
    chunks.append(("TXHD", recs, len(names)))
    if palettes:
        chunks.append(("PLHD", b"".join(plhd(n, len(image)) for n in palettes), len(palettes)))
    chunks.append(("IMAG", image + palette, None))
    return wad(chunks)


def empty_wad():
    """A WAD with no chunks: every MBGetFromWad lookup finds nothing (count 0)."""
    return wad([])




def square_points(n):
    """n points around the square, starting at a corner, as s16 (x, y, z)."""
    pts = []
    for i in range(n):
        t = i * 4.0 / n
        side, f = int(t), t - int(t)
        x, z = [(f, 0), (1, f), (1 - f, 1), (0, 1 - f)][side]
        pts.append((int(x * SQUARE), 0, int(z * SQUARE)))
    return pts


def path_file():
    """A path file (ASSETS.md 3.6, port/assets/path.c): four checkpoints on the corners of the square, a
    resurrection path of eight points round it, no alternative paths, and four drone paths of eight points."""
    b = Buf()
    corners = square_points(4)
    #     startTime loopPt finishPt startPt numPoints pad
    for v in (90, 0, 0, 0, len(corners), 0):
        b.u16(v)
    for i in range(10):
        x, y, z = corners[i % len(corners)]
        for v in (x, y, z, 0.0, 1.0, 0.0, 200.0):   # pos, normal, radius
            b.f32(v)
        b.u16(7 if i == 0 else 0)                  # flags: finish, start and loop at checkpoint 0
        b.u16(30)
        b.u16(30)                                  # time[2]
        for k in range(20):
            b.u16(0)                               # pathPts
        b.u16(0)
        b.f32(float(SQUARE))                       # distToNext
    assert len(b) == 0x32C
    res = square_points(8)
    b.u16(len(res))
    b.u16(0)
    b.u32(0)                                       # pts slot (the game fills it)
    b.u8(0)                                        # no alternative paths
    b.raw(bytes(3))
    b.u32(0)                                       # alt slot
    b.u16(len(res))                                # resurrection point count
    for p in res:
        for v in p:
            b.u16(v)
    for _ in range(4):                             # drone paths
        b.u16(len(res))
        b.u16(0)
        b.u32(0)
        for x, y, z in res:
            b.u16(x)
            b.u16(y)
            b.u16(z)
            b.u8(60)                               # mph
            b.u8(0)                                # hints
    b.align(4)
    return b.b


WORLD_NAME = "WORLD"           # every track's world (gWorldNames) and its one node's model
WORLD_NODE = "TRACK"


def world_node(name, pos=(0.0, 0.0, 0.0), next_index=0xFFFF):
    """A WOBJ node, 0x68 bytes: its name (a model name, or a target's), an identity rotation and a position, no
    flags, its next sibling (0xFFFF none), no child, no sounds or texture index, a zero bound."""
    node = Buf()
    node.raw(name16(name))
    for v in (1, 0, 0, 0, 1, 0, 0, 0, 1) + tuple(pos):   # mat[12]: rotation, then translation
        node.f32(float(v))
    node.u32(0)                 # dlflags
    node.u16(next_index)        # next
    node.u16(0xFFFF)            # child: none
    node.u16(0)
    node.u16(0)                 # sfx1, sfx2
    node.u32(0)                 # tidx
    for v in (0, 0, 0, 0, 0, 0):
        node.f32(float(v))      # min[3], max[3]
    assert len(node) == 0x68
    return node.b


def world_wad(targets=()):
    """A world WAD (ASSETS.md 3.4, port/assets/world.c): WHDR names one world, whose root is the WOBJ node TRACK
    (its model is in the track WAD) and its siblings, one per target (name, position): the world walk
    (differential_output) offers each node to the target table (transmission_ratio_get), which makes a level object
    of a node whose name starts with a target's. GDAT one quadtree node, a leaf with nothing in it; GTLD no target
    keys."""
    nodes = [(WORLD_NODE, GROUND_CENTRE)] + list(targets)
    wobj = b"".join(world_node(n, pos, i + 1 if i + 1 < len(nodes) else 0xFFFF) for i, (n, pos) in enumerate(nodes))
    whdr_at = WAD_OBJS          # chunks are laid out in order from just after the header
    whdr = Buf()
    whdr.u32(1)
    whdr.u32(0)                 # woff: the root WOBJ node's file offset, set below
    whdr.raw(name16(WORLD_NAME))
    gdat = Buf()
    gdat.u16(0xFFFF)            # parent: none
    gdat.u8(0)
    gdat.u8(0)                  # flags
    for v in (-SQUARE, SQUARE, -SQUARE, SQUARE):
        gdat.f32(float(v))      # minh, maxh, minv, maxv
    for _ in range(4):
        gdat.u16(0)             # a leaf (flags 0): no target list in any quarter
    wobj_at = (whdr_at + len(whdr) + 7) & ~7
    whdr.at32(4, wobj_at)
    w = wad([("WHDR", whdr.b, None), ("WOBJ", wobj, len(nodes)), ("GDAT", gdat.b, 1), ("GTLD", b"", 0)])
    assert w[wobj_at:wobj_at + 16] == name16(WORLD_NODE)
    return w


def road_file():
    """A road file (ASSETS.md 3.5): flat pavement at y = 0 under the ground, x and z from -1000 to 3000 feet.

    - header: no path or save records, one tree node, one surface, four vertices, a 5-byte vertex list and a 4-byte
      surface list (slnum, which the N64 doesn't read);
    - the tree node (handbrake_apply, func_800AC9BC): the root (parent -1), bounds in whole feet (h is x, v is z,
      half-open), flags 0 so each quarter's child is a byte offset into the surface lists (0 would be "none"), all
      four the same list;
    - the surface (0x18 bytes): type 0 (roadcode 0, pavement; a wall, trigger or disabled code would be skipped or
      act), its vertex count 4 in the low bits of the second word, an identity 3 x 3 matrix in 2^-14 units (row 1 is
      the up normal: entity_update's height above the road is that row's dot product with the point less the first
      vertex), and its vertex list at offset 0;
    - the vertices (s16 feet and a word of 1/32 fractions): the first is the corner (-1000, 0, -1000) in the world,
      the others are in the surface's frame from it, (4000, 0, 0), (4000, 0, 4000), (0, 0, 4000), in the order
      func_800C3AD0's edge tests take as inside;
    - the vertex list (func_800AD5D0): the first index as a big-endian u16, then a run byte 0xE0 | 3 for the next
      three, then the u16 path record index the edge handlers read (0);
    - the surface list (func_800ADCE0): offset 0 is a pad byte (0 means none), then the count 1 and one word, a run
      of 0 more from surface 0."""
    lo, hi = -1000, 3000
    b = Buf()
    for v in (0, 1, 1, 4, 0, 5):    # pnum, tnum, snum, vnum, savenum, vlnum
        b.u16(v)
    b.u32(4)                        # slnum
    b.u16(0xFFFF)                   # tree node 0: no parent
    b.u8(0)
    b.u8(0)                         # flags: every quarter a leaf
    for v in (lo, hi, lo, hi):
        b.u16(v)                    # minh, maxh, minv, maxv
    for _ in range(4):
        b.u16(1)                    # each quarter: the surface list at offset 1
    b.u16(0)                        # surface 0: type 0, pavement
    b.u16(4)                        #            four vertices
    for v in (0x4000, 0, 0, 0, 0x4000, 0, 0, 0, 0x4000):
        b.u16(v)                    #            identity orientation, 1.0 = 0x4000
    b.u16(0)                        #            its vertices at vertex list offset 0
    for x, y, z in ((lo, 0, lo), (hi - lo, 0, 0), (hi - lo, 0, hi - lo), (0, 0, hi - lo)):
        for v in (x, y, z, 0):
            b.u16(v)                # vertex: s16 x, y, z, then the fractions word
    b.raw(bytes([0x00, 0x00, 0xE3, 0x00, 0x00]))    # vertex list: 0, a run of 3 more, the path record index 0
    b.raw(bytes([0x00, 0x01, 0x00, 0x00]))          # surface list: the pad, one surface: a run of 0 from surface 0
    return b.b


# ---------------------------------------------------------------- the cartridge


def boot_and_sound(c):
    """What the boot needs (docs/port/SYNTH.md 2): MusyX's tables, the static display lists, the first game state."""
    # MusyX (docs/port/MUSYX_DATA.md 8.6). func_800114C0 sizes the sample cache as D_8002C5D0[mode] x voices;
    # zero gives zero-entry caches whose first node's links land in the next heap block's header.
    c.set_all("boot", "D_8002C5D0", "u16", [2] * 6, "sample cache entries per voice, per mode (non-zero)")
    # Its tuning factors scale the voice and DMA buffer counts from the output rate; 1.0 keeps the rate's own.
    for s in ("D_8002D8B8", "D_8002D8BC", "D_8002D8C0", "D_8002D8C4", "D_8002D8C8"):
        c.set("boot", s, "f32", 1.0, "MusyX buffer scale (neutral)")
    # The service table AudioInitSys hands to the library: the eight functions at the end of the game's code,
    # in the order the library calls them (MUSYX_DATA.md 8.1; port/main.c's musyx_services is the same list).
    for i, f in enumerate(("func_8010FBE0", "sync_acquire_menu", "func_8010FC60", "func_8010FC70",
                           "struct_init_and_call", "func_8010FD60", "resource_alloc_init", "synced_model_render")):
        c.func_ptr("game", "D_8011EAAC", i, f, "MusyX service slot")
    # The sfx groups AudioInitSys pushes (Instrument4 {u16 other, id}); the project has a group for each.
    for j, g in enumerate(SFX_GROUPS):
        c.set("game", "D_8011F0A0", "u16", g, "sfx group id pushed at boot", offset=4 * j + 2)

    # The boot data's two static display lists (Gfx[], called from every frame's list): a list of zeros is a run of
    # no-ops that walks off its end on the N64 as on the PC, so each is just its end command.
    for s in ("D_8002E4A0", "D_8002E528"):
        c.set("boot", s, "u32", 0xDF000000, "G_ENDDL: the static display list ends at once")

    # The game's state machine (groups/state/state.c): gGameState 0 and gNewGameState 1, so the first frame's
    # updateGameState runs initGameState's state 1 (InitWaves, overlay "Start", SetOptions, InitRecords), which
    # moves on to state 2 (the front end's waves). Zero for both is a game with no state, where nothing happens.
    c.set("game", "state_word_b", "u32", 0x1, "gNewGameState: start in state 1 (initGameState)")
    # gSkipToGameState (D_801174BC): a screen whose state isn't this one passes straight on to its next screen,
    # unless it is 1, which every screen writes back once it is reached (and the race start, gameover's
    # "Quit"...). 0 skips the start prompt and the main menu; 1 is "no skip".
    c.set("game", "gSkipToGameState", "u32", 0x1, "gSkipToGameState: 1, no screen skipped")

    for lang, wave in zip(("EN", "ES", "IT", "DE", "NL", "FR"), range(6)):
        c.file(wave, text_bank(lang=lang), f"text bank {lang}")
    c.file(6, musyx_project(SFX_GROUPS, []), "MusyX project: the sfx groups, empty")
    c.file(7, musyx_pool(), "MusyX pool: empty")
    c.file(8, musyx_sdir(), "MusyX sample directory: empty")
    for s in range(12):
        c.file(10 + s, musyx_song(), f"song {s}: no tracks")
    for f in range(16):
        c.file(22 + f, font_wad(), f"font {f} texture WAD", model=True)
        c.file(38 + f, font_data(), f"font {f} metrics")


# ---------------------------------------------------------------- the elementary functions

# The game's own sinf, cosf, tanf, atan, atan2, asin and acos read their constants from data: zero on a blank
# cartridge, where sin x = x, cos x = -|x|, tan x = 0 and atan returns 0 or NaN, so no projection, camera or rotation
# works. The constants here are computed from the functions' structure (the formula each evaluates is written next
# to it): reduction constants from pi, and the polynomial or rational coefficients as Pade approximants or a least-
# squares fit made here from the Taylor series. None is a table copied from anywhere.


def tan_pade():
    """tan f = f (1 + P1 g + P2 g^2 + P3 g^3) / (1 + Q1 g + ... + Q4 g^4), g = f^2: the convergent of Lambert's
    continued fraction tan f = f / (1 - g / (3 - g / (5 - ...))) with those degrees, exact, then normalised."""
    num, den = [Fraction(15)], [Fraction(1)]          # the fraction's tail, T = 15, as num(g) / den(g)
    for k in (13, 11, 9, 7, 5, 3, 1):                 # T = k - g / T = (k num - g den) / num
        new = [k * x for x in num] + [Fraction(0)] * (len(den) + 1 - len(num))
        for i, x in enumerate(den):
            new[i + 1] -= x
        while len(new) > 1 and new[-1] == 0:
            new.pop()
        num, den = new, num
    p, q = den, num                                   # tan f = f / T = f den / num
    p = [x / p[0] for x in p]
    q = [x / q[0] for x in q]
    assert len(p) == 4 and len(q) == 5, (len(p), len(q))
    return [float(x) for x in p[1:]], [float(x) for x in q[1:]]


def atan_pade():
    """atan t / t = N(g) / D(g), g = t^2, N of degree 4 and D of degree 5: the [4/5] Pade approximant of the series
    sum (-g)^k / (2k + 1), scaled so that D is monic; then N and D share their constant term, as func_8008C5E0's one
    constant for both says. Returns (c0, [n1..n4], [d1..d4])."""
    L, M = 4, 5
    c = [Fraction((-1) ** k, 2 * k + 1) for k in range(L + M + 1)]
    # D = 1 + d1 g + ... + d5 g^5 with sum_j d_j c_(k-j) = 0 for k = L+1 .. L+M
    rows = [[c[k - j] if k - j >= 0 else Fraction(0) for j in range(1, M + 1)] + [-c[k]]
            for k in range(L + 1, L + M + 1)]
    for i in range(M):                                 # Gauss-Jordan, exact
        piv = next(r for r in range(i, M) if rows[r][i] != 0)
        rows[i], rows[piv] = rows[piv], rows[i]
        rows[i] = [x / rows[i][i] for x in rows[i]]
        for r in range(M):
            if r != i and rows[r][i] != 0:
                f = rows[r][i]
                rows[r] = [a - f * b for a, b in zip(rows[r], rows[i])]
    d = [Fraction(1)] + [rows[i][M] for i in range(M)]
    n = [sum(c[k - j] * d[j] for j in range(0, k + 1)) for k in range(L + 1)]
    n, d = [x / d[M] for x in n], [x / d[M] for x in d]
    assert d[M] == 1 and n[0] == d[0]
    return float(n[0]), [float(x) for x in n[1:]], [float(x) for x in d[1:M]]


def sin_fit():
    """sin x = x + x^3 (P1 + P2 g + P3 g^2 + P4 g^3), g = x^2, on |x| <= pi/2: a least-squares fit of
    (sin x - x) / x^3 at 64 Chebyshev points of g in [0, (pi/2)^2], solved by Gaussian elimination."""
    gmax = (math.pi / 2) ** 2
    pts = [gmax * (1 - math.cos(math.pi * (i + 0.5) / 64)) / 2 for i in range(64)]

    def target(g):
        x = math.sqrt(g)
        if x < 1e-2:                                   # the series, where the quotient loses its digits
            return -1 / 6 + g / 120 - g * g / 5040
        return (math.sin(x) - x) / (x * x * x)
    a = [[sum(gi ** (i + j) for gi in pts) for j in range(4)] + [sum(target(gi) * gi ** i for gi in pts)]
         for i in range(4)]
    for i in range(4):
        piv = max(range(i, 4), key=lambda r: abs(a[r][i]))
        a[i], a[piv] = a[piv], a[i]
        for r in range(4):
            if r != i:
                f = a[r][i] / a[i][i]
                a[r] = [x - f * y for x, y in zip(a[r], a[i])]
    return [a[i][4] / a[i][i] for i in range(4)]


def math_constants(c):
    """The constants of the game's elementary functions (see above)."""
    # libultra's sinf and cosf (boot data, double): reduce by pi (n = round(x / pi), dx = x - n (pi_hi + pi_lo), the
    # sign flipped for odd n; cosf by (n - 1/2) pi), then sin dx = dx + dx^3 P(dx^2). pi_hi has 21 significant bits,
    # so n pi_hi is exact for every n below 2^28 / pi, past which both give up and return 0.
    pi_hi = math.floor(math.pi * 2 ** 19) / 2 ** 19
    coeffs = sin_fit()
    for sym, scale, hi, lo in (("gSinCoeffs", "gTwoOverPi", "gPiOver2Hi", "gPiOver2Lo"),
                               ("gCosCoeffs", "gCosAngleScale", "gCosPiOver2Hi", "gCosPiOver2Lo")):
        c.set("boot", sym, "f64", 1.0, "the sine polynomial's x term (never read)")
        for i, v in enumerate(coeffs, 1):
            c.set("boot", sym, "f64", v, f"sin x = x + x^3 (P1 + P2 x^2 + ...): P{i}, a least-squares fit", index=i)
        c.set("boot", scale, "f64", 1 / math.pi, "the reduction: x / pi")
        c.set("boot", hi, "f64", pi_hi, "the reduction: pi, its first 21 bits")
        c.set("boot", lo, "f64", math.pi - pi_hi, "the reduction: the rest of pi")
    c.set("boot", "gNaN", "f32", 0.0, "sinf's result for |x| >= 2^28: 0")
    c.set("boot", "gCosOne", "f32", 0.0, "cosf's result for |x| >= 2^28: 0")
    c.set("boot", "gNaNf", "u32", 0x7FC00000, "sinf's result for a NaN: a quiet NaN", elem="f32")
    c.set("boot", "gOrthoScale", "f64", math.pi / 180, "guPerspectiveF's fovy factor: degrees to radians")

    # tanf (func_800A557C): xn = round(x 2/pi); f = (x - xn C1) - xn C2 with C1 + C2 = pi/2 (C1 = 201/128, 8 bits,
    # so xn C1 is exact); f itself below eps, else f (1 + P(g)) / (1 + Q(g)), and -1 over that for odd xn; 0 past ymax.
    p, q = tan_pade()
    c.set("game", "D_80123B98", "f32", math.pi / 2 * 2 ** 12, "tanf: the largest argument it reduces")
    c.set("game", "D_80123B9C", "f32", 2 / math.pi, "tanf: x 2/pi counts the quarter turns")
    c.set("game", "D_80123BA0", "f32", 201 / 128, "tanf: pi/2, its first 8 bits (C1)")
    c.set("game", "D_80123BA4", "f32", math.pi / 2 - 201 / 128, "tanf: the rest of pi/2 (C2)")
    c.set("game", "D_80123BA8", "f32", 2 ** -12, "tanf: below this, tan f = f")
    for sym, v, what in (("D_80123BB4", p[0], "P1"), ("D_80123BB0", p[1], "P2"), ("D_80123BAC", p[2], "P3"),
                         ("D_80123BC4", q[0], "Q1"), ("D_80123BC0", q[1], "Q2"), ("D_80123BBC", q[2], "Q3"),
                         ("D_80123BB8", q[3], "Q4")):
        c.set("game", sym, "f32", v, f"tanf: {what} of f (1 + P(g)) / (1 + Q(g)), Lambert's continued fraction")

    # atan: the kernel func_8008C5E0 is t N(g) / D(g) on |t| <= tan(pi/8); func_8008C680 maps x > tan(3pi/8) to
    # pi/2 - K(1/x) and the middle to pi/4 + K((x - 1) / (x + 1)); atan2 (func_8008C768) adds +-pi/2 and +-pi.
    c0, n, d = atan_pade()
    c.set("game", "D_801238C0", "f32", c0, "atan kernel: N's and D's constant term ([4/5] Pade, D monic)")
    for sym, v, what in (("D_801238D0", n[0], "n1"), ("D_801238CC", n[1], "n2"), ("D_801238C8", n[2], "n3"),
                         ("D_801238C4", n[3], "n4"), ("D_801238E0", d[0], "d1"), ("D_801238DC", d[1], "d2"),
                         ("D_801238D8", d[2], "d3"), ("D_801238D4", d[3], "d4")):
        c.set("game", sym, "f32", v, f"atan kernel: {what} of atan t = t N(g) / D(g)")
    for sym, v, what in (("D_801238E4", math.tan(math.pi / 8), "the kernel's own range, tan(pi/8)"),
                         ("D_801238E8", math.tan(3 * math.pi / 8), "past this, pi/2 - K(1/x): tan(3pi/8)"),
                         ("D_801238EC", math.pi / 2, "pi/2"), ("D_801238F0", math.pi / 4, "pi/4"),
                         ("D_801238F4", math.pi / 2, "atan2: +pi/2"), ("D_801238F8", -math.pi / 2, "atan2: -pi/2"),
                         ("D_801238FC", math.pi, "atan2: +pi"), ("D_80123900", -math.pi, "atan2: -pi")):
        c.set("game", sym, "f32", v, f"atan: {what}")
    # asin and acos (func_8009C3F8): a literal rational for |x| <= 1/2 and asin x = pi/2 - 2 asin sqrt((1 - x) / 2)
    # above, joined by {a0, a1, b0, b1}: asin = (a[i] + r) + a[i], acos = (a[i] - r) + a[i] or (b[i] + r) + b[i].
    c.set_all("game", "D_8011F010", "f32", [0.0, math.pi / 4, math.pi / 2, math.pi / 4],
              "asin/acos: the offsets {0, pi/4, pi/2, pi/4}")
    # The vector helpers' floors, and the tire slip ratio's slope past |z| 400 (continuous with x / z there).
    c.set("game", "D_80123888", "f32", 1e-10, "func_8008B424: the least squared length it takes 1/sqrt of")
    c.set("game", "D_8012394C", "f32", 1e-6, "func_8008E0B8: a length it doesn't normalise")
    c.set("game", "D_80123F80", "f32", 1e-5, "func_800C4180: matrix entries it takes for zero (gimbal lock)")
    c.set("game", "D_80123E04", "f32", 1 / 400, "func_800BC21C: x / z for z >= 400 (continuous)")
    c.set("game", "D_80123E08", "f32", 1 / 400, "func_800BC21C: -x / z for z <= -400 (continuous)")


SCREEN_W, SCREEN_H = 320, 220   # the game's framebuffer (display())
VI_X_PER_PIXEL = 1024 / 640     # gViewportFloatA: 2.10 xScale per pixel of a 640-pixel line
VI_Y_PER_LINE = 1024 / 240      # gViewportFloatB: 2.10 yScale per line of a 240-line field


def vi_mode_ntsc_lpn1():
    """An NTSC, low-resolution, point-sampled, non-interlaced, 16-bit VI mode as 20 big-endian words (libultra's
    OSViMode: the type byte and 3 pad bytes; comRegs ctrl, width, burst, vSync, hSync, leap, hStart, xScale, vCurrent;
    fldRegs[2] origin, yScale, vStart, vBurst, vIntr), computed from what the VI registers mean, for the game's
    320 x 220 framebuffer on NTSC's 240 lines. Its scales are what display_update makes of the screen size
    (`width x FloatA`, `(height x FloatB + 16.5) & ~15`): display_mode_tick shows the picture (osViBlack(0)) only
    once the mode it made has the table's yScale."""
    ctrl = 0x2 | 0x4 | 0x8 | 0x10 | 0x300 | 0x3000  # 16-bit, gamma dither, gamma, divot, resample only, advance 3
    burst = (62 << 20) | (5 << 16) | (34 << 8) | 57  # burst start, vsync width, burst width, hsync width
    v_sync = 525                                      # lines a frame
    h_sync = 3093                                     # a line, in quarter pixels
    h_start = (108 << 16) | 748                       # the active line: pixels 108-748
    x_scale = int(SCREEN_W * VI_X_PER_PIXEL)          # 2.10: 320 pixels over the 640-pixel line
    y_scale = int(SCREEN_H * VI_Y_PER_LINE + 16.5) & 0xFFF0   # 2.10: 220 lines over 240
    comregs = (ctrl, SCREEN_W, burst, v_sync, h_sync, (h_sync << 16) | h_sync, h_start, x_scale, 0)
    #         origin (a line's bytes), yScale, active half-lines 37-511, burst half-lines, VI interrupt line
    field = (SCREEN_W * 2, y_scale, (37 << 16) | 511, (14 << 16) | 516, 2)
    return [0] + list(comregs) + list(field) * 2      # word 0: the type, 0, in its first byte


def display(c):
    """The screen and the video mode (lib_21f0.c's viewport_setup and display_update, gfx_frame_begin)."""
    # The game's framebuffers are heap blocks of 0x22620 bytes, 32 more than a 320 x 220 16-bit image (aligned up to
    # 64 in _InitGfx); gfx_frame_begin sets the colour image and clears it at the screen size D_8002AFC0 x D_8002AFC4,
    # and ZOID_ResetWindows makes the windows from it. Zero is a colour image 4096 pixels wide (its width field is
    # width - 1) and windows of no size.
    c.set("boot", "D_8002AFC0", "u32", SCREEN_W, "the screen's width (a framebuffer is 320 x 220 x 2 bytes)")
    c.set("boot", "D_8002AFC4", "u32", SCREEN_H, "the screen's height")
    # The VI modes (gViModeTableBase, OSViMode[5]): viewport_setup takes mode get_tv_offset(), 0 for NTSC (osTvType
    # 1), and display_update divides by its active line's and field's lengths (0 / 0 with a zero mode).
    c.set("boot", "gViModeTableBase", "u8", 0, "VI mode 0: NTSC LPN1, type 0 (vi_mode_ntsc_lpn1)")
    for k, v in enumerate(vi_mode_ntsc_lpn1()[1:], 1):
        c.set("boot", "gViModeTableBase", "u32", v, "VI mode 0: its registers" if k == 1 else "(same)", offset=4 * k)
    # display_update scales the mode by the screen size: xScale = width x FloatA, yScale = (height x FloatB + 16.5) &
    # ~15. FloatA = 1024 / 640 and FloatB = 1024 / 240 are the 2.10 scales of a pixel of the 640-pixel line and of a
    # line of the 240-line field (vi_mode_ntsc_lpn1 uses the same). FloatC and FloatD are the seconds a retrace takes,
    # PAL and NTSC (gScaleSecondsPerTick; the ticks a second are 50 and 60).
    c.set("boot", "gViewportFloatA", "f32", VI_X_PER_PIXEL, "xScale per pixel of the line (2.10)")
    c.set("boot", "gViewportFloatB", "f32", VI_Y_PER_LINE, "yScale per line of the field (2.10)")
    c.set("boot", "gViewportFloatC", "f32", 1 / 50, "a PAL retrace, in seconds")
    c.set("boot", "gViewportFloatD", "f32", 1 / 60, "an NTSC retrace, in seconds")


def front_end(c):
    """The front end's screens, from the start prompt to the car select (SYNTH.md 2)."""
    # The 19-entry button map ControllerUpdate turns the pads' buttons into the game's with (game bit j = any N64
    # button in D_8011FAD4[j]). A zero map gives no buttons at all, so no screen that waits for one ever ends. This
    # one is made up from how the code uses the bits: 0x400/0x800/0x1000/0x2000 are what func_800E7134 makes of the
    # stick (up, down, left, right), so those are the d-pad; bits 0-2 are what the start prompt and the pak
    # warning take to go on (Start, A, B here); the rest are spread over the other buttons.
    n64_buttons = {"A": 0x8000, "B": 0x4000, "Z": 0x2000, "START": 0x1000, "DU": 0x0800, "DD": 0x0400,
                   "DL": 0x0200, "DR": 0x0100, "L": 0x0020, "R": 0x0010, "CU": 0x0008, "CD": 0x0004,
                   "CL": 0x0002, "CR": 0x0001}
    button_map = ["START", "A", "B", "Z", "L", "R", "CU", "CD", "CL", "CR", "DU", "DD", "DL", "DR"]
    for j, b in enumerate(button_map):
        c.set("game", "D_8011FAD4", "u32", n64_buttons[b], f"game button bit {j}: the pad's {b}", index=j)

    # The control table (gControlTable, D_801118E4, u32[26]): a player's ten controls are option bytes, and SetControls
    # (drone_target_update) makes each the mask D_801118E4[option] that the car's readers test against the game
    # buttons (func_800E681C's group). Zero gives no gas, brake, shift or fire. The readers take a value of 0x13 to
    # 0x19 as an analog source (0x13 and 0x19 the stick, func_800E627C, func_800E6460) and anything else as game
    # button bits; the default controls (SetDefaultControls, func_800C77C4: gas 1, brake 2, steering 0x13, ...) are
    # set by code. So option k < 19 is game button k (the 19 bits of the button map), and 19-25 are those analog
    # codes themselves. With the button map above, gas is A (bit 1), brake B (bit 2), steering the stick.
    for k in range(26):
        v = 1 << k if k < 19 else k
        c.set("game", "D_801118E4", "u32", v, "the control table: option k < 19 is game button k, 19-25 the analog "
              "codes" if k == 0 else "(same)", index=k)

    # The main menu's options (validOption, D_803B65E4[10], image A, s8; optionValid@startscr.c): updateStartScreen
    # skips to a valid one with `while (!optionValid(i)) i++`, which never ends when none is. Five, each a screen:
    # 0 the race (the player screen), 1 0x2000, 2 0x200, 3 0x800, 4 0x1000; 5 and 8 do nothing, 6 and 7 are the
    # cheats' and the unlocked levels' (the code decides those), 9 quits.
    c.set_all("ovl_a", "D_803B65E4", "u8", [1, 1, 1, 1, 1, 0, 0, 0, 0, 0],
              "the main menu's valid options: the five screens 0-4")

    # The options screen's valid items (validOption, D_80116D14, s32[14]; optionscr_buttons): options_initScreen
    # starts at the first valid one with `while (!valid[i]) i++`, which never ends when none is. All fourteen.
    c.set_all("game", "D_80116D14", "u32", [1] * 14, "the options screen's valid items: all fourteen")

    # The songs' names (gSongNames, D_80110030, char *[12]): the audio screen copies each (func_800BE6A4), so NULL
    # faults.
    # They follow the four sfx groups' table (D_8011F0A0, Instrument4[4]), as the game image has its strings after
    # some tables.
    songs = c.strings_after("D_8011F0A0", 4 * 4, [f"SONG {i + 1}" for i in range(12)], "a song's name")
    for i in range(12):
        c.set("game", "D_80110030", "ptr", songs[i], "a song's name" if i == 0 else "(same)", index=i)

    # The mode screen's table of which of its 7 modes each player count may pick (D_803B4679[players][mode], image
    # A, s8; optionValid@modescr.c). updateModeScreen skips to a valid mode with `while (!optionValid(i)) i++`, which
    # never ends when none is. Rows 1-4 (0x803B4680-0x803B469B) are bytes no declaration covers; row 0 is never read.
    for players in range(1, 5):
        c.set_bytes("ovl_a", c.syms.addr("D_803B4679") + 7 * players, bytes([1] * 7),
                    f"modes valid for {players} player(s): all seven")

    # The track select's map objects (image A, trackMapObjNames D_803B7738[20], char *): it saves each map object's
    # first LOD, puts its own map display list there, and puts the saved one back on the way out, so they must be
    # different models (one model for all of them gets back a freed list). Each has a name of its own, which the
    # front-end WAD has a model for.
    for i, name in enumerate(MAP_MODELS):
        c.set("ovl_a", "D_803B7738", "ptr", c.string(name), f"track map object {i}", index=i)

    # The car select's name tables (game data, char *): textures for the car features (D_8011B3A0[24], D_8011B404[10]),
    # the cars' sheens (D_8011B438[11], sfx_stop: once the car's parts are loaded and its model is made, with the
    # retrace clock running) and a model (D_8011B400[1]). MBOX_FindTexture copies its name before the lookup, so a NULL
    # there faults (on the N64 too: address 0 is unmapped). Each is the empty name, which every texture WAD here has
    # a texture for; MBOX_FindModel takes "" as AAANULLOBJ.
    empty = c.string("")
    # The car select's first blit (D_803B2AB8, image A, NewMultiBlit's 38): the empty texture name, kept after the
    # warning screen's blit (D_803B83A4, image A: one MultiBlit) as image A keeps its strings after some tables.
    c.set("ovl_a", "D_803B2AB8", "ptr", c.strings_after("D_803B83A4", 36, [""], "a car select blit's name", "ovl_a")[0],
          "the car select's first blit: the empty texture name")
    for sym, n in (("D_8011B3A0", 24), ("D_8011B400", 1), ("D_8011B404", 10), ("D_8011B438", 11)):
        for i in range(n):
            c.set("game", sym, "ptr", empty, "a car select name: the empty one" if i == 0 else "(same)", index=i)

    # The front end's WADs (ASSETS.md 2.3): the locked one (54, "attract") has every texture and model the code
    # names; the others (58, 59 locked; the screens 55, 56, 57, 60) are empty but valid.
    c.file(54, texture_wad(CODE_TEXTURES, CODE_MODELS + MAP_MODELS),
           "front-end WAD: the textures and models the code names", model=True)
    for w in (55, 56, 57, 58, 59, 60):
        c.file(w, empty_wad(), "front-end WAD: empty", model=True)
    # Paths (forward 158-176, backward 177-182): the track select's maps read 158-163, a race its track's.
    for w in range(158, 183):
        c.file(w, path_file(), f"path {w - 158}: a square loop")


def race(c):
    """A race on an empty track (SYNTH.md 2): LoadTrack's files, the cars' physics, and the tables race setup reads."""
    # Each track's WAD (101-119) has the world node's model; its world (120-138) is one node, its road (139-157)
    # one empty collision node. The race, HUD and car WADs (61-68, 76-100) have the textures and models the code
    # names. gWorldNames (D_8011A924, char *[19]) names every track's world.
    for t in range(19):
        c.set("game", "D_8011A924", "ptr", c.string(WORLD_NAME), f"gWorldNames: track {t}'s world", index=t)
        c.file(101 + t, texture_wad(CODE_TEXTURES, CODE_MODELS + [WORLD_NODE]), f"track {t} WAD", model=True)
        icons = ARENA_ICONS if t in BATTLE_ARENAS else ()
        c.file(120 + t, world_wad(icons), f"track {t} world: one node" + (f" and {len(icons)} weapon icons" if icons
                                                                           else ""))
        c.file(139 + t, road_file(), f"track {t} road: flat pavement, one surface")
    for w in list(range(61, 69)) + list(range(76, 101)):
        c.file(w, texture_wad(CODE_TEXTURES, CODE_MODELS), "race, HUD or car WAD", model=True)

    # The target info table (D_80117530, 122 records of 0x30, docs/port/POINTERS.md 10): a world node whose name
    # starts with a target's name becomes that target. The names are read with strlen, so NULL faults, and an empty
    # name is a prefix of every node: each target has a name of its own that no node has.
    for i in range(1, 122):
        c.set("game", "D_80117530", "ptr", c.string(f"TARGET{i:03}"), "a target's name" if i == 1 else "(same)",
              offset=0x30 * i)
    # Target 0 is the battle arenas' weapon icon (MAT3's _StartWeaponIcon, nonmatching/game/StartWeaponIcon.c): a
    # world node named WEAPONnn becomes one (transmission_ratio_get). Its flags: 0x20 (made without the Expansion
    # Pak), 8 (a record of its category), 0x4000 (its start function runs when it is made: StartWeaponIcon's set-up
    # hides it, marks it 0x20 and counts one more power-up, which battle mode's power-up slots, func_8039133C, are
    # made from; UpdateBattleHealth picks one of them, and with none it spins for ever). Category 5, the battle
    # slots; shape 350, the first weapon (StartWeaponIcon's switch, and the object definition D_801427C0[350]); no
    # model file (AAANULLOBJ).
    c.set("game", "D_80117530", "ptr", c.string("WEAPON"), "target 0: the weapon icon's name")
    c.func_ptr("game", "D_80117530", 2, "StartWeaponIcon", "target 0's start function")
    c.set("game", "D_80117530", "u16", 0x4028, "target 0's flags: start function, category record, no Expansion Pak "
          "needed", offset=0x12)
    c.set("game", "D_80117530", "u16", 350, "target 0's shape: the first weapon", offset=0x14)
    c.set("game", "D_80117530", "u8", 5, "target 0's category: battle's power-up slots", offset=0x16)
    # The weapons' ammunition (ammoCnts, D_80121D60, s32[8]): what a pickup gives. Five shots each.
    c.set_all("game", "D_80121D60", "u32", [5] * 8, "a weapon's ammunition: five")
    # The collision sizes per body type (collsize, D_8011F844: front, rear, side, height, f32), which the car-car
    # collision and the shield weapon read: the tires sit 4 ahead and behind and 2.5 aside, so a 9 x 5 x 3 box.
    for body in range(13):
        for k, v in enumerate((4.5, 4.5, 2.5, 3.0)):
            c.set("game", "D_8011F844", "f32", v, "a body's collision box: front, rear, side, height" if body == k == 0
                  else "(same)", offset=16 * body + 4 * k)

    # The arenas' six puff textures (D_80117480, char *[6]): physics_collision_test looks each up by name on the
    # tracks from 6 on (the stunt and battle arenas), and MBOX_FindTexture copies the name first. The empty name.
    for i in range(6):
        c.set("game", "D_80117480", "ptr", c.string(""), "an arena's puff texture: the empty name" if i == 0
              else "(same)", index=i)

    # The level objects' record size per category (D_80117510, u8[8]): engine_sound_update allocates a heap block of
    # size x count for each category with objects. Battle mode always has 16 of category 5 (camera_lerp_position),
    # its power-up slots, and writes all 16 136-byte records (Slot136): a size of 0 is a zero-size block, and the
    # writes go over the next blocks' headers.
    c.set("game", "D_80117510", "u8", 136, "category 5's record size (battle's power-up slots, Slot136)", index=5)

    battle(c)

    # The drones' update schedule (drone_sched, D_80120E74, Row3[12] of s16; update_drone_models): row r gives how
    # many drones to update each model tick, ended by -1, where the walk goes back to the row's start. The race uses
    # row n for n drones, n + 7 with two humans or more (world_physics_tick). With zeros it never meets -1 and walks
    # on through the tables after it, updating whatever cars their values say (gRace 2's car with no time step:
    # NaN); a row asking for more drones than there are updates the players' cars, whose time step is then 0 (NaN in
    # the 3- and 4-player races). Every row: all its drones every tick.
    for r in range(12):
        for k, v in enumerate((r if r < 7 else r - 7, -1, -1)):
            c.set("game", "D_80120E74", "u16", v, "the drone schedule: row n and n + 7 are {n, -1}" if r == k == 0
                  else "(same)", offset=6 * r + 2 * k)

    # The time limit of a gRace 2 race on tracks 0-5 (D_80111754, f32[6]; func_800FBED8, 600 s for the others): the
    # race ends when it runs out (countdown's last branch), then the results (0x2000000, 4 s) and the game over
    # screen (0x800000). Zero ended the race in its first frame. 15 s.
    c.set_all("game", "D_80111754", "f32", [15.0] * 6, "a gRace 2 race's time limit, s")

    # The demo path start points per track (D_801173D8, s16 *[12], with their counts D_80117408[12]): one start
    # point, the path's first (0, so its byte order doesn't matter).
    first_point = c.blob(be16(0) + be16(0), "a demo path start list: point 0")
    for t in range(12):
        c.set("game", "D_801173D8", "ptr", first_point, "a track's demo path start points" if t == 0 else "(same)",
              index=t)
        c.set("game", "D_80117408", "u16", 1, "one start point" if t == 0 else "(same)", index=t)

    # The cars' physics descriptors (D_80110D08, Car *[13], docs/port/CARS.md): 0xB4-byte records of words, which
    # the research puts at 0x801108D0.. inside D_801108C8 (f32[], so the loader swaps them as words). One descriptor
    # for every car type: zero but for a mass, an engine inertia, four tire curves and the four tires' positions (x
    # forward: the wheelbase is front x - rear x). The tire curves are the two six-float records after the torque
    # curve, D_8011F814[2][6] (front, rear: func_800D09E8 copies six), which only these u32 fields reach: declared in
    # car_resistance for the PC, so the loader swaps them as floats (BRINGUP.md 3). Every value 1.
    base = c.syms.addr("D_801108C8")
    desc, susp = 0x801108D0, 0x80110A00
    curves = c.syms.addr("D_8011F814")
    for i in range(12):
        c.set("game", "D_8011F814", "f32", 1.0, "the tire curves (front, rear): every value 1" if i == 0 else "(same)",
              index=i)
    c.set("game", "D_801108C8", "f32", 3000.0, "car descriptor: mass", offset=desc + 80 - base)
    c.set("game", "D_801108C8", "f32", 1.0, "car descriptor: engine inertia", offset=desc + 84 - base)
    for i in range(4):
        c.set("game", "D_801108C8", "u32", curves + 24 * (i // 2),
              f"car descriptor: tire {i}'s curve (a u32 window address)", offset=desc + 96 + 4 * i - base, elem="f32")
    for i, pos in enumerate(((4.0, 0.0, 2.5), (4.0, 0.0, -2.5), (-4.0, 0.0, 2.5), (-4.0, 0.0, -2.5))):
        for k, v in enumerate(pos):
            c.set("game", "D_801108C8", "f32", v, f"car descriptor: tire {i}'s position",
                  offset=desc + 120 + 12 * i + 4 * k - base)
    for t in range(13):
        c.set("game", "D_80110D08", "ptr", desc, "a car type's physics descriptor" if t == 0 else "(same)", index=t)
    # The suspensions (carSuspensions, D_80111560, const _tSusp *[3]): nine f32 each (forward, back, side: force
    # minimum, force range, angle), in the same f32 table past the descriptor. One suspension, all three entries:
    # no tilt until 1000 units of force, then up to 0.1 rad over another 1000.
    for k, v in enumerate((1000.0, 1000.0, 0.1, -1000.0, 1000.0, -0.1, 1000.0, 1000.0, 0.1)):
        c.set("game", "D_801108C8", "f32", v, "a suspension" if k == 0 else "(same)", offset=susp + 4 * k - base)
    for i in range(3):
        c.set("game", "D_80111560", "ptr", susp, "carSuspensions: the one suspension" if i == 0 else "(same)",
              index=i)

    # The car types' names (D_80110D3C, char *[13]), which sfx_volume_set puts in front of each car part's name:
    # after the shadow scales (D_8011F914, f32[14][4]: AnimateShadow's per body type, 13 with the appearance bit).
    names = c.strings_after("D_8011F914", 14 * 16, [f"CAR{t}" for t in range(13)], "a car type's name")
    for t in range(13):
        c.set("game", "D_80110D3C", "ptr", names[t], "a car type's name" if t == 0 else "(same)", index=t)

    car_tables(c)
    hud(c)
    blits(c)

    # The tracks' car colours (gTrackColors, D_8011AF90, tRGBA *[19]): each points at RGBA byte quadruples, which
    # the cars' IR colour comes from. Sixteen mid greys per track (bytes, so copied as they are).
    colours = c.blob(bytes([0x80, 0x80, 0x80, 0xFF]) * 16, "a track's car colours: sixteen greys")
    for t in range(19):
        c.set("game", "D_8011AF90", "ptr", colours, "gTrackColors: a track's colours" if t == 0 else "(same)",
              index=t)


def car_tables(c):
    """The car physics' tables, which the race start (track_info_display, display_list_flush) turns into each car's
    model record. With zeros the gravity is 0, so a car's mass is 0 and the model thread's first step divides 0 by
    0 (func_800E1540): every car's position is NaN from the first frame. Units are the model's: feet, seconds,
    pounds (its gravity is 32.2)."""
    # PrePlay sets gravity to 32.2 x D_801202F8[the gravity option]: the option's factor, 1 for each of the nine.
    c.set_all("game", "D_801202F8", "f32", [1.0] * 9, "the gravity option's factor (32.2 ft/s^2 x): 1 for each")
    # func_800D0B14, a car's mass in slugs (its weight is that x gravity): (D_80110DD8[type] + (D_80111274[mass
    # option] - D_80124154 + the pak's mass bonus) x D_80124150) x the tune's factor. 3000 lb is 93 slugs; the mass
    # options add nothing; one slug a unit.
    c.set_all("game", "D_80110DD8", "f32", [93.0] * 13, "a car type's mass, in slugs (3000 lb)")
    c.set_all("game", "D_80111274", "f32", [0.0] * 6, "a mass option's difference: none")
    c.set("game", "D_80124150", "f32", 1.0, "slugs per unit of mass difference")
    # A car type's weight in pounds (D_80110E0C, f32[14]; + 1000 lb per unit of mass difference), the model's +0x5C4,
    # which the car-car collision (menu_options_screen) shares the push out by: 0 for both is 0 / 0.
    c.set_all("game", "D_80110E0C", "f32", [3000.0] * 14, "a car type's weight, in pounds")
    # Two cars that overlap are pushed apart along the line between them with force D_80124104 (menu_load_options),
    # or along x with D_80124100 when they are closer than sqrt(D_801240FC): 0 / 0 when they coincide, as the six
    # cars do at the start here. The push is the size of the collision response's own (menu_options_screen: 2000).
    c.set("game", "D_801240FC", "f32", 1e-4, "car push: a squared distance it takes for none")
    c.set("game", "D_80124100", "f32", 2000.0, "car push: the force when the cars coincide")
    c.set("game", "D_80124104", "f32", 2000.0, "car push: the force along the line between them")
    # A car type's brake factor (D_80110DA4, f32[13]): track_info_display's brake gains are mass x the tire's
    # brake value x this, so zero is a car without brakes. 1.
    c.set_all("game", "D_80110DA4", "f32", [1.0] * 13, "a car type's brake factor: 1")
    # The car tunes (D_801116D0, three Tune44 of eleven f32): +4 is the mass factor; 1 in each, the rest 1 too.
    for t in range(3):
        for k in range(11):
            c.set("game", "D_801116D0", "f32", 1.0, "a tune's eleven factors: 1 (+4: the mass factor)" if t == k == 0
                  else "(same)", offset=44 * t + 4 * k)


def multiblit(c, sym, index, texname, x, y, w, h, func=None):
    """MultiBlit sym[index] (36 bytes: char *texname; s16 dulx, duly, width, height, top, bot, left, right; u32
    zdepth, alpha; animfunc; u32 animid): a blit of the texture's first 16 x 16 texels at (x, y), w x h, opaque, or
    with texname -1 and func, a sprite whose draw function func is (NewMultiBlit)."""
    base = 36 * index
    name = 0xFFFFFFFF if texname is None else c.string(texname)
    c.set("game", sym, "ptr", name, f"{sym}[{index}]: {texname or 'a sprite'}", offset=base)
    for k, v in enumerate((x, y, w, h, 0, 15, 0, 15)):
        c.set("game", sym, "u16", v, "(same)", offset=base + 4 + 2 * k)
    c.set("game", sym, "u32", 1, "(same)", offset=base + 0x14)                 # zdepth
    c.set("game", sym, "u32", 255, "(same)", offset=base + 0x18)               # alpha
    if func:
        c.set("game", sym, "ptr", c.syms.func(func), f"(same): {func}", offset=base + 0x1C)


def hud(c):
    """The race HUD (ShowHUD, skid_mark_render: NewMultiBlit of the HUD set D_801158DC[8] and the race set
    D_80114C58[89]). With zeros every blit has no texture and no size. Two of the HUD set's eight: a 48 x 12 panel
    of the BUTTON texture top left, and the digital speedometer (animSpeedText, which the 2D sprite pass calls every
    frame: gShowSpeed is option 5, 1 by default), whose digits go at speedPos (D_80115A68, s32 [windows][slot][2])."""
    multiblit(c, "D_801158DC", 0, "BUTTON", 8, 8, 48, 12)
    multiblit(c, "D_801158DC", 1, None, 0, 0, 0, 0, func="animSpeedText")
    corners = {1: [(300, 200)], 2: [(300, 95), (300, 205)], 3: [(140, 95), (300, 95), (140, 205)],
               4: [(140, 95), (300, 95), (140, 205), (300, 205)]}
    for windows, slots in corners.items():
        for slot, (x, y) in enumerate(slots):
            for k, v in enumerate((x, y)):
                c.set("game", "D_80115A68", "u32", v, "speedPos: the speedometer's place per view" if windows == 1
                      and slot == k == 0 else "(same)", offset=32 * (windows - 1) + 8 * slot + 4 * k)


def blits(c):
    """NewMultiBlit's descriptor tables that game C declared without their layout (s32, u8[]), which the PC declares
    as MultiBlits now (struct pc_multiblit, BRINGUP.md 1): with a texture name in each, a view without the pointer
    would read the name from the wrong bytes. The attract demo's blit, made before the race's files are loaded: the
    name -1, a blit without a texture (the sign-extended sentinel only matches with the pointer in place: read as
    words it is 0xFFFFFFFF and the next field, the address the demo faulted at). The game over title, the controls
    screens' first buttons (front end, paused alone, paused with others) and the menu backgrounds (createBG's two
    blits, front end and track select): BUTTON blits."""
    multiblit(c, "D_80114128", 0, None, 8, 8, 48, 12)
    multiblit(c, "D_80114700", 0, "BUTTON", 136, 20, 48, 12)
    for sym in ("D_80112AB8", "D_8011361C", "D_80113910"):
        multiblit(c, sym, 0, "BUTTON", 16, 16, 48, 12)
    for sym in ("D_801141D4", "D_8011421C"):
        for i in range(2):
            multiblit(c, sym, i, "BUTTON", 8 + 160 * i, 200, 48, 12)


def battle(c):
    """What battle mode (gRace 6, image B, "Extra") reads at its race start (SYNTH.md 2)."""
    # The weapons' texture names (image B: D_80394330[5], D_80394344[5], char *): func_803908D0 looks each up with
    # MBOX_FindTexture, which copies the name first (NULL faults). The empty name. Its model names (D_803942EC[10],
    # D_80394314[7]) stay NULL: MBOX_FindModel takes those as AAANULLOBJ.
    empty = c.string("")
    for sym in ("D_80394330", "D_80394344"):
        for i in range(5):
            c.set("ovl_b", sym, "ptr", empty, "a weapon texture: the empty name" if i == 0 else "(same)", index=i)


SFX_GROUPS = [12, 13, 14, 15]
BATTLE_ARENAS = range(6, 14)   # battle mode's track select starts at track 6, stunt mode's at 14 (the arenas: 6 on)
ARENA_ICONS = [(f"WEAPON{i:02}", (x, 0.0, z)) for i, (x, z) in
               enumerate(((500.0, 500.0), (1500.0, 500.0), (1500.0, 1500.0), (500.0, 1500.0)))]
MAP_MODELS = [f"MAPOBJ{i:02}" for i in range(20)]


def make(syms):
    c = Cart(syms)
    boot_and_sound(c)
    math_constants(c)
    display(c)
    front_end(c)
    race(c)
    return c


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--out", default=str(ROOT / "build/synth/rush2049_synth.z64"))
    ap.add_argument("--data-symbols", default=str(ROOT / "build/pc/pc-data/data_symbols.txt"),
                    help="the build's data report: every symbol's address and element kinds")
    ap.add_argument("--report", help="where to list every value and file (default: OUT with .txt)")
    args = ap.parse_args()
    syms = Symbols(args.data_symbols)
    if not syms.checked:
        raise SystemExit(f"synth_rom: {args.data_symbols} not found: build the PC port first (its data report gives "
                         "every symbol's address and element kinds)")
    c = make(syms)
    rom = c.build()
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(rom)
    rep = Path(args.report) if args.report else out.with_suffix(".txt")
    rep.write_text(c.report())
    print(f"synth_rom: {out} ({len(rom)} bytes, {len(c.log)} values, {len(c.files)} files); report {rep}")


if __name__ == "__main__":
    main()
