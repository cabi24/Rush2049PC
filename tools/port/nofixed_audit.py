#!/usr/bin/env python3
"""The inventory for running the game with no fixed address (docs/port/NOFIXED.md 3).

    python3 tools/port/nofixed_audit.py [--build build/pc] [--jobs N] [--no-compile | --warnings LOG]
        [--out FILE] [--json FILE] [--sites CLASS[,CLASS...]] [--baseline FILE] [--write-baseline FILE]

Today the RDRAM window is at host 0x80000000 and a 32-bit N64 address of game data is its host address. This lists
every place that relies on that, by file:line and class, so that moving the window (phase B) can be planned and
checked. Three sources:

1. The compiler (--build: its compile_commands.json). Every game C file (built with pc_prelude.h) is
   compiled again with -fsyntax-only and the warnings the build turns off: -Wint-conversion (an integer used as a
   pointer, or a pointer as an integer, with no cast) and the casts between a pointer and an integer of another
   size (-Wint-to-pointer-cast, -Wpointer-to-int-cast). A warning in a function the file's weak header lists is a
   weak copy, which is never linked. --warnings LOG parses a build log instead (a build of a copy with
   -Wno-int-conversion taken out of CMakeLists.txt's game_flags, `cmake --build ... -- -k 0`).
2. The sources, with comments and strings told apart from code: the N64P and PC_N64W uses and the files' N64P
   definitions, the conversions the compiler can't see (an integer cast through uintptr_t to a narrower type, a
   pointer made from an integer through uintptr_t), the 4-byte N64 address fields (enum n64p_), the literal
   K0/K1 addresses (in code: an address or a bit; in comments; beside a declaration; in group.json notes), and the
   PC layer's own reliance on the fixed window (port/: the mapping, PC_RDRAM_BASE, the 4 GiB tests, the literal
   window addresses, the translations already there).
3. The generators' tables (--build): data_layout.json (gen_data: the arena symbols at their N64 addresses by
   arena, the names inside own storage, own storage, decoupled, reserved) and aliases.ld (gen_stubs: absolute
   RDRAM and cartridge addresses), and with nm which objects reach the arena names by name.

The report (--out, default stdout) has the counts by class, then every site of the classes --sites names
(default: every class but the symbol lists; `all` for those too). --json writes the counts and sites.
--baseline FILE (counts as --write-baseline writes them) fails, exit 1, if a class's count grew (a class the file
doesn't have counts from 0): CI holds the line with tools/port/nofixed_baseline.json, made from a Debug build with
--build, while phase C shrinks them. Without --build only the source classes are counted.
Stdlib only, Python 3.9+.
"""
import argparse
import collections
import json
import os
import re
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import n64syms  # noqa: E402  the N64's segments (port/include/pc_n64_map.h)

# KSEG1 addresses of devices, not memory: the RCP's registers and the PIF's ROM and RAM.
RCP_REGS_K1 = (n64syms.KSEG1 | 0x04000000, n64syms.KSEG1 | 0x04900000)
PIF_K1 = (n64syms.KSEG1 | 0x1FC00000, n64syms.KSEG2)

ROOT = Path(__file__).resolve().parents[2]
GAME_DIRS = ("src", "nonmatching")   # the game C (and the m2c contexts beside it)
PC_DIRS = ("port",)
SOURCE_SUFFIXES = (".c", ".h")

# --------------------------------------------------------------------------- comments and code


def split_code(text):
    """(code, comments): the text twice, with comments (code) or code (comments) blanked; newlines kept, so line
    numbers stay. String and character literals are blanked in both."""
    code, comm = [], []
    i, n = 0, len(text)

    def blank(s):
        return "".join("\n" if ch == "\n" else " " for ch in s)

    while i < n:
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            code.append(blank(text[i:j]))
            comm.append(text[i:j])
            i = j
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            code.append(blank(text[i:j]))
            comm.append(text[i:j])
            i = j
        elif text[i] in "\"'":
            q, j = text[i], i + 1
            while j < n and text[j] != q and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            code.append(q + blank(text[i + 1:j - 1]) + q if j - i >= 2 else blank(text[i:j]))
            comm.append(blank(text[i:j]))
            i = j
        else:
            j = i
            while j < n and text[j] not in "/\"'":
                j += 1
            if j == i:   # a lone slash
                j = i + 1
            code.append(text[i:j])
            comm.append(blank(text[i:j]))
            i = j
    return "".join(code), "".join(comm)


# --------------------------------------------------------------------------- the sites


class Inventory:
    def __init__(self):
        self.sites = collections.defaultdict(list)   # class -> [(file, line, text)]
        self.counts = collections.Counter()           # class -> count (for classes counted without sites)

    def add(self, cls, path, line, text):
        self.sites[cls].append((str(path), line, text.strip()[:160]))

    def count(self, cls, n):
        self.counts[cls] += n

    def total(self, cls):
        return len(self.sites[cls]) + self.counts[cls]


LIT = re.compile(r"\b0[xX]([89ABab][0-9A-Fa-f]{7})\b")
LIT80 = re.compile(r"0x80[0-9A-Fa-f]{6}")   # the census pattern (docs/port/NOFIXED.md 3): K0 addresses
DECL = re.compile(r"\b(extern|static|typedef|struct|union|enum)\b|^\s*#\s*define\b|^\s*[A-Za-z_][\w\s\*]*\b[A-Za-z_]\w*"
                  r"\s*(\[[^\]]*\]\s*)*(=[^;]*)?[;,]\s*$|^\s*[A-Za-z_][\w\s\*]*\b[A-Za-z_]\w*\s*\([^;{]*\)\s*;\s*$")
BIT_CONTEXT = re.compile(r"flags|AnimID|identifier|index|state|status|2\.1474836e9f|\bmask\b|sign|entries", re.I)
ADDR_CONTEXT = re.compile(r"pc_n64_ptr|addr|osMemSize|->end\b|\*\s*\)\s*\(?\s*0[xX]8|N64_HI_|\+\s*0[xX]8|"
                          r"0[xX]8\w+\s*\+|\(\s*void\s*\*\s*\)|K0BASE", re.I)
REGION_CONTEXT = re.compile(r"0[xX][fF]0000000|0[xX][fF][fF]000000|\bregion\b")
COND = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)$")
TRUNC = re.compile(r"\(\s*(?:u32|s32|unsigned(?:\s+int)?|int|uint32_t|int32_t|u16|s16|unsigned\s+short|short)\s*\)\s*"
                   r"\(\s*(?:uintptr_t|intptr_t|__UINTPTR_TYPE__|__INTPTR_TYPE__|size_t|long|unsigned\s+long)\s*\)")
ZEXT = re.compile(r"\(\s*(?:const\s+)?[A-Za-z_][\w\s]*?\*+\s*\)\s*\(\s*(?:uintptr_t|intptr_t|__UINTPTR_TYPE__|"
                  r"__INTPTR_TYPE__)\s*\)")
N64P_USE = re.compile(r"\bN64P\s*\(")
# A pointer stored in a 4-byte N64 address field: the prelude's PC_N64W (a file's own N64W was that, until
# tools/port/one_spelling.py inlined them).
N64W_USE = re.compile(r"\b(PC_)?N64W\s*\(")
# A file's own macro that translates through the prelude (phase B's N64A, N64PTR...; NOFIXED.md 2.3).
TRANSLATE_DEF = re.compile(r"^\s*#\s*define\s+\w+\s*\([^)]*\).*\bPC_(N64A|N64P|SLOT_N64|SLOT_SET)\s*\(")
DEF_N64P = re.compile(r"^\s*#\s*define\s+N64P\s*\(\s*(\w+)\s*\)\s*(.*)$")
DEF_N64W = re.compile(r"^\s*#\s*define\s+N64W\s*\(\s*(\w+)\s*\)\s*(.*)$")
N64P_FIELD = re.compile(r"\benum\s+n64p_\w+\s*\**\s*\w+\s*(\[[^\]]*\])?\s*[;,]")
PORT_RULES = [   # (class, pattern) over the PC layer's code
    ("port.mapping", re.compile(r"\bMAP_FIXED\w*|\bMapViewOfFileEx\b|\bpc_mem_fixed\s*\(|\bpc_fix_absolute_got\b|"
                                r"\bpc_mem_fix_absolute\b")),
    ("port.rdram_base", re.compile(r"\bPC_RDRAM_BASE\b|\bpc_rdram\s*\(\s*\)")),
    ("port.host_test", re.compile(r"(?=.*(?:uintptr_t|UINTPTR_MAX|void\s*\*|&\w|->host|\bheap\b|g_rdram|\b[av]\b))"
                                  r"(?:.*(?:>>\s*32\b|>>\s*16\s*>>\s*16|<<\s*32\b|>\s*0[xX][fF]{8}[uU]?[lL]*\b))")),
    ("port.translate", re.compile(r"\bpc_n64_ptr\s*\(|\bpc_n64_addr\s*\(|\bosVirtualToPhysical\s*\(|"
                                  r"\bosPhysicalToVirtual\s*\(")),
    ("port.trunc", TRUNC),
    ("port.zext", ZEXT),
]


def def_kind(body):
    b = body.strip()
    if b in ("x", "(x)") or re.fullmatch(r"\(?\s*x\s*\)?", b):
        return "n64"         # the N64 side: the value as it is
    if "_Generic" in b:
        return "generic"     # a typed cast of a 4-byte field (enum n64p_X) to its pointer
    if b.startswith("PC_N64W"):
        return "pc_n64w"     # the prelude's u32 conversion (RUSH_CHECK_CASTS checks it)
    if re.search(r"\(\s*\w[\w\s]*\*\s*\)\s*\(\s*(uintptr_t|__UINTPTR_TYPE__)\s*\)", b):
        return "cast"        # a plain zero-extending cast
    if re.search(r"\(\s*\w[\w\s]*\*\s*\)\s*PC_N64P\s*\(", b):
        return "translate"   # the prelude's translation (PC_N64P, phase B: NOFIXED.md 2.3)
    return "function"        # a translation of the file's own (pc_voice_node...)


def lit_code_class(value, code_line):
    """An address (a pointer, a K0 base or end, a lui half), a region test on an N64 address's top bits, a bit (a
    flag or sign bit that happens to be 0x80000000), a K1 or cartridge address, or a hardware register."""
    v = int(value, 16)
    if RCP_REGS_K1[0] <= v < RCP_REGS_K1[1] or PIF_K1[0] <= v < PIF_K1[1]:
        return "src.lit.code.hw_register"
    if v >= n64syms.KSEG1:
        return "src.lit.code.k1"
    if REGION_CONTEXT.search(code_line):
        return "src.lit.code.region"
    if v != n64syms.KSEG0:
        return "src.lit.code.address"
    if ADDR_CONTEXT.search(code_line) and not BIT_CONTEXT.search(code_line):
        return "src.lit.code.address"
    return "src.lit.code.bit"


def n64_only_lines(code_lines):
    """The lines only the N64 build compiles: inside `#ifndef TARGET_PC`, or the #else of `#ifdef TARGET_PC`
    (or of `#if defined(TARGET_PC)`)."""
    stack, out = [], set()
    for k, c in enumerate(code_lines):
        m = COND.match(c)
        if m:
            d, rest = m.group(1), m.group(2)
            pc = "TARGET_PC" in rest
            neg = d == "ifndef" or bool(re.search(r"!\s*defined\s*\(?\s*TARGET_PC", rest))
            if d in ("if", "ifdef", "ifndef"):
                stack.append(("n64" if neg else "pc") if pc else "other")
            elif d == "elif" and stack:
                stack[-1] = "other"
            elif d == "else" and stack:
                stack[-1] = {"pc": "n64", "n64": "pc"}.get(stack[-1], "other")
            elif d == "endif" and stack:
                stack.pop()
            continue
        if "n64" in stack:
            out.add(k)
    return out


def scan_game_file(inv, path, rel, built):
    """built: the game's compiled C files (None without --build). Code that only the N64 build compiles, or in a
    .c file the PC build doesn't compile (the libultra units port/ultra replaces), gets the suffix .n64only or
    .unbuilt and is counted apart."""
    text = path.read_text(errors="replace")
    lines = text.split("\n")
    if path.suffix not in SOURCE_SUFFIXES:   # group.json notes and the like: the census pattern only
        for k, line in enumerate(lines):
            if LIT80.search(line):
                inv.add("src.lit.metadata", rel, k + 1, line)
        return
    code, comm = (s.split("\n") for s in split_code(text))
    n64only = n64_only_lines(code)
    unbuilt = built is not None and path.suffix == ".c" and str(path.resolve()) not in built

    def add(cls, ln, line):
        k = ln - 1
        inv.add(cls + (".unbuilt" if unbuilt else ".n64only" if k in n64only else ""), rel, ln, line)

    for k, line in enumerate(lines):
        c, m = code[k], comm[k]
        ln = k + 1
        # literal addresses: the census's K0 pattern on any line, and K1/cartridge ones in code
        if LIT80.search(line):
            hits = [h for h in LIT.finditer(c) if h.group(1)[:2].lower() == "80"]
            if hits:
                add(lit_code_class(hits[0].group(1), c), ln, line)
            elif c.strip() and DECL.search(c):
                inv.add("src.lit.annotation", rel, ln, line)
            else:
                inv.add("src.lit.comment", rel, ln, line)
        for h in LIT.finditer(c):
            if h.group(1)[:1].lower() in "ab":
                add(lit_code_class(h.group(1), c), ln, line)
                break
        if not c.strip() or unbuilt:
            continue
        dp, dw = DEF_N64P.match(c), DEF_N64W.match(c)
        if dp:
            inv.add(f"src.def.n64p.{def_kind(dp.group(2))}", rel, ln, line)
        elif dw:
            inv.add(f"src.def.n64w.{def_kind(dw.group(2))}", rel, ln, line)
        if k in n64only:
            continue
        if not (dp or dw):
            if N64P_USE.search(c):
                inv.add("src.n64p", rel, ln, line)
            if N64W_USE.search(c):
                inv.add("src.n64w", rel, ln, line)
        if N64P_FIELD.search(c):
            inv.add("src.n64p_field", rel, ln, line)
        if TRUNC.search(c):
            inv.add("src.trunc", rel, ln, line)
        if ZEXT.search(c) and not dp:
            inv.add("src.zext", rel, ln, line)
        if TRANSLATE_DEF.search(c) and not (dp or dw):
            inv.add("src.def.translate", rel, ln, line)


def scan_port_file(inv, path, rel):
    text = path.read_text(errors="replace")
    lines = text.split("\n")
    code = split_code(text)[0].split("\n")
    tests = "/tests/" in f"/{rel}" or Path(rel).name.startswith("test_")
    for k, c in enumerate(code):
        if not c.strip():
            continue
        for cls, pat in PORT_RULES:
            if pat.search(c):
                inv.add(cls + (".tests" if tests else ""), rel, k + 1, lines[k])
        for h in LIT.finditer(c):
            v = int(h.group(1), 16)
            if n64syms.KSEG0 <= v < n64syms.KSEG2:
                inv.add("port.lit" + (".tests" if tests else ""), rel, k + 1, lines[k])
                break


def scan_sources(inv, built):
    for d in GAME_DIRS:
        for p in sorted((ROOT / d).rglob("*")):
            if p.is_file() and (p.suffix in SOURCE_SUFFIXES or p.name.endswith(".json")):
                scan_game_file(inv, p, p.relative_to(ROOT), built)
    for d in PC_DIRS:
        for p in sorted((ROOT / d).rglob("*")):
            if p.is_file() and p.suffix in SOURCE_SUFFIXES:
                scan_port_file(inv, p, p.relative_to(ROOT))


# --------------------------------------------------------------------------- the compiler

WARN = re.compile(r"^(?P<file>[^:\s][^:]*):(?P<line>\d+):(?P<col>\d+): (?P<kind>warning|error|note): (?P<msg>.*?)"
                  r"(?: \[(?P<flag>-W[\w=-]+)\])?$")
IN_FUNC = re.compile(r"^(?P<file>[^:]+): In function [‘'](?P<func>[^’']+)[’']:")
WANTED = {"-Wint-conversion", "-Wint-to-pointer-cast", "-Wpointer-to-int-cast"}


def game_entries(build):
    db = json.loads((build / "compile_commands.json").read_text())
    seen, out = set(), []
    for e in db:
        args = e["arguments"] if "arguments" in e else shlex.split(e["command"])
        f = str(Path(e["file"]).resolve())
        if not any(a.endswith("pc_prelude.h") for a in args) or "/probe/" in f or f in seen:
            continue
        seen.add(f)
        out.append((f, e["directory"], args))
    return out


def compile_one(entry):
    f, d, args = entry
    a, skip = [], False
    for x in args:
        if skip:
            skip = False
            continue
        if x == "-o":
            skip = True
            continue
        if x in ("-c", "-Wno-int-conversion") or x.startswith("-fdiagnostics-color"):
            continue
        a.append(x)
    a += ["-fsyntax-only", "-Wint-conversion", "-Wint-to-pointer-cast", "-Wpointer-to-int-cast",
          "-fno-diagnostics-show-caret", "-fdiagnostics-color=never"]
    r = subprocess.run(a, cwd=d, capture_output=True, text=True, errors="replace")
    return r.stderr


def weak_functions(build, rel):
    h = build / "weak" / (str(rel).replace("/", "__").rsplit(".", 1)[0] + ".h")
    if not h.exists():
        return set()
    return set(re.findall(r"^#pragma weak (\w+)", h.read_text(), re.M))


def parse_warnings(log, inv, build):
    """Each wanted warning once, at its use site (the outermost macro expansion), with its function."""
    func = {}
    pending = None
    seen = set()
    weak_cache = {}

    def flush():
        nonlocal pending
        if not pending:
            return
        file, line, col, msg, flag, fn = pending
        pending = None
        p = Path(file)
        try:
            rel = p.resolve().relative_to(ROOT)
        except ValueError:
            rel = p
        if str(rel).split("/")[0] not in GAME_DIRS:
            return
        key = (str(rel), line, col, msg)
        if key in seen:
            return
        seen.add(key)
        if flag == "-Wint-to-pointer-cast":
            cls = "cc.int_to_ptr"
        elif flag == "-Wpointer-to-int-cast":
            cls = "cc.ptr_to_int"
        elif "makes pointer from integer" in msg:
            cls = "cc.implicit.int_to_ptr"
        elif "makes integer from pointer" in msg:
            cls = "cc.implicit.ptr_to_int"
        else:
            cls = "cc.implicit.other"
        if build is not None:
            if str(rel) not in weak_cache:
                weak_cache[str(rel)] = weak_functions(build, rel)
            if fn and fn in weak_cache[str(rel)]:
                cls += ".weak"
        inv.add(cls, rel, line, f"{fn or '?'}: {msg}")

    for raw in log.splitlines():
        m = IN_FUNC.match(raw)
        if m:
            flush()
            func[m.group("file")] = m.group("func")
            continue
        m = WARN.match(raw)
        if not m:
            continue
        if m.group("kind") == "note" and pending and "in expansion of macro" in m.group("msg"):
            pending = (m.group("file"), int(m.group("line")), int(m.group("col")), pending[3], pending[4],
                       pending[5])
            continue
        if m.group("kind") == "warning":
            flush()
            if m.group("flag") in WANTED:
                inv.count(f"context.cc_warnings.{m.group('flag')[2:]}", 1)
                pending = (m.group("file"), int(m.group("line")), int(m.group("col")), m.group("msg"),
                           m.group("flag"), func.get(m.group("file")))
    flush()


def compiler_pass(inv, build, jobs):
    entries = game_entries(build)
    with ThreadPoolExecutor(max_workers=jobs) as ex:
        logs = list(ex.map(compile_one, entries))
    inv.count("context.game_files_compiled", len(entries))
    parse_warnings("\n".join(logs), inv, build)


# --------------------------------------------------------------------------- the tables


def tables_pass(inv, build, jobs):
    lay = json.loads((build / "pc-data" / "data_layout.json").read_text())
    arenas = list(zip(("boot", "game", "ovl_a", "ovl_b"), lay["arenas"]))
    absolute = {}
    for name, addr in lay["absolute"]:
        where = next((a for a, (lo, hi) in arenas if lo <= addr <= hi), "outside")
        absolute[name] = addr
        inv.add(f"symbols.arena.{where}", "data_layout.json", 0, f"{name} 0x{addr:08X}")
    for name, base, off in lay["sets"]:
        inv.add("symbols.inside_own", "data_layout.json", 0, f"{name} = {base} + 0x{off:X}")
    for name, size in lay["own"]:
        inv.add("symbols.own", "data_layout.json", 0, f"{name} {size} B")
    for pc, size, retired in lay["decoupled"]:
        inv.add("symbols.decoupled", "data_layout.json", 0, f"{pc} {size} B, was {', '.join(retired[:3])}")
    for name, addr, size in lay.get("reserved", []):
        absolute[name] = addr
        inv.add("symbols.reserved", "data_layout.json", 0, f"{name} 0x{addr:08X} {size} B")
    if lay.get("nofixed"):   # a RUSH_NOFIXED build: relative to the window section, nothing absolute (NOFIXED.md 2.2)
        for name, addr in lay.get("window_names", []):
            absolute[name] = addr
            inv.add("symbols.window_names", "data_layout.json", 0, f"{name} 0x{addr:08X}")
        inv.count("context.nofixed_build", 1)
    al = build / "pc-data" / "aliases.ld"
    if al.exists():
        for k, line in enumerate(al.read_text().splitlines()):
            m = re.match(r"PROVIDE\((\w+) = ABSOLUTE\(0x([0-9A-Fa-f]+)\)\);", line)
            if m:
                v = int(m.group(2), 16)
                absolute[m.group(1)] = v
                inv.add("symbols.aliases." + ("rdram" if v < n64syms.KSEG1 else "cartridge"), "aliases.ld", k + 1, line)
    # who reaches them by name: nm -u over the game's and the PC layer's objects
    nm = os.environ.get("NM", "nm")
    def port_objects():
        """The PC layer's objects in the program (not the tests'), one per source file."""
        seen = {}
        for o in sorted(build.rglob("*.o*")):
            if o.suffix not in (".o", ".obj"):
                continue
            parts = o.relative_to(build).parts
            target = next((x for x in parts if x.endswith(".dir")), "")
            src = "/".join(parts[parts.index(target) + 1:]) if target else ""
            if not src.startswith("port/") or "/tests/" in src or Path(src).name.startswith("test_") or \
                    re.search(r"test|probe|^game|check", target):
                continue
            seen.setdefault(src, str(o))
        return list(seen.values())

    lists = [("symbols.by_name.game_objects", None), ("symbols.by_name.port_objects", port_objects())]
    for cls, objs in lists:
        if objs is None:
            lst = build / "game_objects.txt"
            if not lst.exists():
                continue
            objs = [o for o in lst.read_text().split() if o.endswith((".o", ".obj"))]

        def undefined(o):
            r = subprocess.run([nm, "-u", o], capture_output=True, text=True)
            return o, {ln.split()[-1] for ln in r.stdout.splitlines() if ln.strip()}

        with ThreadPoolExecutor(max_workers=jobs) as ex:
            refs = list(ex.map(undefined, objs))
        names = collections.Counter()
        for o, und in refs:
            hit = sorted(n for n in und if n in absolute)
            if hit:
                try:
                    rel = Path(o).resolve().relative_to(build.resolve())
                except ValueError:
                    rel = Path(o)
                inv.add(cls, rel, 0, f"{len(hit)}: {' '.join(hit[:8])}{' ...' if len(hit) > 8 else ''}")
                names.update(hit)
        inv.count(cls.replace("by_name", "names_reached"), len(names))


# --------------------------------------------------------------------------- the report

ORDER = [
    ("symbols.arena.", "gen_data: arena symbols at their N64 addresses (absolute .set in data.c), by arena"),
    ("symbols.reserved", "gen_data: reserved PC data at an absolute window address (reserved.txt)"),
    ("symbols.aliases.", "gen_stubs: absolute symbols in aliases.ld (low memory, heap start, image base; cartridge)"),
    ("symbols.by_name.", "objects that reach an absolute name by name (nm -u), and the names they reach"),
    ("symbols.names_reached.", ""),
    ("symbols.inside_own", "context: names inside own storage (relative already)"),
    ("symbols.own", "context: own storage (host layout, outside the window)"),
    ("symbols.decoupled", "context: decoupled (PC-owned, outside the window)"),
    ("cc.", "compiler: casts of another size and implicit conversions (.weak: in a weak copy, never linked)"),
    ("src.n64p", "N64P: a 4-byte N64 address field read as a pointer"),
    ("src.n64w", "PC_N64W (or a file's N64W): a pointer stored as a 4-byte N64 address"),
    ("src.def.", "the files' N64P/N64W definitions, by kind (n64: the N64 side; translate: through PC_N64P), and "
                 "their own translating macros"),
    ("src.n64p_field", "4-byte N64 address fields (enum n64p_X) in N64-layout records"),
    ("src.trunc", "silent: an integer through uintptr_t/intptr_t cast to a narrower type"),
    ("src.zext", "silent: a pointer made from an integer through uintptr_t/intptr_t"),
    ("src.lit.", "literal K0 addresses (0x80xxxxxx; the census pattern) and K1/cartridge ones in code"),
    ("port.", "the PC layer (port/; .tests: test programs)"),
    ("context.", ""),
]
DEFAULT_SITE_PREFIXES = ("cc.", "src.", "port.", "symbols.aliases", "symbols.reserved", "symbols.by_name.port")


def class_files(inv, cls):
    return len({f for f, _, _ in inv.sites[cls]})


def report(inv, out, site_classes):
    classes = sorted(set(inv.sites) | set(inv.counts))
    w = out.write
    w("# nofixed_audit: every place that relies on the RDRAM window at host 0x80000000 (docs/port/NOFIXED.md 3)\n\n")
    w(f"{'class':44} {'sites':>7} {'files':>6}\n")
    done = set()
    for prefix, title in ORDER:
        group = [c for c in classes if c.startswith(prefix) and c not in done]
        if not group:
            continue
        if title:
            w(f"\n## {title}\n")
        for c in group:
            done.add(c)
            files = class_files(inv, c) if inv.sites[c] else 0
            w(f"{c:44} {inv.total(c):7} {files if files else '':>6}\n")
    for c in classes:
        if c not in done:
            w(f"{c:44} {inv.total(c):7}\n")
    for c in classes:
        if not inv.sites[c] or not any(not s or c == s or c.startswith(s if s.endswith(".") else s + ".")
                                       for s in site_classes):
            continue
        w(f"\n## {c} ({len(inv.sites[c])})\n")
        for f, line, text in inv.sites[c]:
            w(f"{f}:{line}: {text}\n" if line else f"{f}: {text}\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build", type=Path, help="a configured and built CMake build (build/pc)")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--no-compile", action="store_true", help="skip the compiler pass")
    ap.add_argument("--warnings", type=Path, help="parse this build log instead of compiling")
    ap.add_argument("--out", type=Path)
    ap.add_argument("--json", type=Path)
    ap.add_argument("--sites", help="classes (or prefixes ending in .) to list site by site; 'all' for every class")
    ap.add_argument("--baseline", type=Path, help="fail if a count grew past this file's")
    ap.add_argument("--write-baseline", type=Path)
    a = ap.parse_args()

    inv = Inventory()
    build = a.build.resolve() if a.build else None
    built = {f for f, _, _ in game_entries(build)} if build else None
    scan_sources(inv, built)
    if a.warnings:
        parse_warnings(a.warnings.read_text(errors="replace"), inv, build)
    elif build and not a.no_compile:
        compiler_pass(inv, build, a.jobs)
    if build:
        tables_pass(inv, build, a.jobs)

    if a.sites == "all":
        site_classes = [""]
    elif a.sites:
        site_classes = a.sites.split(",")
    else:
        site_classes = list(DEFAULT_SITE_PREFIXES)
    if a.out:
        a.out.parent.mkdir(parents=True, exist_ok=True)
        with a.out.open("w") as f:
            report(inv, f, site_classes)
    else:
        report(inv, sys.stdout, site_classes)
    counts = {c: inv.total(c) for c in sorted(set(inv.sites) | set(inv.counts))}
    if a.json:
        a.json.write_text(json.dumps({"counts": counts, "sites": {c: inv.sites[c] for c in sorted(inv.sites)}},
                                     indent=1) + "\n")
    if a.write_baseline:
        a.write_baseline.write_text(json.dumps(counts, indent=1, sort_keys=True) + "\n")
    if a.baseline:
        base = json.loads(a.baseline.read_text())
        grew = [(c, base.get(c, 0), counts.get(c, 0)) for c in sorted(set(base) | set(counts))
                if counts.get(c, 0) > base.get(c, 0)]   # a class the file doesn't have counts from 0
        for c, was, now in grew:
            print(f"nofixed_audit: {c} grew from {was} to {now}", file=sys.stderr)
        if grew:
            return 1
        print(f"nofixed_audit: no class grew past {a.baseline} ({len(base)} classes)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
