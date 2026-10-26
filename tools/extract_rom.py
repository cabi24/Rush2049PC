#!/usr/bin/env python3
"""Extract the ROM-derived inputs of the build from your own cartridge image.

    python3 tools/extract_rom.py [baserom.us.z64]

Nothing below is shipped in this repository; every file is cut or inflated
from the user's ROM and checked against a recorded SHA-256 before use:

    assets/us/ipl3.bin        ROM 0x40..0x1000 (IPL3 boot code)
    assets/us/data.bin        ROM 0x283D0..0xC00000 (the data segment: RSP
                              microcode, static data, the three compressed
                              code streams, cartridge assets)
    build/game_code.bin       the game-code image, raw DEFLATE at ROM 0xB0CB10,
                              647,072 B at 0x80086A50
    build/ovl/a/image.bin     runtime image A, raw DEFLATE at ROM 0xB5C534
    build/ovl/b/image.bin     runtime image B, raw DEFLATE at ROM 0xB6FEC4

The three images are only consulted for data: their .data/.rodata and the
alignment runs between functions are .incbin'd, and each compiled function's
own literals/jump tables are verified and placed against them. Every code word
comes from src/ (C) or asm/us/ (the assembly targets).
"""
import hashlib
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROM_SHA1 = "3f99351d7bb61656614bdb2aa1a90cfe55d1922c"

SLICES = (
    ("assets/us/ipl3.bin", 0x40, 0x1000,
     None),
    ("assets/us/data.bin", 0x283D0, 0xC00000,
     None),
)
STREAMS = (
    # path, ROM offset, stream length, image size, image sha256
    ("build/game_code.bin", 0xB0CB10, 326180, 647072,
     "bf7da3fa6283428a97372250cd4076d15e9eae10f9d5709c0387fe0742d43a1d"),
    ("build/ovl/a/image.bin", 0xB5C534, 80272, 194128,
     "0d6702c320df84cc6cbc3c5967dde08d44b6e476e110667fe2a43dc2dd536667"),
    ("build/ovl/b/image.bin", 0xB6FEC4, 23639, 43888,
     "b55fc2d1b22eb1ebdf01286a69a181b496da7b45ff7aec888ff74b7db748e7cd"),
)


def write(path, data):
    path = ROOT / path
    if path.is_file() and path.read_bytes() == data:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return True


def main(argv):
    rom_path = Path(argv[0]) if argv else ROOT / "baserom.us.z64"
    try:
        rom = rom_path.read_bytes()
    except OSError as exc:
        sys.exit(f"extract_rom: cannot read {rom_path}: {exc}")
    sha1 = hashlib.sha1(rom).hexdigest()
    if sha1 != ROM_SHA1:
        sys.exit(f"extract_rom: {rom_path} has SHA-1 {sha1}, expected {ROM_SHA1} "
                 "(San Francisco Rush 2049, USA, big-endian .z64)")
    for path, lo, hi, _ in SLICES:
        if write(path, rom[lo:hi]):
            print(f"extract_rom: {path} ({hi - lo} B)")
    for path, offset, length, size, sha in STREAMS:
        z = zlib.decompressobj(-15)
        image = z.decompress(rom[offset:offset + length])
        if not z.eof or z.unused_data or len(image) != size \
                or hashlib.sha256(image).hexdigest() != sha:
            sys.exit(f"extract_rom: the stream at ROM 0x{offset:X} is not the expected image")
        if write(path, image):
            print(f"extract_rom: {path} ({size} B, inflated from ROM 0x{offset:X})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
