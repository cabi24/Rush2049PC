#!/usr/bin/env python3
"""Checks of the controller paks a run of the synthetic cartridge leaves (docs/port/SYNTH.md 2, ULTRA.md).

    python3 tools/port/pak_check.py [FILE.mpk ...] [--log GAME.log] [--found N:memory|rumble ...]
                                    [--motor N ...]

Each FILE must be a controller pak as port/ultra writes one: 32 KiB, an ID block whose checksums are the native Pfs
C's (host order: little-endian u16 sums), and an ID of its own, different from every other FILE's (the game takes
two paks with one ID for one pak moved from port to port). With --log, the program's log must say that the game
read controller N's accessory, a controller pak (memory) or a rumble pak (port/ultra/si.c logs it once each time
one goes in), for each --found N:KIND, and that it turned controller N's motor on, for each --motor N (a log at -vv).
Exit status: the number of failed checks.
"""
import argparse
import struct
import sys

PAK_SIZE = 0x8000


def id_ok(block):
    """The ID block's checksums over 14 little-endian u16 words, as __osIdCheckSum sums them natively."""
    words = struct.unpack("<14H", block[:28])
    total = sum(words) & 0xFFFF
    inverse = sum(~w & 0xFFFF for w in words) & 0xFFFF
    return struct.unpack("<HH", block[28:32]) == (total, inverse)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("files", nargs="*")
    ap.add_argument("--log")
    ap.add_argument("--found", action="append", default=[], help="N:memory or N:rumble")
    ap.add_argument("--motor", action="append", default=[], type=int, help="a controller whose motor went on")
    args = ap.parse_args()
    fails = []

    def check(ok, what):
        print(f"pak_check: {'ok  ' if ok else 'FAIL'} {what}")
        if not ok:
            fails.append(what)

    ids = {}
    for path in args.files:
        try:
            data = open(path, "rb").read()
        except OSError as e:
            check(False, f"{path}: {e.strerror}")
            continue
        check(len(data) == PAK_SIZE, f"{path}: {len(data)} bytes")
        if len(data) != PAK_SIZE:
            continue
        check(id_ok(data[0x20:0x40]), f"{path}: the ID block's checksums")
        other = ids.get(data[0x20:0x40])
        check(other is None, f"{path}: an ID of its own{f' (the same as {other})' if other else ''}")
        ids.setdefault(data[0x20:0x40], path)
    if args.log:
        log = open(args.log, encoding="utf-8", errors="replace").read()
        for f in args.found:
            n, kind = f.split(":")
            what = {"memory": "the controller pak", "rumble": "the rumble pak"}[kind]
            check(f"controller {n}: the game reads {what}" in log, f"the game reads controller {n}'s {kind} pak")
        for n in args.motor:
            check(f"rumble {n} on" in log, f"the game turns controller {n}'s motor on")
    return len(fails)


if __name__ == "__main__":
    sys.exit(main())
