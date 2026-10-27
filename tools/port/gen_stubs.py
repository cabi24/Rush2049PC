#!/usr/bin/env python3
"""Generate stubs.c and aliases.ld: a definition for every symbol the game objects still leave undefined.

    python3 tools/port/gen_stubs.py --objects-list build/pc/game_link_objects.txt \\
        --defined-list build/pc/port_objects.txt --out build/pc/pc-data/stubs.c --aliases build/pc/pc-data/aliases.ld

--objects-list names the game and generated-data objects: what they reference and nothing defines is filled
here. --defined-list names the PC layer's objects (port/ultra and the other parts): their definitions count,
weak ones included, but their own references (the C library, zlib, pthreads) are left to the linker.

- A name that differs from a definition only by the case of its address digits (func_8001fe58 for
  func_8001FE58), or an address name (D_/func_XXXXXXXX) whose N64 address belongs to a defined symbol of the
  same kind (D_80000318 for osMemSize), becomes a PROVIDE alias in aliases.ld, an implicit linker script.
- Functions become stubs that call pc_stub_hit (port/ultra/misc.c): it prints the name, the N64 thread and a
  symbolized backtrace, then aborts. The first one hit is the next thing to implement.
- Data becomes zeroed placeholders (twice the N64 size, at least 16 bytes; 4 KiB when the size is unknown).
  Not game data: each one left is a to-do.

Names the host C library provides are left to it. Stdlib only, Python 3.9+.
"""
import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import n64syms  # noqa: E402

# Called by the game C under their own names and provided by the host C library and libm.
HOST_LIBC = {"bcopy", "bzero", "bcmp", "memcpy", "memset", "memmove", "memcmp", "strlen", "strcpy", "strcmp",
             "strncpy", "strchr", "sqrtf", "sqrt", "fabsf", "fabs", "sinf", "cosf", "abort", "printf", "sprintf",
             "modff"}
_ADDR_NAME = re.compile(r"^(D|func)_([0-9A-Fa-f]{8})$")


def declared_as_function(src, names):
    """Names an extern declaration under src/ follows with '(' (a function), for names not in the symbol file."""
    funcs = set()
    pat = re.compile(r"\b(" + "|".join(map(re.escape, sorted(names))) + r")\s*\(") if names else None
    if pat:
        for path in Path(src).rglob("*.c"):
            for line in path.read_text(errors="replace").splitlines():
                if line.startswith("extern") or line[:1].isalpha():
                    funcs.update(m.group(1) for m in pat.finditer(line))
    return funcs


def aliases_for(undefined, defined, syms):
    """undefined name -> defined name it can stand for (see the module docstring)."""
    by_lower = {}
    for d in defined:
        if _ADDR_NAME.match(d):
            by_lower.setdefault(d.lower(), []).append(d)
    by_addr = {}
    for d in defined:
        addr = n64syms.address_of(d, syms)
        if addr is not None:
            by_addr.setdefault((addr, n64syms.kind_of(d, syms)), []).append(d)
    out = {}
    for name in sorted(undefined):
        m = _ADDR_NAME.match(name)
        if not m:
            continue
        same = by_lower.get(name.lower(), [])
        if len(same) == 1:
            out[name] = same[0]
            continue
        kind = "func" if m.group(1) == "func" else "data"
        cands = sorted(set(by_addr.get((int(m.group(2), 16), kind), [])))
        if len(cands) == 1:
            out[name] = cands[0]
    return out


STUB_PRELUDE = r"""
/* No system headers: stub names such as fcvt would clash with the C library's declarations. pc_stub_hit
 * (port/ultra/misc.c) prints the name, the N64 thread and a symbolized backtrace, then aborts. */
void pc_stub_hit(const char *name) __attribute__((noreturn));
"""


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--objects-list", type=Path, required=True, help="game and generated-data objects")
    ap.add_argument("--defined-list", type=Path, help="PC-layer objects: definitions only")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--aliases", type=Path, help="implicit linker script of PROVIDE aliases")
    ap.add_argument("--nm", default="nm")
    ap.add_argument("--root", type=Path, default=n64syms.ROOT)
    a = ap.parse_args()

    syms = n64syms.load_symbols(a.root / "symbol_addrs.us.txt")
    game = a.objects_list.read_text().split()
    port = a.defined_list.read_text().split() if a.defined_list else []
    game_defs, game_refs = n64syms.nm_table(game, a.nm)
    port_defs, _ = n64syms.nm_table(port, a.nm) if port else ({}, set())
    defined = set(game_defs) | set(port_defs)
    undefined = (game_refs - defined) - HOST_LIBC
    alias = aliases_for(undefined, defined, syms)
    undefined -= alias.keys()
    unknown = {n for n in undefined if n64syms.address_of(n, syms) is None}
    unknown_funcs = declared_as_function(a.root / "src", unknown)
    funcs, data = [], []
    for name in sorted(undefined):
        is_func = name in unknown_funcs if name in unknown else n64syms.kind_of(name, syms) == "func"
        (funcs if is_func else data).append(name)

    L = ["/* stubs.c: generated by tools/port/gen_stubs.py. Not game code or data: what is still missing. */",
         STUB_PRELUDE]
    L += [f"void {n}(void) {{ pc_stub_hit(\"{n}\"); }}" for n in funcs]
    L.append("")
    for n in data:
        size = syms[n][2] if n in syms and syms[n][2] else None
        L.append(f"__attribute__((aligned(16))) unsigned char {n}[{max(16, size * 2) if size else 4096}];")
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text("\n".join(L) + "\n")
    if a.aliases:
        A = ["/* aliases.ld: generated by tools/port/gen_stubs.py; names the game uses for a symbol defined",
             " * under another spelling or name at the same N64 address. */"]
        A += [f"PROVIDE({n} = {t});" for n, t in sorted(alias.items())]
        a.aliases.write_text("\n".join(A) + "\n")
    print(f"gen_stubs: {len(funcs)} function stubs, {len(data)} data placeholders, {len(alias)} aliases -> {a.out}")


if __name__ == "__main__":
    main()
