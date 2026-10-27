#!/usr/bin/env python3
"""Port census: how far the PC build is from linking, written to docs/port/STATUS.md.

    python3 tools/port/census.py --build build/pc [--jobs N] [--nm nm] [--no-warnings]

Reads the CMake build's compile_commands.json, so every file is checked with the build's own flags.

1. Compile: every C file is compiled again with -fsyntax-only, counting failures and warnings by flag.
   The 64-bit pointer warnings (-Wpointer-to-int-cast, -Wint-to-pointer-cast) are the porting work list.
2. Link: nm over the build's objects (game code, generated data, and the PC layer as it grows). Every
   symbol still undefined is classified by kind (function or data) and N64 memory region.

The link test's stubs.c (tools/port/gen_stubs.py) is left out: it stands in for what is still missing.
Also writes build/pc/census.json and build/pc/undefined.txt (one name per line) for the generators.
Stdlib only, Python 3.9+.
"""
import argparse
import collections
import datetime
import json
import os
import re
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import n64syms  # noqa: E402

ROOT = n64syms.ROOT
POINTER_WARNINGS = ("-Wpointer-to-int-cast", "-Wint-to-pointer-cast")
# Names the game defines that the host C library also defines: they must be renamed or dropped.
LIBC_NAMES = {"main", "memcpy", "memset", "memmove", "memchr", "memcmp", "bcopy", "bzero", "bcmp", "strlen",
              "strchr", "strcpy", "strcmp", "strncpy", "sprintf", "sinf", "cosf", "sqrtf", "modf", "modff",
              "abs", "rand", "srand", "fabsf", "atan2f", "qsort", "bsearch", "malloc", "free", "__isinf",
              "__isnan"}
_WARN = re.compile(r": warning: .*\[(-W[A-Za-z0-9-]+)")


def load_entries(build):
    path = build / "compile_commands.json"
    if not path.exists():
        sys.exit(f"census: {path} not found; configure with CMake first (it exports it)")
    entries = []
    probe = (build / "probe").resolve()
    for e in json.loads(path.read_text()):
        if probe in Path(e["file"]).resolve().parents:
            continue   # gen_weak's probe wrappers: the same files again
        if Path(e["file"]).name == "stubs.c":
            continue   # the link test's stand-ins for what is still missing
        args = e["arguments"] if "arguments" in e else shlex.split(e["command"])
        out = e.get("output")
        if out is None and "-o" in args:
            out = args[args.index("-o") + 1]
        entries.append({"file": Path(e["file"]).resolve(), "dir": Path(e["directory"]), "args": args,
                        "obj": (Path(e["directory"]) / out).resolve() if out else None})
    return entries


def syntax_args(args):
    """The compile command with its output removed and -fsyntax-only in place of -c."""
    out, skip = [], False
    for a in args:
        if skip:
            skip = False
            continue
        if a in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if a in ("-MD", "-MMD"):
            continue
        out.append("-fsyntax-only" if a == "-c" else a)
    return out


def check(entry):
    r = subprocess.run(syntax_args(entry["args"]), cwd=entry["dir"], capture_output=True, text=True)
    warns = collections.Counter(_WARN.findall(r.stderr))
    first_error = next((l for l in r.stderr.splitlines() if ": error:" in l), "")
    return entry["file"], r.returncode == 0, warns, first_error


def rel(p):
    try:
        return str(Path(p).relative_to(ROOT))
    except ValueError:
        return str(p)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--build", type=Path, default=ROOT / "build" / "pc")
    ap.add_argument("--out", type=Path, default=ROOT / "docs" / "port" / "STATUS.md")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--nm", default="nm")
    ap.add_argument("--no-warnings", action="store_true", help="skip the -fsyntax-only pass")
    a = ap.parse_args()
    build = a.build.resolve()
    entries = load_entries(build)
    cc = "?"
    if entries:
        r = subprocess.run([entries[0]["args"][0], "--version"], capture_output=True, text=True)
        cc = (r.stdout.splitlines() or ["?"])[0]

    # 1. compile
    results = []
    if not a.no_warnings:
        with ThreadPoolExecutor(a.jobs) as pool:
            results = list(pool.map(check, entries))
    failing = sorted((rel(f), err) for f, ok, _, err in results if not ok)
    warn_kinds = collections.Counter()
    per_file = collections.Counter()
    for f, _, w, _ in results:
        warn_kinds.update(w)
        per_file[rel(f)] = sum(w[k] for k in POINTER_WARNINGS)
    pointer_total = sum(per_file.values())
    pointer_files = sum(1 for v in per_file.values() if v)

    # 2. link
    syms = n64syms.load_symbols()
    objs = [e["obj"] for e in entries if e["obj"] and e["obj"].exists()]
    # Undefined = what the game and its generated data reference and nothing defines. The PC layer's own
    # references (the host C library, SDL) are not the game's to-do list, but its definitions count.
    port_dir = (ROOT / "port").resolve()
    game_objs = [e["obj"] for e in entries if e["obj"] in objs and port_dir not in e["file"].parents]
    port_objs = [o for o in objs if o not in game_objs]
    defs, undefined = n64syms.nm_table(game_objs, a.nm)
    game_defined = set(defs)
    for name, d in n64syms.nm_table(port_objs, a.nm)[0].items():
        defs.setdefault(name, []).extend(d)
    # Names the link satisfies with generated linker aliases (gen_stubs.py: another spelling at the same address).
    aliases = build / "pc-data" / "aliases.ld"
    if aliases.exists():
        for name in re.findall(r"PROVIDE\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*=", aliases.read_text()):
            defs.setdefault(name, []).append((str(aliases), "A"))
    defined = set(defs)
    undefined -= defined
    # One strong definition per symbol (tools/port/gen_weak.py makes the non-owned copies weak).
    strong_dups = sorted(n for n, d in defs.items() if sum(1 for _, t in d if t in "TDBRS") > 1)
    weak_resolved = sum(1 for d in defs.values() for _, t in d if t in "WV")
    classes = collections.Counter()
    unknown = []
    for name in sorted(undefined):
        addr = n64syms.address_of(name, syms)
        if addr is None:
            unknown.append(name)
            continue
        classes[n64syms.kind_of(name, syms), n64syms.region_of(addr)] += 1
    libc = sorted(game_defined & LIBC_NAMES)

    # outputs
    (build / "undefined.txt").write_text("".join(n + "\n" for n in sorted(undefined)))
    (build / "census.json").write_text(json.dumps({
        "files": len(entries), "failing": failing, "warnings": dict(warn_kinds),
        "pointer_warnings": {f: n for f, n in per_file.items() if n}, "defined": len(defined),
        "strong_duplicates": strong_dups, "weak_copies": weak_resolved,
        "undefined": len(undefined), "undefined_by_class": {f"{k} / {r}": n for (k, r), n in classes.items()},
        "not_in_symbol_file": unknown, "libc_names": libc}, indent=1))

    L = ["# PC port status", "",
         f"Generated by `tools/port/census.py` on {datetime.date.today()} with {cc}. Do not edit; rerun the tool.",
         "", "## Compile", "", "| | |", "|---|---|"]
    if results:
        L += [f"| C files that compile | {len(entries) - len(failing)} of {len(entries)} |",
              f"| 64-bit pointer warnings | {pointer_total} in {pointer_files} files |"]
    else:
        L += [f"| C files in the build | {len(entries)} (compile pass skipped) |"]
    L += [f"| Objects built | {len(objs)} |", ""]
    if failing:
        L += ["Failing:", ""] + [f"- `{f}`: {err.split(': error: ', 1)[-1]}" for f, err in failing] + [""]
    if warn_kinds:
        L += ["| Warning | Count |", "|---|---:|"] + [f"| `{k}` | {n} |" for k, n in warn_kinds.most_common()] + [""]
        L += ["Most pointer warnings:", "", "| File | Warnings |", "|---|---:|"]
        L += [f"| `{f}` | {n} |" for f, n in per_file.most_common(15) if n] + [""]
    L += ["## Link", "", f"Defined: {len(defined)}. Still undefined: **{len(undefined)}**.", "",
          f"Defined more than once (strong): **{len(strong_dups)}**"
          + (": " + ", ".join(f"`{n}`" for n in strong_dups[:40]) if strong_dups else "")
          + f". Non-owned copies made weak by `gen_weak.py`: {weak_resolved}.", "",
          "| Kind | Region | Undefined |", "|---|---|---:|"]
    L += [f"| {k} | {r} | {n} |" for (k, r), n in sorted(classes.items())]
    L += [f"| ? | not in the symbol file | {len(unknown)} |", ""]
    if unknown:
        L += ["Not in the symbol file: " + ", ".join(f"`{n}`" for n in unknown[:60])
              + (" ..." if len(unknown) > 60 else ""), ""]
    L += ["Game functions with host C library names (rename or drop): "
          + (", ".join(f"`{n}`" for n in libc) if libc else "none"), ""]
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text("\n".join(L))
    print(f"census: {len(entries) - len(failing)}/{len(entries)} compile, {pointer_total} pointer warnings, "
          f"{len(undefined)} undefined -> {rel(a.out)}")


if __name__ == "__main__":
    main()
