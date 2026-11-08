#!/usr/bin/env python3
"""The notes on a controller pak image as port/ultra writes one (docs/port/SYNTH.md 2, ULTRA.md "Controller pak").

    python3 tools/port/pak_notes.py FILE.mpk [--note NAME.EXT ...] [--no-note NAME.EXT ...] [--notes N]
                                    [--ghost NAME.EXT:TRACK:BACKWARD ...] [--data] [--copy-to COPY]
                                    [--compare BEFORE [--changed NAME.EXT ...] [--same NAME.EXT ...]
                                    [--gone NAME.EXT ...]]

Prints the directory (16 entries of 32 bytes at 0x300, the native Pfs C's: u32 game_code, u16 company_code,
u8 bank, u8 page, u8 status, s8 reserved, u16 data_sum, ext_name[4], game_name[16], multi-byte fields in host
order, little-endian), each used note's name in ASCII through the N64 pak font (16-25 digits, 26-51 A-Z, 15 a
space, 52-65 punctuation), its pages (the inode table's chain at 0x100) and a CRC-32 of its data. A profile
(2064 bytes) shows its checksum, id and name (+13); a ghost (an 88-byte header, then its zipped samples) its
header: checksum over +4..+0x44, revision, track, backward, car, name (+0x16), finish (+0x38), zipped and unzipped
sizes, the zipped data's checksum, each checked against the game's checksum (CalcChecksumInternal).
The header and the profile are the game's file data: big-endian on the N64, host order (little-endian) here.

Checks, each a failure counted in the exit status: --note, a note of that name and extension must be there;
--no-note, it must not; --notes N, exactly N notes; --ghost NAME.EXT:TRACK:BACKWARD, a ghost note whose header
is revision 17, checks out and has that track and direction. With --compare BEFORE (an earlier image of the pak),
each note is listed as new, gone, the same or changed in place (the same directory entry, start page and pages,
other data), and --changed, --same and --gone check that of a note. --copy-to writes a copy of FILE (for a later
run's --compare; a name not ending in .mpk is not deleted by synth_paths.py).
"""
import argparse
import struct
import sys
import zlib

PAK_SIZE = 0x8000
FONT = {0: "", 15: " "}
FONT.update({16 + i: chr(ord("0") + i) for i in range(10)})
FONT.update({26 + i: chr(ord("A") + i) for i in range(26)})
FONT.update({52 + i: ch for i, ch in enumerate("!\"#'*+,-./:=?@")})


def text(codes):
    out = ""
    for c in codes:
        if c == 0:
            break
        out += FONT.get(c, f"<{c}>")
    return out.rstrip()


def game_sum(data):
    """CalcChecksumInternal (the save code's _CalcChecksum, hash.c): each byte, by its low three bits, subtracted,
    or'ed, and'ed, xor'ed, multiplied, divided into or added to a 32-bit result."""
    r = 0
    for v in data:
        k = v & 7
        if k == 0:
            r -= v
        elif k == 1:
            r |= v
        elif k == 2:
            r &= v
        elif k == 3:
            r ^= v
        elif k == 4:
            r *= v
        elif k == 5:
            r //= v
        else:
            r += v
        r &= 0xFFFFFFFF
    return r


def chain(img, bank, page):
    """The note's pages: the inode table (page 1) from its start page; host-order u16 entries, 1 the end."""
    pages, seen = [], set()
    while True:
        if bank != 0 or not 5 <= page < 128 or page in seen:
            return pages, f"bad page {bank}:{page}"
        pages.append(page)
        seen.add(page)
        b0, b1 = img[0x100 + 2 * page], img[0x101 + 2 * page]
        if struct.unpack("<H", bytes((b0, b1)))[0] == 1:
            return pages, None
        bank, page = b0, b1


def notes(img):
    out = []
    for i in range(16):
        e = img[0x300 + 32 * i:0x320 + 32 * i]
        game_code, company, bank, page, status, _, data_sum = struct.unpack("<IHBBBbH", e[:12])
        if game_code == 0 and company == 0:
            continue
        name, ext = text(e[16:32]), text(e[12:16])
        pages, err = chain(img, bank, page)
        data = b"".join(img[256 * p:256 * p + 256] for p in pages)
        out.append({"index": i, "game_code": game_code, "company": company, "start": (bank, page), "status": status,
                    "data_sum": data_sum, "name": name, "ext": ext, "pages": pages, "err": err, "data": data,
                    "raw": e})
    return out


def ghost_header(d):
    h = {"sum": struct.unpack("<I", d[0:4])[0], "revision": d[4], "flag": struct.unpack("b", d[5:6])[0],
         "track": struct.unpack("b", d[6:7])[0], "backward": d[7], "car": d[8],
         "name": d[0x16:0x23].split(b"\0")[0].decode("latin-1"),
         "finish": struct.unpack("<f", d[0x38:0x3C])[0]}
    h["zipped"], h["unzipped"], h["zsum"], h["usum"] = struct.unpack("<IIII", d[0x3C:0x4C])
    h["sum_ok"] = h["sum"] == game_sum(d[4:0x44])
    h["zsum_ok"] = 0x58 + h["zipped"] <= len(d) and h["zsum"] == game_sum(d[0x58:0x58 + h["zipped"]])
    return h


def show(path, img, with_data):
    ns = notes(img)
    print(f"pak_notes: {path}: {len(ns)} note(s), {sum(len(n['pages']) for n in ns)} of 123 pages used")
    for n in ns:
        gc = n["game_code"].to_bytes(4, "big").decode("latin-1")
        co = n["company"].to_bytes(2, "big").decode("latin-1")
        print(f"  entry {n['index']:2}: {n['name']!r}.{n['ext']!r}  game {gc!r} (0x{n['game_code']:08X}) company "
              f"{co!r} (0x{n['company']:04X}), start {n['start'][0]}:{n['start'][1]}, status 0x{n['status']:02X}, "
              f"data_sum {n['data_sum']}, {len(n['pages'])} pages {n['pages'][:1]}..{n['pages'][-1:]}"
              f"{', ' + n['err'] if n['err'] else ''}, data CRC-32 {zlib.crc32(n['data']):08X}")
        print(f"    directory entry: {n['raw'].hex(' ')}")
        d = n["data"]
        if len(n["pages"]) == 9 and d[12] == 16:   # a profile: 2064 bytes, its version byte 16 at +12
            s = struct.unpack("<I", d[0:4])[0]
            ok = s == game_sum(d[4:28])
            ids = struct.unpack("<II", d[4:12])
            print(f"    profile: checksum 0x{s:08X} ({'ok' if ok else 'BAD'}), id 0x{ids[0]:08X} 0x{ids[1]:08X}, "
                  f"name {d[13:26].split(bytes(1))[0].decode('latin-1')!r}")
        elif len(d) >= 0x58 and d[4] == 17:
            h = ghost_header(d)
            print(f"    ghost header: checksum 0x{h['sum']:08X} ({'ok' if h['sum_ok'] else 'BAD'}), revision "
                  f"{h['revision']}, flag {h['flag']}, track {h['track']}, backward {h['backward']}, car {h['car']}, "
                  f"name {h['name']!r}, finish {h['finish']:.3f} s, zipped {h['zipped']} B, unzipped "
                  f"{h['unzipped']} B, zipped checksum 0x{h['zsum']:08X} ({'ok' if h['zsum_ok'] else 'BAD'})")
            print(f"    first 88 bytes: {d[:0x58].hex(' ')}")
        if with_data:
            for off in range(0, len(d), 32):
                if any(d[off:off + 32]):
                    print(f"    +0x{off:04X}: {d[off:off + 32].hex(' ')}")
    return ns


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("file")
    ap.add_argument("--note", action="append", default=[], help="NAME.EXT that must be there")
    ap.add_argument("--no-note", action="append", default=[], help="NAME.EXT that must not")
    ap.add_argument("--notes", type=int, help="the number of notes")
    ap.add_argument("--ghost", action="append", default=[], help="NAME.EXT:TRACK:BACKWARD, a valid ghost")
    ap.add_argument("--data", action="store_true", help="print each note's data (its nonzero 32-byte rows)")
    ap.add_argument("--copy-to", help="write a copy of FILE here")
    ap.add_argument("--compare", help="an earlier image of the pak")
    ap.add_argument("--changed", action="append", default=[], help="NAME.EXT changed in place since --compare")
    ap.add_argument("--same", action="append", default=[], help="NAME.EXT the same as in --compare")
    ap.add_argument("--gone", action="append", default=[], help="NAME.EXT in --compare and gone now")
    args = ap.parse_args()
    img = open(args.file, "rb").read()
    if len(img) != PAK_SIZE:
        print(f"pak_notes: FAIL {args.file}: {len(img)} bytes, not 32 KiB")
        return 1
    ns = show(args.file, img, args.data)
    by = {f"{n['name']}.{n['ext']}": n for n in ns}
    fails = 0

    def check(ok, what):
        nonlocal fails
        print(f"pak_notes: {'ok  ' if ok else 'FAIL'} {what}")
        fails += not ok

    for n in ns:
        check(n["err"] is None, f"{n['name']}.{n['ext']}: its page chain")
    for name in args.note:
        check(name in by, f"a note {name}")
    for name in args.no_note:
        check(name not in by, f"no note {name}")
    if args.notes is not None:
        check(len(ns) == args.notes, f"{args.notes} note(s)")
    for g in args.ghost:
        name, track, back = g.rsplit(":", 2)
        n = by.get(name)
        h = ghost_header(n["data"]) if n and len(n["data"]) >= 0x58 else None
        check(h is not None and h["revision"] == 17 and h["sum_ok"] and h["zsum_ok"] and h["track"] == int(track)
              and h["backward"] == int(back), f"a ghost {name}: revision 17, its checksums, track {track}, "
              f"backward {back}")
    if args.compare:
        before = {f"{n['name']}.{n['ext']}": n for n in notes(open(args.compare, "rb").read())}
        how = {}
        for name in sorted(set(before) | set(by)):
            a, b = before.get(name), by.get(name)
            if not a:
                how[name] = "new"
            elif not b:
                how[name] = "gone"
            elif a["data"] == b["data"] and a["raw"] == b["raw"]:
                how[name] = "the same"
            elif (a["index"], a["start"], a["pages"]) == (b["index"], b["start"], b["pages"]):
                how[name] = "changed in place"
            else:
                how[name] = "moved"
            print(f"pak_notes: since {args.compare}: {name}: {how[name]}"
                  f"{'' if how[name] != 'changed in place' else ', bytes ' + changed_spans(a['data'], b['data'])}")
        for name in args.changed:
            check(how.get(name) == "changed in place", f"{name} changed in place since {args.compare}")
        for name in args.same:
            check(how.get(name) == "the same", f"{name} the same as in {args.compare}")
        for name in args.gone:
            check(how.get(name) == "gone", f"{name} gone since {args.compare}")
    if args.copy_to:
        open(args.copy_to, "wb").write(img)
    return fails


def changed_spans(a, b):
    """The byte ranges where a and b differ, as +start..end."""
    spans, i = [], 0
    while i < min(len(a), len(b)):
        if a[i] != b[i]:
            j = i
            while j < min(len(a), len(b)) and a[j] != b[j]:
                j += 1
            if spans and i - spans[-1][1] < 8:
                spans[-1][1] = j
            else:
                spans.append([i, j])
            i = j
        else:
            i += 1
    return ", ".join(f"+0x{x:X}..0x{y:X}" for x, y in spans[:12]) + (" ..." if len(spans) > 12 else "")


if __name__ == "__main__":
    sys.exit(main())
