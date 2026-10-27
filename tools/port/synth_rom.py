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
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import n64syms  # noqa: E402

ROOT = n64syms.ROOT
ROM_SIZE = 0xC00000
FILES_ROM = 0x100000        # the file table's files go from here (below the game image's slot at 0xB0CB10)
N_WAVES = 183               # docs/port/ASSETS.md 2.2

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
        elif w == 1:
            data = bytes([value & 0xFF])
        elif w == 2:
            data = be16(value)
        else:
            data = be32(value)
        self._put(image, addr, data, elem or kind, f"{sym}[{index}]+{offset}")
        self.log.append(f"{image:5} 0x{addr:08X} {sym}[{index}]{'+%d' % offset if offset else ''} {kind} = "
                        f"{value if kind == 'f32' else hex(value)}: {why}")

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


def obhd(name, dl, vtx):
    """An OBHD model record, 0x58 bytes: one LOD whose display list and vertices are file-relative (SetupWaveModel
    adds the file's address); the other three LODs zero."""
    b = Buf()
    b.raw(name16(name))
    b.f32(100.0)                # radius
    b.u16(0)                    # kind
    b.u16(1)                    # lodCount
    b.u16(0)                    # lod 0: texidx
    b.u16(0)                    #        flags
    b.f32(1e6)                  #        lodRad
    b.u32(dl)                   #        displayList
    b.u32(vtx)                  #        vtxList
    b.raw(bytes(0x30))          # lods 1-3
    return b.b


def model_objs():
    """OBJS: a display list that ends at once (G_ENDDL), then one Vtx (16 bytes) for the vertex list."""
    return be32(0xDF000000) + be32(0) + bytes(16)


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
    RGBA16 image in IMAG, models (OBHD, sorted the same way for MBOX_FindModel) that draw nothing, and palettes
    (PLHD: the car paint looks its palette up by a name from a format string, which is "" here) of 256 greys."""
    image = be16(0x7BDF) * (TEX_W * TEX_H)      # a light grey, opaque (5-5-5-1)
    palette = be16(0x7BDF) * 256
    names = sorted(set(names), key=lambda n: n.encode())
    models = sorted(set(models), key=lambda n: n.encode())
    palettes = sorted(set(palettes), key=lambda n: n.encode())
    recs = b"".join(txhd(n, TEX_W, TEX_H, 2, 0, 0) for n in names)
    chunks = [("OBJS", model_objs(), None)]
    if models:
        chunks.append(("OBHD", b"".join(obhd(m, WAD_OBJS, WAD_OBJS + 8) for m in models), len(models)))
    chunks.append(("TXHD", recs, len(names)))
    if palettes:
        chunks.append(("PLHD", b"".join(plhd(n, len(image)) for n in palettes), len(palettes)))
    chunks.append(("IMAG", image + palette, None))
    return wad(chunks)


def empty_wad():
    """A WAD with no chunks: every MBGetFromWad lookup finds nothing (count 0)."""
    return wad([])


SQUARE = 2000                  # a path's track: a square loop this many units a side, at y = 0


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


def world_wad():
    """A world WAD (ASSETS.md 3.4, port/assets/world.c): WHDR names one world, whose root is the one WOBJ node: no
    sibling or child, an identity transform, its model named TRACK (in the track WAD), and a zero bound; GDAT one
    quadtree node, a leaf with nothing in it; GTLD no target keys."""
    node = Buf()
    node.raw(name16(WORLD_NODE))
    for v in (1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0):   # mat[12]: rotation, then translation
        node.f32(float(v))
    node.u32(0)                 # dlflags
    node.u16(0xFFFF)            # next: none
    node.u16(0xFFFF)            # child: none
    node.u16(0)
    node.u16(0)                 # sfx1, sfx2
    node.u32(0)                 # tidx
    for v in (0, 0, 0, 0, 0, 0):
        node.f32(float(v))      # min[3], max[3]
    assert len(node) == 0x68
    whdr_at = WAD_OBJS          # chunks are laid out in order from just after the header
    whdr = Buf()
    whdr.u32(1)
    whdr.u32(0)                 # woff: the WOBJ node's file offset, set below
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
    w = wad([("WHDR", whdr.b, None), ("WOBJ", node.b, 1), ("GDAT", gdat.b, 1), ("GTLD", b"", 0)])
    assert w[wobj_at:wobj_at + 16] == name16(WORLD_NODE)
    return w


def road_file():
    """A road file (ASSETS.md 3.5): no path records, one collision tree node, a leaf with no surfaces in it (the
    lookup, handbrake_apply, takes a quarter's child as a subnode if the flags say so, else as an offset into the
    surface lists, 0 for none), and no surfaces, vertices or save records."""
    b = Buf()
    for v in (0, 1, 0, 0, 0, 0):    # pnum, tnum, snum, vnum, savenum, vlnum
        b.u16(v)
    b.u32(0)                        # slnum
    b.u16(0xFFFF)                   # tree: parent none
    b.u8(0)
    b.u8(0)
    for v in (-SQUARE, SQUARE, -SQUARE, SQUARE):
        b.u16(v)                    # minh, maxh, minv, maxv
    for _ in range(4):
        b.u16(0)                    # a leaf (flags 0), and no surface list in any quarter (offset 0)
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

    # The car select's name tables (game data, char *): textures for the car features (D_8011B3A0[24], D_8011B404[10])
    # and a model (D_8011B400[1]). MBOX_FindTexture copies its name before the lookup, so a NULL there faults (on
    # the N64 too: address 0 is unmapped). Each is the empty name, which every texture WAD here has a texture for;
    # MBOX_FindModel takes "" as AAANULLOBJ.
    empty = c.string("")
    for sym, n in (("D_8011B3A0", 24), ("D_8011B400", 1), ("D_8011B404", 10)):
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
        c.file(120 + t, world_wad(), f"track {t} world: one node")
        c.file(139 + t, road_file(), f"track {t} road: one empty collision node")
    for w in list(range(61, 69)) + list(range(76, 101)):
        c.file(w, texture_wad(CODE_TEXTURES, CODE_MODELS), "race, HUD or car WAD", model=True)

    # The target info table (D_80117530, 122 records of 0x30, docs/port/POINTERS.md 10): a world node whose name
    # starts with a target's name becomes that target. The names are read with strlen, so NULL faults, and an empty
    # name is a prefix of every node: each target has a name of its own that no node has.
    for i in range(122):
        c.set("game", "D_80117530", "ptr", c.string(f"TARGET{i:03}"), "a target's name" if i == 0 else "(same)",
              offset=0x30 * i)

    # The demo path start points per track (D_801173D8, s16 *[12], with their counts D_80117408[12]): one start
    # point, the path's first (0, so its byte order doesn't matter).
    first_point = c.blob(be16(0) + be16(0), "a demo path start list: point 0")
    for t in range(12):
        c.set("game", "D_801173D8", "ptr", first_point, "a track's demo path start points" if t == 0 else "(same)",
              index=t)
        c.set("game", "D_80117408", "u16", 1, "one start point" if t == 0 else "(same)", index=t)

    # The cars' physics descriptors (D_80110D08, Car *[13], docs/port/CARS.md): 0xB4-byte records of words, which
    # the research puts at 0x801108D0.. inside D_801108C8 (f32[], so the loader swaps them as words). One descriptor
    # for every car type: zero but for a mass, an engine inertia, four tire curves (all one 23-word record, right
    # after it) and the four tires' positions (x forward: the wheelbase is front x - rear x).
    base = c.syms.addr("D_801108C8")
    desc, curve, susp = 0x801108D0, 0x801108D0 + 0xB4, 0x80110A00
    for i in range(23):
        c.set("game", "D_801108C8", "f32", 1.0, "a tire curve: every value 1" if i == 0 else "(same)",
              offset=curve + 4 * i - base)
    c.set("game", "D_801108C8", "f32", 3000.0, "car descriptor: mass", offset=desc + 80 - base)
    c.set("game", "D_801108C8", "f32", 1.0, "car descriptor: engine inertia", offset=desc + 84 - base)
    for i in range(4):
        c.set("game", "D_801108C8", "u32", curve, f"car descriptor: tire {i}'s curve (a u32 window address)",
              offset=desc + 96 + 4 * i - base, elem="f32")
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

    # The tracks' car colours (gTrackColors, D_8011AF90, tRGBA *[19]): each points at RGBA byte quadruples, which
    # the cars' IR colour comes from. Sixteen mid greys per track (bytes, so copied as they are).
    colours = c.blob(bytes([0x80, 0x80, 0x80, 0xFF]) * 16, "a track's car colours: sixteen greys")
    for t in range(19):
        c.set("game", "D_8011AF90", "ptr", colours, "gTrackColors: a track's colours" if t == 0 else "(same)",
              index=t)


SFX_GROUPS = [12, 13, 14, 15]
MAP_MODELS = [f"MAPOBJ{i:02}" for i in range(20)]


def make(syms):
    c = Cart(syms)
    boot_and_sound(c)
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
