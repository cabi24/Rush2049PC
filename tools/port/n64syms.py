"""Shared helpers for the PC port tools: the decompilation's symbol file, N64 memory regions, nm output.

Stdlib only, Python 3.9+. Used by census.py, gen_stubs.py, gen_weak.py and gen_data.py.
"""
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# The four images whose data the PC build fills from the ROM (tools/extract_rom.py, docs/memory_map.md):
# name, how it is stored, ROM offset, stored length, image size, N64 base. "raw" is a plain copy (the boot
# segment tail); "deflate" a raw DEFLATE stream.
IMAGES = [
    ("boot", "raw", 0x283D0, 0x7110, 0x7110, 0x800277D0),
    ("game", "deflate", 0xB0CB10, 326180, 647072, 0x80086A50),
    ("ovl_a", "deflate", 0xB5C534, 80272, 194128, 0x8038A400),
    ("ovl_b", "deflate", 0xB6FEC4, 23639, 43888, 0x8038A400),
]
# Each image's data and BSS: image, N64 start, end of initialized data (= image end), end of BSS. The two
# runtime images' code overlaps at 0x8038A400, but their data and BSS ranges are disjoint.
ARENAS = [
    ("boot", 0x800277D0, 0x8002E8E0, 0x80086A50),
    ("game", 0x8010FD7C, 0x801249F0, 0x8017A640),
    ("ovl_a", 0x803AF978, 0x803B9A50, 0x803BAE20),
    ("ovl_b", 0x80393B34, 0x80394F70, 0x8039B440),
]
# Code ranges per image, for telling function addresses from data.
CODE = {"boot": (0x80000400, 0x800277D0), "game": (0x80086A50, 0x8010FD7C),
        "ovl_a": (0x8038A400, 0x803AF978), "ovl_b": (0x8038A400, 0x80393B34)}
# The symbol tables the matching build links each image's compiled C against.
IMAGE_SYMBOLS = {"game": "asm/us/blob/symbols.json", "ovl_a": "asm/us/ovl_a/symbols.json",
                 "ovl_b": "asm/us/ovl_b/symbols.json"}
# C library functions the game calls and the host's C library provides (link stubs and the census skip them).
HOST_LIBC = {"bcopy", "bzero", "bcmp", "memcpy", "memset", "memmove", "memcmp", "strlen", "strcpy", "strcmp",
             "strncpy", "strchr", "sqrtf", "sqrt", "fabsf", "fabs", "sinf", "cosf", "abort", "printf", "sprintf",
             "vsprintf", "modff"}
# Names the host linker defines itself; x86-64 code references _GLOBAL_OFFSET_TABLE_ directly. Never stubbed.
LINKER_DEFINED = {"_GLOBAL_OFFSET_TABLE_", "_DYNAMIC", "__dso_handle", "__ehdr_start", "__executable_start",
                  "_edata", "_end", "__bss_start", "__TMC_END__",
                  # port/data/overlays.ld's range bounds, which data.c names (stubbed, they were 4 KiB arrays the
                  # script's symbols overrode)
                  "pc_ovl_a_start", "pc_ovl_a_end", "pc_ovl_b_start", "pc_ovl_b_end"}
# The sanitizer runtimes' entry points, which instrumented objects reference (docs/port/SANITIZE.md). Never stubbed.
SANITIZER_PREFIXES = ("__asan_", "__ubsan_", "__tsan_", "__lsan_", "__sanitizer_", "__hwasan_")
# On Windows (PE), data.c's weak references are to pc_weak.<symbol>, which pc-data/weak.ld defines (gen_data.py).
PE_WEAK_PREFIX = "pc_weak."


def types_objects(build):
    """The game objects whose DWARF the tools read, one path each: the build's -O0 copy (types_objects.txt), which
    is its game objects in a Debug build. Optimized DWARF leaves out what the optimizer removed
    (docs/port/OPTIMIZED.md). A build made before types_objects.txt existed has only game_objects.txt."""
    build = Path(build)
    path = build / "types_objects.txt"
    if not path.exists():
        path = build / "game_objects.txt"
    return path.read_text().split() if path.exists() else []

_IMAGE_DIRS = (("/src/rom/", "boot"), ("/src/blob/", "game"), ("/src/ovl_a/", "ovl_a"), ("/src/ovl_b/", "ovl_b"),
               ("/nonmatching/game/", "game"), ("/nonmatching/ovl_a/", "ovl_a"), ("/nonmatching/ovl_b/", "ovl_b"))

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


def _nm_line(line):
    """(object, fields) of an `nm -A` line; an archive member's object is `archive:member`."""
    obj, _, rest = line.rpartition(":") if line.count(":") > 1 else line.partition(":")
    return obj, rest.split()


def _pe_weak_definitions(lines):
    """PE (COFF) objects show a weak definition X as `w X`, as they show an undefined weak reference, and keep
    its body under a default symbol `.weak.X.<another symbol>` in a section; an undefined weak reference's
    default is absolute 0. The (object, X) whose default is in a section; empty for ELF objects."""
    out = set()
    for line in lines:
        if ".weak." not in line:
            continue
        obj, parts = _nm_line(line)   # with or without nm -S's size field
        if len(parts) >= 3 and parts[-1].startswith(".weak.") and parts[-2] not in "AaUuw":
            out.add((obj, parts[-1][6:].split(".", 1)[0]))
    return out


def nm_table(paths, nm="nm"):
    """(definitions, undefined): name -> [(object, nm type letter)] for global definitions, and the names
    some object references but none defines. Weak (W/V, and PE's weak definitions as W) and common (C) symbols
    count as definitions."""
    defs, refs = {}, set()
    paths = [str(p) for p in paths]
    for i in range(0, len(paths), 200):   # keep the command line short
        out = subprocess.run([nm, "-g", "-A", *paths[i:i + 200]], capture_output=True, text=True,
                             check=True).stdout
        pe_weak = _pe_weak_definitions(out.splitlines())
        for line in out.splitlines():
            obj, parts = _nm_line(line)
            if len(parts) == 2 and parts[0] == "U":
                refs.add(parts[1])
            elif len(parts) == 2 and parts[0] == "w" and (obj, parts[1]) in pe_weak:
                defs.setdefault(parts[1], []).append((obj, "W"))
            elif len(parts) == 3 and parts[1] not in ("U", "w", "v"):
                defs.setdefault(parts[2], []).append((obj, parts[1]))
    return defs, refs - defs.keys()


def nm_symbols(paths, nm="nm"):
    """(defined, undefined) global symbol names over object files or static libraries."""
    defs, undefined = nm_table(paths, nm)
    return set(defs), undefined


def nm_objects(paths, nm="nm"):
    """object -> (definitions {name: (nm type letter, size or None)}, references {name}) for global symbols.
    Weak undefined references (w/v) are left out, as in nm_table (PE's weak definitions are W, as there)."""
    table = {}
    paths = [str(p) for p in paths]
    for i in range(0, len(paths), 200):
        out = subprocess.run([nm, "-g", "-A", "-S", *paths[i:i + 200]], capture_output=True, text=True,
                             check=True).stdout
        pe_weak = _pe_weak_definitions(out.splitlines())
        for line in out.splitlines():
            obj, parts = _nm_line(line)
            defs, refs = table.setdefault(obj, ({}, set()))
            if len(parts) == 2 and parts[0] == "U":
                refs.add(parts[1])
            elif len(parts) == 2 and parts[0] == "w" and (obj, parts[1]) in pe_weak:
                defs[parts[1]] = ("W", None)
            elif len(parts) >= 3 and parts[-2] not in ("U", "w", "v"):
                size = int(parts[1], 16) if len(parts) == 4 else None
                defs[parts[-1]] = (parts[-2], size)
    return table


def object_image(path):
    """'boot', 'game', 'ovl_a' or 'ovl_b' for an object compiled from src/rom, src/blob, src/ovl_a or src/ovl_b
    (or nonmatching/<image>)."""
    p = str(path).replace("\\", "/")
    for part, image in _IMAGE_DIRS:
        if part in p:
            return image
    return None


def load_image_symbols(image, root=ROOT):
    """name -> address from the image's symbol table (IMAGE_SYMBOLS); {} for the boot segment."""
    path = IMAGE_SYMBOLS.get(image)
    if not path:
        return {}
    return {k: int(v, 16) for k, v in json.loads((Path(root) / path).read_text())["symbols"].items()}


def load_layout_functions(image, root=ROOT):
    """[(address, name)] of the image's functions in layout/<image>.json order."""
    out = []
    for region in json.loads((Path(root) / "layout" / f"{image}.json").read_text())["regions"]:
        out += [(e["vaddr"], e["target_id"]) for e in region["entries"] if e["kind"] == "function"]
    return out
