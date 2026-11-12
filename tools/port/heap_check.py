#!/usr/bin/env python3
"""Check the game heap's exit line in a run's log (docs/port/MEMORY.md 4).

    python3 tools/port/heap_check.py LOG [--memory MIB] [--peak-above BYTES] [--floor] [--short N | --short-min N]
                                         [--none N]

rush2049pc ends a run with

    rush2049pc: heap: peak N of M bytes, free-low F (largest block L), osMemSize S MiB, the heap from B bytes in;
                A allocations, short K, none Z[ (test floor at FLOOR MiB)]

(B: where the heap's header is in RDRAM as the N64 counted it, docs/port/MEMORY.md 4.)
--memory: osMemSize is MIB and the heap runs to its end, B + M bytes (the heap really grew);
--peak-above: the heap held more than BYTES at its peak (sizes take K and M); --floor: RUSH2049_TEST_HEAP_FLOOR was
applied and the game's own blocks reached past the floor (the peak is more than the heap up to it);
--short/--short-min: the game fell back for a short heap exactly N or at least N times; --none: allocations nothing
could hold (default 0). Exit status 1 on a failed check. Stdlib only, Python 3.9+.
"""
import argparse
import re
import sys

LINE = re.compile(r"rush2049pc: heap: peak (\d+) of (\d+) bytes, free-low (\d+) \(largest block (\d+)\), "
                  r"osMemSize (\d+) MiB, the heap from (\d+) bytes in; (\d+) allocations, short (\d+), none (\d+)"
                  r"(?: \(test floor at (\d+) MiB\))?")


def size(text):
    m = re.match(r"^(\d+)([KM]?)$", text.upper())
    if not m:
        raise argparse.ArgumentTypeError(f"'{text}' is not a size (bytes, or with K or M)")
    return int(m.group(1)) << {"": 0, "K": 10, "M": 20}[m.group(2)]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("log")
    ap.add_argument("--memory", type=int)
    ap.add_argument("--peak-above", type=size)
    ap.add_argument("--floor", action="store_true")
    ap.add_argument("--short", type=int)
    ap.add_argument("--short-min", type=int)
    ap.add_argument("--none", type=int, default=0)
    a = ap.parse_args()
    lines = [m for m in map(LINE.search, open(a.log, encoding="utf-8", errors="replace")) if m]
    if not lines:
        print(f"heap_check: {a.log} has no heap line")
        sys.exit(1)
    m = lines[-1]
    peak, total, free_low, largest, mem, base = (int(m.group(i)) for i in range(1, 7))
    allocs, short, none = (int(m.group(i)) for i in range(7, 10))
    floor = int(m.group(10)) << 20 if m.group(10) else 0
    print(f"heap_check: peak {peak} of {total} bytes (from {base} bytes into RDRAM), free-low {free_low}, largest "
          f"{largest}; osMemSize {mem} MiB; {allocs} allocations, short {short}, none {none}"
          f"{f', test floor at {floor >> 20} MiB' if floor else ''}")
    bad = []
    if a.memory is not None and (mem != a.memory or base + total != a.memory << 20):
        bad.append(f"not {a.memory} MiB")
    if peak + free_low != total or base + total > mem << 20:
        bad.append("the numbers don't add up")
    if a.peak_above is not None and peak <= a.peak_above:
        bad.append(f"peak {peak} not above {a.peak_above}")
    if a.floor and not floor:
        bad.append("no test floor")
    elif a.floor and peak <= floor - base:
        bad.append(f"the peak {peak} doesn't reach past the floor ({floor >> 20} MiB, {floor - base} bytes into the "
                   f"heap)")
    if a.short is not None and short != a.short:
        bad.append(f"short {short}, not {a.short}")
    if a.short_min is not None and short < a.short_min:
        bad.append(f"short {short}, fewer than {a.short_min}")
    if none != a.none:
        bad.append(f"none {none}, not {a.none}")
    if bad:
        print("heap_check: " + "; ".join(bad))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
