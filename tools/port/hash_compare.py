#!/usr/bin/env python3
"""Compare the state hashes of two runs (docs/port/LOCKSTEP.md 5, 8).

    python3 tools/port/hash_compare.py A.log B.log [--expect-from N [--expect-region R]]
    python3 tools/port/hash_compare.py --reports A B [--symbols build/pc/pc-data/data_symbols.txt]
    python3 tools/port/hash_compare.py --check-decoupled

Two logs: each run's `--log-hash` lines (`rush2049pc: hash vi=N: STATE region HASH [ptrs=K] [unnamed=K] ...`) by
retrace. It names the first retrace whose hashes differ and the regions that differ there (the curated ones every
retrace; window, own and decoupled at the retraces the full hash was taken), or says how many retraces agree. Exit 0
when every retrace both logs hashed agrees (and they hashed the same retraces), else 1. --expect-from N (the mutation
check, RUSH_SIM_DESYNC): exit 0 when instead the first difference is at retrace N, every hashed retrace from N on
differs (the state hash, or with --expect-region R that region where it was taken), and none before.

--reports A B: two --hash-report files of one retrace (A, A.window, A.own, A.decoupled and B's): the curated regions'
bytes, the window's words (each named by data_symbols.txt's nearest symbol), own storage's and the decoupled data's
that differ. The tool for "which word made the hashes differ" across processes or OSes.

--check-decoupled: port/ultra/statehash.c's DECOUPLED list names every item of port/data/decoupled.txt, in order
(ctest hash_decoupled_list): a decoupled item the hash leaves out would be game state no full hash sees.

Stdlib only, Python 3.9+.
"""
import argparse
import bisect
import re
import sys
from pathlib import Path

import n64syms   # KSEG0: the window's N64 address (port/include/pc_n64_map.h)

ROOT = Path(__file__).resolve().parents[2]
_HASH = re.compile(r"^rush2049pc: hash vi=(\d+): ([0-9a-f]{16})((?: \S+ [0-9a-f]{16}(?: ptrs=\d+)?(?: unnamed=\d+)?)*)\s*$")
_REGION = re.compile(r" (\S+) ([0-9a-f]{16})((?: ptrs=\d+)?)((?: unnamed=\d+)?)")


def hashes(text):
    """{retrace: {"state": h, region: h, region + ".ptrs": "ptrs=k", ...}} of a log's hash lines."""
    out = {}
    for line in text.splitlines():
        m = _HASH.match(line.rstrip("\r"))
        if not m:
            continue
        d = {"state": m.group(2)}
        for r in _REGION.finditer(m.group(3)):
            d[r.group(1)] = r.group(2)
            if r.group(3):
                d[r.group(1) + ".ptrs"] = r.group(3).strip()
            if r.group(4):
                d[r.group(1) + ".unnamed"] = r.group(4).strip()
        out[int(m.group(1))] = d
    return out


def differ(a, b):
    """The regions whose hashes (or pointer counts) differ between two retraces' dicts."""
    keys = [k for k in a if k in b and a[k] != b[k]]
    keys += [k for k in a if k not in b] + [k for k in b if k not in a]
    return sorted(set(keys))


def compare(ha, hb, quiet=False):
    """(first differing retrace or None, its regions, retraces compared, note)"""
    common = sorted(set(ha) & set(hb))
    only = sorted(set(ha) ^ set(hb))
    for n in common:
        d = differ(ha[n], hb[n])
        if d:
            return n, d, common, only
    return None, [], common, only


def logs_main(a, b, expect_from, expect_region):
    ha = hashes(Path(a).read_text(encoding="utf-8", errors="replace"))
    hb = hashes(Path(b).read_text(encoding="utf-8", errors="replace"))
    if not ha or not hb:
        print(f"hash_compare: no hash lines in {a if not ha else b} (--log-hash)")
        return 1
    first, regions, common, only = compare(ha, hb)
    if expect_from is None:
        if first is None and not only:
            full = sum(1 for n in common if "window" in ha[n])
            print(f"hash_compare: same: {len(common)} retraces ({common[0]}-{common[-1]}), the full hash at {full} of them")
            return 0
        if first is None:
            print(f"hash_compare: the {len(common)} retraces both hashed agree, but only one log hashed {len(only)} "
                  f"(from retrace {only[0]}): one run ended sooner or hashed other retraces")
            return 1
        print(f"hash_compare: DIFFERS from retrace {first}: {', '.join(regions)}")
        for r in regions:
            print(f"    {r}: {ha[first].get(r, '(none)')} against {hb[first].get(r, '(none)')}")
        return 1
    # the mutation check
    key = expect_region or "state"
    bad = []
    if first != expect_from:
        bad.append(f"the first difference is at {first}, not {expect_from}" if first is not None
                   else "no difference at all")
    same_after = [n for n in common if n >= expect_from and key in ha[n] and ha[n].get(key) == hb[n].get(key)]
    if same_after:
        bad.append(f"{key} agrees again at {len(same_after)} retraces from {expect_from} on (the first {same_after[0]})")
    taken = [n for n in common if n >= expect_from and key in ha[n]]
    if not taken:
        bad.append(f"no hash of {key} from retrace {expect_from} on")
    if bad:
        print(f"hash_compare: the change at retrace {expect_from} isn't caught as it should be: {'; '.join(bad)}")
        return 1
    print(f"hash_compare: caught: the hashes agree up to retrace {expect_from - 1} and differ from {expect_from} on "
          f"({key} at all {len(taken)} retraces it was hashed; at {expect_from}: {', '.join(regions)})")
    return 0


# ---------------------------------------------------------------- --reports

def read_symbols(path):
    rows = []
    if path and Path(path).exists():
        for line in Path(path).read_text().splitlines():
            if line.startswith("0x"):
                f = line.split("\t")
                rows.append((int(f[0], 16), f[-1].split()[0], f[1]))
    rows.sort()
    return rows


def name_of(rows, addr):
    i = bisect.bisect_right([r[0] for r in rows], addr) - 1
    if i < 0:
        return f"0x{addr:08X}"
    base, name, kind = rows[i]
    return f"{name}+0x{addr - base:X} ({kind})"


def layout(report, kind):
    """The report's `own NAME N64 OFFSET SIZE` / `decoupled NAME OFFSET SIZE` lines: [(name, offset, size)]."""
    out = []
    for line in Path(report).read_text().splitlines():
        f = line.split()
        if f and f[0] == kind:
            out.append((f[1], int(f[-2]), int(f[-1])))
    return out


def curated_dump(report):
    """{region: bytes} from the report's hex dumps."""
    out, cur = {}, None
    for line in Path(report).read_text().splitlines():
        m = re.match(r"^== (\S+) \(\d+ bytes, as hashed\)", line)
        if m:
            cur = m.group(1)
            out[cur] = bytearray()
            continue
        if cur and re.match(r"^[0-9a-f]{6}:", line):
            out[cur] += bytes.fromhex(line.split(":", 1)[1].replace(" ", ""))
        elif not line.strip():
            continue
        elif not re.match(r"^[0-9a-f]{6}:", line):
            cur = None
    return out


def words_differ(a, b, limit=40):
    n = min(len(a), len(b))
    return [o for o in range(0, n - n % 4, 4) if a[o:o + 4] != b[o:o + 4]][:limit], len(a) != len(b)


def reports_main(a, b, symbols):
    rows = read_symbols(symbols)
    total = 0
    ca, cb = curated_dump(a), curated_dump(b)
    for region in ca:
        offs, size = words_differ(ca[region], cb.get(region, b""))
        for o in offs:
            print(f"  {region}+0x{o:X}: {ca[region][o:o + 4].hex()} against {cb[region][o:o + 4].hex()}")
        total += len(offs) + size
    wa, wb = Path(a + ".window"), Path(b + ".window")
    if wa.exists() and wb.exists():
        A, B = wa.read_bytes(), wb.read_bytes()
        offs, size = words_differ(A, B, limit=200)
        for o in offs:
            print(f"  window 0x{n64syms.KSEG0 + o:08X} {name_of(rows, n64syms.KSEG0 + o)}: {A[o:o + 4].hex()} against "
                  f"{B[o:o + 4].hex()}")
        total += len(offs) + size
    for kind in ("own", "decoupled"):
        fa, fb = Path(f"{a}.{kind}"), Path(f"{b}.{kind}")
        if not (fa.exists() and fb.exists()):
            continue
        A, B, lay = fa.read_bytes(), fb.read_bytes(), layout(a, kind)
        for name, off, size in lay:
            offs, _ = words_differ(A[off:off + size], B[off:off + size], limit=8)
            for o in offs:
                print(f"  {kind} {name}+0x{o:X}: {A[off + o:off + o + 8].hex()} against {B[off + o:off + o + 8].hex()}")
            total += len(offs)
    print(f"hash_compare: {total} words differ between the reports {a} and {b}")
    return 1 if total else 0


def check_decoupled():
    names = [l.split()[0] for l in (ROOT / "port/data/decoupled.txt").read_text().splitlines()
             if l.strip() and not l.startswith("#")]
    src = (ROOT / "port/ultra/statehash.c").read_text()
    m = re.search(r"#define DECOUPLED\(X\)(.*?)\n(?!\s)", src, re.S)
    listed = re.findall(r"X\((\w+)\)", m.group(1)) if m else []
    if listed != names:
        missing = [n for n in names if n not in listed]
        extra = [n for n in listed if n not in names]
        print(f"hash_compare: port/ultra/statehash.c's DECOUPLED list isn't port/data/decoupled.txt's: missing "
              f"{missing or 'none'}, not decoupled {extra or 'none'}{', order differs' if not missing and not extra else ''}")
        return 1
    print(f"hash_compare: the full hash takes all {len(names)} decoupled items")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("logs", nargs="*", help="two logs with --log-hash lines")
    ap.add_argument("--expect-from", type=int, help="the mutation check: the retrace the hashes must differ from")
    ap.add_argument("--expect-region", help="with --expect-from: the region that must differ from then on")
    ap.add_argument("--reports", nargs=2, metavar=("A", "B"), help="two --hash-report files of one retrace")
    ap.add_argument("--symbols", default=str(ROOT / "build/pc/pc-data/data_symbols.txt"))
    ap.add_argument("--check-decoupled", action="store_true")
    a = ap.parse_args()
    if a.check_decoupled:
        sys.exit(check_decoupled())
    if a.reports:
        sys.exit(reports_main(a.reports[0], a.reports[1], a.symbols))
    if len(a.logs) != 2:
        ap.error("give two logs, --reports or --check-decoupled")
    sys.exit(logs_main(a.logs[0], a.logs[1], a.expect_from, a.expect_region))


if __name__ == "__main__":
    main()
