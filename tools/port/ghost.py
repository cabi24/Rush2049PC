#!/usr/bin/env python3
"""The PC saves' files (port/ultra/pakfs.h): a controller pak's notes, a profile or a ghost each, as files, read and
written byte for byte as port/ultra/pakfs.c, pakstore.c and pakfile.c read and write them.

    python3 tools/port/ghost.py info FILE...              what a .mpk, a save file or a saves folder holds
    python3 tools/port/ghost.py validate [--as-import] FILE...
                                                         a line each (a .r49ghost checked as an import is, a .r49save
                                                         or .r49note as the game reads it); exit status: the number
                                                         of files refused
    python3 tools/port/ghost.py export SRC NOTE --out DIR
                                                         a ghost of a .mpk or a saves folder (NOTE: its slot or its
                                                         file name) into DIR as a .r49ghost (origin export)
    python3 tools/port/ghost.py import FILE.r49ghost --to FOLDER
                                                         as the game's import: checked whole, already there or written
                                                         (origin import) under a name of its own, and pinned
    python3 tools/port/ghost.py mpk2files MPK --to FOLDER [--force]
                                                         as the first start with files: each note a file at its slot,
                                                         pak-id.bin, pak.txt, imported.txt; the .mpk only read
    python3 tools/port/ghost.py files2mpk FOLDER --out MPK [--controller N] [--game-names PREFIX:EXT]
                                                         the pak the game would be given from the folder, as a .mpk
    python3 tools/port/ghost.py make --track T --backward 0|1 --name NAME --time SECONDS [--frames N] [--car C]
                                     --out FILE.r49ghost  a synthetic ghost the game's checks take, for tests

The formats (pakfs.h has them in full): a pak image is the native Pfs C's .mpk (32 KiB, its u16 and u32 fields
little-endian); a save file is a 128-byte header ("R49S", version 1, the kind, origin and slot, the note's directory
entry, facts for tools, a CRC-32 of the file) and the note's pages. Track numbers in the files are 0-5; the names and
labels say 1-6, as the game's file names do. Nothing here is from the game's ROM. Exit status: 0, or 1 when a command
fails (validate: the number of files refused).
"""
import argparse
import datetime
import math
import os
import re
import struct
import sys
import zlib
from functools import cmp_to_key

PAGE = 256
NOTES = 16
DATA_PAGES = 123
PAK_SIZE = 0x8000
NOTE_HEADER = 128
RUSH_GAME_CODE = 0x4E525545
RUSH_COMPANY = 0x3544
GHOST_HEADER = 88
GHOST_REVISION = 17
GHOST_TRACKS = 6
GHOST_CARS = 13
GHOST_MAX_FRAMES = 60 * 60 * 20
PROFILE_SIZE = 2064
INODE, INODE_BACKUP, DIR0 = 0x100, 0x200, 0x300
FIRST_PAGE = 5
PFS_EOF = 1
ID_COPIES = (0x20, 0x60, 0x80, 0xC0)
OTHER, PROFILE, GHOST = 0, 1, 2
ORIGIN_GAME, ORIGIN_IMPORT, ORIGIN_EXPORT = 0, 1, 2
MAX_FILE = NOTE_HEADER + DATA_PAGES * PAGE
RESERVE_NOTES, RESERVE_PAGES = 1, 24
BACKUPS = 3
EXTS = (".r49save", ".r49ghost", ".r49note")
WRITER = "ghost.py"
KIND_NAMES = ("other", "profile", "ghost")
ORIGIN_NAMES = ("game", "import", "export")
M32 = 0xFFFFFFFF


class Refused(Exception):
    """A file or an image the C would refuse, with its reason."""


# ---------------------------------------------------------------- numbers as the C has them

def u16(b, o):
    return b[o] | b[o + 1] << 8


def u32(b, o):
    return b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24


def put16(b, o, v):
    b[o:o + 2] = struct.pack("<H", v & 0xFFFF)


def put32(b, o, v):
    b[o:o + 4] = struct.pack("<I", v & M32)


def s8(v):
    return v - 256 if v >= 128 else v


def f32(x):
    """x rounded to a C float (a product of two floats is exact in a double, so f32(a * b) is the C's a * b)."""
    return struct.unpack("<f", struct.pack("<f", x))[0]


def f32_at(b, o):
    return struct.unpack_from("<f", b, o)[0]


def lrint(x):
    """lrintf in the default rounding mode: to nearest, ties to even (Python's round)."""
    return int(round(x))


def cfmt_g(x):
    """printf's %g of a float as glibc prints it."""
    if math.isnan(x):
        return "-nan" if math.copysign(1.0, x) < 0 else "nan"
    return "%g" % x


def bkey(name):
    """strcmp's order of a file name: its bytes."""
    return name.encode("utf-8", "surrogateescape")


def ascii_lower(s):
    return "".join(chr(ord(c) + 32) if "A" <= c <= "Z" else c for c in s)


def save_hash(data):
    """pc_save_hash: the save code's checksum (format_string_parse), a 32-bit word."""
    r = 0
    for v in data:
        k = v & 7
        if k == 0:
            r = (r - v) & M32
        elif k == 1:
            r |= v
        elif k == 2:
            r &= v
        elif k == 3:
            r ^= v
        elif k == 4:
            r = (r * v) & M32
        elif k == 5:
            r //= v
        else:
            r = (r + v) & M32
    return r


# ---------------------------------------------------------------- the pak's character set (for names shown and made)

PAK_CHARS = {0x0F: " "}
PAK_CHARS.update({0x10 + i: chr(48 + i) for i in range(10)})
PAK_CHARS.update({0x1A + i: chr(65 + i) for i in range(26)})
PAK_CHARS.update(dict(zip(range(0x34, 0x42), "!\"#'*+,-./:=?@")))
PAK_CODES = {c: v for v, c in PAK_CHARS.items()}


def pak_text(b):
    out = []
    for v in b:
        if v == 0:
            break
        out.append(PAK_CHARS.get(v, "\\x%02X" % v))
    return "".join(out)


def pak_encode(text, n, what):
    out = bytearray(n)
    text = text.upper()
    if len(text) > n:
        raise ValueError(f"{what} {text!r}: more than {n} characters")
    for i, c in enumerate(text):
        if c not in PAK_CODES:
            raise ValueError(f"{what} {text!r}: {c!r} is not in the pak's character set (A-Z, 0-9, space, "
                             "!\"#'*+,-./:=?@)")
        out[i] = PAK_CODES[c]
    return bytes(out)


# ---------------------------------------------------------------- the pak image (pakfile.c)

def pak_id_sums(idb, be=False):
    s = i = 0
    for j in range(0, 28, 2):
        w = idb[j] << 8 | idb[j + 1] if be else idb[j] | idb[j + 1] << 8
        s = (s + w) & 0xFFFF
        i = (i + (~w & 0xFFFF)) & 0xFFFF
    return s, i


def pak_id_valid(idb, be=False):
    s, i = pak_id_sums(idb, be)
    if be:
        return (s, i) == (idb[0x1C] << 8 | idb[0x1D], idb[0x1E] << 8 | idb[0x1F])
    return (s, i) == (u16(idb, 0x1C), u16(idb, 0x1E))


def inode_ok(page):
    """__osSumcalc from the first data page's entry (byte 10) against the page's byte 1."""
    return sum(page[2 * FIRST_PAGE:PAGE]) & 0xFF == page[1]


def pak_format():
    """pc_pak_format: a blank formatted pak (controller 1's ID)."""
    img = bytearray(PAK_SIZE)
    img[0] = 0x81
    for i in range(1, 32):
        img[i] = i
    idb = bytearray(32)
    put32(idb, 0, 0xFFFFFFFF)
    put32(idb, 4, 0x135F1A05)
    put16(idb, 0x18, 0x0001)
    idb[0x1A] = 1
    idb[0x1B] = 0
    s, i = pak_id_sums(idb)
    put16(idb, 0x1C, s)
    put16(idb, 0x1E, i)
    for c in ID_COPIES:
        img[c:c + 32] = idb
    for page in range(FIRST_PAGE, 128):
        put16(img, INODE + 2 * page, 0x0003)
    img[INODE + 1] = sum(img[INODE + 10:INODE + PAGE]) & 0xFF
    img[INODE_BACKUP:INODE_BACKUP + PAGE] = img[INODE:INODE + PAGE]
    return img


def pak_own_id(img, ch):
    """pc_pak_own_id: controller ch's own ID (ch 0-3; 0 leaves the formatted one), in the block and its copies."""
    idb = bytearray(img[0x20:0x40])
    put32(idb, 4, u32(idb, 4) ^ ((ch * 0x2545F491) & M32))
    s, i = pak_id_sums(idb)
    put16(idb, 0x1C, s)
    put16(idb, 0x1E, i)
    for c in ID_COPIES:
        img[c:c + 32] = idb


IMAGE_OK, IMAGE_N64, IMAGE_DAMAGED = 0, 1, 2


def pak_image_check(img):
    """pc_pak_image_check: (IMAGE_*, why)."""
    if len(img) != PAK_SIZE:
        if not img:
            return IMAGE_DAMAGED, "it is empty"
        return IMAGE_DAMAGED, f"it is {len(img)} bytes, not 32 KiB ({PAK_SIZE} bytes) as a controller pak is"
    host = any(pak_id_valid(img[c:c + 32]) for c in ID_COPIES)
    n64 = any(pak_id_valid(img[c:c + 32], True) for c in ID_COPIES)
    if not host and n64:
        return IMAGE_N64, "it is in N64 byte order (v5's or an emulator's)"
    if not host:
        return IMAGE_DAMAGED, "its ID block and the three copies of it are damaged"
    if not inode_ok(img[INODE:INODE + PAGE]) and not inode_ok(img[INODE_BACKUP:INODE_BACKUP + PAGE]):
        return IMAGE_DAMAGED, "its file table and the backup of it are damaged"
    return IMAGE_OK, ""


def pak_controller(img):
    """Which controller's own ID (pc_pak_own_id) the image has, 1-4, or None."""
    rnd = u32(img, 0x24) ^ 0x135F1A05
    for ch in range(4):
        if rnd == (ch * 0x2545F491) & M32:
            return ch + 1
    return None


# ---------------------------------------------------------------- notes (pakfs.c)

class Note:
    """A note: its directory entry (32 bytes, its start page zero), its slot (0-15, -1 none) and its pages."""

    __slots__ = ("dir", "slot", "data")

    def __init__(self, dirent, slot, data):
        d = bytearray(dirent)
        d[6] = d[7] = 0
        self.dir = bytes(d)
        self.slot = slot
        self.data = bytes(data)

    @property
    def pages(self):
        return len(self.data) // PAGE

    def game_code(self):
        return u32(self.dir, 0)

    def company(self):
        return u16(self.dir, 4)

    def copy(self, slot=None, dirent=None):
        return Note(self.dir if dirent is None else dirent, self.slot if slot is None else slot, self.data)


def same_key(a, b):
    """pc_note_same_key: the same codes, extension and name."""
    return a[0:6] == b[0:6] and a[12:32] == b[12:32]


def note_equal(a, b):
    """pc_note_equal: the same directory entry (its start page aside) and pages."""
    da, db = bytearray(a.dir), bytearray(b.dir)
    da[6] = da[7] = db[6] = db[7] = 0
    return da == db and a.data == b.data


class Info:
    """pc_note_info's PcNoteInfo."""

    def __init__(self):
        self.kind = OTHER
        self.track = self.backward = self.car = -1
        self.name = b""
        self.finish = 0.0
        self.id = (0, 0)


def rush_note(dirent):
    return u32(dirent, 0) == RUSH_GAME_CODE and u16(dirent, 4) == RUSH_COMPANY


def is_profile(d):
    return len(d) >= PROFILE_SIZE and d[12] == 16 and save_hash(d[4:28]) == u32(d, 0)


def is_ghost(d):
    return len(d) >= GHOST_HEADER and d[4] == GHOST_REVISION and save_hash(d[4:0x44]) == u32(d, 0)


def cstr(b):
    n = b.find(0)
    return bytes(b if n < 0 else b[:n])


def note_info(note):
    info = Info()
    if not rush_note(note.dir):
        return info
    d = note.data
    if is_ghost(d):
        info.kind = GHOST
        info.track = s8(d[6])
        info.backward = d[7]
        info.car = d[8]
        info.name = cstr(d[0x16:0x16 + 13])
        info.finish = f32_at(d, 0x38)
        info.id = (u32(d, 0x28), u32(d, 0x2C))
    elif is_profile(d):
        info.kind = PROFILE
        info.name = cstr(d[13:13 + 13])
        info.id = (u32(d, 4), u32(d, 8))
    return info


def _zlib_code(err):
    m = re.search(r"Error (-?\d+)", str(err))
    return int(m.group(1)) if m else -3


def _inflate_like_c(z, usize):
    """inflate(Z_FINISH) of raw deflate into usize + 1 bytes, as pc_ghost_check runs it: (zlib's return, total_out,
    the bytes). Exact for a stream that ends, one cut short and one that runs past the size; for a stream with bad
    data the byte count is the output up to the input byte that fails (zlib may have written a little more)."""
    d = zlib.decompressobj(-15)
    try:
        out = d.decompress(z, usize + 1)
    except zlib.error as e:
        total, inc = 0, zlib.decompressobj(-15)
        for i in range(len(z)):
            try:
                total += len(inc.decompress(z[i:i + 1], usize + 1 - total))
            except zlib.error:
                break
            if total > usize:
                break
        return _zlib_code(e), total, None
    if d.eof:
        return 1, len(out), out          # Z_STREAM_END
    if len(out) == usize + 1:            # the buffer full: the end-of-block code is still read without room
        try:
            more = d.decompress(d.unconsumed_tail, 1)
        except zlib.error:
            more = b"?"
        return (1 if d.eof and not more else -5), len(out), out
    return -5, len(out), out             # Z_BUF_ERROR: the input ran out before the stream's end


def ghost_check(d, inflate=True):
    """pc_ghost_check: None when the game takes the ghost's bytes, else why (the C's words)."""
    size = len(d)
    if size < GHOST_HEADER:
        return f"{size} bytes: shorter than a ghost's {GHOST_HEADER}-byte header"
    if d[4] != GHOST_REVISION:
        return f"its ghost header's revision is {d[4]}, not {GHOST_REVISION}"
    if save_hash(d[4:0x44]) != u32(d, 0):
        return "its ghost header's checksum is wrong"
    track = s8(d[6])
    if track < 0 or track >= GHOST_TRACKS:
        return f"its track is {track}, not a race track (0-{GHOST_TRACKS - 1})"
    if d[7] > 1:
        return f"its direction is {d[7]}, not 0 (forward) or 1 (backward)"
    if d[8] >= GHOST_CARS:
        return f"its car is {d[8]}, not one of the {GHOST_CARS} cars"
    frame, finish = f32_at(d, 0x34), f32_at(d, 0x38)
    if not (frame > 0.0 and frame <= f32(0.1)):
        return f"its frame time is {cfmt_g(frame)} s, not a retrace's"
    if not (finish >= 0.0 and finish < 36000.0):
        return f"its race time is {cfmt_g(finish)} s"
    zsize, usize = u32(d, 0x3C), u32(d, 0x40)
    if zsize == 0 or zsize > size - GHOST_HEADER:
        return f"its zipped samples are {zsize} bytes, and the note holds {size - GHOST_HEADER} after the header"
    z = bytes(d[GHOST_HEADER:GHOST_HEADER + zsize])
    if save_hash(z) != u32(d, 0x44):
        return "its samples' checksum is wrong"
    if usize == 0 or usize % 3 or usize // 3 > GHOST_MAX_FRAMES:
        return f"its samples unzip to {usize} bytes, not 1 to {GHOST_MAX_FRAMES} frames of 3 bytes"
    if not inflate:
        return None
    r, total, out = _inflate_like_c(z, usize)
    if not (r == 1 and total == usize):
        return f"its samples don't unzip to the {usize} bytes its header says (zlib {r}, {total} bytes)"
    if save_hash(out) != u32(d, 0x48):
        return "its unzipped samples' checksum is wrong"
    return None


def pak_notes(img):
    """pc_pak_notes: (the notes in directory order, the number of damaged ones left out). Refused when neither inode
    table is good."""
    if inode_ok(img[INODE:INODE + PAGE]):
        inode = INODE
    elif inode_ok(img[INODE_BACKUP:INODE_BACKUP + PAGE]):
        inode = INODE_BACKUP
    else:
        raise Refused("its file table and the backup of it are damaged")
    notes, bad = [], 0
    for slot in range(NOTES):
        e = DIR0 + 32 * slot
        if u32(img, e) == 0 or u16(img, e + 4) == 0:
            continue
        chain, seen, ok, link = [], set(), True, e + 6
        while True:
            ipage = u16(img, link)
            if ipage < FIRST_PAGE:
                ok = ipage == PFS_EOF
                break
            bank, page = img[link], img[link + 1]
            if bank != 0 or page < FIRST_PAGE or page >= 128 or page in seen or len(chain) == DATA_PAGES:
                ok = False
                break
            seen.add(page)
            chain.append(page)
            link = inode + 2 * page
        if not ok or not chain:
            bad += 1
            continue
        notes.append(Note(img[e:e + 32], slot, b"".join(bytes(img[p * PAGE:(p + 1) * PAGE]) for p in chain)))
    return notes, bad


def pak_build(notes, page0=None):
    """pc_pak_build: (the image, the note index at each slot or -1). Refused when they don't fit."""
    if len(notes) > NOTES:
        raise Refused(f"{len(notes)} notes: a pak holds {NOTES}")
    total = 0
    for n in notes:
        if n.pages < 1 or n.pages > DATA_PAGES:
            raise Refused(f"a note of {n.pages} pages")
        total += n.pages
    if total > DATA_PAGES:
        raise Refused(f"{total} pages: a pak holds {DATA_PAGES}")
    at = [-1] * NOTES
    for i, n in enumerate(notes):
        if 0 <= n.slot < NOTES and at[n.slot] < 0:
            at[n.slot] = i
    for i in range(len(notes)):
        if i not in at:
            at[at.index(-1)] = i
    img = pak_format()
    if page0 is not None:
        img[0:PAGE] = page0[:PAGE]
    page = FIRST_PAGE
    for s in range(NOTES):
        if at[s] < 0:
            continue
        n = notes[at[s]]
        e = DIR0 + 32 * s
        img[e:e + 32] = n.dir
        img[e + 6], img[e + 7] = 0, page
        for k in range(n.pages):
            img[page * PAGE:(page + 1) * PAGE] = n.data[k * PAGE:(k + 1) * PAGE]
            if k == n.pages - 1:
                put16(img, INODE + 2 * page, PFS_EOF)
            else:
                img[INODE + 2 * page], img[INODE + 2 * page + 1] = 0, page + 1
            page += 1
    img[INODE + 1] = sum(img[INODE + 10:INODE + PAGE]) & 0xFF
    img[INODE_BACKUP:INODE_BACKUP + PAGE] = img[INODE:INODE + PAGE]
    return img, at


# ---------------------------------------------------------------- save files

class FileInfo:
    def __init__(self):
        self.version = self.kind = self.origin = 0
        self.slot = -1
        self.writer = b""


def file_crc(b):
    crc = zlib.crc32(bytes(b[0:0x10]))
    crc = zlib.crc32(b"\0\0\0\0", crc)
    return zlib.crc32(bytes(b[0x14:]), crc) & M32


def save_file_make(note, origin, writer=WRITER):
    """pc_save_file_make: the file's bytes."""
    info = note_info(note)
    b = bytearray(NOTE_HEADER + len(note.data))
    b[0:4] = b"R49S"
    put16(b, 4, 1)
    put16(b, 6, NOTE_HEADER)
    b[8] = info.kind
    b[9] = origin
    b[10] = note.slot if 0 <= note.slot < NOTES else 0xFF
    b[11] = ord("L")
    put32(b, 12, len(note.data))
    b[0x14:0x34] = note.dir
    b[0x1A] = b[0x1B] = 0
    b[0x34] = info.track & 0xFF if info.track >= 0 else 0xFF
    b[0x35] = info.backward & 0xFF if info.backward >= 0 else 0xFF
    b[0x36] = info.car & 0xFF if info.car >= 0 else 0xFF
    ms = M32
    if info.kind == GHOST and info.finish >= 0 and info.finish < 36000.0:
        ms = lrint(f32(info.finish * 1000.0)) & M32
    put32(b, 0x38, ms)
    name = info.name[:15]
    b[0x3C:0x3C + len(name)] = name
    w = writer if isinstance(writer, bytes) else writer.encode("utf-8")
    w = cstr(w)[:31]
    b[0x4C:0x4C + len(w)] = w
    b[NOTE_HEADER:] = note.data
    put32(b, 0x10, file_crc(b))
    return bytes(b)


def save_file_read(b):
    """pc_save_file_read: (the note, the header's facts), or Refused with the C's reason."""
    size = len(b)
    if size < 4 or b[0:4] != b"R49S":
        raise Refused("it is not a Rush 2049 PC save file")
    if size < NOTE_HEADER:
        raise Refused(f"it is {size} bytes, shorter than the {NOTE_HEADER}-byte header")
    fi = FileInfo()
    fi.version = u16(b, 4)
    if fi.version != 1:
        raise Refused(f"it is version {fi.version} of the format (this program reads version 1)")
    if u16(b, 6) != NOTE_HEADER:
        raise Refused(f"its header says it is {u16(b, 6)} bytes, not {NOTE_HEADER}")
    data = u32(b, 12)
    if data == 0 or data % PAGE or data // PAGE > DATA_PAGES:
        raise Refused(f"it says it holds {data} bytes, not 1 to {DATA_PAGES} pages of 256")
    if size != NOTE_HEADER + data:
        raise Refused(f"it is {size} bytes, and its header says {NOTE_HEADER + data}")
    if file_crc(b) != u32(b, 0x10):
        raise Refused("its CRC is wrong (the file is damaged)")
    if b[11] != ord("L"):
        raise Refused("its note is in the other byte order ('%s')" % (chr(b[11]) if 32 <= b[11] < 127 else "?"))
    fi.kind, fi.origin = b[8], b[9]
    fi.slot = b[10] if b[10] < NOTES else -1
    fi.writer = cstr(b[0x4C:0x4C + 32])
    if fi.kind > GHOST or fi.origin > ORIGIN_EXPORT or (b[10] >= NOTES and b[10] != 0xFF):
        raise Refused("its header's kind, origin or slot is out of range")
    note = Note(b[0x14:0x34], fi.slot, b[NOTE_HEADER:])
    if note.game_code() == 0 or note.company() == 0:
        raise Refused("its directory entry has no game code")
    info = note_info(note)
    if info.kind != fi.kind:
        kinds = ("another game's note", "a profile", "a ghost")
        detail = ghost_check(bytes(note.data), False) if fi.kind == GHOST else None
        what = "not one of the game's" if info.kind == OTHER and rush_note(note.dir) else kinds[info.kind]
        raise Refused(f"its header says {kinds[fi.kind]} and the note is {what}" + (f" ({detail})" if detail else ""))
    return note, fi


def header_disagreements(b, note, fi):
    """The header's fields that differ from what pc_save_file_make writes for its note (the C's reader looks only at
    the kind; the rest is covered by the CRC alone)."""
    want = save_file_make(note, fi.origin, bytes(b[0x4C:0x6C]).split(b"\0")[0])
    fields = [("its start page", 0x1A, 0x1C), ("track", 0x34, 0x35), ("direction", 0x35, 0x36), ("car", 0x36, 0x37),
              ("byte 0x37", 0x37, 0x38), ("time", 0x38, 0x3C), ("name", 0x3C, 0x4C), ("writer", 0x4C, 0x6C),
              ("bytes 0x6C-0x7F", 0x6C, 0x80)]
    return [what for what, a, z in fields if b[a:z] != want[a:z]]


def safe_name(s):
    """pc_save_safe_name."""
    reserved = ["CON", "PRN", "AUX", "NUL"] + [f"COM{i}" for i in range(1, 10)] + [f"LPT{i}" for i in range(1, 10)]
    s = "".join("_" if ord(c) < 32 or ord(c) > 126 or c in "\\/:*?\"<>|" else c for c in s)
    s = s.lstrip(" ").rstrip(" .")
    if not s:
        s = "unnamed"
    base = s.find(".")
    base = len(s) if base < 0 else base
    for r in reserved:
        if len(r) == base and s[:base].upper() == r:
            s = s[:base] + "_" + s[base:]
            break
    return s


def finish_text(finish):
    """The time as the file names have it: 1m15.25s."""
    if finish > 0 and finish < 36000.0:
        cs = lrint(f32(finish * 100.0))
        return "%dm%02d.%02ds" % (cs // 6000, cs // 100 % 60, cs % 100)
    return "no time"


def save_file_name(note):
    """pc_save_file_name."""
    info = note_info(note)
    name = safe_name(info.name.decode("latin-1"))
    if info.kind == PROFILE:
        return f"{name}.r49save"
    if info.kind == GHOST:
        way = "backward" if info.backward else "forward"
        return f"track {info.track + 1} {way} - {name} - {finish_text(info.finish)}.r49ghost"
    return "note %08X %d.r49note" % (note.game_code(), note.slot if note.slot >= 0 else 0)


def label(note, info=None):
    """pc_store_list's label, as the F1 Saves page shows a note."""
    info = info or note_info(note)
    name = info.name.decode("latin-1")
    if info.kind == GHOST:
        cs = int(f32(f32(info.finish * 100.0) + 0.5)) if 0 < info.finish < 36000.0 else 0
        return "T%d %s %.13s %d:%02d.%02d" % (info.track + 1, "bwd" if info.backward else "fwd", name, cs // 6000,
                                               cs // 100 % 60, cs % 100)
    if info.kind == PROFILE:
        return name[:13] if name else "(no name)"
    return "note %08X" % note.game_code()


def describe(note, info=None):
    """A note in words, for info and validate."""
    info = info or note_info(note)
    name = info.name.decode("latin-1")
    if info.kind == GHOST:
        d = note.data
        frames = u32(d, 0x40) // 3
        return (f"ghost of {name!r}, track {info.track + 1} {'backward' if info.backward else 'forward'}, "
                f"{finish_text(info.finish)}, car {info.car}, {frames} frames, player id {info.id[0]:08X} "
                f"{info.id[1]:08X}, {note.pages} pages")
    if info.kind == PROFILE:
        return f"profile {name!r}, player id {info.id[0]:08X} {info.id[1]:08X}, {note.pages} pages"
    rush = " (the game's codes: a note being written?)" if rush_note(note.dir) else ""
    return f"note of game {note.game_code():08X} company {note.company():04X}{rush}, {note.pages} pages"


def dir_text(dirent):
    ext = pak_text(dirent[12:16])
    return f"{pak_text(dirent[16:32])}{'.' + ext if ext else ''}"


# ---------------------------------------------------------------- files and folders

def read_file(path, cap):
    """A file's first cap + 1 bytes (so a bigger one is told by its size)."""
    with open(path, "rb") as f:
        return f.read(cap + 1)


def write_atomic(path, data):
    """pc_file_write_atomic: a temp file written and synced, renamed over the name, the folder synced."""
    tmp = path + ".tmp"
    try:
        with open(tmp, "wb") as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    except OSError:
        try:
            os.remove(tmp)
        except OSError:
            pass
        raise
    if os.name != "nt":
        try:
            fd = os.open(os.path.dirname(path) or ".", os.O_RDONLY | getattr(os, "O_DIRECTORY", 0))
            try:
                os.fsync(fd)
            finally:
                os.close(fd)
        except OSError:
            pass


def has_ext(name, ext):
    return len(name) > len(ext) and ascii_lower(name[-len(ext):]) == ext


def is_note_file(name):
    return any(has_ext(name, e) for e in EXTS)


def list_notes(folder):
    """list_dir: the folder's note files (not dot files, not folders), in strcmp order."""
    try:
        names = os.listdir(folder)
    except OSError:
        return []
    names = [n for n in names if not n.startswith(".") and is_note_file(n)
             and not os.path.isdir(os.path.join(folder, n))]
    return sorted(names, key=bkey)


class Entry:
    def __init__(self, file, note, origin):
        self.file = file
        self.note = note
        self.info = note_info(note)
        self.origin = origin
        self.pin = 0
        self.last = 0


def backup_path(folder, file, i):
    return os.path.join(folder, "backup", f"{file}.bak" if i <= 1 else f"{file}.bak{i}")


def read_save(path):
    """A note file as load_file reads it: (note, facts) or Refused."""
    try:
        b = read_file(path, MAX_FILE)
    except OSError as e:
        raise Refused(f"it can't be read: {e.strerror}")
    if len(b) > MAX_FILE:
        raise Refused("it is bigger than any note")
    return checked_save(b)


def checked_save(b):
    """pakstore.c's read_note: save_file_read, and for a ghost the import's check (a ghost put in the folder by hand)."""
    note, fi = save_file_read(b)
    why = ghost_check(bytes(note.data)) if fi.kind == GHOST else None
    if why:
        raise Refused(why)
    return note, fi


def load_folder(folder, warn=None):
    """pc_store_open's reading, without changing anything: the entries in load order, pins.txt and pak.txt read. A
    damaged file is taken from its newest good backup, as the game would put that back."""
    entries = []
    for name in list_notes(folder):
        try:
            note, fi = read_save(os.path.join(folder, name))
        except Refused as why:
            for i in range(1, BACKUPS + 1):
                try:
                    b = read_file(backup_path(folder, name, i), MAX_FILE)
                    if len(b) > MAX_FILE:
                        continue
                    note, fi = checked_save(b)
                except (OSError, Refused):
                    continue
                if warn:
                    warn(f"{name} can't be used: {why}; its backup {os.path.basename(backup_path(folder, name, i))} "
                         "is taken in its place (the game would put it back)")
                break
            else:
                if warn:
                    warn(f"{name} can't be used: {why}; there is no good backup of it: left out")
                continue
        entries.append(Entry(name, note, fi.origin))
    read_lists(folder, entries)
    return entries


def _lines(path):
    """fgets' lines, each cut at its first CR or LF; [] when there is no file."""
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError:
        return []
    return [re.split(rb"[\r\n]", ln, maxsplit=1)[0] for ln in data.split(b"\n") if ln]


def find_entry(entries, file):
    for i, e in enumerate(entries):
        if e.file == file:
            return i
    return -1


def read_lists(folder, entries):
    """read_lists: pins.txt (each ghost once, in order) and pak.txt (slot TAB file: the pak the game had last)."""
    pin = 0
    for ln in _lines(os.path.join(folder, "pins.txt")):
        i = find_entry(entries, ln.decode("utf-8", "surrogateescape"))
        if i >= 0 and not entries[i].pin and entries[i].info.kind == GHOST:
            pin += 1
            entries[i].pin = pin
    for ln in _lines(os.path.join(folder, "pak.txt")):
        tab = ln.find(b"\t")
        m = re.match(rb"\s*([+-]?\d+)", ln)
        if tab < 0 or not m:
            continue
        s = int(m.group(1))
        if s < 0 or s >= NOTES:
            continue
        i = find_entry(entries, ln[tab + 1:].decode("utf-8", "surrogateescape"))
        if i >= 0 and not entries[i].last:
            entries[i].last = s + 1


def read_page0(folder):
    """read_page0: pak-id.bin when it is a page with a good ID block, else None."""
    try:
        b = read_file(os.path.join(folder, "pak-id.bin"), PAGE)
    except OSError:
        return None
    return b if len(b) == PAGE and pak_id_valid(b[0x20:0x40]) else None


def unique_file(folder, note, taken):
    """unique_file: the note's own name, or "NAME (2).ext"..., that no entry and no file in the folder has."""
    want = save_file_name(note)
    dot = want.rfind(".")
    base, ext = want[:dot], want[dot:]
    k = 1
    while True:
        out = want if k == 1 else f"{base[:150]} ({k}){ext}"
        if out not in taken and not os.path.exists(os.path.join(folder, out)):
            return out
        k += 1


# ---------------------------------------------------------------- the pak the game sees (pakstore.c's choice)

def place(e, k, game_names=None):
    """place: an entry's directory entry on the pak; an import's renamed (its last two characters k's digits)."""
    out = bytearray(e.note.dir)
    out[6] = out[7] = 0
    if e.origin != ORIGIN_IMPORT:
        return bytes(out)
    out[8] |= 0x02
    if game_names:
        out[16:24] = game_names[0]
        out[12:16] = game_names[1]
    n = 0
    while n < 16 and out[16 + n]:
        n += 1
    while n < 10:
        out[16 + n] = 0x10
        n += 1
    out[16 + n - 2] = 0x10 + k // 10 % 10
    out[16 + n - 1] = 0x10 + k % 10
    return bytes(out)


def want_slot(e):
    return e.last - 1 if e.last else e.note.slot


def choose(entries, game_names=None):
    """choose: [(entry index, its directory entry on the pak)] in the order taken: 1 the profiles and other notes by
    slot, 2 the pins in order, 3 the ghosts on the pak last time by slot, 4 each player's fastest ghost per track and
    way (an import apart from the player's own) while 1 note and 24 pages stay free."""
    chosen, state = [], {"pages": 0, "imports": 0}

    def try_add(i, reserve):
        e = entries[i]
        if any(i == j for j, _ in chosen):
            return
        d = place(e, state["imports"], game_names)
        if any(same_key(c, d) for _, c in chosen):
            return
        notes = NOTES - (RESERVE_NOTES if reserve else 0)
        pages = DATA_PAGES - (RESERVE_PAGES if reserve else 0)
        if len(chosen) + 1 > notes or state["pages"] + e.note.pages > pages:
            return
        chosen.append((i, d))
        state["pages"] += e.note.pages
        state["imports"] += e.origin == ORIGIN_IMPORT

    def by_slot_then_file(i):
        s = want_slot(entries[i])
        return (99 if s < 0 else s, bkey(entries[i].file))

    def by_race(a, b):
        x, y = entries[a], entries[b]
        if x.info.track != y.info.track:
            return x.info.track - y.info.track
        if x.info.backward != y.info.backward:
            return x.info.backward - y.info.backward
        if x.info.finish != y.info.finish:
            return -1 if x.info.finish < y.info.finish else 1
        fx, fy = bkey(x.file), bkey(y.file)
        return (fx > fy) - (fx < fy)

    for i in sorted((i for i, e in enumerate(entries) if e.info.kind != GHOST), key=by_slot_then_file):
        try_add(i, False)
    for pin in range(1, len(entries) + 1):
        for i, e in enumerate(entries):
            if e.pin == pin:
                try_add(i, False)
    for i in sorted((i for i, e in enumerate(entries) if e.info.kind == GHOST and e.last), key=by_slot_then_file):
        try_add(i, False)
    best = []
    for i, e in enumerate(entries):
        if e.info.kind != GHOST:
            continue
        ok = True
        for j, o in enumerate(entries):
            if not ok:
                break
            if (j == i or o.info.kind != GHOST or o.info.track != e.info.track or o.info.backward != e.info.backward
                    or o.info.id != e.info.id or o.info.name != e.info.name
                    or (o.origin == ORIGIN_IMPORT) != (e.origin == ORIGIN_IMPORT)):
                continue
            ok = o.info.finish > e.info.finish or (o.info.finish == e.info.finish and j > i)
        if ok:
            best.append(i)
    for i in sorted(best, key=cmp_to_key(by_race)):
        try_add(i, True)
    return chosen


def folder_image(entries, page0, game_names=None):
    """build(apply 0): (the image, the choice, the slots' note indexes into the choice)."""
    chosen = choose(entries, game_names)
    notes = [entries[i].note.copy(slot=want_slot(entries[i]), dirent=d) for i, d in chosen]
    img, at = pak_build(notes, page0)
    return img, chosen, at


def own_page0(controller):
    img = pak_format()
    pak_own_id(img, controller - 1)
    return bytes(img[:PAGE])


# ---------------------------------------------------------------- what a path is

def read_pak(path):
    """A .mpk's bytes (all of them: the size is told as it is)."""
    with open(path, "rb") as f:
        return f.read()


def path_kind(path):
    if os.path.isdir(path):
        return "folder"
    if ascii_lower(path).endswith(".mpk"):
        return "mpk"
    try:
        with open(path, "rb") as f:
            head = f.read(4)
        if head != b"R49S" and os.path.getsize(path) == PAK_SIZE:
            return "mpk"
    except OSError:
        pass
    return "save"


def _say(lines):
    for ln in lines:
        print(ln)


# ---------------------------------------------------------------- info

def info_save(path):
    out = []
    try:
        b = read_file(path, 1 << 24)
    except OSError as e:
        return [f"{path}: can't be read: {e.strerror}"], False
    out.append(f"{path}: {len(b)} bytes")
    if len(b) >= 0x80 and b[0:4] == b"R49S":
        crc_ok = file_crc(b) == u32(b, 0x10)
        out.append(f"  header: version {u16(b, 4)}, header size {u16(b, 6)}, kind {b[8]} "
                   f"({KIND_NAMES[b[8]] if b[8] < 3 else '?'}), origin {b[9]} "
                   f"({ORIGIN_NAMES[b[9]] if b[9] < 3 else '?'}), slot {'none' if b[10] == 0xFF else b[10]}, "
                   f"byte order {chr(b[11]) if 32 <= b[11] < 127 else '?'!r}, data {u32(b, 12)} bytes, "
                   f"CRC {u32(b, 0x10):08X} ({'right' if crc_ok else 'WRONG: computed %08X' % file_crc(b)})")
        de = b[0x14:0x34]
        out.append(f"  directory entry: game {u32(de, 0):08X}, company {u16(de, 4):04X}, status {de[8]:02X}, "
                   f"data sum {u16(de, 10):04X}, name {dir_text(de)!r}")
        ms = u32(b, 0x38)
        out.append(f"  facts: track {b[0x34] if b[0x34] != 0xFF else '-'}, direction "
                   f"{b[0x35] if b[0x35] != 0xFF else '-'}, car {b[0x36] if b[0x36] != 0xFF else '-'}, time "
                   f"{'-' if ms == M32 else str(ms) + ' ms'}, name {cstr(b[0x3C:0x4C]).decode('latin-1')!r}, "
                   f"writer {cstr(b[0x4C:0x6C]).decode('latin-1')!r}")
    try:
        note, fi = save_file_read(b)
    except Refused as why:
        out.append(f"  refused: {why}")
        return out, False
    out.append(f"  note: {describe(note)}")
    dis = header_disagreements(b, note, fi)
    if dis:
        out.append(f"  warning: the header's {', '.join(dis)} differ from the note's own (the C reads the note's)")
    if note_info(note).kind == GHOST:
        why = ghost_check(note.data)
        out.append(f"  ghost check (the samples inflated): {'ok' if why is None else 'refused: ' + why}")
        if why is not None:
            return out, False
    name = save_file_name(note)
    if name != os.path.basename(path):
        out.append(f"  its own file name: {name}")
    return out, True


def controller_text(ctl):
    return " (the port's controller %d ID)" % ctl if ctl else ""


def damaged_text(bad):
    return "; %d damaged (its pages don't chain) left out" % bad if bad else ""


def info_mpk(path):
    out = []
    try:
        img = read_pak(path)
    except OSError as e:
        return [f"{path}: can't be read: {e.strerror}"], False
    status, why = pak_image_check(img)
    out.append(f"{path}: {len(img)} bytes: {'a controller pak image' if status == IMAGE_OK else why}")
    if len(img) != PAK_SIZE or status == IMAGE_N64:
        return out, False
    good = ["0x%02X" % c for c in ID_COPIES if pak_id_valid(img[c:c + 32])]
    ctl = pak_controller(img)
    out.append(f"  ID: random word {u32(img, 0x24):08X}{controller_text(ctl)}, "
               f"device {u16(img, 0x38):04X}, banks {img[0x3A]}, version {img[0x3B]}; right sums in "
               f"{', '.join(good) if good else 'no copy'}")
    out.append(f"  file table: {'ok' if inode_ok(img[INODE:INODE + PAGE]) else 'DAMAGED'}, its backup: "
               f"{'ok' if inode_ok(img[INODE_BACKUP:INODE_BACKUP + PAGE]) else 'DAMAGED'}")
    try:
        notes, bad = pak_notes(img)
    except Refused as why:
        out.append(f"  no notes: {why}")
        return out, False
    used = sum(n.pages for n in notes)
    out.append(f"  {len(notes)} note{'s' if len(notes) != 1 else ''} in {used} of {DATA_PAGES} pages"
               f"{damaged_text(bad)}")
    for n in notes:
        out.append(f"  slot {n.slot:2d}: {dir_text(n.dir)!r}: {describe(n)}")
        if note_info(n).kind == GHOST:
            why = ghost_check(n.data)
            if why:
                out.append(f"           ghost check: {why}")
    return out, status == IMAGE_OK


def info_folder(path, game_names=None, controller=1):
    out = [f"{path}: a saves folder"]
    warnings = []
    entries = load_folder(path, warnings.append)
    page0 = read_page0(path)
    if page0 is not None:
        ctl = pak_controller(page0)
        out.append(f"  pak-id.bin: the pak's ID, random word {u32(page0, 0x24):08X}"
                   f"{controller_text(ctl)}")
    else:
        out.append("  pak-id.bin: none (or not a pak's ID): a new ID would be made")
    marker = os.path.join(path, "imported.txt")
    if os.path.exists(marker):
        first = _lines(marker)
        out.append(f"  imported.txt: {first[0].decode('utf-8', 'replace') if first else '(empty)'}")
    for w in warnings:
        out.append(f"  warning: {w}")
    try:
        _, chosen, at = folder_image(entries, page0 if page0 is not None else own_page0(controller), game_names)
        slot_of = {chosen[at[s]][0]: s for s in range(NOTES) if at[s] >= 0}
    except Refused as why:
        out.append(f"  the pak can't be built: {why}")
        slot_of = {}
    rank = {PROFILE: 0, GHOST: 1, OTHER: 2}
    order = sorted(range(len(entries)), key=lambda i: (
        rank[entries[i].info.kind], entries[i].info.track if entries[i].info.kind == GHOST else 0,
        entries[i].info.backward if entries[i].info.kind == GHOST else 0,
        entries[i].info.finish if entries[i].info.kind == GHOST else 0,
        entries[i].info.name if entries[i].info.kind != GHOST else b"", bkey(entries[i].file)))
    out.append(f"  {len(entries)} note file{'s' if len(entries) != 1 else ''}; pak = the slot on the pak the game "
               "would be given (files2mpk), last = pak.txt's")
    for i in order:
        e = entries[i]
        flags = [f"pak {slot_of[i]:2d}" if i in slot_of else "pak  -", f"last {e.last - 1:2d}" if e.last else
                 "last  -", ORIGIN_NAMES[e.origin] if e.origin < 3 else "?"]
        if e.pin:
            flags.append(f"pin {e.pin}")
        out.append(f"  {label(e.note, e.info):<24} {' '.join(flags):<28} {e.note.pages:3d} pages  {e.file}")
    return out, not warnings


def cmd_info(args):
    bad = 0
    for path in args.files:
        kind = path_kind(path)
        if kind == "folder":
            lines, ok = info_folder(path)
        elif kind == "mpk":
            lines, ok = info_mpk(path)
        else:
            lines, ok = info_save(path)
        _say(lines)
        bad += not ok
    return 1 if bad else 0


# ---------------------------------------------------------------- validate

def validate_save(path, as_import=None):
    """A save file's verdict: (None, what it is) or (why, None). A .r49ghost (or any file with as_import) as an import
    checks it (import_one: only the game's ghosts, the samples inflated); a .r49save or .r49note as the game reads a
    folder's file (pc_save_file_read), a ghost in it with its samples inflated too."""
    if as_import is None or not as_import:
        as_import = has_ext(path, ".r49ghost")
    try:
        b = read_file(path, MAX_FILE)
    except OSError as e:
        return f"it can't be read: {e.strerror}", None
    if len(b) > MAX_FILE:
        return (f"it is bigger than a note can be ({MAX_FILE} bytes)" if as_import else
                f"it is bigger than any note ({os.path.getsize(path)} bytes)"), None
    try:
        note, fi = save_file_read(b)
    except Refused as why:
        return str(why), None
    if as_import and (fi.kind != GHOST or not rush_note(note.dir)):
        return "it is not a Rush 2049 ghost (only ghosts are imported)", None
    if fi.kind == GHOST:
        why = ghost_check(note.data)
        if why:
            return why, None
    what = describe(note)
    dis = header_disagreements(b, note, fi)
    if dis:
        what += f" (warning: the header's {', '.join(dis)} differ from the note's)"
    return None, what


def cmd_validate(args):
    refused = 0
    for path in args.files:
        kind = path_kind(path)
        if kind == "folder":
            files = [os.path.join(path, n) for n in list_notes(path)]
            page0 = os.path.join(path, "pak-id.bin")
            if os.path.exists(page0) and read_page0(path) is None:
                print(f"{page0}: warning: not a pak's ID (the game makes a new one)")
            if not files:
                print(f"{path}: no note files")
        elif kind == "mpk":
            try:
                img = read_pak(path)
            except OSError as e:
                print(f"{path}: refused: it can't be read: {e.strerror}")
                refused += 1
                continue
            status, why = pak_image_check(img)
            if status == IMAGE_OK:
                try:
                    notes, bad = pak_notes(img)
                except Refused as e:
                    status, why = IMAGE_DAMAGED, str(e)
            if status != IMAGE_OK:
                print(f"{path}: refused: {why}")
                refused += 1
                continue
            extra = [f"{bad} damaged note{'s' if bad != 1 else ''} left out"] if bad else []
            for n in notes:
                if note_info(n).kind == GHOST:
                    w = ghost_check(n.data)
                    if w:
                        extra.append(f"the ghost at slot {n.slot}: {w}")
            print(f"{path}: ok: {len(notes)} note{'s' if len(notes) != 1 else ''}"
                  f"{' (warning: ' + '; '.join(extra) + ')' if extra else ''}")
            continue
        else:
            files = [path]
        for f in files:
            why, what = validate_save(f, args.as_import)
            if why:
                print(f"{f}: refused: {why}")
                refused += 1
            else:
                print(f"{f}: ok: {what}")
    return refused


# ---------------------------------------------------------------- export

def _pick(notes_named, which):
    """notes_named: [(file name, slot or None, note)]. By name, else by slot."""
    for name, _, note in notes_named:
        if name == which:
            return note, name
    if re.fullmatch(r"\d+", which):
        hits = [(n, note) for n, s, note in notes_named if s == int(which)]
        if len(hits) == 1:
            return hits[0][1], hits[0][0]
        if len(hits) > 1:
            raise Refused(f"slot {which} is more than one note's ({', '.join(h[0] for h in hits)}): name the file")
    raise Refused(f"no note {which!r} (a slot number or a file name: `info` lists them)")


def cmd_export(args):
    try:
        if os.path.isdir(args.src):
            entries = load_folder(args.src, lambda w: print(f"warning: {w}", file=sys.stderr))
            named = [(e.file, e.last - 1 if e.last else None, e.note) for e in entries]
            if not any(s is not None for _, s, _ in named):   # no pak.txt: the files' own slots
                named = [(e.file, e.note.slot if e.note.slot >= 0 else None, e.note) for e in entries]
        else:
            img = read_pak(args.src)
            status, why = pak_image_check(img)
            if status == IMAGE_N64:
                raise Refused(why)
            if status != IMAGE_OK:
                print(f"warning: {args.src}: {why}", file=sys.stderr)
            notes, _ = pak_notes(img)
            named = [(save_file_name(n), n.slot, n) for n in notes]
        note, name = _pick(named, args.note)
    except (OSError, Refused) as e:
        print(f"export: {args.src}: {e.strerror if isinstance(e, OSError) else e}", file=sys.stderr)
        return 1
    if note_info(note).kind != GHOST:
        print(f"export: {name}: only ghosts are exported", file=sys.stderr)
        return 1
    why = ghost_check(note.data)
    if why:
        print(f"warning: {name}: {why} (exported as the game would; an import will refuse it)", file=sys.stderr)
    note = note.copy(slot=-1)
    os.makedirs(args.out, exist_ok=True)
    want = save_file_name(note)
    dot = want.rfind(".")
    for k in range(1, 1000):
        path = os.path.join(args.out, want if k == 1 else f"{want[:dot]} ({k}){want[dot:]}")
        if not os.path.exists(path):
            break
    else:
        print(f"export: {args.out}: {want} and 998 more of its names are taken", file=sys.stderr)
        return 1
    try:
        write_atomic(path, save_file_make(note, ORIGIN_EXPORT, args.writer))
    except OSError as e:
        print(f"export: {path}: {e.strerror}", file=sys.stderr)
        return 1
    print(f"{name} exported to {path}")
    return 0


# ---------------------------------------------------------------- import

def import_one(folder, path, writer=WRITER, warn=None):
    """import_one: ("imported" or "already", the file in the folder), or Refused."""
    try:
        b = read_file(path, MAX_FILE)
    except OSError as e:
        raise Refused(f"it can't be read: {e.strerror}")
    if len(b) > MAX_FILE:
        raise Refused(f"it is bigger than a note can be ({MAX_FILE} bytes)")
    note, fi = save_file_read(b)
    if fi.kind != GHOST or not rush_note(note.dir):
        raise Refused("it is not a Rush 2049 ghost (only ghosts are imported)")
    why = ghost_check(note.data)
    if why:
        raise Refused(why)
    note = note.copy(slot=-1)
    os.makedirs(folder, exist_ok=True)
    entries = load_folder(folder, warn)
    for e in entries:
        if note_equal(e.note, note):
            return "already", e.file
    file = unique_file(folder, note, {e.file for e in entries})
    try:
        write_atomic(os.path.join(folder, file), save_file_make(note, ORIGIN_IMPORT, writer))
    except OSError as e:
        raise Refused(f"{file} can't be written: {e.strerror}")
    pins = [e.file for e in sorted((e for e in entries if e.pin), key=lambda e: e.pin)] + [file]
    write_atomic(os.path.join(folder, "pins.txt"), "".join(p + "\n" for p in pins).encode("utf-8", "surrogateescape"))
    return "imported", file


def cmd_import(args):
    bad = 0
    for path in args.files:
        try:
            what, file = import_one(args.to, path, args.writer, lambda w: print(f"warning: {w}", file=sys.stderr))
        except Refused as why:
            print(f"{path}: refused: {why}")
            bad += 1
            continue
        except OSError as e:
            print(f"{path}: not imported: {e.filename}: {e.strerror}")
            bad += 1
            continue
        print(f"{path}: imported as {file} (pinned)" if what == "imported" else
              f"{path}: already in the library as {file}")
    return 1 if bad else 0


# ---------------------------------------------------------------- mpk2files

def mpk2files(mpk, folder, force=False, writer=WRITER, now=None):
    """migrate: the .mpk's notes into files at their slots; the lines done. Refused (the C's words) when the .mpk
    isn't taken."""
    existing = list_notes(folder) if os.path.isdir(folder) else []
    if not force and (existing or os.path.exists(os.path.join(folder, "imported.txt"))):
        raise Refused(f"{folder} has notes already{' (imported.txt)' if not existing else ''}: the game imports an "
                      ".mpk only into a folder without them (--force adds the .mpk's notes beside them)")
    try:
        img = read_pak(mpk)
    except OSError as e:
        raise Refused(f"the controller pak {mpk} can't be read ({e.strerror}): nothing imported")
    status, why = pak_image_check(img)
    if status != IMAGE_OK:
        raise Refused(f"the controller pak {mpk} is not imported: {why}")
    try:
        notes, bad = pak_notes(img)
    except Refused as why:
        raise Refused(f"the controller pak {mpk} is not imported: {why}")
    os.makedirs(folder, exist_ok=True)
    for sub in ("import", "export", "trash", "backup"):
        os.makedirs(os.path.join(folder, sub), exist_ok=True)
    write_atomic(os.path.join(folder, "pak-id.bin"), bytes(img[:PAGE]))
    entries = load_folder(folder) if existing else []
    taken = {e.file for e in entries}
    date = (now or datetime.datetime.now()).strftime("%Y-%m-%d %H:%M:%S")
    text = [f"Imported from {mpk} on {date}: {len(notes)} note{'' if len(notes) == 1 else 's'}, "
            "the .mpk left as it was.\n"]
    done, lines = [], []
    for n in notes:
        same = [e.file for e in entries if note_equal(e.note, n)]
        if same:
            lines.append(f"slot {n.slot}: already in the folder as {same[0]}")
            done.append((n.slot, same[0]))
            text.append(f"slot {n.slot}\t{same[0]}\n")
            continue
        file = unique_file(folder, n, taken)
        write_atomic(os.path.join(folder, file), save_file_make(n, ORIGIN_GAME, writer))
        taken.add(file)
        done.append((n.slot, file))
        text.append(f"slot {n.slot}\t{file}\n")
        lines.append(f"slot {n.slot}: {file}")
    if bad:
        text.append(f"{bad} damaged note{'' if bad == 1 else 's'} left out (its pages don't chain).\n")
        lines.append(f"{bad} damaged note{'' if bad == 1 else 's'} left out (its pages don't chain)")
    write_atomic(os.path.join(folder, "imported.txt"), "".join(text).encode("utf-8", "surrogateescape"))
    write_atomic(os.path.join(folder, "pak.txt"),
                 "".join(f"{s}\t{f}\n" for s, f in done).encode("utf-8", "surrogateescape"))
    return lines


def cmd_mpk2files(args):
    try:
        lines = mpk2files(args.mpk, args.to, args.force, args.writer)
    except Refused as why:
        print(f"mpk2files: {why}", file=sys.stderr)
        return 1
    except OSError as e:
        print(f"mpk2files: {e.filename}: {e.strerror}", file=sys.stderr)
        return 1
    _say(lines)
    print(f"{args.mpk}: {len([ln for ln in lines if ln.startswith('slot')])} notes into {args.to} "
          "(the .mpk left as it was)")
    return 0


# ---------------------------------------------------------------- files2mpk

def parse_game_names(text):
    m = re.fullmatch(r"([0-9A-Fa-f]{16}):([0-9A-Fa-f]{8})", text or "")
    if not m:
        raise argparse.ArgumentTypeError("PREFIX:EXT as 16 and 8 hex digits (the pak's character set)")
    return bytes.fromhex(m.group(1)), bytes.fromhex(m.group(2))


def files2mpk(folder, out=None, controller=1, game_names=None, warn=None):
    """build(apply 0) as pc_store_export_mpk runs it: (the image, the report's lines). With out, written as
    pc_store_export_mpk writes it (the old one kept as .bak, .bak2, .bak3)."""
    if not os.path.isdir(folder):
        raise Refused(f"{folder} is not a folder")
    entries = load_folder(folder, warn)
    page0 = read_page0(folder)
    lines = []
    if page0 is None:
        page0 = own_page0(controller)
        lines.append(f"no pak-id.bin: controller {controller}'s new ID")
    img, chosen, at = folder_image(entries, page0, game_names)
    for s in range(NOTES):
        if at[s] >= 0:
            i, d = chosen[at[s]]
            e = entries[i]
            why = []
            if e.pin:
                why.append("pinned")
            if e.origin == ORIGIN_IMPORT:
                why.append(f"imported, on the pak as {dir_text(d)!r}")
            lines.append(f"slot {s:2d}: {e.file} ({label(e.note, e.info)}, {e.note.pages} pages"
                         f"{', ' + ', '.join(why) if why else ''})")
    left = [e.file for i, e in enumerate(entries) if i not in {c[0] for c in chosen}]
    pages = sum(entries[i].note.pages for i, _ in chosen)
    lines.append(f"{len(chosen)} notes in {pages} pages; {len(left)} not on the pak"
                 f"{': ' + ', '.join(left[:6]) + (' ...' if len(left) > 6 else '') if left else ''}")
    if out:
        for i in range(BACKUPS, 1, -1):
            frm = f"{out}.bak" if i == 2 else f"{out}.bak{i - 1}"
            if os.path.exists(frm):
                os.replace(frm, f"{out}.bak{i}")
        if os.path.exists(out):
            os.replace(out, f"{out}.bak")
            lines.append(f"the old {out} kept as {out}.bak")
        write_atomic(out, bytes(img))
    return img, lines


def cmd_files2mpk(args):
    try:
        _, lines = files2mpk(args.folder, args.out, args.controller, args.game_names,
                             lambda w: print(f"warning: {w}", file=sys.stderr))
    except Refused as why:
        print(f"files2mpk: {why}", file=sys.stderr)
        return 1
    except OSError as e:
        print(f"files2mpk: {e.filename}: {e.strerror}", file=sys.stderr)
        return 1
    _say(lines)
    print(f"wrote {args.out}")
    return 0


# ---------------------------------------------------------------- make

def synthetic_samples(frames):
    """Three planes of frames bytes each, as CompressGhost lays them out: gear and flags, steering, gas and brake."""
    gear = bytes(min(5, 1 + i * 5 // frames) | (0x10 if i % 600 < 30 else 0) for i in range(frames))
    steer = bytes((48 - abs(i % 192 - 96)) & 0xFF for i in range(frames))
    pedal = bytes(0xC0 if i % 192 in range(88, 104) else 0x7F for i in range(frames))
    return gear + steer + pedal


def make_ghost(track, backward, name, finish, frames=None, car=0, player_id=(0x2049, 0x11), prefix="TESTPREF",
               ext="G", slot=-1):
    """A ghost note from scratch, as CompressGhost and SaveGhost leave one: the 88-byte header, the samples zipped
    (raw deflate, level 9, 2^10 window, memLevel 4)."""
    if not 0 <= track < GHOST_TRACKS:
        raise ValueError(f"track {track}: 0-{GHOST_TRACKS - 1}")
    if backward not in (0, 1):
        raise ValueError("backward: 0 or 1")
    if not 0 <= car < GHOST_CARS:
        raise ValueError(f"car {car}: 0-{GHOST_CARS - 1}")
    raw_name = name.encode("ascii", "replace")
    if len(raw_name) > 12 or any(c < 32 or c > 126 for c in raw_name):
        raise ValueError(f"name {name!r}: 0-12 printable ASCII characters")
    finish = f32(finish)
    if not 0.0 <= finish < 36000.0:
        raise ValueError(f"time {finish}: 0 to 36000 seconds")
    frame = f32(1.0 / 60)
    if frames is None:
        frames = min(GHOST_MAX_FRAMES, max(1, lrint(finish / frame)))
    if not 1 <= frames <= GHOST_MAX_FRAMES:
        raise ValueError(f"frames {frames}: 1-{GHOST_MAX_FRAMES}")
    raw = synthetic_samples(frames)
    c = zlib.compressobj(9, zlib.DEFLATED, -10, 4)
    z = c.compress(raw) + c.flush()
    pages = (GHOST_HEADER + len(z) + PAGE - 1) // PAGE
    if pages > DATA_PAGES:
        raise ValueError(f"{pages} pages zipped: a note holds {DATA_PAGES}")
    h = bytearray(pages * PAGE)
    h[4], h[5], h[6], h[7], h[8], h[9] = GHOST_REVISION, 0, track, backward, car, 0
    h[0x16:0x16 + len(raw_name)] = raw_name
    put32(h, 0x28, player_id[0])
    put32(h, 0x2C, player_id[1])
    h[0x34:0x38] = struct.pack("<f", frame)
    h[0x38:0x3C] = struct.pack("<f", finish)
    put32(h, 0x3C, len(z))
    put32(h, 0x40, len(raw))
    h[GHOST_HEADER:GHOST_HEADER + len(z)] = z
    put32(h, 0x44, save_hash(z))
    put32(h, 0x48, save_hash(raw))
    put32(h, 0, save_hash(h[4:0x44]))
    room = 16 - len(prefix) - 2
    note_name = prefix + "".join(ch if ch.upper() in PAK_CODES else " " for ch in name)[:max(0, room)] + \
        str(track + 1) + ("B" if backward else "F")
    dirent = bytearray(32)
    put32(dirent, 0, RUSH_GAME_CODE)
    put16(dirent, 4, RUSH_COMPANY)
    dirent[8] = 0x02
    dirent[12:16] = pak_encode(ext, 4, "extension")
    dirent[16:32] = pak_encode(note_name[:16], 16, "note name")
    note = Note(dirent, slot, bytes(h))
    why = ghost_check(note.data)
    assert why is None, why
    return note


def cmd_make(args):
    try:
        pid = tuple(int(x, 16) for x in args.player_id.split(","))
        if len(pid) != 2:
            raise ValueError("--player-id: two hex words, A,B")
        note = make_ghost(args.track, args.backward, args.name, args.time, args.frames, args.car, pid, args.prefix,
                          args.ext)
    except ValueError as e:
        print(f"make: {e}", file=sys.stderr)
        return 1
    out = args.out
    if os.path.isdir(out) or out.endswith(("/", os.sep)):
        out = os.path.join(out, save_file_name(note))
    try:
        os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
        write_atomic(out, save_file_make(note, ORIGIN_EXPORT, args.writer))
    except OSError as e:
        print(f"make: {out}: {e.strerror}", file=sys.stderr)
        return 1
    print(f"{out}: {describe(note)}")
    return 0


# ---------------------------------------------------------------- the commands

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("info", help="what a .mpk, a save file or a saves folder holds")
    p.add_argument("files", nargs="+")
    p.set_defaults(fn=cmd_info)
    p = sub.add_parser("validate", help="a line a file; exit status: the number refused")
    p.add_argument("files", nargs="+")
    p.add_argument("--as-import", action="store_true",
                   help="every file as an import checks it (only the game's ghosts); a .r49ghost always is")
    p.set_defaults(fn=cmd_validate)
    p = sub.add_parser("export", help="a ghost of a .mpk or a saves folder into a .r49ghost")
    p.add_argument("src")
    p.add_argument("note", help="its slot, or its file name (as `info` shows it)")
    p.add_argument("--out", required=True, help="the folder for the .r49ghost")
    p.add_argument("--writer", default=WRITER)
    p.set_defaults(fn=cmd_export)
    p = sub.add_parser("import", help="a .r49ghost into a saves folder, as the game's import")
    p.add_argument("files", nargs="+")
    p.add_argument("--to", required=True)
    p.add_argument("--writer", default=WRITER)
    p.set_defaults(fn=cmd_import)
    p = sub.add_parser("mpk2files", help="a .mpk's notes as files, as the first start with files")
    p.add_argument("mpk")
    p.add_argument("--to", required=True)
    p.add_argument("--force", action="store_true", help="into a folder that has notes already")
    p.add_argument("--writer", default=WRITER)
    p.set_defaults(fn=cmd_mpk2files)
    p = sub.add_parser("files2mpk", help="the pak the game would be given from a saves folder, as a .mpk")
    p.add_argument("folder")
    p.add_argument("--out", required=True)
    p.add_argument("--controller", type=int, default=1, choices=(1, 2, 3, 4),
                   help="whose new ID, without pak-id.bin (default 1)")
    p.add_argument("--game-names", type=parse_game_names, metavar="PREFIX:EXT",
                   help="the game's note name prefix (8) and ghost extension (4) in hex, as "
                        "pc_store_set_game_names gives an import on the pak")
    p.set_defaults(fn=cmd_files2mpk)
    p = sub.add_parser("make", help="a synthetic ghost for tests")
    p.add_argument("--track", type=int, required=True, help="0-5")
    p.add_argument("--backward", type=int, required=True, choices=(0, 1))
    p.add_argument("--name", required=True, help="the player, 0-12 ASCII characters")
    p.add_argument("--time", type=float, required=True, help="the race time, seconds")
    p.add_argument("--frames", type=int, help="samples (default: the time at 60 a second)")
    p.add_argument("--car", type=int, default=0, help="0-12")
    p.add_argument("--player-id", default="2049,11", help="the player's id words, hex (default 2049,11)")
    p.add_argument("--prefix", default="TESTPREF", help="the note name's prefix (the pak's characters)")
    p.add_argument("--ext", default="G", help="the note's extension (the pak's characters)")
    p.add_argument("--out", required=True, help="the file, or a folder for the file's own name")
    p.add_argument("--writer", default=WRITER)
    p.set_defaults(fn=cmd_make)
    args = ap.parse_args(argv)
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
