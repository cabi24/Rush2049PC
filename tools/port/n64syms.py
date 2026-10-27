"""Shared helpers for the PC port tools: the decompilation's symbol file, N64 memory regions, nm output.

Stdlib only, Python 3.9+. Used by census.py and gen_bss.py.
"""
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# N64 virtual memory regions (docs/memory_map.md). Both runtime images load at 0x8038A400.
REGIONS = [
    (0x80000000, 0x80000400, "low memory"),
    (0x80000400, 0x800277D0, "boot code"),
    (0x800277D0, 0x8002E8E0, "boot data"),
    (0x8002E8E0, 0x80086A50, "boot bss"),
    (0x80086A50, 0x8010FD7C, "game code"),
    (0x8010FD7C, 0x801249F0, "game data"),
    (0x801249F0, 0x8017A640, "game bss"),
    (0x8038A400, 0x80400000, "overlay"),
]
BSS_REGIONS = {"boot bss": (0x8002E8E0, 0x80086A50), "game bss": (0x801249F0, 0x8017A640)}

_ADDR_NAME = re.compile(r"^(?:D|func|sym|jtbl)_([0-9A-Fa-f]{8})$")
_SYM_LINE = re.compile(r"^\s*([A-Za-z_]\w*)\s*=\s*0x([0-9A-Fa-f]+)\s*;\s*(?://(.*))?$")
_SIZE = re.compile(r"\bsize:(0x[0-9A-Fa-f]+|\d+)")


def region_of(addr):
    for lo, hi, name in REGIONS:
        if lo <= addr < hi:
            return name
    return "other"


def load_symbols(path=ROOT / "symbol_addrs.us.txt"):
    """name -> (address, 'func' | 'data', size or None). The first definition of a name wins."""
    syms = {}
    for line in Path(path).read_text(errors="replace").splitlines():
        m = _SYM_LINE.match(line)
        if not m:
            continue
        comment = m.group(3) or ""
        size = _SIZE.search(comment)
        kind = "func" if "type:func" in comment else "data"
        syms.setdefault(m.group(1), (int(m.group(2), 16), kind, int(size.group(1), 0) if size else None))
    return syms


def address_of(name, syms):
    """The N64 address of a symbol: from the symbol file, else from a D_XXXXXXXX / func_XXXXXXXX name."""
    if name in syms:
        return syms[name][0]
    m = _ADDR_NAME.match(name)
    return int(m.group(1), 16) if m else None


def kind_of(name, syms):
    if name in syms:
        return syms[name][1]
    return "func" if name.startswith("func_") else "data"


def nm_table(paths, nm="nm"):
    """(definitions, undefined): name -> [(object, nm type letter)] for global definitions, and the names
    some object references but none defines. Weak (W/V) and common (C) symbols count as definitions."""
    defs, refs = {}, set()
    paths = [str(p) for p in paths]
    for i in range(0, len(paths), 200):   # keep the command line short
        out = subprocess.run([nm, "-g", "-A", *paths[i:i + 200]], capture_output=True, text=True,
                             check=True).stdout
        for line in out.splitlines():
            obj, _, rest = line.rpartition(":") if line.count(":") > 1 else line.partition(":")
            parts = rest.split()
            if len(parts) == 2 and parts[0] == "U":
                refs.add(parts[1])
            elif len(parts) == 3 and parts[1] not in ("U", "w", "v"):
                defs.setdefault(parts[2], []).append((obj, parts[1]))
    return defs, refs - defs.keys()


def nm_symbols(paths, nm="nm"):
    """(defined, undefined) global symbol names over object files or static libraries."""
    defs, undefined = nm_table(paths, nm)
    return set(defs), undefined
