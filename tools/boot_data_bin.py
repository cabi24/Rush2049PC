#!/usr/bin/env python3
"""Cut an assembled boot-segment-tail routine to its exact data.bin slot.

    boot_data_bin.py ELF OUT --source kernels.s|func_8002C5E0.s [--objcopy ...]

Distilled from the project's boot-data tooling; definitions verbatim.
"""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


SOURCES = {
    "kernels.s": (0x800277D0, 0x80027DD0),
    "func_8002C5E0.s": (0x8002C5E0, 0x8002C5FC),
}


def slot_bytes(elf, source, objcopy="mips-linux-gnu-objcopy"):
    """The assembled source's exact slot bytes: its .text, whose bytes past the
    slot must be the assembler's zero padding (a nonzero byte means the source
    grew past its slot, which must fail, not be cut off)."""
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "text.bin"
        subprocess.run([objcopy, "-O", "binary", "-j", ".text", str(elf), str(out)], check=True)
        return trim_to_slot(out.read_bytes(), source)


def trim_to_slot(data, source):
    lo, hi = SOURCES[source]
    if len(data) < hi - lo or any(data[hi - lo:]):
        raise ValueError(f"{source}: assembled {len(data)} bytes; its slot holds exactly {hi - lo}")
    return data[:hi - lo]


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("elf")
    p.add_argument("out")
    p.add_argument("--source", choices=sorted(SOURCES), required=True)
    p.add_argument("--objcopy", default="mips-linux-gnu-objcopy")
    a = p.parse_args(argv)
    Path(a.out).write_bytes(slot_bytes(a.elf, a.source, a.objcopy))
    return 0


if __name__ == "__main__":
    sys.exit(main())
