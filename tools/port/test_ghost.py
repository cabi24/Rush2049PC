#!/usr/bin/env python3
"""Tests of tools/port/ghost.py: its files against the C's (port/ultra/pakfs.c, pakstore.c, pakfile.c).

    python3 tools/port/test_ghost.py [C_SCRATCH] [--c-check BIN] [--work DIR] [--keep]

Without arguments: the formats on their own (the pak's blank image and IDs, names, ghosts made and read back, every
kind of damage refused with the C's words, images of notes and the notes of images, imports, exports, the choice of
what goes on the pak, mpk -> files -> mpk). C_SCRATCH: the folder port/ultra/tests/test_pakstore.c was given (its
argv[1]), with the files it leaves: every note file the C took is taken, the ones it refused are refused, its .mpk
made into files gives the C's own files (but the writer and the CRC), and those files give its .mpk back byte for byte.
--c-check: tools/port/test_ghost_c.c built against the same C: the C's verdict on every file the tool makes, its pak
of each folder against the tool's byte for byte, and its imports against the tool's. Exit status: the number of
failed checks.
"""
import argparse
import contextlib
import hashlib
import io
import os
import random
import shutil
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ghost as G  # noqa: E402

FAILS = []
CHECKS = [0]


def check(ok, what):
    CHECKS[0] += 1
    if not ok:
        FAILS.append(what)
        print(f"FAIL {what}")
    return ok


def run(*argv):
    """ghost.py's main, its output captured: (exit status, stdout, stderr)."""
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        try:
            r = G.main([str(a) for a in argv])
        except SystemExit as e:
            r = e.code
    return r, out.getvalue(), err.getvalue()


def rd(path):
    with open(path, "rb") as f:
        return f.read()


def wr(path, data):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def digest(path):
    return hashlib.sha256(rd(path)).hexdigest()


def fix_crc(b):
    b = bytearray(b)
    G.put32(b, 0x10, G.file_crc(b))
    return bytes(b)


def same_but_writer_crc(a, b):
    """Two save files equal but the writer field and the CRC."""
    if len(a) != len(b):
        return False
    a, b = bytearray(a), bytearray(b)
    for x in (a, b):
        x[0x10:0x14] = bytes(4)
        x[0x4C:0x6C] = bytes(32)
    return a == b


class C:
    """test_ghost_c, when given."""

    def __init__(self, path):
        self.path = path

    def check(self, files):
        r = subprocess.run([self.path, "check"] + [str(f) for f in files], stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE)
        out = {}
        for ln in r.stdout.decode("utf-8", "surrogateescape").splitlines():
            f, _, why = ln.rpartition("\t")
            out[f] = why
        return out

    def mpk(self, folder, out, ch=0, names=None):
        args = [self.path, "mpk", folder, out, str(ch)]
        if names:
            args += [names[0].hex(), names[1].hex()]
        r = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return r.returncode == 0 and os.path.exists(out)

    def imports(self, folder, files):
        r = subprocess.run([self.path, "import", folder] + [str(f) for f in files], stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE)
        out = {}
        for ln in r.stdout.decode("utf-8", "surrogateescape").splitlines():
            f, _, msg = ln.partition("\t")
            out[f] = msg
        return out


# ---------------------------------------------------------------- notes of the test's own (as test_pakstore.c's)

def make_profile(player, slot):
    """test_pakstore.c's make_profile."""
    d = bytearray(32)
    G.put32(d, 0, G.RUSH_GAME_CODE)
    G.put16(d, 4, G.RUSH_COMPANY)
    d[8] = 0xA6
    d[12:16] = G.pak_encode("P", 4, "ext")
    d[16:32] = G.pak_encode(("TESTPREF" + player)[:16], 16, "name")
    data = bytearray(9 * G.PAGE)
    G.put32(data, 4, 0x2049)
    G.put32(data, 8, slot << 3 | 1)
    data[12] = 16
    nm = player.encode()[:12]
    data[13:13 + len(nm)] = nm
    G.put32(data, 0, G.save_hash(data[4:28]))
    for i in range(32, 2064):
        data[i] = (i * 7 + slot) & 0xFF
    return G.Note(d, slot, data)


def make_other(game, pages, slot, seed=1):
    d = bytearray(32)
    G.put32(d, 0, game)
    G.put16(d, 4, 0x3031)
    d[8] = 0xA6
    d[12:16] = G.pak_encode("A", 4, "ext")
    d[16:32] = G.pak_encode("OTHERGAME", 16, "name")
    x, data = seed, bytearray(pages * G.PAGE)
    for i in range(len(data)):
        x = (x * 1103515245 + 12345) & G.M32
        data[i] = x >> 16 & 0xFF
    return G.Note(d, slot, data)


def put_file(folder, name, note, origin=G.ORIGIN_GAME):
    wr(os.path.join(folder, name), G.save_file_make(note, origin, "test"))


# ---------------------------------------------------------------- the formats on their own

def test_basics():
    img = G.pak_format()
    check(G.pak_image_check(img) == (G.IMAGE_OK, ""), "a blank pak is a good image")
    check(G.pak_notes(img) == ([], 0), "a blank pak has no notes")
    check(G.u32(img, 0x24) == 0x135F1A05 and img[0] == 0x81 and img[31] == 31, "the blank pak's label and ID")
    for ch in range(4):
        x = G.pak_format()
        G.pak_own_id(x, ch)
        check(all(G.pak_id_valid(x[c:c + 32]) for c in G.ID_COPIES) and G.pak_controller(x) == ch + 1,
              f"controller {ch + 1}'s own ID: its four copies' sums right")
    check(G.save_hash(b"") == 0 and G.save_hash(bytes([6, 4])) == 24 and G.save_hash(bytes([8])) == 0xFFFFFFF8 and
          G.save_hash(bytes([6, 4, 13])) == 1 and G.save_hash(bytes([7, 1, 2])) == 2, "the save code's checksum")
    for s, want in (("A:B*C?\"<>|/\\. ", "A_B_C_______"), ("CON", "CON_"), ("  com3.txt", "com3_.txt"),
                    ("...", "unnamed"), ("lpt9", "lpt9_"), ("COM10", "COM10"), ("a\x7fb", "a_b")):
        check(G.safe_name(s) == want, f"safe name of {s!r}: {G.safe_name(s)!r}, not {want!r}")
    check(G.save_file_name(make_profile("AUX", 0)) == "AUX_.r49save", "a profile named AUX")
    g = G.make_ghost(3, 0, "ALICE", 75.25, 1200, 3)
    check(G.save_file_name(g) == "track 4 forward - ALICE - 1m15.25s.r49ghost", f"a ghost's name: "
          f"{G.save_file_name(g)}")
    check(G.save_file_name(make_other(0x4E414B45, 3, 12)) == "note 4E414B45 12.r49note", "another game's note's name")
    check(G.save_file_name(G.make_ghost(0, 1, "BOB", 0.0, 10)) == "track 1 backward - BOB - no time.r49ghost",
          "a ghost of no time")
    b = G.save_file_make(g.copy(slot=4), G.ORIGIN_GAME, "v-test")
    check(len(b) == 128 + g.pages * 256 and b[8] == G.GHOST and b[10] == 4 and b[0x34] == 3 and b[0x35] == 0 and
          b[0x36] == 3 and G.u32(b, 0x38) == 75250 and b[0x3C:0x42] == b"ALICE\0" and b[0x4C:0x53] == b"v-test\0",
          "a ghost's file header (test_pakstore.c's checks)")
    note, fi = G.save_file_read(b)
    check(G.note_equal(note, g) and note.slot == 4 and fi.kind == G.GHOST and fi.origin == 0, "read back")
    # images: four notes, one asking for a taken slot (test_pakstore.c's test_images)
    n = [make_profile("ALICE", 3), G.make_ghost(2, 1, "ALICE", 61.5, 900, slot=7), make_other(0x12345678, 5, -1),
         make_profile("BOB", 3)]
    img, at = G.pak_build(n)
    check(at[3] == 0 and at[7] == 1 and at[0] == 2 and at[1] == 3, f"slots: the own ones, then the first free: {at}")
    back, bad = G.pak_notes(img)
    check(len(back) == 4 and bad == 0 and G.pak_image_check(img)[0] == G.IMAGE_OK, "four notes back")
    check(img[0x300 + 32 * 3 + 8] == 0xA6, "the status byte kept")
    img2, _ = G.pak_build(back, img[:256])
    check(img2 == img, "notes -> image -> notes -> image: byte for byte")
    broken = bytearray(img)
    broken[0x100 + 40] ^= 0xFF
    check(len(G.pak_notes(broken)[0]) == 4, "the inode table's backup taken")
    broken[0x200 + 40] ^= 0xFF
    try:
        G.pak_notes(broken)
        check(False, "both inode tables damaged: refused")
    except G.Refused as why:
        check("file table" in str(why), f"both inode tables damaged: {why}")
    for t in range(3):
        b2 = bytearray(img)
        e = 0x300 + 32 * 7
        first = b2[e + 7]
        if t == 0:
            b2[0x100 + 2 * first], b2[0x100 + 2 * first + 1] = 0, first
        elif t == 1:
            b2[e + 7] = 2
        else:
            b2[0x100 + 2 * first], b2[0x100 + 2 * first + 1] = 1, first + 1
        b2[0x101] = sum(b2[0x10A:0x200]) & 0xFF
        k, bad = G.pak_notes(b2)
        check(len(k) == 3 and bad == 1, f"broken chain {t}: left out and counted")
    full = [make_other(0x100 + i, 7 if i < 15 else 18, i, i) for i in range(16)]
    check(len(G.pak_notes(G.pak_build(full)[0])[0]) == 16, "a full pak: 16 notes, 123 pages")
    for notes, words in ((full[:15] + [make_other(0x200, 19, 15)], "124 pages"),
                         (full + [make_other(0x300, 1, 0)], "17 notes")):
        try:
            G.pak_build(notes)
            check(False, f"{words}: refused")
        except G.Refused as why:
            check(words in str(why), f"{words}: {why}")


def ghost_cases(base):
    """(name, the file's bytes, the C's reason, the tool may add a detail) for a good ghost file base."""
    note, _ = G.save_file_read(base)
    d0 = bytes(note.data)
    usize, zsize = G.u32(d0, 0x40), G.u32(d0, 0x3C)
    out = []

    def file(mut=None, crc=True, cut=None, extra=b""):
        b = bytearray(base)
        if mut:
            mut(b)
        b = bytes(b)
        if crc:
            b = fix_crc(b)
        if cut is not None:
            b = b[:cut]
        return b + extra

    def ghost(mut, fix=True):
        def m(b):
            d = bytearray(b[128:])
            mut(d)
            if fix:
                G.put32(d, 0, G.save_hash(d[4:0x44]))
            b[128:] = d
        return m

    def setb(o, v):
        return lambda b: b.__setitem__(o, v)

    def set32(o, v):
        return lambda b: G.put32(b, o, v)

    def setf(o, v):
        return lambda b: b.__setitem__(slice(o, o + 4), struct.pack("<f", v))

    size = len(base)
    kinds = ("another game's note", "a profile", "a ghost")
    out += [
        ("truncated", file(cut=100, crc=False), "it is 100 bytes, shorter than the 128-byte header", False),
        ("short of a page", file(cut=size - 256, crc=False), f"it is {size - 256} bytes, and its header says {size}",
         False),
        ("header only", file(cut=128, crc=False), f"it is 128 bytes, and its header says {size}", False),
        ("oversized", file(extra=bytes(256)), f"it is {size + 256} bytes, and its header says {size}", False),
        ("bigger than a note", file(extra=bytes(G.MAX_FILE)), f"it is bigger than a note can be ({G.MAX_FILE} bytes)",
         False),
        ("a data byte (the CRC)", file(lambda b: b.__setitem__(200, b[200] ^ 0x55), crc=False),
         "its CRC is wrong (the file is damaged)", False),
        ("the magic", file(setb(0, ord("X"))), "it is not a Rush 2049 PC save file", False),
        ("version 2", file(lambda b: G.put16(b, 4, 2)), "it is version 2 of the format (this program reads version 1)",
         False),
        ("header size 64", file(lambda b: G.put16(b, 6, 64)), "its header says it is 64 bytes, not 128", False),
        ("the size field", file(lambda b: G.put32(b, 12, G.u32(b, 12) ^ 0x13)),
         f"it says it holds {(size - 128) ^ 0x13} bytes, not 1 to 123 pages of 256", False),
        ("the byte order", file(setb(11, ord("B"))), "its note is in the other byte order ('B')", False),
        ("a byte order of 0", file(setb(11, 0)), "its note is in the other byte order ('?')", False),
        ("the kind", file(setb(8, 1)), f"its header says {kinds[1]} and the note is {kinds[2]}", False),
        ("kind 3", file(setb(8, 3)), "its header's kind, origin or slot is out of range", False),
        ("origin 3", file(setb(9, 3)), "its header's kind, origin or slot is out of range", False),
        ("slot 0x20", file(setb(10, 0x20)), "its header's kind, origin or slot is out of range", False),
        ("no game code", file(set32(0x14, 0)), "its directory entry has no game code", False),
        ("another game's code", file(set32(0x14, 0x12345678)), f"its header says {kinds[2]} and the note is {kinds[0]}",
         False),
        ("the ghost header's checksum", file(ghost(lambda d: G.put32(d, 0, G.u32(d, 0) ^ 0x12345678), fix=False)),
         f"its header says {kinds[2]} and the note is not one of the game's", True),
        ("revision 16", file(ghost(setb(4, 16))), f"its header says {kinds[2]} and the note is not one of the game's",
         True),
        ("track 6", file(ghost(setb(6, 6))), "its track is 6, not a race track (0-5)", False),
        ("track -1", file(ghost(setb(6, 0xFF))), "its track is -1, not a race track (0-5)", False),
        ("direction 2", file(ghost(setb(7, 2))), "its direction is 2, not 0 (forward) or 1 (backward)", False),
        ("car 13", file(ghost(setb(8, 13))), "its car is 13, not one of the 13 cars", False),
        ("a frame time of 0", file(ghost(setf(0x34, 0.0))), "its frame time is 0 s, not a retrace's", False),
        ("a frame time of 0.2", file(ghost(setf(0x34, 0.2))), "its frame time is 0.2 s, not a retrace's", False),
        ("a frame time of -nan", file(ghost(set32(0x34, 0xFFC00000))), "its frame time is -nan s, not a retrace's",
         False),
        ("a NaN race time", file(ghost(set32(0x38, 0x7FC00000))), "its race time is nan s", False),
        ("a race time of -1", file(ghost(setf(0x38, -1.0))), "its race time is -1 s", False),
        ("a race time of 36000", file(ghost(setf(0x38, 36000.0))), "its race time is 36000 s", False),
        ("an infinite race time", file(ghost(set32(0x38, 0x7F800000))), "its race time is inf s", False),
        ("zipped size 0", file(ghost(set32(0x3C, 0))),
         f"its zipped samples are 0 bytes, and the note holds {size - 128 - 88} after the header", False),
        ("zipped size past the note", file(ghost(set32(0x3C, 0x10000))),
         f"its zipped samples are 65536 bytes, and the note holds {size - 128 - 88} after the header", False),
        ("the samples' checksum", file(ghost(set32(0x44, G.u32(d0, 0x44) ^ 0xABCD))), "its samples' checksum is wrong",
         False),
        ("unzipped not whole frames", file(ghost(set32(0x40, usize - 1))),
         f"its samples unzip to {usize - 1} bytes, not 1 to 72000 frames of 3 bytes", False),
        ("a billion frames", file(ghost(set32(0x40, 0xFFFFFFF0))),
         "its samples unzip to 4294967280 bytes, not 1 to 72000 frames of 3 bytes", False),
        ("72001 frames", file(ghost(set32(0x40, 72001 * 3))),
         "its samples unzip to 216003 bytes, not 1 to 72000 frames of 3 bytes", False),
        ("unzipped larger than the stream", file(ghost(set32(0x40, usize + 3))),
         f"its samples don't unzip to the {usize + 3} bytes its header says (zlib 1, {usize} bytes)", False),
        ("unzipped smaller than the stream (it would run past the buffer)", file(ghost(set32(0x40, usize - 3))),
         f"its samples don't unzip to the {usize - 3} bytes its header says (zlib -5, {usize - 2} bytes)", False),
        ("the unzipped checksum", file(ghost(set32(0x48, G.u32(d0, 0x48) ^ 1))),
         "its unzipped samples' checksum is wrong", False),
    ]

    def cut_stream(d):   # the stream cut short (its zipped size and checksum made right)
        k = zsize // 2
        d[88 + k:88 + zsize] = bytes(zsize - k)
        G.put32(d, 0x3C, k)
        G.put32(d, 0x44, G.save_hash(d[88:88 + k]))
    out.append(("the stream cut short", file(ghost(cut_stream)), None, False))

    def bad_stream(d):   # a block of bad codes in the stream (its checksum made right)
        for i in range(88 + 2, 88 + 6):
            d[i] ^= 0xFF
        G.put32(d, 0x44, G.save_hash(d[88:88 + zsize]))
    out.append(("bad data in the stream", file(ghost(bad_stream)), None, False))
    return out


def test_ghosts(work, cbin):
    made = []
    for i, (t, b, name, time, frames, car) in enumerate((
            (0, 0, "ALICE", 70.0, None, 0), (5, 1, "BOB", 123.456, None, 12), (2, 0, "C", 0.0, 1, 3),
            (3, 1, "LONGNAME1234", 35999.99, 72000, 7), (1, 0, "lower x", 59.995, 3000, 1),
            (4, 1, "A.B", 61.005, 10, 2))):
        out = os.path.join(work, "made", f"m{i}.r49ghost")
        argv = ["make", "--track", t, "--backward", b, "--name", name, "--time", time, "--car", car, "--out", out]
        if frames:
            argv += ["--frames", frames]
        r, so, se = run(*argv)
        if not check(r == 0, f"make {argv}: {se.strip()}"):
            continue
        made.append(out)
        why, _ = G.validate_save(out)
        check(why is None, f"made ghost {i} validates: {why}")
        note, fi = G.save_file_read(rd(out))
        info = G.note_info(note)
        check(info.kind == G.GHOST and info.track == t and info.backward == b and info.car == car and
              info.name == name.encode() and info.finish == G.f32(time) and fi.origin == G.ORIGIN_EXPORT and
              fi.slot == -1, f"made ghost {i}: its fields")
        want = frames or min(72000, max(1, round(G.f32(time) / G.f32(1 / 60))))
        check(G.u32(note.data, 0x40) == 3 * want and G.f32_at(note.data, 0x34) == G.f32(1 / 60) and
              note.dir[8] == 0x02 and G.u32(note.dir, 0) == G.RUSH_GAME_CODE, f"made ghost {i}: frames, retrace, "
              "status, codes")
    r, so, _ = run("make", "--track", 1, "--backward", 0, "--name", "D", "--time", 5, "--out",
                   os.path.join(work, "made"))
    check(r == 0 and os.path.exists(os.path.join(work, "made", "track 2 forward - D - 0m05.00s.r49ghost")),
          "make into a folder: the file's own name")
    for bad in (("--track", 6), ("--car", 13), ("--time", 36000), ("--frames", 72001), ("--name", "THIRTEENCHARS"),
                ("--time", -1)):
        argv = {"--track": 0, "--backward": 0, "--name": "X", "--time": 60, "--out": os.path.join(work, "x.r49ghost")}
        argv[bad[0]] = bad[1]
        r, _, _ = run("make", *[v for kv in argv.items() for v in kv])
        check(r == 1, f"make {bad}: refused")
    r, so, _ = run("validate", *made)
    check(r == 0 and so.count(": ok:") == len(made), "validate of the made ghosts: exit 0")
    if cbin:
        verdict = cbin.check(made)
        check(len(verdict) == len(made) and all(v == "ok" for v in verdict.values()),
              f"the C takes the made ghosts: {verdict}")
    # every kind of damage
    base = rd(made[0])
    cases = ghost_cases(base)
    paths = []
    for k, (what, b, want, detail) in enumerate(cases):
        p = os.path.join(work, "refused", f"r{k:02d}.r49ghost")
        wr(p, b)
        paths.append(p)
        why, _ = G.validate_save(p)
        if want is None:
            check(why is not None and why.startswith("its samples don't unzip"), f"{what}: refused: {why}")
        elif detail:
            check(why is not None and why.startswith(want + " (") and why.endswith(")"), f"{what}: {why!r}, not "
                  f"{want!r} and a detail")
        else:
            check(why == want, f"{what}: {why!r}, not {want!r}")
    r, so, _ = run("validate", *paths)
    check(r == len(paths), f"validate of {len(paths)} damaged ghosts: exit {r}")
    if cbin:
        verdict = cbin.check(paths)
        for p, (what, b, want, detail) in zip(paths, cases):
            why, _ = G.validate_save(p)
            c = verdict.get(p)
            if what == "bigger than a note":   # the C's reader says the sizes; an import says this
                check(c is not None and c.startswith(f"it is {len(b)} bytes, and its header says"), f"{what}: C {c}")
            else:   # the C names why a "ghost" isn't one, as the tool does
                check(c == why, f"{what}: the C says {c!r}, the tool {why!r}")
    return made


def test_fuzz(work, cbin, n=360):
    """Mutated ghosts (bytes in the stream, cut streams, sizes near and hostile; the checksums made right so the
    inflate is reached): the C's verdict and reason, word for word and byte count for byte count."""
    rnd = random.Random(2049)
    base = G.make_ghost(2, 1, "FUZZ", 95.5, 5000, 4)
    paths = []
    for k in range(n):
        d = bytearray(base.data)
        zsize = G.u32(d, 0x3C)
        kind = k % 6
        if kind == 0:
            for _ in range(rnd.randint(1, 4)):
                d[88 + rnd.randrange(zsize)] = rnd.randrange(256)
        elif kind == 1:
            G.put32(d, 0x3C, rnd.randrange(1, zsize))
        elif kind == 2:
            G.put32(d, 0x40, max(3, (G.u32(d, 0x40) // 3 + rnd.randint(-5, 5)) * 3))
        elif kind == 3:
            d[88 + zsize // 2 + rnd.randrange(zsize // 2)] ^= 1 << rnd.randrange(8)
        elif kind == 4:
            for j in range(88 + rnd.randrange(zsize), 88 + zsize):
                d[j] = rnd.randrange(256)
        else:
            G.put32(d, 0x40, rnd.randrange(1 << 32))
            G.put32(d, 0x3C, rnd.randrange(40000))
        zs = G.u32(d, 0x3C)
        if 0 < zs <= len(d) - 88:
            G.put32(d, 0x44, G.save_hash(d[88:88 + zs]))
        G.put32(d, 0, G.save_hash(d[4:0x44]))
        p = os.path.join(work, "fuzz", f"f{k:03d}.r49ghost")
        wr(p, G.save_file_make(G.Note(base.dir, -1, bytes(d)), G.ORIGIN_EXPORT))
        paths.append(p)
    verdict = cbin.check(paths)
    differ = [(p, G.validate_save(p)[0] or "ok", verdict.get(p)) for p in paths]
    differ = [x for x in differ if x[1] != x[2]]
    taken = sum(v == "ok" for v in verdict.values())
    check(not differ and 0 < taken < n, f"{n} mutated ghosts: the C's verdicts word for word ({taken} taken): "
          f"{differ[:3]}")


def test_flow(work, cbin, made):
    """A folder of the test's own: the choice, imports, exports, mpk -> files -> mpk; each against the C."""
    f = os.path.join(work, "lib")
    os.makedirs(f)
    put_file(f, "ALICE.r49save", make_profile("ALICE", 0))
    put_file(f, "BOB.r49save", make_profile("BOB", 1))
    put_file(f, "note 4E414B45 12.r49note", make_other(0x4E414B45, 3, 12))
    who = ("ALICE", "BOB", "CAROL")
    for i in range(30):   # test_choice's 30 ghosts: three players, five tracks, both ways
        g = G.make_ghost(i // 6 % 5, i // 3 % 2, who[i % 3], 60.0 + i, 2000 + 300 * i, i % 13,
                         player_id=(0x2049, [0x11, 0x1, 0x21][i % 3]))
        put_file(f, f"g{i:02d}.r49ghost", g)
    out = os.path.join(work, "lib.mpk")
    r, so, se = run("files2mpk", f, "--out", out)
    check(r == 0, f"files2mpk of 2 profiles, another game's note and 30 ghosts: {se}")
    img = rd(out)
    notes, bad = G.pak_notes(img)
    pages = sum(n.pages for n in notes)
    kinds = [G.note_info(n).kind for n in notes]
    check(kinds.count(G.PROFILE) == 2 and kinds.count(G.OTHER) == 1 and bad == 0, "the profiles and the other note on")
    check(len(notes) <= 15 and pages <= 99 or len(notes) == 3, f"the reserve kept: {len(notes)} notes, {pages} pages")
    check(G.pak_controller(img) == 1, "no pak-id.bin: controller 1's ID")
    r, _, _ = run("files2mpk", f, "--out", out, "--controller", 3)
    check(G.pak_controller(rd(out)) == 3 and os.path.exists(out + ".bak"), "--controller 3; the old .mpk as .bak")
    if cbin:
        for ch in (0, 2):
            cf, co, to = os.path.join(work, f"lib-c{ch}"), os.path.join(work, f"lib-c{ch}.mpk"), \
                os.path.join(work, f"lib-t{ch}.mpk")
            shutil.copytree(f, cf)
            run("files2mpk", f, "--out", to, "--controller", ch + 1)
            check(cbin.mpk(cf, co, ch) and rd(co) == rd(to), f"the choice of 33 notes, controller {ch + 1}: the C's "
                  "image byte for byte")
    # pins and the pak last time (with lines the C passes over)
    wr(os.path.join(f, "pins.txt"), b"g29.r49ghost\r\nnone.r49ghost\nALICE.r49save\ng29.r49ghost\ng17.r49ghost\n")
    wr(os.path.join(f, "pak.txt"), b"3\tg05.r49ghost\n 4\tg06.r49ghost\r\n16\tg07.r49ghost\nx\tg08.r49ghost\n"
                                   b"9\tg09.r49ghost\n2\tnone.r49ghost\n")
    r, so, _ = run("files2mpk", f, "--out", out)
    names = {G.save_file_name(n) for n in G.pak_notes(rd(out))[0]}
    g29 = G.save_file_name(G.save_file_read(rd(os.path.join(f, "g29.r49ghost")))[0])
    check(r == 0 and g29 in names, "a pinned ghost on the pak")
    slots = {n.slot: G.save_file_name(n) for n in G.pak_notes(rd(out))[0]}
    g05 = G.save_file_name(G.save_file_read(rd(os.path.join(f, "g05.r49ghost")))[0])
    check(slots.get(3) == g05, "a ghost of pak.txt at its slot")
    if cbin:
        cf, co = os.path.join(work, "lib-pins-c"), os.path.join(work, "lib-pins-c.mpk")
        shutil.copytree(f, cf)
        check(cbin.mpk(cf, co) and rd(co) == rd(out), "pins and pak.txt: the C's image byte for byte")
    # imports: a friend's ghost of a player's own name and track, one already there, a profile, garbage
    imp = os.path.join(work, "imp")
    friend = G.make_ghost(0, 0, "ALICE", 59.0, 1500, 5, player_id=(0x7777, 0x1))
    wr(os.path.join(imp, "friend.r49ghost"), G.save_file_make(friend, G.ORIGIN_EXPORT, "elsewhere"))
    again = G.save_file_read(rd(os.path.join(f, "g00.r49ghost")))[0]
    wr(os.path.join(imp, "again.r49ghost"), G.save_file_make(again.copy(slot=-1), G.ORIGIN_EXPORT, "x"))
    wr(os.path.join(imp, "profile.r49ghost"), G.save_file_make(make_profile("EVE", 0), G.ORIGIN_EXPORT, "x"))
    wr(os.path.join(imp, "garbage.r49ghost"), b"R49S garbage garbage garbage")
    wr(os.path.join(imp, "hostile.r49ghost"), ghost_cases(rd(made[0]))[-3][1])
    ins = [os.path.join(imp, n) for n in ("friend.r49ghost", "again.r49ghost", "profile.r49ghost",
                                          "garbage.r49ghost", "hostile.r49ghost")] + made[1:3]
    fc = os.path.join(work, "lib-imp-c")
    if cbin:
        shutil.copytree(f, fc)
    pins_before = rd(os.path.join(f, "pins.txt"))
    pak_before = rd(os.path.join(f, "pak.txt"))
    msgs = {}
    for p in ins:
        r, so, se = run("import", p, "--to", f)
        msgs[p] = (r, so.strip())
    check(msgs[ins[0]][0] == 0 and "imported as track 1 forward - ALICE - 0m59.00s.r49ghost" in msgs[ins[0]][1],
          f"the friend's ghost imported: {msgs[ins[0]]}")
    check(msgs[ins[1]][0] == 0 and "already in the library as g00.r49ghost" in msgs[ins[1]][1],
          f"one already there: {msgs[ins[1]]}")
    check(all(msgs[p][0] == 1 and "refused" in msgs[p][1] for p in ins[2:5]), "a profile, garbage, a hostile one: "
          "refused")
    check(rd(os.path.join(f, "pak.txt")) == pak_before, "an import leaves pak.txt")
    pins = rd(os.path.join(f, "pins.txt")).decode().splitlines()
    check(pins == ["g29.r49ghost", "g17.r49ghost", "track 1 forward - ALICE - 0m59.00s.r49ghost"] +
          [G.save_file_name(G.save_file_read(rd(p))[0]) for p in made[1:3]],
          f"pins.txt: the pins in order and the imports after them: {pins} (was {pins_before})")
    if cbin:
        cm = cbin.imports(fc, ins)
        for p in ins:
            tool = msgs[p][1].split(": ", 1)[1] if ": " in msgs[p][1] else msgs[p][1]
            c = cm.get(p, "")
            if c.startswith("Imported "):
                same = tool == f"imported as {c[9:]} (pinned)"
            elif c.startswith("Already in the library: "):
                same = tool == f"already in the library as {c[24:]}"
            else:
                same = c == f"Refused: {tool[len('refused: '):]}"
            check(same, f"import of {os.path.basename(p)}: the C says {c!r}, the tool {tool!r}")
        for name in G.list_notes(f):
            if os.path.exists(os.path.join(fc, name)):
                check(same_but_writer_crc(rd(os.path.join(f, name)), rd(os.path.join(fc, name))),
                      f"{name}: the tool's import is the C's (but the writer and CRC)")
            else:
                check(False, f"{name}: the C made no such file")
        check(sorted(G.list_notes(f)) == sorted(G.list_notes(fc)), "the same files as the C's imports")
        check(rd(os.path.join(fc, "pins.txt")) == rd(os.path.join(f, "pins.txt")), "the same pins.txt as the C's")
    r, so, se = run("files2mpk", f, "--out", out)
    img = rd(out)
    imp_notes = [n for n in G.pak_notes(img)[0] if n.dir[16:24] == G.pak_encode("TESTPREF", 8, "x")
                 and G.pak_text(n.dir[16:32]).endswith(("00", "01", "02"))]
    check(r == 0 and len(imp_notes) == 3,
          f"the three imports on the pak, renamed: {[G.dir_text(n.dir) for n in imp_notes]}")
    names = (bytes.fromhex("0102030405060708"), bytes.fromhex("09000000"))
    out2 = os.path.join(work, "lib-names.mpk")
    r, _, _ = run("files2mpk", f, "--out", out2, "--game-names", "0102030405060708:09000000")
    renamed = [n for n in G.pak_notes(rd(out2))[0] if n.dir[16:24] == names[0] and n.dir[12:16] == names[1]]
    check(r == 0 and len(renamed) == 3 and all(n.dir[8] & 2 for n in renamed), "--game-names: the game's prefix and "
          "extension on the imports")
    if cbin:
        for nm, o in ((None, out), (names, out2)):
            cf, co = os.path.join(work, f"lib-imp-c-{bool(nm)}"), os.path.join(work, f"lib-imp-c-{bool(nm)}.mpk")
            shutil.copytree(f, cf)
            check(cbin.mpk(cf, co, 0, nm) and rd(co) == rd(o),
                  f"imports on the pak{' with the game names' if nm else ''}: the C's image of the tool's folder byte "
                  "for byte")
    # exports: from the folder by name, from the .mpk by slot; a profile not exported
    ex = os.path.join(work, "export")
    r1, so1, _ = run("export", f, "g03.r49ghost", "--out", ex)
    slot = next(n.slot for n in G.pak_notes(img)[0] if G.note_info(n).kind == G.GHOST)
    r2, so2, _ = run("export", out, str(slot), "--out", ex)
    r3, _, _ = run("export", f, "ALICE.r49save", "--out", ex)
    r4, _, _ = run("export", f, "nothing", "--out", ex)
    r5, so5, _ = run("export", f, "g03.r49ghost", "--out", ex)
    exported = [os.path.join(ex, n) for n in G.list_notes(ex)]
    check(r1 == 0 and r2 == 0 and r3 == 1 and r4 == 1 and r5 == 0 and len(exported) == 3,
          f"exports: two ghosts and the first again (a name of its own), not a profile, not a missing one: {exported}")
    check(any(n.endswith(" (2).r49ghost") for n in exported), "the second export of one ghost: NAME (2)")
    for p in exported:
        note, fi = G.save_file_read(rd(p))
        check(fi.origin == G.ORIGIN_EXPORT and fi.slot == -1 and G.validate_save(p)[0] is None,
              f"{os.path.basename(p)}: origin export, no slot, valid")
    if cbin:
        verdict = cbin.check(exported)
        check(all(v == "ok" for v in verdict.values()) and len(verdict) == 3, f"the C takes the exports: {verdict}")
    # mpk -> files -> mpk with the imports' names on it
    f2 = os.path.join(work, "lib-back")
    before = digest(out)
    r, so, se = run("mpk2files", out, "--to", f2)
    check(r == 0 and digest(out) == before, f"mpk2files: done, the .mpk unchanged: {se}")
    r, _, _ = run("files2mpk", f2, "--out", os.path.join(work, "lib-back.mpk"))
    check(r == 0 and rd(os.path.join(work, "lib-back.mpk")) == rd(out), "mpk -> files -> mpk: byte for byte")
    r, _, se = run("mpk2files", out, "--to", f2)
    check(r == 1 and "has notes already" in se, f"mpk2files into a folder with notes: refused ({se.strip()})")
    r, so, se = run("mpk2files", out, "--to", f2, "--force")
    check(r == 0 and "already in the folder" in so and len(G.list_notes(f2)) == len(G.pak_notes(rd(out))[0]),
          "--force: the notes already there not doubled")
    f3 = os.path.join(work, "lib-marker")
    os.makedirs(f3)
    wr(os.path.join(f3, "imported.txt"), b"x\n")
    r, _, _ = run("mpk2files", out, "--to", f3)
    check(r == 1, "mpk2files into a folder with imported.txt: refused")
    for p in (f, out, made[0], f2):
        r, so, se = run("info", p)
        check(r == 0 and so, f"info {p}: {se.strip()}")


def padded(note, pages):
    """A ghost of exactly this many pages (the room after its samples zeros, as the game's last page has)."""
    assert pages >= note.pages
    return G.Note(note.dir, note.slot, note.data + bytes(G.PAGE * (pages - note.pages)))


def test_choice_edges(work, cbin):
    """The choice at its edges: the reserve's 24 pages and its note to the page, and two of one player's ghosts of one
    time (the first file the best), an import of that time apart."""
    def pak_of(folder, name):
        out = os.path.join(work, f"{name}.mpk")
        r, _, se = run("files2mpk", folder, "--out", out)
        check(r == 0, f"{name}: files2mpk: {se}")
        if cbin:
            cf, co = os.path.join(work, f"{name}-c"), os.path.join(work, f"{name}-c.mpk")
            shutil.copytree(folder, cf)
            check(cbin.mpk(cf, co) and rd(co) == rd(out), f"{name}: the C's image byte for byte")
        return {G.save_file_name(n) for n in G.pak_notes(rd(out))[0]}

    def folder(name, notes):
        f = os.path.join(work, name)
        os.makedirs(f)
        put_file(f, "ALICE.r49save", make_profile("ALICE", 0))
        put_file(f, "BOB.r49save", make_profile("BOB", 1))
        files = {}
        for file, note, origin in notes:
            put_file(f, file, note, origin)
            files[file] = G.save_file_name(note)
        return f, files

    # 18 pages of profiles, three ghosts of 27: 99, the reserve's edge; a page more doesn't go on
    gs = [(f"g{t}.r49ghost", padded(G.make_ghost(t, 0, "ALICE", 60.0 + t, 300), 27), 0) for t in range(3)]
    gs.append(("g9.r49ghost", G.make_ghost(5, 1, "ALICE", 70.0, 30), 0))
    check(gs[-1][1].pages == 1, "a one-page ghost")
    f, names = folder("edge-pages", gs)
    on = pak_of(f, "edge-pages")
    check(all(names[f"g{t}.r49ghost"] in on for t in range(3)) and names["g9.r49ghost"] not in on,
          "the reserve: 99 pages used, one more page not taken")
    # 2 profiles and 13 ghosts: 15 notes, the reserve's note; a 14th ghost (the last in race order) doesn't go on
    gs = [(f"n{i:02d}.r49ghost", G.make_ghost(i % 6, i // 6 % 2, ("ALICE", "BOB")[i // 12], 60.0 + i, 30), 0)
          for i in range(14)]
    f, names = folder("edge-notes", gs)
    on = pak_of(f, "edge-notes")
    check(len(on) == 15 and names["n11.r49ghost"] not in on, f"the reserve: 15 notes, the last ghost by track, way "
          f"and time (n11: track 6 backward) off ({len(on)})")
    # one player's two ghosts of one track, way and time: the first file's; an import of that time apart
    a = G.make_ghost(2, 0, "ALICE", 80.0, 100)
    b = G.make_ghost(2, 0, "ALICE", 80.0, 200, car=1)
    c = G.make_ghost(2, 0, "ALICE", 80.0, 300, car=2)
    b = G.Note(b.dir[:16] + G.pak_encode("TESTPREFALICEX3F", 16, "n"), -1, b.data)   # names of their own
    c = G.Note(c.dir[:16] + G.pak_encode("TESTPREFALICEY3F", 16, "n"), -1, c.data)
    f, names = folder("edge-tie", [("b.r49ghost", b, 0), ("a.r49ghost", a, 0), ("c.r49ghost", c, G.ORIGIN_IMPORT)])
    out = os.path.join(work, "edge-tie.mpk")
    pak_of(f, "edge-tie")
    on = [n for n in G.pak_notes(rd(out))[0] if G.note_info(n).kind == G.GHOST]
    check(len(on) == 2 and any(n.data == a.data for n in on) and any(n.data == c.data for n in on),
          "a tie: the first file's ghost (a.r49ghost) and the import of the same time")


# ---------------------------------------------------------------- against test_pakstore's files

def note_files(folder):
    """Every save file test_pakstore left that the C took: the note files and their backups (not import/refused/)."""
    out = []
    for root, dirs, files in os.walk(folder):
        if os.path.basename(root) == "refused":
            continue
        for n in files:
            if G.is_note_file(n) or any(n.endswith(e + ".bak") or (e + ".bak") in n for e in G.EXTS):
                out.append(os.path.join(root, n))
    return sorted(out)


def test_c_files(cdir, work, cbin):
    files = note_files(cdir)
    check(len(files) > 40, f"test_pakstore's files found in {cdir}: {len(files)}")
    for p in files:
        why, _ = G.validate_save(p)
        check(why is None, f"{p}: the C took it, the tool refuses it: {why}")
    refused = os.path.join(cdir, "imp", "import", "refused")
    for name, want in (("garbage.r49ghost", "it is 28 bytes, shorter than the 128-byte header"),
                       ("profile.r49ghost", "it is not a Rush 2049 ghost (only ghosts are imported)")):
        why, _ = G.validate_save(os.path.join(refused, name))
        check(why == want, f"{name}: refused as the C did: {why}")
    for n in os.listdir(os.path.join(cdir, "imp")):
        if ".corrupt-" in n:
            check(G.validate_save(os.path.join(cdir, "imp", n))[0] is not None, f"{n}: refused")
    r, so, _ = run("validate", os.path.join(cdir, "mig", "old.mpk"), os.path.join(cdir, "mig2", "bad.mpk"))
    check(r == 1 and "bad.mpk: refused: it is 5 bytes, not 32 KiB" in so, "the .mpk taken, the short one refused")
    # mig/old.mpk into files: the C's files
    mpk = os.path.join(cdir, "mig", "old.mpk")
    csaves = os.path.join(cdir, "mig", "saves", "p1")
    cfiles = csaves if G.list_notes(csaves) else os.path.join(cdir, "mig", "x")   # test_pakstore moves them to x/
    before = digest(mpk)
    t = os.path.join(work, "mig")
    r, so, se = run("mpk2files", mpk, "--to", t)
    check(r == 0 and digest(mpk) == before, f"mpk2files of old.mpk, the .mpk unchanged: {se}")
    tn, cn = G.list_notes(t), G.list_notes(cfiles)
    check(tn == cn and len(tn) == 5, f"the C's file names: {tn} / {cn}")
    for n in tn:
        a, b = rd(os.path.join(t, n)), rd(os.path.join(cfiles, n))
        check(same_but_writer_crc(a, b), f"{n}: the C's file but the writer and the CRC")
        check(G.u32(a, 0x10) == G.file_crc(a), f"{n}: the tool's CRC right")
    check(rd(os.path.join(t, "pak-id.bin")) == rd(os.path.join(csaves, "pak-id.bin")), "pak-id.bin: the C's")
    check(rd(os.path.join(t, "pak.txt")) == rd(os.path.join(csaves, "pak.txt")), "pak.txt: the C's (after its build)")
    tl = rd(os.path.join(t, "imported.txt")).decode().splitlines()
    cl = rd(os.path.join(csaves, "imported.txt")).decode().splitlines()
    check(tl[1:] == cl[1:] and tl[0].startswith(f"Imported from {mpk} on ") and
          tl[0].endswith(": 5 notes, the .mpk left as it was."), f"imported.txt: the C's lines: {tl} / {cl}")
    check(all(os.path.isdir(os.path.join(t, s)) for s in ("import", "export", "trash", "backup")), "the subfolders")
    # and back: the .mpk byte for byte
    o = os.path.join(work, "mig.mpk")
    r, so, se = run("files2mpk", t, "--out", o)
    check(r == 0 and rd(o) == rd(mpk), f"files2mpk of the tool's files: old.mpk byte for byte {se}")
    check(rd(o) == rd(os.path.join(cdir, "mig", "back.mpk")), "and the C's own export of them (back.mpk)")
    cm = os.path.join(work, "cmig")
    os.makedirs(cm)
    for n in cn + ["pak-id.bin", "pak.txt"]:
        shutil.copy(os.path.join(cfiles if n in cn else csaves, n), cm)
    r, so, se = run("files2mpk", cm, "--out", o)
    check(r == 0 and rd(o) == rd(mpk), "files2mpk of the C's own files: old.mpk byte for byte")
    # the other folders: what the C's last build left in pak.txt is what the tool's choice gives again
    for name in ("choice", "imp", "w"):
        folder = os.path.join(cdir, name)
        o = os.path.join(work, f"c-{name}.mpk")
        r, so, se = run("files2mpk", folder, "--out", o)
        if not check(r == 0, f"files2mpk of {name}: {se}"):
            continue
        img = rd(o)
        on = {n.slot: n for n in G.pak_notes(img)[0]}
        entries = G.load_folder(folder)
        last = {e.last - 1: e for e in entries if e.last}
        for s, e in last.items():
            check(s in on and G.note_equal(on[s], e.note), f"{name}: pak.txt's {e.file} at slot {s}")
        if name != "imp":   # imp/ has an import pinned since its last build: on the pak beside them
            check(len(on) == len(last), f"{name}: only pak.txt's notes on the pak ({len(on)}, {len(last)})")
        else:
            extra = [n for s, n in on.items() if s not in last]
            check(len(extra) == 1 and G.pak_text(extra[0].dir[16:32]).endswith("00"), f"imp: the pinned import beside "
                  "them, renamed")
        if cbin:
            cf, co = os.path.join(work, f"c-{name}-copy"), os.path.join(work, f"c-{name}-c.mpk")
            shutil.copytree(folder, cf)
            check(cbin.mpk(cf, co, {"choice": 0, "imp": 3, "w": 1}[name]) and rd(co) == img,
                  f"{name}: the C's image of its folder byte for byte")
    if cbin:
        verdict = cbin.check(files)
        check(all(v == "ok" for v in verdict.values()), "the C check takes them too: "
              f"{[k for k, v in verdict.items() if v != 'ok']}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("c_scratch", nargs="?", help="test_pakstore's folder of files")
    ap.add_argument("--c-check", help="test_ghost_c, built")
    ap.add_argument("--work", help="a folder for the test's files (default: a temporary one)")
    ap.add_argument("--keep", action="store_true", help="keep the test's files")
    args = ap.parse_args()
    work = args.work or tempfile.mkdtemp(prefix="test_ghost-")
    if args.work:
        shutil.rmtree(work, ignore_errors=True)
        os.makedirs(work)
    cbin = C(os.path.abspath(args.c_check)) if args.c_check else None
    try:
        test_basics()
        made = test_ghosts(work, cbin)
        if cbin:
            test_fuzz(work, cbin)
        test_flow(work, cbin, made)
        test_choice_edges(work, cbin)
        if args.c_scratch:
            test_c_files(args.c_scratch, work, cbin)
        else:
            print("skip: test_pakstore's files (no C_SCRATCH)")
        if not cbin:
            print("skip: the C's verdicts (no --c-check)")
    finally:
        if not args.keep and not args.work:
            shutil.rmtree(work, ignore_errors=True)
        elif args.keep:
            print(f"the test's files: {work}")
    print(f"{CHECKS[0] - len(FAILS)} of {CHECKS[0]} checks passed")
    return len(FAILS)


if __name__ == "__main__":
    sys.exit(main())
