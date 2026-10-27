#!/usr/bin/env python3
"""Make integer declarations of pointer-holding data symbols pointer-sized (docs/port/POINTERS.md).

    python3 tools/port/ptr64_decls.py [--dry-run] [SYMBOL...]   # default: the pointer/integer conflicts in
                                                                 # build/pc/pc-data/data_layout.txt

tools/port/gen_data.py reads every file's declared type of each data symbol from DWARF. When some files
declare a symbol as a pointer (so it gets 8-byte pointer storage on PC) and others as s32/u32 (reading or
writing 4 bytes of it, or indexing with a 4-byte stride), this changes the integer declarations, in the files
that use the symbol, to intptr_t/uintptr_t: the same types on the N64, pointer-sized on PC.

- `extern [volatile] s32 SYM[...];` -> `extern [volatile] intptr_t SYM[...];` on the same line (u32 and
  unsigned int -> uintptr_t, int -> intptr_t);
- a combined declaration (`extern s32 A, SYM, B;`) gets an #ifdef TARGET_PC copy with the symbols split out.
The file also gets the N64 uintptr_t/intptr_t typedefs if it lacks them (ptr64_edit.py need). Idempotent.
Files of the overlay-loader work are skipped. Stdlib only, Python 3.9+.
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ptr64_edit  # noqa: E402

ROOT = ptr64_edit.ROOT
WIDE = {"s32": "intptr_t", "int": "intptr_t", "signed int": "intptr_t", "u32": "uintptr_t",
        "unsigned int": "uintptr_t", "unsigned": "uintptr_t"}
_INT = r"(?:volatile\s+)?(?:s32|u32|int|signed int|unsigned int|unsigned)"
_CONFLICT = re.compile(r"^\s+0x[0-9A-F]+ (\S+): chose (.+?) \(\d+\), also (.+?) \(\d+: .+\)$")


def conflict_symbols(layout):
    lines = layout.read_text().split("\n")
    start = next(i for i, l in enumerate(lines) if l.startswith("## Pointer conflicts"))
    syms = {}
    for l in lines[start + 1:]:
        if l.startswith("## "):
            break
        m = _CONFLICT.match(l)
        if not m:
            continue
        name, chose, also = m.groups()
        intlike = re.fullmatch(r"(volatile )?(s32|u32|int|unsigned int)(\[\d*\])*", also)
        if intlike and ("*" in chose or re.fullmatch(r"OSMesg(\[\d*\])*", chose)) and "intptr_t" not in chose:
            syms[name.split("/")[0]] = chose
    return syms


def _split_type(kw):
    vol = "volatile " if kw.startswith("volatile") else ""
    return vol, kw[len(vol):].strip()


def widen_file(path, syms, dry):
    """Widen the declarations of every symbol in `syms` that the file also uses. Returns the symbols done."""
    lines = path.read_text().split("\n")
    words = {s: re.compile(r"\b" + re.escape(s) + r"\b") for s in syms}
    decl = {i for i, l in enumerate(lines) if l.lstrip().startswith("extern")}
    used = {s for s, w in words.items() if any(w.search(l) for i, l in enumerate(lines) if i not in decl)}
    done = []
    for i in sorted(decl, reverse=True):   # bottom up: a split doesn't move the lines still to do
        l = lines[i]
        if i and lines[i - 1] == "#else" and "#ifdef TARGET_PC" in lines[max(0, i - 6):i - 1]:
            continue   # the N64 branch of an earlier split
        here = [s for s in used if words[s].search(l)]
        if not here:
            continue
        single = re.match(r"^(\s*extern\s+)(" + _INT + r")(\s+(\w+)\s*(?:\[[^\]]*\]\s*)*;.*)$", l)
        if single and single.group(4) in here:
            vol, core = _split_type(single.group(2))
            lines[i] = single.group(1) + vol + WIDE[core] + single.group(3)
            done.append(single.group(4))
            continue
        multi = re.match(r"^(\s*extern\s+)(" + _INT + r")\s+([^;()]*);(.*)$", l)
        if not multi:
            continue
        vol, core = _split_type(multi.group(2))
        names = [n.strip() for n in multi.group(3).split(",")]
        wide = [n for n in names if re.match(r"(\w+)", n) and re.match(r"(\w+)", n).group(1) in here]
        if not wide:
            continue   # declared through a pointer declarator here: already pointer-sized
        rest = [n for n in names if n not in wide]
        pc = [f"{multi.group(1)}{vol}{WIDE[core]} {n};" for n in wide]
        if rest:
            pc.append(f"{multi.group(1)}{multi.group(2)} {', '.join(rest)};{multi.group(4)}")
        lines[i:i + 1] = ["#ifdef TARGET_PC"] + pc + ["#else", l, "#endif"]
        done += [re.match(r"(\w+)", n).group(1) for n in wide]
    if done and not dry:
        path.write_text(ptr64_edit.ensure_ptr_typedefs("\n".join(lines)))
    return done


def main(argv):
    dry = "--dry-run" in argv
    names = [a for a in argv if not a.startswith("--")]
    syms = list(dict.fromkeys(names)) if names else list(conflict_symbols(ROOT / "build/pc/pc-data/data_layout.txt"))
    per_sym = {s: [] for s in syms}
    edits = 0
    for p in ptr64_edit.game_files():
        t = p.read_text()
        cand = [s for s in syms if s in t]
        if not cand:
            continue
        done = widen_file(p, cand, dry)
        if done:
            edits += 1
            for s in done:
                per_sym[s].append(ptr64_edit.rel(p))
    for s, fs in per_sym.items():
        if fs:
            print(f"{s}: {len(fs)} files: " + ", ".join(fs))
    print(f"ptr64_decls: {len(syms)} symbols, {edits} files" + (" (dry run)" if dry else ""))


if __name__ == "__main__":
    main(sys.argv[1:])
