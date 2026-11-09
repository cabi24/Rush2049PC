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
        self.rom_bytes = []     # (ROM offset, bytes, what): raw ROM the game reads by address (the samples)

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

    def rom_at(self, offset, data, what):
        """Bytes at a ROM offset the game reads by its own address (MusyX's samples, from ROM 0x39B40): outside every
        image's slot and the files."""
        for im in self.images:
            if offset < im.rom_off + im.rom_len and im.rom_off < offset + len(data):
                raise SystemExit(f"synth_rom: {what} at ROM 0x{offset:X} overlaps image {im.name}")
        if offset + len(data) > FILES_ROM:
            raise SystemExit(f"synth_rom: {what} at ROM 0x{offset:X} runs into the files")
        self.rom_bytes.append((offset, bytes(data), what))

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
            self._put(game, self.syms.addr("wave_rom_offsets") + 4 * wave, be32(off), "u32", "file table offset")
            self._put("boot", self.syms.addr("gWaveSizes") + 4 * wave, be32(size), "u32", "file table size")
            self._put(game, self.syms.addr("wave_storage_kinds") + 4 * wave, be32(0), "u32", "file table kind")
            self._put(game, self.syms.addr("wave_is_model") + wave, bytes([1 if wave in self.model_waves else 0]),
                      "u8", "file table model flag")
        self.place = place

        for off, data, what in self.rom_bytes:
            self.rom[off:off + len(data)] = data
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
        for off, data, what in self.rom_bytes:
            out.append(f"rom 0x{off:06X} {len(data):6} {what}")
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


# ---------------------------------------------------------------- the sound bank (--sounds)
#
# For tests of the sound's voices (docs/port/AUDIO.md 10): the default cartridge's project is empty, so every sound the
# game starts is refused (MusyX knows no such sfx) and no voice ever plays. With --sounds, sfx group 12 has an sfx for
# each of the game's sound indices (0-227: gEffectLoop's extent; the sound manager starts sfx `index`), each a macro on
# one of two ADPCM samples: a loop for the sounds the game loops (gEffectLoop, set here: the engines' two layers and the
# screech) and a short one-shot for the rest; and the engine table gives every car its two layers, with levels.
SOUND_IDS = 228
SOUND_LOOPS = (1, 2, 36)        # the engines' layers (the engine table below) and the screech (UpdateCarSounds)
SAMPLE_BASE_ROM = 0x39B40       # MusyX's sample base: AudioInitSys's D_00039B40
LOOP_SAMPLE, SHOT_SAMPLE = 5, 6
LOOP_MACRO, SHOT_MACRO = 1, 2


def adpcm_square(samples, half, level):
    """ADPCM (MUSYX_DATA.md 3.3, AUDIO.md 2) of a square wave: a 0x100-byte codebook of zeros (no prediction), then
    40-byte blocks of two 32-sample frames: each frame's first two samples as big-endian s16, then its header (book 0,
    shift 2: a nibble n is n * 1024) and 15 bytes of nibbles for the other 30. samples: a multiple of 64."""
    out = bytearray(0x100)
    n = level // 1024
    def value(i):
        return n if (i // half) % 2 == 0 else -n
    for blk in range(samples // 64):
        frames = []
        for f in range(2):
            first = blk * 64 + f * 32
            nib = bytearray()
            for k in range(2, 32, 2):
                a, b = value(first + k) & 0xF, value(first + k + 1) & 0xF
                nib.append(a << 4 | b)
            frames.append((struct.pack(">hh", value(first) * 1024, value(first + 1) * 1024), bytes([0x02]) + nib))
        out += frames[0][0] + frames[1][0] + frames[0][1] + frames[1][1]
    return bytes(out)


def musyx_sound_project(sfx_groups):
    """The project with sounds: group sfx_groups[0] lists the two macros and samples and has an sfx table entry for
    every sound index {u16 id; u16 macro; u8 max voices (255: no limit of its own); u8 priority; u8 volume; u8 pan;
    u8 key; u8 group; 2 bytes}; the other groups are empty, as musyx_project's."""
    b = Buf()
    for n, gid in enumerate(sfx_groups):
        g = b.u32(0)
        b.u16(gid)
        b.u16(1)
        offs = [b.u32(0) for _ in range(6)]
        empty = b.u16(0xFFFF)
        if n == 0:
            macros = len(b)
            b.u16(LOOP_MACRO), b.u16(SHOT_MACRO), b.u16(0xFFFF)
            samples = len(b)
            b.u16(LOOP_SAMPLE), b.u16(SHOT_SAMPLE), b.u16(0xFFFF)
            b.at32(offs[0], macros - g - 8)
            b.at32(offs[1], samples - g - 8)
            for k in range(2, 5):
                b.at32(offs[k], empty - g - 8)
        else:
            for k in range(5):
                b.at32(offs[k], empty - g - 8)
        b.align(4)
        b.at32(offs[5], len(b) - g - 8)
        if n == 0:
            b.u16(SOUND_IDS)
            b.u16(0)
            for i in range(SOUND_IDS):
                b.u16(i)
                b.u16(LOOP_MACRO if i in SOUND_LOOPS else SHOT_MACRO)
                b.raw(bytes([255, 0x40, 0x7F, 0x40, 60, 0, 0, 0]))
        else:
            b.u16(0)
            b.u16(0)
        b.at32(g, b.align(4) - g)
    b.u32(0xFFFFFFFF)
    return b.b


def musyx_sound_pool():
    """The pool with the two macros (8-byte steps, two u32 words; MUSYX_DATA.md 3.2, macros.c): the loop starts
    its sample, waits until it is keyed off (WAITMS 0xFFFF, the key-off bit) and stops its sample (STOPSAMPLE: the
    driver's voice fades out; END alone would leave a looping sample playing on a voice the library has freed); the
    one-shot starts its sample and waits until the sample has played (the sample-end bit); each then ends, which
    frees the voice."""
    b = Buf()
    offs = [b.u32(0) for _ in range(4)]
    b.at32(offs[0], len(b))
    for mid, sample, wait, stop in ((LOOP_MACRO, LOOP_SAMPLE, 1 << 8, True), (SHOT_MACRO, SHOT_SAMPLE, 1 << 24, False)):
        b.u32(8 + (4 if stop else 3) * 8)
        b.u16(mid)
        b.u16(0)
        b.u32(sample << 8 | 0x10), b.u32(0)          # STARTSAMPLE sample, at 0, mode 0
        b.u32(wait | 0x07), b.u32(0xFFFF << 16)      # WAITMS forever: until key-off, or until the sample ends
        if stop:
            b.u32(0x11), b.u32(0)                    # STOPSAMPLE
        b.u32(0), b.u32(0)                           # END
    b.u32(0xFFFFFFFF)
    for k in range(1, 4):
        b.at32(offs[k], b.u32(0xFFFFFFFF))
    return b.b


def musyx_sound_sdir(loop_len, shot_len, shot_off):
    """The sample directory: the two samples' records {u16 id; u16 refs; u32 offset; u32 address (written); u32 key
    << 24 | rate; u32 format << 24 | samples; u32 loop start; u32 loop length}, ADPCM (format 3), key 60 at 22050 Hz,
    the first looping whole."""
    b = Buf()
    for sid, off, n, loop in ((LOOP_SAMPLE, 0, loop_len, loop_len), (SHOT_SAMPLE, shot_off, shot_len, 0)):
        b.u16(sid), b.u16(0), b.u32(off), b.u32(0), b.u32(60 << 24 | 22050), b.u32(3 << 24 | n)
        b.u32(0), b.u32(loop)
    b.u16(0xFFFF)
    b.raw(bytes(0x1A))
    return b.b


def sound_bank(c):
    """--sounds: the sound bank, the loop flags and the engine table (see SOUND_IDS)."""
    loop_pcm, shot_pcm = 4096, 1024
    loop = adpcm_square(loop_pcm, 24, 6144)
    shot = adpcm_square(shot_pcm, 12, 4096)
    shot_off = (len(loop) + 15) & ~15
    c.rom_at(SAMPLE_BASE_ROM, loop, "the looping sample (ADPCM square wave)")
    c.rom_at(SAMPLE_BASE_ROM + shot_off, shot, "the one-shot sample (ADPCM square wave)")
    c.file(6, musyx_sound_project(SFX_GROUPS), "MusyX project: an sfx for every sound index (--sounds)")
    c.file(7, musyx_sound_pool(), "MusyX pool: a looping and a one-shot macro (--sounds)")
    c.file(8, musyx_sound_sdir(loop_pcm, shot_pcm, shot_off), "MusyX sample directory: two samples (--sounds)")
    for i in SOUND_LOOPS:
        c.set("game", "gEffectLoop", "u8", 1, "a sound the sound manager loops (--sounds)", index=i)
    # The engine table (gEngineSpecs, Definitions64[9]: a car category's two layers, each {s32 sound; u16 rev points
    # r0, r1, b0, b1, b2; 2 bytes; f32 levels p0, p1, p2; 4 bytes}; UpdateEngineSound reads it): layers 1 and 2, the
    # level rising with the revs.
    for cat in range(9):
        for layer in range(2):
            base = 64 * cat + 32 * layer
            c.set("game", "gEngineSpecs", "u32", 1 + layer, "a car's engine layer: its sound (--sounds)" if base == 0
                  else "(same)", offset=base)
            for k, v in enumerate((1000, 4000, 1000, 3000, 6000)):
                c.set("game", "gEngineSpecs", "u16", v, "its rev points" if base == 0 and k == 0 else "(same)",
                      offset=base + 4 + 2 * k)
            for k, v in enumerate((0.6, 0.8, 1.0)):
                c.set("game", "gEngineSpecs", "f32", v, "its levels" if base == 0 and k == 0 else "(same)",
                      offset=base + 16 + 4 * k)


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


# Texture names the game's C looks up with MBOX_FindTexture (MBOX_FindTexture_WORK): string literals in the code, so each
# must be in some loaded WAD or the caller gets NULL. "" stands for every name that comes from initialized data
# (MultiBlit tables, rodata strings), which is zero here.
CODE_TEXTURES = ["", "A1", "B1", "BACKGROUND_SFX", "BUTTON", "BUTTON_SELECT", "BUTTON_TITLE", "CNTDWN3",
                 "CURSOR", "CURSOR_B", "HIT_WALL"]
TEX_W, TEX_H = 16, 16          # every such texture: RGBA16, one solid colour
# Texture names of the cartridge's own, for data it writes on both sides (a name in data and its texture), with their
# (size, format) where it isn't RGBA16: the track select's fog blit (its name is image A's track_fog_texture_name, char[4]) and
# battle mode's health bar (BRINGUP.md 7, 10). Each is an 8-bit IA or I texture, the two formats the 2D draw colours
# with the 8-byte colour pair its blit's palette table points to (BlitColors).
SYN_FOG, SYN_BAR = "SYF", "SYN_BAR"
OWN_TEXTURES = [SYN_FOG, SYN_BAR]
TEX_FORMATS = {SYN_FOG: (1, 3), SYN_BAR: (1, 4)}


# Model names the C looks up with MBOX_FindModel (MBOX_FindObject_WORK): "AAANULLOBJ" is what it looks up for a NULL
# or empty name, which every name from initialized data is here; the rest are string literals in the code.
CODE_MODELS = ["AAANULLOBJ", "MINEG1"]
WAD_OBJS = 8                   # the OBJS chunk comes first in these WADs, right after the header


def obhd(name, dl, vtx, radius=100.0, lods=None):
    """An OBHD model record, 0x58 bytes: one LOD whose display list and vertices are file-relative (SetupWaveModel
    adds the file's address); the other three LODs zero. lods: [(display list, vertices, lodRad)] instead, up to four
    (zoid_draw_object: level k is drawn out to its lodRad, the last one's is the model's distance limit, 0 none)."""
    lods = lods or [(dl, vtx, 1e6)]
    b = Buf()
    b.raw(name16(name))
    b.f32(radius)               # radius
    b.u16(0)                    # kind
    b.u16(len(lods))            # lodCount
    for ldl, lvtx, rad in lods:
        b.u16(0)                # lod k: texidx
        b.u16(0)                #        flags: no texture of its own (0x8000), no lighting (0x10)
        b.f32(rad)              #        lodRad
        b.u32(ldl)              #        displayList
        b.u32(lvtx)             #        vtxList
    b.raw(bytes(0x10 * (4 - len(lods))))
    assert len(b) == 0x58
    return b.b


# ---------------------------------------------------------------- display lists (F3DEX2, port/gfx/gfx_gbi.h)

def gfx(w0, w1=0):
    return be32(w0) + be32(w1)


# Each model's list sets the state it draws with, so it doesn't depend on what the game set before it: one-cycle
# mode, the shade colour (G_CC_SHADE in both cycles), an opaque z-buffered surface without blending, no texture, and
# smooth shading with back faces culled, without fog, lighting or texture generation. Then its vertices (G_VTX, w1
# file-relative: the WAD relocator, relocate_model_dl, adds the file's address), its triangles (G_TRI2), G_ENDDL. The
# faces wind clockwise seen from outside in the world's axes: the scene's modelview negates x (zoid_draw_window's
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
    """A Vtx (16 bytes): s16 position, the u16 flags at +6 (bit 15: another vertex follows, which mirror_vertex_texcoords's walk
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
    lists of its own: SetupWaveModel relocates every model's lists in place (relocate_model_dl adds the file's
    address to each address), so a list two models shared would be relocated twice."""
    out, where = b"", {}
    for m in models:
        ground = m == WORLD_NODE
        if m in DETAIL_MODELS:                    # the detail world's (detail_world): a level a box each
            lods = []
            for size, rgba, rad in DETAIL_MODELS[m]:
                verts, dlist = mesh([(q, rgba) for q, _ in box_quads(*size)])
                vtx_off = WAD_OBJS + len(out)
                out += verts
                dl_off = WAD_OBJS + len(out)
                out += dlist(vtx_off)
                lods.append((dl_off, vtx_off, rad))
            where[m] = (lods[0][0], lods[0][1], 100.0, lods)
            continue
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
    recs = b"".join(txhd(n, TEX_W, TEX_H, *TEX_FORMATS.get(n, (2, 0)), 0) for n in names)
    objs, where = model_objs(models)
    chunks = [("OBJS", objs, None)]
    if models:
        chunks.append(("OBHD", b"".join(obhd(m, *where[m][:3], lods=where[m][3] if len(where[m]) > 3 else None)
                                        for m in models), len(models)))
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


def path_file(lap=False):
    """A path file (ASSETS.md 3.6, port/assets/path.c): four checkpoints on the corners of the square, a
    resurrection path of eight points round it, no alternative paths, and four drone paths of eight points.
    With lap, the lap path's checkpoints and resurrection path instead (LAP_CHECKPOINTS, lap_res_points())."""
    b = Buf()
    corners = square_points(4)
    #     startTime loopPt finishPt startPt numPoints pad
    for v in (90, 0, 0, 0, len(LAP_CHECKPOINTS) if lap else len(corners), 0):
        b.u16(v)
    for i in range(10):
        x, y, z = corners[i % len(corners)]
        if lap:
            (x, y, z), flags = LAP_CHECKPOINTS[i] if i < len(LAP_CHECKPOINTS) else ((0, 0, 0), 0)
            for v in (x, y, z, 0.0, 1.0, 0.0):   # pos, normal
                b.f32(v)
            b.u32(100 * 100)                      # radius: squared, an integer (ASSETS.md 3.6)
            b.u16(flags)
        else:
            for v in (x, y, z, 0.0, 1.0, 0.0, 200.0):   # pos, normal, radius
                b.f32(v)
            b.u16(7 if i == 0 else 0)                  # flags: finish, start and loop at checkpoint 0
        b.u16(30)
        b.u16(30)                                  # time[2]
        for k in range(20):
            b.u16(0)                               # pathPts (the game sets them)
        b.u16(0)
        b.f32(float(SQUARE))                       # distToNext
    assert len(b) == 0x32C
    res = square_points(8)
    respath = lap_res_points() if lap else res
    b.u16(len(respath))
    b.u16(0)
    b.u32(0)                                       # pts slot (the game fills it)
    b.u8(0)                                        # no alternative paths
    b.raw(bytes(3))
    b.u32(0)                                       # alt slot
    b.u16(len(respath))                            # resurrection point count
    for p in respath:
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


def world_node(name, pos=(0.0, 0.0, 0.0), next_index=0xFFFF, flags=0, box=(0, 0, 0, 0, 0, 0)):
    """A WOBJ node, 0x68 bytes: its name (a model name, or a target's), an identity rotation and a position, its
    flags (the scene node's, NewWorldObject), its next sibling (0xFFFF none), no child, no sounds or texture
    index, its bound (min[3], max[3] from its place: zero for none; a top-level node's is the world's visibility
    cell, WorldCullObjects, and its scene node's culling box, ZOID_BuildCullingList)."""
    node = Buf()
    node.raw(name16(name))
    for v in (1, 0, 0, 0, 1, 0, 0, 0, 1) + tuple(pos):   # mat[12]: rotation, then translation
        node.f32(float(v))
    node.u32(flags)             # dlflags
    node.u16(next_index)        # next
    node.u16(0xFFFF)            # child: none
    node.u16(0)
    node.u16(0)                 # sfx1, sfx2
    node.u32(0)                 # tidx
    for v in box:
        node.f32(float(v))      # min[3], max[3]
    assert len(node) == 0x68
    return node.b


# The detail world (track 0, docs/port/DETAIL.md 6): what the game's distance limits read, so the draw distance and
# detail settings have something to change. Its root is the ground (TRACK, as every track's) with a bound round the
# loop, the visibility cell the eye is in; then five parts, each a model of its own in the track's WAD:
#   SYN_NEAR     a tower 300 ft outside the loop's first side, about 1,500 ft ahead of the start: the ground's row
#                lets it through
#   SYN_HIDDEN   one inside the loop, about 1,800 ft from the start: not in the ground's row (the N64 hides it), in
#                its own cell's row, which far adds (within 2,000 ft of the eye)
#   SYN_FAR      one 4,000 ft past the loop, 6,000 ft from the start: in no row the eye reaches, past the matrices'
#                2,048 ft and far's 4,096 ft: max alone draws it
#   SYN_LOD      two levels, red out to 200 ft and blue out to 1,200 ft, about 2,000 ft from the start: past its last
#                level (the N64 leaves it out), blue at far, red with detail high
#   SYN_SCENERY  far scenery (flag 0x400000) 21,000 ft out: pulled in to 1,600 ft (build_far_node_matrix), max at its place
# The paths that read it (tools/port/synth/detail*.press) race two seconds from the start, the eye within 100 ft of it.
# The rows (track1Parts, track 0's table): the ground's lets through the ground, NEAR, LOD and SCENERY; each part's
# lets through the ground and itself (HIDDEN's NEAR too).
DETAIL_TRACK = 0   # the track select of this cartridge offers no other (its option 0, the track, isn't valid)
DETAIL_MODELS = {   # name: [(box half-sizes in sixteenths of a foot, colour, lodRad)] a level each
    "SYN_NEAR": [((20 * UNITS, 300 * UNITS, 20 * UNITS), (220, 220, 220, 255), 0.0)],
    "SYN_HIDDEN": [((20 * UNITS, 300 * UNITS, 20 * UNITS), (240, 140, 40, 255), 0.0)],
    "SYN_FAR": [((40 * UNITS, 600 * UNITS, 40 * UNITS), (200, 40, 200, 255), 0.0)],
    "SYN_LOD": [((30 * UNITS, 200 * UNITS, 30 * UNITS), (230, 40, 40, 255), 200.0),
                ((30 * UNITS, 200 * UNITS, 30 * UNITS), (40, 60, 230, 255), 1200.0)],
    "SYN_SCENERY": [((2000 * UNITS, 1500 * UNITS, 2000 * UNITS), (90, 90, 160, 255), 0.0)],
}
DETAIL_PARTS = [   # (name, place, flags, bound), the world's top-level nodes after the ground, and the rows
    ("SYN_NEAR", (1500.0, 0.0, -300.0), 0, (-40, 0, -40, 40, 300, 40)),
    ("SYN_HIDDEN", (1200.0, 0.0, 1300.0), 0, (-40, 0, -40, 40, 300, 40)),
    ("SYN_FAR", (6000.0, 0.0, 1000.0), 0, (-80, 0, -80, 80, 600, 80)),
    ("SYN_LOD", (1800.0, 0.0, -1000.0), 0, (-60, 0, -60, 60, 200, 60)),
    ("SYN_SCENERY", (21000.0, 0.0, 1000.0), 0x400000, (-2000, 0, -2000, 2000, 1500, 2000)),
]
DETAIL_ROWS = [0x33, 0x03, 0x07, 0x09, 0x11, 0x21]   # parts 0-5: the ground, then DETAIL_PARTS
DETAIL_GROUND_BOUND = (-1100, -50, -1100, 1100, 500, 1100)   # round the loop, from the ground's place


def world_wad(targets=(), nodes=None):
    """A world WAD (ASSETS.md 3.4, port/assets/world.c): WHDR names one world, whose root is the WOBJ node TRACK
    (its model is in the track WAD) and its siblings, one per target (name, position): the world walk
    (NewWorldObject) offers each node to the target table (SetupTarget), which makes a level object
    of a node whose name starts with a target's. GDAT one quadtree node, a leaf with nothing in it; GTLD no target
    keys."""
    nodes = nodes or [(WORLD_NODE, GROUND_CENTRE)] + list(targets)
    wobj = b"".join(world_node(n[0], n[1], i + 1 if i + 1 < len(nodes) else 0xFFFF, *n[2:])
                    for i, n in enumerate(nodes))
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
    - the tree node (updownleaf, downleaf): the root (parent -1), bounds in whole feet (h is x, v is z,
      half-open), flags 0 so each quarter's child is a byte offset into the surface lists (0 would be "none"), all
      four the same list;
    - the surface (0x18 bytes): type 0 (roadcode 0, pavement; a wall, trigger or disabled code would be skipped or
      act), its vertex count 4 in the low bits of the second word, an identity 3 x 3 matrix in 2^-14 units (row 1 is
      the up normal: TireSurf's height above the road is that row's dot product with the point less the first
      vertex), and its vertex list at offset 0;
    - the vertices (s16 feet and a word of 1/32 fractions): the first is the corner (-1000, 0, -1000) in the world,
      the others are in the surface's frame from it, (4000, 0, 0), (4000, 0, 4000), (0, 0, 4000), in the order
      PointInSurf's edge tests take as inside;
    - the vertex list (DecodeVlist): the first index as a big-endian u16, then a run byte 0xE0 | 3 for the next
      three, then the u16 path record index the edge handlers read (0);
    - the surface list (DecodeSlist): offset 0 is a pad byte (0 means none), then the count 1 and one word, a run
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
    # MusyX (docs/port/MUSYX_DATA.md 8.6). sound_hw_alloc_dma sizes the sample cache as sound_sample_blocks_per_voice[mode] x voices;
    # zero gives zero-entry caches whose first node's links land in the next heap block's header.
    c.set_all("boot", "sound_sample_blocks_per_voice", "u16", [2] * 6, "sample cache entries per voice, per mode (non-zero)")
    # Its tuning factors scale the voice and DMA buffer counts from the output rate; 1.0 keeps the rate's own.
    for s in ("sound_sfd_margin", "sound_sfd_margin_half_rate", "sound_dma_margin", "sound_dma_margin_half_rate", "sound_sample_cache_factor"):
        c.set("boot", s, "f32", 1.0, "MusyX buffer scale (neutral)")
    # The service table AudioInitSys hands to the library: the eight functions at the end of the game's code,
    # in the order the library calls them (MUSYX_DATA.md 8.1; port/main.c's musyx_services is the same list).
    for i, f in enumerate(("musyx_submit_audio_task", "musyx_wait_audio_task", "musyx_set_retrace_callback", "musyx_clear_retrace_callback",
                           "musyx_start_dma_list", "musyx_cart_address", "musyx_alloc", "musyx_free")):
        c.func_ptr("game", "audio_driver_services", i, f, "MusyX service slot")
    # The sfx groups AudioInitSys pushes (Instrument4 {u16 other, id}); the project has a group for each.
    for j, g in enumerate(SFX_GROUPS):
        c.set("game", "boot_sfx_groups", "u16", g, "sfx group id pushed at boot", offset=4 * j + 2)

    # zlib 1.0.4's extra bits of the bit-length codes (extra_blbits, extra_blbits, int[19]: zlib's own constants), which
    # only static_bl_desc points to: declared in zlib_deflate for the PC, so the loader converts them (BRINGUP.md 13).
    c.set_all("game", "extra_blbits", "u32", [0] * 16 + [2, 3, 7], "zlib's extra_blbits")

    # The boot data's two static display lists (Gfx[], called from every frame's list): a list of zeros is a run of
    # no-ops that walks off its end on the N64 as on the PC, so each is just its end command.
    for s in ("rdp_init_dl", "rsp_init_dl"):
        c.set("boot", s, "u32", 0xDF000000, "G_ENDDL: the static display list ends at once")

    # The game's state machine (groups/state/state.c): gGameState 0 and gNewGameState 1, so the first frame's
    # updateGameState runs initGameState's state 1 (InitWaves, overlay "Start", SetOptions, InitRecords), which
    # moves on to state 2 (the front end's waves). Zero for both is a game with no state, where nothing happens.
    c.set("game", "gNewGameState", "u32", 0x1, "gNewGameState: start in state 1 (initGameState)")
    # gGameState before that: -1, the game's own "no state" (UpdateHUDPostCamera tests gGameState != -1), which has every
    # state's bit, and which the host's --until-state must not take for the one it waits for (BRINGUP.md 6).
    c.set("game", "gGameState", "u32", 0xFFFFFFFF, "gGameState: -1, no state yet (UpdateHUDPostCamera)")
    # gSkipToGameState (gSkipToGameState): a screen whose state isn't this one passes straight on to its next screen,
    # unless it is 1, which every screen writes back once it is reached (and the race start, gameover's
    # "Quit"...). 0 skips the start prompt and the main menu; 1 is "no skip".
    c.set("game", "gSkipToGameState", "u32", 0x1, "gSkipToGameState: 1, no screen skipped")
    # The pak server's port in a prompt (paksys' gErrPak): it scans only that port while one is set, and puts
    # -1 back after each prompt. -1 at start, so the paks on controllers 2-4 are scanned too (0 scans port 1 only).
    c.set("game", "gErrPak", "u8", 0xFF, "the pak server's prompt port: -1, none (every port is scanned)")

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
    sum (-g)^k / (2k + 1), scaled so that D is monic; then N and D share their constant term, as fxatan's one
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

    # tanf (tan_cody_waite): xn = round(x 2/pi); f = (x - xn C1) - xn C2 with C1 + C2 = pi/2 (C1 = 201/128, 8 bits,
    # so xn C1 is exact); f itself below eps, else f (1 + P(g)) / (1 + Q(g)), and -1 over that for odd xn; 0 past ymax.
    p, q = tan_pade()
    c.set("game", "tan_max_arg", "f32", math.pi / 2 * 2 ** 12, "tanf: the largest argument it reduces")
    c.set("game", "tan_two_over_pi", "f32", 2 / math.pi, "tanf: x 2/pi counts the quarter turns")
    c.set("game", "tan_half_pi_hi", "f32", 201 / 128, "tanf: pi/2, its first 8 bits (C1)")
    c.set("game", "tan_half_pi_lo", "f32", math.pi / 2 - 201 / 128, "tanf: the rest of pi/2 (C2)")
    c.set("game", "tan_epsilon", "f32", 2 ** -12, "tanf: below this, tan f = f")
    for sym, v, what in (("tan_p1", p[0], "P1"), ("tan_p2", p[1], "P2"), ("tan_p3", p[2], "P3"),
                         ("tan_q1", q[0], "Q1"), ("tan_q2", q[1], "Q2"), ("tan_q3", q[2], "Q3"),
                         ("tan_q4", q[3], "Q4")):
        c.set("game", sym, "f32", v, f"tanf: {what} of f (1 + P(g)) / (1 + Q(g)), Lambert's continued fraction")

    # atan: the kernel fxatan is t N(g) / D(g) on |t| <= tan(pi/8); fsatan maps x > tan(3pi/8) to
    # pi/2 - K(1/x) and the middle to pi/4 + K((x - 1) / (x + 1)); atan2 (fatan2) adds +-pi/2 and +-pi.
    c0, n, d = atan_pade()
    c.set("game", "atan_p0", "f32", c0, "atan kernel: N's and D's constant term ([4/5] Pade, D monic)")
    for sym, v, what in (("atan_p1", n[0], "n1"), ("atan_p2", n[1], "n2"), ("atan_p3", n[2], "n3"),
                         ("atan_p4", n[3], "n4"), ("atan_q1", d[0], "d1"), ("atan_q2", d[1], "d2"),
                         ("atan_q3", d[2], "d3"), ("atan_q4", d[3], "d4")):
        c.set("game", sym, "f32", v, f"atan kernel: {what} of atan t = t N(g) / D(g)")
    for sym, v, what in (("atan_sq2m1", math.tan(math.pi / 8), "the kernel's own range, tan(pi/8)"),
                         ("atan_sq2p1", math.tan(3 * math.pi / 8), "past this, pi/2 - K(1/x): tan(3pi/8)"),
                         ("atan_pio2", math.pi / 2, "pi/2"), ("atan_pio4", math.pi / 4, "pi/4"),
                         ("atan2_pio2", math.pi / 2, "atan2: +pi/2"), ("atan2_neg_pio2", -math.pi / 2, "atan2: -pi/2"),
                         ("atan2_pi", math.pi, "atan2: +pi"), ("atan2_neg_pi", -math.pi, "atan2: -pi")):
        c.set("game", sym, "f32", v, f"atan: {what}")
    # asin and acos (farcsine): a literal rational for |x| <= 1/2 and asin x = pi/2 - 2 asin sqrt((1 - x) / 2)
    # above, joined by {a0, a1, b0, b1}: asin = (a[i] + r) + a[i], acos = (a[i] - r) + a[i] or (b[i] + r) + b[i].
    c.set_all("game", "asin_offsets", "f32", [0.0, math.pi / 4, math.pi / 2, math.pi / 4],
              "asin/acos: the offsets {0, pi/4, pi/2, pi/4}")
    # The vector helpers' floors, and the tire slip ratio's slope past |z| 400 (continuous with x / z there).
    c.set("game", "invmagnitude_floor", "f32", 1e-10, "invmagnitude: the least squared length it takes 1/sqrt of")
    c.set("game", "normalvector_floor", "f32", 1e-6, "NormalVector: a length it doesn't normalise")
    c.set("game", "extract_yaw_epsilon", "f32", 1e-5, "ExtractYaw: matrix entries it takes for zero (gimbal lock)")
    c.set("game", "calcalpha_far_scale_positive", "f32", 1 / 400, "calcalpha: x / z for z >= 400 (continuous)")
    c.set("game", "calcalpha_far_scale_negative", "f32", 1 / 400, "calcalpha: -x / z for z <= -400 (continuous)")


SCREEN_W, SCREEN_H = 320, 220   # the game's framebuffer (display())
VI_X_PER_PIXEL = 1024 / 640     # video_x_scale_factor: 2.10 xScale per pixel of a 640-pixel line
VI_Y_PER_LINE = 1024 / 240      # video_y_scale_factor: 2.10 yScale per line of a 240-line field


def vi_mode_ntsc_lpn1():
    """An NTSC, low-resolution, point-sampled, non-interlaced, 16-bit VI mode as 20 big-endian words (libultra's
    OSViMode: the type byte and 3 pad bytes; comRegs ctrl, width, burst, vSync, hSync, leap, hStart, xScale, vCurrent;
    fldRegs[2] origin, yScale, vStart, vBurst, vIntr), computed from what the VI registers mean, for the game's
    320 x 220 framebuffer on NTSC's 240 lines. Its scales are what video_build_adjusted_mode makes of the screen size
    (`width x FloatA`, `(height x FloatB + 16.5) & ~15`): video_apply_pending_mode shows the picture (osViBlack(0)) only
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
    """The screen and the video mode (video_mode.c's video_set_mode and video_build_adjusted_mode, StartGraphics)."""
    # The game's framebuffers are heap blocks of 0x22620 bytes, 32 more than a 320 x 220 16-bit image (aligned up to
    # 64 in _InitGfx); StartGraphics sets the colour image and clears it at the screen size screen_width x screen_height,
    # and ZOID_ResetWindows makes the windows from it. Zero is a colour image 4096 pixels wide (its width field is
    # width - 1) and windows of no size.
    c.set("boot", "screen_width", "u32", SCREEN_W, "the screen's width (a framebuffer is 320 x 220 x 2 bytes)")
    c.set("boot", "screen_height", "u32", SCREEN_H, "the screen's height")
    # The VI modes (osViModeTable, OSViMode[5]): video_set_mode takes mode video_mode_base_index(), 0 for NTSC (osTvType
    # 1), and video_build_adjusted_mode divides by its active line's and field's lengths (0 / 0 with a zero mode).
    c.set("boot", "osViModeTable", "u8", 0, "VI mode 0: NTSC LPN1, type 0 (vi_mode_ntsc_lpn1)")
    for k, v in enumerate(vi_mode_ntsc_lpn1()[1:], 1):
        c.set("boot", "osViModeTable", "u32", v, "VI mode 0: its registers" if k == 1 else "(same)", offset=4 * k)
    # video_build_adjusted_mode scales the mode by the screen size: xScale = width x FloatA, yScale = (height x FloatB + 16.5) &
    # ~15. FloatA = 1024 / 640 and FloatB = 1024 / 240 are the 2.10 scales of a pixel of the 640-pixel line and of a
    # line of the 240-line field (vi_mode_ntsc_lpn1 uses the same). FloatC and FloatD are the seconds a retrace takes,
    # PAL and NTSC (gRetraceTime; the ticks a second are 50 and 60).
    c.set("boot", "video_x_scale_factor", "f32", VI_X_PER_PIXEL, "xScale per pixel of the line (2.10)")
    c.set("boot", "video_y_scale_factor", "f32", VI_Y_PER_LINE, "yScale per line of the field (2.10)")
    c.set("boot", "retrace_time_pal", "f32", 1 / 50, "a PAL retrace, in seconds")
    c.set("boot", "retrace_time_ntsc", "f32", 1 / 60, "an NTSC retrace, in seconds")


def front_end(c):
    """The front end's screens, from the start prompt to the car select (SYNTH.md 2)."""
    # The 19-entry button map ControllerUpdate turns the pads' buttons into the game's with (game bit j = any N64
    # button in n64_button_map[j]). A zero map gives no buttons at all, so no screen that waits for one ever ends. This
    # one is made up from how the code uses the bits: 0x400/0x800/0x1000/0x2000 are what ControlIRQ makes of the
    # stick (up, down, left, right), so those are the d-pad; bits 0-2 are what the start prompt and the pak
    # warning take to go on (Start, A, B here); the rest are spread over the other buttons.
    n64_buttons = {"A": 0x8000, "B": 0x4000, "Z": 0x2000, "START": 0x1000, "DU": 0x0800, "DD": 0x0400,
                   "DL": 0x0200, "DR": 0x0100, "L": 0x0020, "R": 0x0010, "CU": 0x0008, "CD": 0x0004,
                   "CL": 0x0002, "CR": 0x0001}
    button_map = ["START", "A", "B", "Z", "L", "R", "CU", "CD", "CL", "CR", "DU", "DD", "DL", "DR"]
    for j, b in enumerate(button_map):
        c.set("game", "n64_button_map", "u32", n64_buttons[b], f"game button bit {j}: the pad's {b}", index=j)

    # The control table (gControlTable, gControlTable, u32[26]): a player's ten controls are option bytes, and SetControls
    # (SetControls) makes each the mask gControlTable[option] that the car's readers test against the game
    # buttons (UpdateModelControls's group). Zero gives no gas, brake, shift or fire. The readers take a value of 0x13 to
    # 0x19 as an analog source (0x13 and 0x19 the stick, ReadSteeringWheel, ReadGasAndBrake) and anything else as game
    # button bits; the default controls (SetDefaultControls, SetDefaultControls: gas 1, brake 2, steering 0x13, ...) are
    # set by code. So option k < 19 is game button k (the 19 bits of the button map), and 19-25 are those analog
    # codes themselves. With the button map above, gas is A (bit 1), brake B (bit 2), steering the stick.
    for k in range(26):
        v = 1 << k if k < 19 else k
        c.set("game", "gControlTable", "u32", v, "the control table: option k < 19 is game button k, 19-25 the analog "
              "codes" if k == 0 else "(same)", index=k)

    # The main menu's options (validOption, startscr_validOption[10], image A, s8; optionValid@startscr.c): updateStartScreen
    # skips to a valid one with `while (!optionValid(i)) i++`, which never ends when none is. Five, each a screen:
    # 0 the race (the player screen), 1 0x2000, 2 0x200, 3 0x800, 4 0x1000; 5 and 8 do nothing, 6 and 7 are the
    # cheats' and the unlocked levels' (the code decides those), 9 quits.
    c.set_all("ovl_a", "startscr_validOption", "u8", [1, 1, 1, 1, 1, 0, 0, 0, 0, 0],
              "the main menu's valid options: the five screens 0-4")

    # Cheat codes (eggs.c's Egg: a step count, then u32 held[20] and pressed[20] game-button masks; CheckForEgg).
    # Made up here, with the button map above: the main menu's code (start_screen_egg, the start screen's own InitEgg),
    # hold L and press Z, makes option 6 valid (gCheatActive[21]), the cheat screen (0x8000000); cheat 0's
    # (gEggs[0]), hold R and press Z, then hold C-up and press Z, lets RIGHT switch it. The masks are u32
    # elements: a byte view of gEggs (an opaque Rec164 that once won gen_data's ranking) stops this generator.
    # The cheats path (synth/cheats.press) opens the screen: the start screen once called eggs.c's InitEgg, whose
    # InitEgg(23) pointed past the table, and the screen never opened (docs/port/QA.md).
    c.set("ovl_a", "start_screen_egg", "u8", 1, "the main menu's cheat code: one step")
    c.set("ovl_a", "start_screen_egg", "u32", 1 << 4, "its held mask: L", offset=4)
    c.set("ovl_a", "start_screen_egg", "u32", 1 << 3, "its pressed mask: Z", offset=84)
    c.set("ovl_a", "gEggs", "u8", 2, "cheat 0's code: two steps")
    for i, held in enumerate((1 << 5, 1 << 6)):
        c.set("ovl_a", "gEggs", "u32", held, "its held masks: R, then C-up", offset=4 + 4 * i)
        c.set("ovl_a", "gEggs", "u32", 1 << 3, "its pressed masks: Z", offset=84 + 4 * i)

    # The options screen's valid items (validOption, options_validOption, s32[14]; optionscr_buttons): options_initScreen
    # starts at the first valid one with `while (!valid[i]) i++`, which never ends when none is. All fourteen.
    c.set_all("game", "options_validOption", "u32", [1] * 14, "the options screen's valid items: all fourteen")

    # The songs' names (gSongNames, gSongNames, char *[12]): the audio screen copies each (TB_Strcpy), so NULL
    # faults.
    # They follow the four sfx groups' table (boot_sfx_groups, Instrument4[4]), as the game image has its strings after
    # some tables.
    songs = c.strings_after("boot_sfx_groups", 4 * 4, [f"SONG {i + 1}" for i in range(12)], "a song's name")
    for i in range(12):
        c.set("game", "gSongNames", "ptr", songs[i], "a song's name" if i == 0 else "(same)", index=i)

    # The mode screen's table of which of its 7 modes each player count may pick (modescr_validOption[players - 1][mode],
    # image A, s8[4][7]; optionValid@modescr.c). updateModeScreen skips to a valid mode with `while (!optionValid(i))
    # i++`, which never ends when none is. Its rows are 0x803B4680-0x803B469B.
    for players in range(1, 5):
        c.set_bytes("ovl_a", c.syms.addr("modescr_validOption") + 7 * (players - 1), bytes([1] * 7),
                    f"modes valid for {players} player(s): all seven")

    # The track select's map objects (image A, trackMapObjNames trackMapObjNames[20], char *): it saves each map object's
    # first LOD, puts its own map display list there, and puts the saved one back on the way out, so they must be
    # different models (one model for all of them gets back a freed list). Each has a name of its own, which the
    # front-end WAD has a model for.
    for i, name in enumerate(MAP_MODELS):
        c.set("ovl_a", "trackMapObjNames", "ptr", c.string(name), f"track map object {i}", index=i)

    # The car select's name tables (game data, char *): textures for the car features (RimNames[24], TexNames[10]),
    # the cars' sheens (sheenName[11], SetSheen: once the car's parts are loaded and its model is made, with the
    # retrace clock running) and a model (tire_object_names[1]). MBOX_FindTexture copies its name before the lookup, so a NULL
    # there faults (on the N64 too: address 0 is unmapped). Each is the empty name, which every texture WAD here has
    # a texture for; MBOX_FindModel takes "" as AAANULLOBJ.
    empty = c.string("")
    # The car select's first blit (carscr_mb, image A, NewMultiBlit's 38): the empty texture name, kept after the
    # warning screen's blit (warningscr_blits, image A: one MultiBlit) as image A keeps its strings after some tables.
    c.set("ovl_a", "carscr_mb", "ptr", c.strings_after("warningscr_blits", 36, [""], "a car select blit's name", "ovl_a")[0],
          "the car select's first blit: the empty texture name")
    for sym, n in (("RimNames", 24), ("tire_object_names", 1), ("TexNames", 10), ("sheenName", 11)):
        for i in range(n):
            c.set("game", sym, "ptr", empty, "a car select name: the empty one" if i == 0 else "(same)", index=i)

    # The player screens' model slots (playerscr_buttonInfo, image A, Slot64[4][22]: unknown0 picks the model name from
    # playerscr_buttonNames, id -1 makes it): every player's, the fourth's too, with name 1 (NULL there: AAANULLOBJ). The value
    # reads 0x01000000 in the ROM's byte order, past the names (BRINGUP.md 12).
    for p in range(4):
        for j in range(22):
            first = p == 0 and j == 0
            c.set("ovl_a", "playerscr_buttonInfo", "u32", 1, "a player screen model slot: name 1" if first else "(same)",
                  offset=0x40 * (22 * p + j))
            c.set("ovl_a", "playerscr_buttonInfo", "u32", 0xFFFFFFFF, "(same): id -1, to be made", offset=0x40 * (22 * p + j) + 4)

    # The track select's fog (BlowFog, SetFogIntensity): with option 14 valid for a single race (validOption,
    # trackscr_validOption[gRace][21], image A, s32), the screen makes a blit of the texture named track_fog_texture_name (image A, char[])
    # whose palette is n64FogPal, a Palette whose colour table is the word at +0x14 (its palBase): the cartridge's
    # SYN_FOG, an IA texture, so the draw reads its prim and env colours from that table. The word must be the arena's
    # (BRINGUP.md 7).
    c.set("ovl_a", "trackscr_validOption", "u32", 1, "the track select's option 14 (the fog) valid in a single race", index=14)
    for i, ch in enumerate(SYN_FOG.encode() + b"\0"):
        c.set("ovl_a", "track_fog_texture_name", "u8", ch, f"the fog's texture name: {SYN_FOG}" if i == 0 else "(same)", index=i)

    # The front end's WADs (ASSETS.md 2.3): the locked one (54, "attract") has every texture and model the code
    # names; the others (58, 59 locked; the screens 55, 56, 57, 60) are empty but valid.
    c.file(54, texture_wad(CODE_TEXTURES + OWN_TEXTURES, CODE_MODELS + MAP_MODELS),
           "front-end WAD: the textures and models the code names", model=True)
    for w in (55, 56, 57, 58, 59, 60):
        c.file(w, empty_wad(), "front-end WAD: empty", model=True)
    # Paths (forward 158-176, backward 177-182): the track select's maps read 158-163, a race its track's.
    for w in range(158, 183):
        if w == LAP_PATH_WAVE:
            continue
        c.file(w, path_file(), f"path {w - 158}: a square loop")


# The lap path (saves(), below): track 0's backward path, wave 177. Its checkpoints, (position, flags): 0 the loop's
# start (flag 4) and 1 the finish line (flag 1) by the resurrection path's point 1, 2 the one before the finish (flag
# 2) by its point 20; 150 ft to the side, so the car's own position never crosses one. Its resurrection path: 40
# points 50 ft apart up the z axis.
LAP_PATH_WAVE = 177
LAP_CHECKPOINTS = [((150, 0, 50), 4), ((150, 0, 50), 1), ((150, 0, 1000), 2)]
LAP_POINTS = 40


def lap_res_points():
    return [(0, 0, 50 * i) for i in range(LAP_POINTS)]


# The cartridge's own names for its controller pak notes (saves(), below): none is the retail game's.
SAVE_PREFIX = "SYNTH049"   # every note's first 8 characters: what SaveServer reads as the game's
PROFILE_EXT = "D"          # a profile's (driver's) extension
GHOST_EXT = "T"            # a ghost's (a recorded trail's)
GHOST_FORMAT = "%s%X%s"    # a ghost's note name: the owner's note name, track + 1 (hex), its direction
GHOST_BACKWARD, GHOST_FORWARD = "R", "N"   # the direction letters: reversed, normal
DIGITS_FORMAT = "%0*u"     # CreatePlayer's collision digits: width 5, the attempt
DEV_NAME = "SYN"           # the profile name that unlocks the cheat screen and the ghost save prompt
# The name entry's 54 characters, six rows of nine (gPlayerNameChars): letters, digits, then the punctuation
# CreatePlayer keeps in a note name (. : ! @ * - = '), then more the pak font has, then four it hasn't.
NAME_GRID = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.:!@*-='\"#+,/?()&%"
# The N64 controller pak's character set (libultra's note names, osPfsFindFile and the pak manager): code 0 the
# end, 15 a space, 16-25 the digits, 26-51 A-Z, 52-65 the punctuation; 66 on are kana, which no name here uses.
PAK_FONT = {0: 0, 15: ord(" ")}
PAK_FONT.update({16 + i: ord("0") + i for i in range(10)})
PAK_FONT.update({26 + i: ord("A") + i for i in range(26)})
PAK_FONT.update({52 + i: ord(ch) for i, ch in enumerate("!\"#'*+,-./:=?@")})


def saves(c):
    """The controller pak's saves (docs/port/SYNTH.md 2): the names the save code gives and reads its notes by, the
    pak font it converts them with, and what the player screen's name entry needs."""
    def chars(sym, text, why, image="game"):
        size = c.syms.data[sym][2]
        if len(text) + 1 > size:
            raise SystemExit(f"synth_rom: {why}: {text!r} doesn't fit {sym}'s {size} bytes")
        for i, ch in enumerate(text.encode() + b"\0"):
            c.set(image, sym, "u8", ch, f"{why}: {text!r}" if i == 0 else "(same)", index=i)

    # The pak font (pak_charset, u16[256]: code -> character): pak_encode_name makes a note's name and extension from
    # ASCII by looking each character up here (the first code whose entry is that character), pak_decode_name makes
    # the ASCII back when the pak server reads the notes (PS_PakServer). Zero has no character at all, so every name
    # became empty and every profile and ghost note would have had the same name.
    for code, ch in sorted(PAK_FONT.items()):
        if ch:
            c.set("game", "pak_charset", "u16", ch, "the pak font: the N64 controller pak's character set" if code == 15
                  else "(same)", index=code)
    # The note names (SaveInit): CreatePlayer names a profile's note player_file_prefix (9 bytes copied: 8
    # characters and the end) and up to six characters of the player's name, with player_file_ext its extension, and
    # looks for a note of that name and extension player_file_ext_match before it (player_file_digits_format then makes digits for an
    # attempt's number). SaveServer reads the notes whose name starts with save_file_name_prefix's 8 characters, profiles those
    # whose extension's first character is player_file_ext_prefix's, ghosts the others. SaveGhost names a ghost's note
    # ghost_file_name_format (the owner's note name, track + 1, ghost_backward_suffix backward or ghost_forward_suffix forward), extension
    # ghost_file_ext. Zero (empty names) gave every note the same name.
    chars("player_file_prefix", SAVE_PREFIX, "a profile note's name prefix (CreatePlayer)")
    chars("save_file_name_prefix", SAVE_PREFIX, "the notes SaveServer reads: the same prefix")
    chars("player_file_ext", PROFILE_EXT, "a profile note's extension (CreatePlayer)")
    chars("player_file_ext_match", PROFILE_EXT, "the extension CreatePlayer's name check compares")
    chars("player_file_ext_prefix", PROFILE_EXT, "the extension SaveServer takes for a profile")
    chars("ghost_file_ext", GHOST_EXT, "a ghost note's extension (SaveGhost)")
    chars("ghost_file_name_format", GHOST_FORMAT, "a ghost note's name (SaveGhost's sprintf: char *, int, char *)")
    chars("ghost_backward_suffix", GHOST_BACKWARD, "a backward ghost's direction letter")
    chars("ghost_forward_suffix", GHOST_FORWARD, "a forward ghost's direction letter")
    chars("player_file_digits_format", DIGITS_FORMAT, "CreatePlayer's attempt digits (sprintf: width, number)")
    # The developer's profile name (cheat_profile_name): ReadPlayerData, loading a profile of this name, makes the main
    # menu's cheat screen valid, sets cheat states 14-16, and sets gSaveGhostFiles, without which updateHighScoreScreen keeps
    # a finished ghost race's ghost in memory only (SaveGhost's limbo) and never offers the save prompt that writes
    # it to a pak. Zero is the empty name, which no profile can have.
    chars("cheat_profile_name", DEV_NAME, "the profile name of the cheat screen and the ghost save prompt")

    # The player screen's name entry (controlCreatePlayer, controlCreatePlayer; drawn by drawCreatePlayer): its 54 cells,
    # nine to a row, are the characters gPlayerNameChars points to (u8 *: NULL faults). Made up, the characters a note
    # name keeps first.
    if len(NAME_GRID) != 54 or len(set(NAME_GRID)) != 54:
        raise SystemExit("synth_rom: the name entry has 54 different characters")
    c.set("game", "gPlayerNameChars", "ptr", c.string(NAME_GRID), "the name entry's 54 characters")
    # The name censor's words (obscenities, u8 *[22]; censor, called on an entered name): the walk ends at
    # the first empty word, so a NULL first entry faults. None: the first entry is the empty word.
    c.set("game", "obscenities", "ptr", c.string(""), "the name censor's words: none (the empty word ends them)")

    # A ghost is deflated when it is kept (CompressGhost: deflate_mem, zlib 1.0.4 in zlib_deflate, level 9). zlib's
    # static tables are initialized data, zero here: tr_static_init's loops shift by extra_dbits[code] - 7 and ran
    # off dist_code. zlib's own constants (RFC 1951's extra bits and code length order; trees.c's descriptors;
    # deflate.c's level 9), as extra_blbits in boot_and_sound.
    c.set_all("game", "extra_lbits", "u32", [0] * 8 + [k // 4 for k in range(4, 24)] + [0],
              "zlib's extra_lbits (the length codes' extra bits)")
    c.set_all("game", "extra_dbits", "u32", [max(0, k // 2 - 1) for k in range(30)],
              "zlib's extra_dbits (the distance codes' extra bits)")
    c.set_all("game", "bl_order", "u8", [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15],
              "zlib's bl_order (the code length codes' order)")
    for sym, tree, extra, base, elems, bits in (("static_l_desc", "static_ltree", "extra_lbits", 257, 286, 15),
                                                ("static_d_desc", "static_dtree", "extra_dbits", 0, 30, 15),
                                                ("static_bl_desc", None, "extra_blbits", 0, 19, 7)):
        why = f"zlib's {dict(static_l_desc='static_l_desc', static_d_desc='static_d_desc', static_bl_desc='static_bl_desc')[sym]}"
        if tree:
            c.set("game", sym, "ptr", c.syms.addr(tree), f"{why}: its static tree")
        c.set("game", sym, "ptr", c.syms.addr(extra), f"{why}: its extra bits", offset=4)
        for k, v in enumerate((base, elems, bits)):
            c.set("game", sym, "u32", v, f"{why}: extra base, elements, max length" if k == 0 else "(same)",
                  offset=8 + 4 * k)
    for k, v in enumerate((32, 258, 258, 4096)):
        c.set("game", "configuration_table", "u16", v, "zlib's configuration_table[9] (good length, max lazy, nice length, "
              "max chain): the level CompressGhost asks for" if k == 0 else "(same)", offset=8 * 9 + 2 * k)

    # The lap path: a ghost race (gRace 2) is three laps, and its ghost is kept only if the car finishes (GhostEnd,
    # GhostEnd). The cartridge's car never crosses a checkpoint: its physics crash it at once (the model's
    # crash flag; the checkpoint test skips a crashed car), and it is respawned every 3.55 s (CheckCrash:
    # 3.5 s after a crash). A respawn moves the car 12 points on along the resurrection path and passes the
    # checkpoint it jumps over (Resurrect, MAT3's Resurrect), but on the last lap it never reaches the
    # finish: the jump is cut to half the points left to the finish's point, from the nearest point inside the
    # window between the last and the next checkpoints' points. On track 0's backward path (a race goes backward with
    # the track select's option 12, which no other path sets) the respawns go round by themselves: each checkpoint's
    # point is the resurrection point nearest to it (SyncCheckpointsToPaths), so the loop's start and the finish share
    # point 1, the window before the finish is empty, its respawn starts from the car's nearest point, at or past
    # point 1, and the finish is passed with nothing to cut. The start passes the finish, and then nearly every
    # respawn a checkpoint (at the path's end, where it wraps to point 1, every other one): the third lap ends 35.6 s
    # after the start (CarReportsGameOver), with the respawns' jump of 12 points (3.55 s at 58 ft/s). The
    # race then waits for the car to stop, which it never does: the ghost path's script pauses and quits, and GhostEnd
    # keeps the finished ghost. A race on track 0 forward (every other path) reads its own path file, 158.
    c.file(LAP_PATH_WAVE, path_file(lap=True), "path 19, track 0 backward: the lap path, three checkpoints that "
           "respawns pass")


def race(c):
    """A race on an empty track (SYNTH.md 2): LoadTrack's files, the cars' physics, and the tables race setup reads."""
    # Each track's WAD (101-119) has the world node's model; its world (120-138) is one node, its road (139-157)
    # one empty collision node. The race, HUD and car WADs (61-68, 76-100) have the textures and models the code
    # names. gWorldNames (gWorldNames, char *[19]) names every track's world.
    for t in range(19):
        c.set("game", "gWorldNames", "ptr", c.string(WORLD_NAME), f"gWorldNames: track {t}'s world", index=t)
        if t == DETAIL_TRACK:
            detail_world(c, t)
            continue
        c.file(101 + t, texture_wad(CODE_TEXTURES + OWN_TEXTURES, CODE_MODELS + [WORLD_NODE]), f"track {t} WAD",
               model=True)
        icons = ARENA_ICONS if t in BATTLE_ARENAS else ()
        c.file(120 + t, world_wad(icons), f"track {t} world: one node" + (f" and {len(icons)} weapon icons" if icons
                                                                           else ""))
        c.file(139 + t, road_file(), f"track {t} road: flat pavement, one surface")
    for w in list(range(61, 69)) + list(range(76, 101)):
        c.file(w, texture_wad(CODE_TEXTURES + OWN_TEXTURES, CODE_MODELS), "race, HUD or car WAD", model=True)

    # The target info table (targetInfo, 122 records of 0x30, docs/port/POINTERS.md 10): a world node whose name
    # starts with a target's name becomes that target. The names are read with strlen, so NULL faults, and an empty
    # name is a prefix of every node: each target has a name of its own that no node has.
    for i in range(1, 122):
        c.set("game", "targetInfo", "ptr", c.string(f"TARGET{i:03}"), "a target's name" if i == 1 else "(same)",
              offset=0x30 * i)
    # Target 0 is the battle arenas' weapon icon (MAT3's _StartWeaponIcon, nonmatching/game/StartWeaponIcon.c): a
    # world node named WEAPONnn becomes one (SetupTarget). Its flags: 0x20 (made without the Expansion
    # Pak), 8 (a record of its category), 0x4000 (its start function runs when it is made: StartWeaponIcon's set-up
    # hides it, marks it 0x20 and counts one more power-up, which battle mode's power-up slots, InitBattlePowerUps, are
    # made from; UpdateBattleHealth picks one of them, and with none it spins for ever). Category 5, the battle
    # slots; shape 350, the first weapon (StartWeaponIcon's switch, and the object definition gObjList[350]); no
    # model file (AAANULLOBJ).
    c.set("game", "targetInfo", "ptr", c.string("WEAPON"), "target 0: the weapon icon's name")
    c.func_ptr("game", "targetInfo", 2, "StartWeaponIcon", "target 0's start function")
    c.set("game", "targetInfo", "u16", 0x4028, "target 0's flags: start function, category record, no Expansion Pak "
          "needed", offset=0x12)
    c.set("game", "targetInfo", "u16", 350, "target 0's shape: the first weapon", offset=0x14)
    c.set("game", "targetInfo", "u8", 5, "target 0's category: battle's power-up slots", offset=0x16)
    # The weapons' ammunition (ammoCnts, ammoCnts, s32[8]): what a pickup gives. Five shots each.
    c.set_all("game", "ammoCnts", "u32", [5] * 8, "a weapon's ammunition: five")
    # The collision sizes per body type (collsize, collsize: front, rear, side, height, f32), which the car-car
    # collision and the shield weapon read: the tires sit 4 ahead and behind and 2.5 aside, so a 9 x 5 x 3 box.
    for body in range(13):
        for k, v in enumerate((4.5, 4.5, 2.5, 3.0)):
            c.set("game", "collsize", "f32", v, "a body's collision box: front, rear, side, height" if body == k == 0
                  else "(same)", offset=16 * body + 4 * k)

    # The arenas' six puff textures (FireTexNames, char *[6]): InitSmoke looks each up by name on the
    # tracks from 6 on (the stunt and battle arenas), and MBOX_FindTexture copies the name first. The empty name.
    for i in range(6):
        c.set("game", "FireTexNames", "ptr", c.string(""), "an arena's puff texture: the empty name" if i == 0
              else "(same)", index=i)

    # The level objects' record size per category (gTargetStructSizes, u8[8]): SetTargetDefs allocates a heap block of
    # size x count for each category with objects. Battle mode always has 16 of category 5 (initTargetStructs),
    # its power-up slots, and writes all 16 136-byte records (Slot136): a size of 0 is a zero-size block, and the
    # writes go over the next blocks' headers.
    c.set("game", "gTargetStructSizes", "u8", 136, "category 5's record size (battle's power-up slots, Slot136)", index=5)
    # The level objects' bodies (BBoxCollInfo, f32[115]): 32 bytes a body (x min, x max, z max, z min, three more, the
    # mass at +0x1C), which CollisionReckon reads as f32 for a kind-4 object, then the static states of categories
    # 0-2. Declared u8[], they kept the ROM's byte order (docs/port/DATA.md 8): the generator refuses an f32 there,
    # and its last element past a shorter table.
    c.set_all("game", "BBoxCollInfo", "f32", [-3.0, 3.0, 3.0, -3.0, 0.0, 0.0, 0.0, 50.0],
              "body 0: a 6 ft box of 50 slugs")
    c.set("game", "BBoxCollInfo", "f32", 0.0, "the bodies' and states' last element", index=114)
    # The respawn records after FSCLS (its f32[3], then {s8 flag; f32 pos[3]} up to trackSpots, which
    # init_camera_on_track reads by trackSpots's pointers, NULL here). As f32[] the flag was swapped with its pad
    # (docs/port/DATA.md 8): the generator refuses a byte there.
    c.set("game", "FSCLS", "u8", 1, "the first respawn record's flag", offset=0xC)

    battle(c)

    # The drones' update schedule (drone_sched, drone_sched, Row3[12] of s16; update_drone_models): row r gives how
    # many drones to update each model tick, ended by -1, where the walk goes back to the row's start. The race uses
    # row n for n drones, n + 7 with two humans or more (init_cars). With zeros it never meets -1 and walks
    # on through the tables after it, updating whatever cars their values say (gRace 2's car with no time step:
    # NaN); a row asking for more drones than there are updates the players' cars, whose time step is then 0 (NaN in
    # the 3- and 4-player races). Every row: all its drones every tick.
    for r in range(12):
        for k, v in enumerate((r if r < 7 else r - 7, -1, -1)):
            c.set("game", "drone_sched", "u16", v, "the drone schedule: row n and n + 7 are {n, -1}" if r == k == 0
                  else "(same)", offset=6 * r + 2 * k)

    # The time limit of a gRace 2 race on tracks 0-5 (ghostRaceTimes, f32[6]; CalcRaceTime, 600 s for the others): the
    # race ends when it runs out (UpdateGame's last branch), then the results (0x2000000, 4 s) and the game over
    # screen (0x800000). Zero ended the race in its first frame. 15 s on tracks 1-5; on track 0, 90 s: its lap path's
    # respawns pass checkpoints only before the time runs out, and the third lap ends 35.6 s in (saves()).
    c.set_all("game", "ghostRaceTimes", "f32", [90.0] + [15.0] * 5, "a gRace 2 race's time limit, s")

    # The demo path start points per track (demo_path_points, s16 *[12], with their counts num_demo_ppoints[12]): one start
    # point, the path's first (0, so its byte order doesn't matter).
    first_point = c.blob(be16(0) + be16(0), "a demo path start list: point 0")
    for t in range(12):
        c.set("game", "demo_path_points", "ptr", first_point, "a track's demo path start points" if t == 0 else "(same)",
              index=t)
        c.set("game", "num_demo_ppoints", "u16", 1, "one start point" if t == 0 else "(same)", index=t)

    # The cars' physics descriptors (gCarModels, Car *[13], docs/port/CARS.md): 0xB4-byte records of words, which
    # the research puts 8 bytes into steady_leg_duration (f32[], so the loader swaps them as words). One descriptor
    # for every car type: zero but for a mass, an engine inertia, four tire curves and the four tires' positions (x
    # forward: the wheelbase is front x - rear x). The tire curves are the two six-float records after the torque
    # curve, frontTire[2][6] (front, rear: copy_tire_info copies six), which only these u32 fields reach: declared in
    # car_resistance for the PC, so the loader swaps them as floats (BRINGUP.md 3). Every value 1.
    base = c.syms.addr("steady_leg_duration")
    desc, susp = base + 0x8, base + 0x138   # the descriptors, and their suspension part
    curves = c.syms.addr("frontTire")
    for i in range(12):
        c.set("game", "frontTire", "f32", 1.0, "the tire curves (front, rear): every value 1" if i == 0 else "(same)",
              index=i)
    c.set("game", "steady_leg_duration", "f32", 3000.0, "car descriptor: mass", offset=desc + 80 - base)
    c.set("game", "steady_leg_duration", "f32", 1.0, "car descriptor: engine inertia", offset=desc + 84 - base)
    for i in range(4):
        c.set("game", "steady_leg_duration", "u32", curves + 24 * (i // 2),
              f"car descriptor: tire {i}'s curve (a u32 window address)", offset=desc + 96 + 4 * i - base, elem="f32")
    for i, pos in enumerate(((4.0, 0.0, 2.5), (4.0, 0.0, -2.5), (-4.0, 0.0, 2.5), (-4.0, 0.0, -2.5))):
        for k, v in enumerate(pos):
            c.set("game", "steady_leg_duration", "f32", v, f"car descriptor: tire {i}'s position",
                  offset=desc + 120 + 12 * i + 4 * k - base)
    for t in range(13):
        c.set("game", "gCarModels", "ptr", desc, "a car type's physics descriptor" if t == 0 else "(same)", index=t)
    # The suspensions (carSuspensions, carSuspensions, const _tSusp *[3]): nine f32 each (forward, back, side: force
    # minimum, force range, angle), in the same f32 table past the descriptor. One suspension, all three entries:
    # no tilt until 1000 units of force, then up to 0.1 rad over another 1000.
    for k, v in enumerate((1000.0, 1000.0, 0.1, -1000.0, 1000.0, -0.1, 1000.0, 1000.0, 0.1)):
        c.set("game", "steady_leg_duration", "f32", v, "a suspension" if k == 0 else "(same)", offset=susp + 4 * k - base)
    for i in range(3):
        c.set("game", "carSuspensions", "ptr", susp, "carSuspensions: the one suspension" if i == 0 else "(same)",
              index=i)

    # The car types' names (CarNames, char *[13]), which InitCarDynamicObjs puts in front of each car part's name:
    # after the shadow scales (shadow_scales, f32[14][4]: AnimateShadow's per body type, 13 with the appearance bit).
    names = c.strings_after("shadow_scales", 14 * 16, [f"SYNC{t:02}" for t in range(13)], "a car type's name")
    for t in range(13):
        c.set("game", "CarNames", "ptr", names[t], "a car type's name" if t == 0 else "(same)", index=t)

    car_tables(c)
    hud(c)
    blits(c)
    track_animations(c)

    # The tracks' car colours (gTrackColors, gTrackColors, tRGBA *[19]): each points at RGBA byte quadruples, which
    # the cars' IR colour comes from. Sixteen mid greys per track (bytes, so copied as they are).
    colours = c.blob(bytes([0x80, 0x80, 0x80, 0xFF]) * 16, "a track's car colours: sixteen greys")
    for t in range(19):
        c.set("game", "gTrackColors", "ptr", colours, "gTrackColors: a track's colours" if t == 0 else "(same)",
              index=t)


def detail_world(c, t):
    """Track t's files and tables for the detail world (DETAIL_PARTS above; docs/port/DETAIL.md 6): its WAD with the
    parts' models, its world, its road; the parts the world's visibility (WorldCullObjects) walks, trackParts[t], and
    its table's rows, track1Parts (track 0's). And the fog the track select's fog option gives one player
    (WorldFogDensity, [players][option]): for options 1-3 the fog's start (gSPFogPosition's min, of 1,000), the far
    plane (ft) and the radius past which a part with a bound is left out (ft, gViewableDistance). No fog regions
    (trackFog stays NULL)."""
    models = CODE_MODELS + [WORLD_NODE] + list(DETAIL_MODELS)
    c.file(101 + t, texture_wad(CODE_TEXTURES + OWN_TEXTURES, models), f"track {t} WAD: the detail world's models",
           model=True)
    nodes = [(WORLD_NODE, GROUND_CENTRE, 0, DETAIL_GROUND_BOUND)] + DETAIL_PARTS
    c.file(120 + t, world_wad(nodes=nodes), f"track {t} world: the detail world, the ground and {len(DETAIL_PARTS)} "
           "parts")
    c.file(139 + t, road_file(), f"track {t} road: flat pavement, one surface")
    assert t == 0   # track1Parts is track 0's table
    c.set("game", "trackParts", "u8", len(nodes), "the detail world's parts (WorldCullObjects)", index=t)
    for r, bits in enumerate(DETAIL_ROWS):
        c.set("game", "track1Parts", "u32", bits, "the detail world's visibility: a cell's row, the parts it lets "
              "through" if r == 0 else "(same)", offset=16 * r)
    for opt, (fogmin, far, radius) in enumerate(((990, 1500, 1200.0), (985, 1000, 800.0), (980, 600, 500.0)), 1):
        c.set("game", "fogMin", "u16", fogmin, "one player's fog, option 1-3: its start (of 1,000)" if opt == 1
              else "(same)", offset=8 * 1 + 2 * opt)
        c.set("game", "fogDistance", "u16", far, "one player's fog: the far plane, ft" if opt == 1 else "(same)",
              offset=8 * 1 + 2 * opt)
        c.set("game", "fog_viewable_distance", "f32", radius, "one player's fog: the radius a part is drawn to, ft" if opt == 1
              else "(same)", offset=16 * 1 + 4 * opt)


def car_tables(c):
    """The car physics' tables, which the race start (carsetup, PrePlay) turns into each car's
    model record. With zeros the gravity is 0, so a car's mass is 0 and the model thread's first step divides 0 by
    0 (accelerations): every car's position is NaN from the first frame. Units are the model's: feet, seconds,
    pounds (its gravity is 32.2)."""
    # PrePlay sets gravity to 32.2 x gravityArray[the gravity option]: the option's factor, 1 for each of the four
    # (f32[4], extents.txt: strings follow it, docs/port/DATA.md 8).
    c.set_all("game", "gravityArray", "f32", [1.0] * 4, "the gravity option's factor (32.2 ft/s^2 x): 1 for each")
    # The wings' place per car type (wingOffsets, f32[13][3], extents.txt): AnimateWing puts a car's two wings at
    # (-x, y, 0) and (x, y, 0), StartWing their sound at height y. Its gap was one row, so the other twelve kept
    # the ROM's byte order (docs/port/DATA.md 8); the generator refuses a value outside the table.
    c.set_all("game", "wingOffsets", "f32", [3.0, 0.5, 0.0] * 13, "a car type's wings: half their span, their height")
    # The rumble request's 36 rising thresholds (percentTable, s32[36]): UpdateModel counts how many the car's speed
    # passes, less 4, for the player's rumble. As one s32 the other 35 kept the ROM's byte order (DATA.md 8), which
    # the generator refuses. Small, so a moving car rumbles as with zeros (the rumble2p path's motors).
    c.set_all("game", "percentTable", "u32", [2 * (i + 1) for i in range(36)], "a rumble threshold (speed, ft/s)")
    # CalcMass, a car's mass in slugs (its weight is that x gravity): (gCarMass[type] + (carDurability[mass
    # option] - mass_durability_base + the pak's mass bonus) x mass_unit) x the tune's factor. 3000 lb is 93 slugs; the mass
    # options add nothing; one slug a unit.
    c.set_all("game", "gCarMass", "f32", [93.0] * 13, "a car type's mass, in slugs (3000 lb)")
    c.set_all("game", "carDurability", "f32", [0.0] * 6, "a mass option's difference: none")
    c.set("game", "mass_unit", "f32", 1.0, "slugs per unit of mass difference")
    # A car type's weight in pounds (gCollisionMass, f32[14]; + 1000 lb per unit of mass difference), the model's +0x5C4,
    # which the car-car collision (setFBCollisionForce) shares the push out by: 0 for both is 0 / 0.
    c.set_all("game", "gCollisionMass", "f32", [3000.0] * 14, "a car type's weight, in pounds")
    # Two cars that overlap are pushed apart along the line between them with force force_apart_force_over_dist (ForceApart),
    # or along x with force_apart_force when they are closer than sqrt(force_apart_min_dist_sq): 0 / 0 when they coincide, as the six
    # cars do at the start here. The push is the size of the collision response's own (setFBCollisionForce: 2000).
    c.set("game", "force_apart_min_dist_sq", "f32", 1e-4, "car push: a squared distance it takes for none")
    c.set("game", "force_apart_force", "f32", 2000.0, "car push: the force when the cars coincide")
    c.set("game", "force_apart_force_over_dist", "f32", 2000.0, "car push: the force along the line between them")
    # A car type's brake factor (gCarBraking, f32[13]): carsetup's brake gains are mass x the tire's
    # brake value x this, so zero is a car without brakes. 1.
    c.set_all("game", "gCarBraking", "f32", [1.0] * 13, "a car type's brake factor: 1")
    # The gear ratios (carTransGearRatio, f32[8][6], port/data/extents.txt): a row per car class, which is the model's
    # byte 9, gear g's ratio at [g + 1] (transmission, find_best_gear). 1 everywhere: with zeros a car's drive torque
    # is 0. The rows past the first are outside a one-row extent, which the generator refuses (BRINGUP.md 11).
    c.set_all("game", "carTransGearRatio", "f32", [1.0] * 48, "the gear ratios: 1 in every row and gear")
    # The car tunes (gTune, three Tune44 of eleven f32): +4 is the mass factor; 1 in each, the rest 1 too.
    for t in range(3):
        for k in range(11):
            c.set("game", "gTune", "f32", 1.0, "a tune's eleven factors: 1 (+4: the mass factor)" if t == k == 0
                  else "(same)", offset=44 * t + 4 * k)


def multiblit(c, sym, index, texname, x, y, w, h, func=None, image="game", animid=0, null_name=False):
    """MultiBlit sym[index] (36 bytes: char *texname; s16 dulx, duly, width, height, top, bot, left, right; u32
    zdepth, alpha; animfunc; u32 animid): a blit of the texture's first 16 x 16 texels at (x, y), w x h, opaque, or
    with texname -1 and func, a sprite whose draw function func is (NewMultiBlit). With a texture name and func, func
    is the blit's animation function, called with animid. null_name: the name NULL (InitBlit's built-in image)."""
    base = 36 * index
    name = 0 if null_name else 0xFFFFFFFF if texname is None else c.string(texname)
    c.set(image, sym, "ptr", name, f"{sym}[{index}]: {texname or 'a sprite'}", offset=base)
    for k, v in enumerate((x, y, w, h, 0, 15, 0, 15)):
        c.set(image, sym, "u16", v, "(same)", offset=base + 4 + 2 * k)
    c.set(image, sym, "u32", 1, "(same)", offset=base + 0x14)                 # zdepth
    c.set(image, sym, "u32", 255, "(same)", offset=base + 0x18)               # alpha
    if func:
        c.set(image, sym, "ptr", c.syms.func(func, image), f"(same): {func}", offset=base + 0x1C)
    if animid:
        c.set(image, sym, "u32", animid, "(same): animid", offset=base + 0x20)


def hud(c):
    """The race HUD (ShowHUD, ShowHUD: NewMultiBlit of the HUD set HudMultiBlit[8] and the race set
    RaceMultiBlit[89]). With zeros every blit has no texture and no size. Six of the HUD set's eight: a 48 x 12 panel
    of the BUTTON texture top left, the digital speedometer (animSpeedText, which the 2D sprite pass calls every
    frame: gShowSpeed is option 5, 1 by default), whose digits go at speedPos (speedPos, s32 [windows][slot][2]),
    and the four players' crossbones (animBones, animBones, animid the player), which only deaths mode shows
    (option 31, 0 by default): an N64-offset body left them drawn top left in every race (BRINGUP.md 15). Their
    places in deaths mode, bonesPos (bonesPos, s32 [windows][player][2]), fill the table: a declaration that
    describes its first word only leaves the rest outside, which the generator refuses."""
    multiblit(c, "HudMultiBlit", 0, "BUTTON", 8, 8, 48, 12)
    multiblit(c, "HudMultiBlit", 1, None, 0, 0, 0, 0, func="animSpeedText")
    for player in range(4):
        multiblit(c, "HudMultiBlit", 4 + player, "BUTTON", 120, 60, 24, 12, func="animBones", animid=player)
    for windows in range(1, 5):
        for player in range(4):
            for k, v in enumerate((90 + 70 * (player % 2), 10 + 105 * (player // 2))):
                c.set("game", "bonesPos", "u32", v, "bonesPos: the crossbones' place per view, in deaths mode"
                      if windows == 1 and player == k == 0 else "(same)",
                      offset=32 * (windows - 1) + 8 * player + 4 * k)
    corners = {1: [(300, 200)], 2: [(300, 95), (300, 205)], 3: [(140, 95), (300, 95), (140, 205)],
               4: [(140, 95), (300, 95), (140, 205), (300, 205)]}
    for windows, slots in corners.items():
        for slot, (x, y) in enumerate(slots):
            for k, v in enumerate((x, y)):
                c.set("game", "speedPos", "u32", v, "speedPos: the speedometer's place per view" if windows == 1
                      and slot == k == 0 else "(same)", offset=32 * (windows - 1) + 8 * slot + 4 * k)


def blits(c):
    """NewMultiBlit's descriptor tables that game C declared without their layout (s32, u8[]), which the PC declares
    as MultiBlits now (struct pc_multiblit, BRINGUP.md 1): with a texture name in each, a view without the pointer
    would read the name from the wrong bytes. The attract demo's blit, made before the race's files are loaded: the
    name NULL, InitBlit's built-in image (read as words, the NULL name and the next field, the blit's x and y, make a
    pointer that isn't). The game over title, the controls screens' first buttons (front end, paused alone, paused
    with others) and the menu backgrounds (createBG's two blits, front end and track select): BUTTON blits (a texture
    name the game C looks up itself)."""
    multiblit(c, "demo_multiblit", 0, "", 8, 8, 48, 12, null_name=True)
    multiblit(c, "gameover_multiblit", 0, "BUTTON", 136, 20, 48, 12)
    for sym in ("buttonsFrontEnd", "buttonsPauseSingle", "buttonsPauseMulti"):
        multiblit(c, sym, 0, "BUTTON", 16, 16, 48, 12)
    for sym in ("frontBack", "trackBack"):
        for i in range(2):
            multiblit(c, sym, i, "BUTTON", 8 + 160 * i, 200, 48, 12)
    # The video screen's two sets (front end, paused) and the audio screen's second set have two blits each, as
    # NewMultiBlit gets them: one-blit declarations left the second outside its table (BRINGUP.md 9).
    for sym in ("video_frontEndBlit", "video_pauseBlit", "audio_pauseBlit"):
        multiblit(c, sym, 1, "BUTTON", 16, 200, 48, 12)
    # The idle overlay (GameLoop: NewMultiBlit of idle_warning_multiblit once no button was held for 300 s, outside the title
    # and the attract demo): an untextured blit over the screen whose animation function, animControllerDim, makes its
    # colour controller_dim_color its Image, R, G, B, A bytes the 2D draw fills it with (BlitRect). An N64-offset body
    # put NULL there and the draw faulted; a word declaration of the colour reversed its bytes (BRINGUP.md 15). The
    # idle path waits for it.
    multiblit(c, "idle_warning_multiblit", 0, "", 0, 0, 320, 220, func="animControllerDim", null_name=True)
    for i, v in enumerate((0, 0, 0, 96)):
        c.set("game", "controller_dim_color", "u8", v, "the idle overlay's colour: black, a third opaque" if i == 0
              else "(same)", index=i)


def track_animations(c):
    """Track 0's animations (InitTextureAnimation sets them up, TextureAnimation runs them every frame): tables only
    pointers reach, which port/data/reached.txt has the loader convert (BRINGUP.md 8). In N64 form, in bytes no
    symbol covers. A palette animation (MAT3 tPaletteAnim, 20 bytes: name, start, end, offset, state, speed, time,
    ptr) that rotates entries 0-3 of the palette named "" every tick, and an object animation (MAT3 tTextureSwap, 20
    bytes: num, startnum, fd, idx, time, initTime, swapTexs) of two frames (MAT3 tTextureBase, 12 bytes:
    name, tex, texBase) of the texture "", each ended by an empty record. Track 0 is not one with the shared
    frames, so InitTextureAnimation looks its frames up."""
    empty = c.string("")
    frames = c.blob(be32(empty) + be32(0) + be32(0) + be32(empty) + be32(0) + be32(0), "track 0's animation frames")
    seq = c.blob(be16(2) + be16(0) + be16(1) + be16(0) + bef32(0.0) + bef32(0.1) + be32(frames) + bytes(20),
                 "track 0's object animation and its end")
    pal = c.blob(be32(empty) + be16(0) + be16(3) + bytes([0, 0]) + be16(1) + bef32(0.0) + be32(0) + bytes(20),
                 "track 0's palette animation and its end")
    c.set("game", "TextureSwapLists", "ptr", seq, "track 0's object animations")
    c.set("game", "PaletteLists", "ptr", pal, "track 0's palette animations")


def battle(c):
    """What battle mode (gRace 6, image B, "Extra") reads at its race start (SYNTH.md 2)."""
    # The weapons' texture names (image B: MineTexNames[5], RipTexNames[5], char *): ShotInit looks each up with
    # MBOX_FindTexture, which copies the name first (NULL faults). The empty name. Its model names (WprObjNames[10],
    # WfxObjNames[7]) stay NULL: MBOX_FindModel takes those as AAANULLOBJ.
    empty = c.string("")
    for sym in ("MineTexNames", "RipTexNames"):
        for i in range(5):
            c.set("ovl_b", sym, "ptr", empty, "a weapon texture: the empty name" if i == 0 else "(same)", index=i)
    # The battle HUD (BattleMultiBlit BattleMultiBlit, image B, NewMultiBlit's 10): player 1's health bar, the cartridge's
    # SYN_BAR texture (I8) with animHealth (animHealth) for part 1 (animid: slot << 4 | part), which colours it
    # through a palette of its own, healthPal (BRINGUP.md 10). An IA blit (SYN_FOG) first, so the draw's last palette
    # isn't NULL when it gets to the bar, as in a game, where a NULL colour table would not pass for the one loaded.
    multiblit(c, "BattleMultiBlit", 0, SYN_FOG, 8, 20, 16, 16, image="ovl_b")
    multiblit(c, "BattleMultiBlit", 1, SYN_BAR, 8, 8, 40, 6, func="animHealth", image="ovl_b", animid=0x01)


SFX_GROUPS = [12, 13, 14, 15]
BATTLE_ARENAS = range(6, 14)   # battle mode's track select starts at track 6, stunt mode's at 14 (the arenas: 6 on)
ARENA_ICONS = [(f"WEAPON{i:02}", (x, 0.0, z)) for i, (x, z) in
               enumerate(((500.0, 500.0), (1500.0, 500.0), (1500.0, 1500.0), (500.0, 1500.0)))]
MAP_MODELS = [f"MAPOBJ{i:02}" for i in range(20)]


def make(syms, sounds=False):
    c = Cart(syms)
    boot_and_sound(c)
    if sounds:
        sound_bank(c)
    math_constants(c)
    display(c)
    front_end(c)
    saves(c)
    race(c)
    return c


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--out", default=str(ROOT / "build/synth/rush2049_synth.z64"))
    ap.add_argument("--data-symbols", default=str(ROOT / "build/pc/pc-data/data_symbols.txt"),
                    help="the build's data report: every symbol's address and element kinds")
    ap.add_argument("--report", help="where to list every value and file (default: OUT with .txt)")
    ap.add_argument("--sounds", action="store_true",
                    help="a sound bank (an sfx for every sound index) and the engine table, for the voice tests "
                         "(docs/port/AUDIO.md 10); without it the project is empty and nothing plays")
    args = ap.parse_args()
    syms = Symbols(args.data_symbols)
    if not syms.checked:
        raise SystemExit(f"synth_rom: {args.data_symbols} not found: build the PC port first (its data report gives "
                         "every symbol's address and element kinds)")
    c = make(syms, sounds=args.sounds)
    rom = c.build()
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(rom)
    rep = Path(args.report) if args.report else out.with_suffix(".txt")
    rep.write_text(c.report())
    print(f"synth_rom: {out} ({len(rom)} bytes, {len(c.log)} values, {len(c.files)} files); report {rep}")


if __name__ == "__main__":
    main()
