#!/usr/bin/env python3
"""Replace each game file's m2c preamble with `#include "game/game.h"` and what only that file needs.

    python3 tools/port/to_headers.py --build build/pc --dry-run          what would change, per file, and the totals
    python3 tools/port/to_headers.py --build build/pc --apply            change the files
    python3 tools/port/to_headers.py --build build/pc --apply src/blob/AbortCar.c ...    only these files
    python3 tools/port/to_headers.py --build build/pc --check            files the headers would change (exit 1 if any)

Almost every game C file under src/blob, src/ovl_a, src/ovl_b and nonmatching/ began with a context the decompiler
(m2c) wrote: thousands of lines of typedefs, structs and extern declarations, then the file's own code. The game's
shared declarations are include/game now (docs/code/README.md "The headers"): every type, datum and function the
files share, declared once. This tool rewrites a file so that it includes those and keeps only what is its own:

- **Its code** stays as it is, byte for byte: function definitions, data definitions, statics, macros, comments.
  Function bodies are never touched.
- **A declaration the headers make** goes. A function's: the headers' prototype is the definition's, which wins over
  the file's (a call through it may compile differently: tools/port/objsame.py shows which units, and
  docs/port/PROTOTYPES.md why that is harmless). A datum's: when the file declared it with the headers' very type as
  the file sees it (an array open-ended: EXTENT, include/game/basic.h).
- **A datum the file reads through another type** (its own struct, a signed copy, an array with a bound) keeps that
  view, as two lines after the include (include/game/view.h):

      VIEW(Cam152[], gCamera);
      #define gCamera gCamera_view

  gCamera_view has the file's type and gCamera's own symbol (an assembler label), so the code compiles exactly as
  with the file's old declaration. A macro of the file's that already names the datum (`#define gCheckpoints
  (gCheckpoints[0])`, `#define ObjInstanceListAlias ObjInstanceList`) names the view instead.
- **A type** the file's code (or a kept declaration, or a view) uses and the headers lack stays, with its comments:
  a file's own view of a structure. A type the headers define under the same name goes when it is the headers' type
  (what the file's DWARF says). One the file defines its own way stays, renamed in that file only by a macro after
  the include, `#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */`, so its code goes
  on naming it. An anonymous struct the headers name only by tag through a pointer (`struct Car *`, in a prototype of
  a function the file defines) gets that tag, so the file's definition and the headers' prototype are one type.
- **Declarations nothing uses** go, and so do m2c's helper block and its comment (M2C_FIELD and the rest:
  include/game/m2c.h), the file's definitions of a macro the headers define alike (NULL, TRUE, FALSE; one defined
  otherwise is #undef'd first), and the old `/* flags: ... */` line of the IDO build.
- **What the headers don't declare** (a PC-layer or libultra function, an assembly routine, a function its callers
  disagree with) stays as the file declared it, when the file uses it.

Whether a datum's type is the headers' comes from DWARF: the build's -O0 objects (types_objects.txt: each file's
declaration of what it uses) and include/game compiled alone (port/data/game_decls.c's object, with every
declaration). So build the tree first (Debug, or any build: types_objects.txt is -O0 in each). A file whose object
the build lacks is converted from its text alone, viewing every datum it declares otherwise than the headers.

Re-runnable: a converted file has nothing left to convert, and a second run changes nothing. Run it after merging
other work that edits the game's files, after a build. A name the headers declare that a file now gives another kind
(a static, a typedef) is reported: rename one or take it out of the headers. The headers themselves are kept by hand
(the generator that first wrote them is described in docs/code/README.md); --check reports a file the headers would
still change. Stdlib and binutils (readelf).
"""
import argparse
import collections
import concurrent.futures
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import ctop  # noqa: E402
import gen_data as G  # noqa: E402

ROOT = HERE.parents[1]
HEADERS = ROOT / "include" / "game"
ID = re.compile(r"[A-Za-z_]\w*")
TAGREF = re.compile(r"\b(struct|union|enum)\s+([A-Za-z_]\w*)")
INCLUDE = '#include "game/game.h"'
# The block m2c wrote into its files: these lines are include/game/m2c.h's now (or nothing uses them).
M2C_BLOCK = """#ifndef M2C_MACROS_H
#define M2C_MACROS_H
/* Unknown types */
typedef s32 M2C_UNK;
typedef s8  M2C_UNK8;
typedef s16 M2C_UNK16;
typedef s32 M2C_UNK32;
typedef s64 M2C_UNK64;
/* Unknown field access, like `*(type_ptr) &expr->unk_offset` */
#define M2C_FIELD(expr, type_ptr, offset) (*(type_ptr)((s8 *)(expr) + (offset)))
/* Bitwise (reinterpret) cast */
#define M2C_BITWISE(type, expr) ((type)(expr))
/* Unaligned reads */
#define M2C_LWL(expr) (expr)
#define M2C_FIRST3BYTES(expr) (expr)
#define M2C_UNALIGNED32(expr) (expr)
/* Unhandled instructions */
#define M2C_ERROR(desc) (0)
#define M2C_TRAP_IF(cond) (0)
#define M2C_BREAK() (0)
#define M2C_SYNC() (0)
#define GLUE_F64(a, b) (0.0)
#define MULT_HI(a, b) (0)
#define MULTU_HI(a, b) (0)
#define DMULT_HI(a, b) (0)
#define DMULTU_HI(a, b) (0)
#define CLZ(x) (0)
#define REVERSE_BITS(x) (0)
#define ROTATE_RIGHT(x, shift) (0)
#define ARM_RRX(x, carry) (0)
#define BSWAP32(x) (0)
#define BSWAP16(x) (0)
#define BSWAP16X2(x) (0)
/* Carry/overflow bits from partially-implemented instructions */
#define M2C_CARRY 0
#define M2C_OVERFLOW(a) (0)
/* Memcpy patterns */
#define M2C_MEMCPY_ALIGNED memcpy
#define M2C_MEMCPY_UNALIGNED memcpy
#define M2C_STRUCT_COPY memcpy
#endif"""
M2C_LINES = {" ".join(line.split()) for line in M2C_BLOCK.splitlines()}
# and the comment it put before that block
M2C_COMMENT = " ".join("""/*
 * This header contains macros emitted by m2c in "valid syntax" mode,
 * which can be enabled by passing `--valid-syntax` on the command line.
 *
 * In this mode, unhandled types and expressions are emitted as macros so
 * that the output is compilable without human intervention.
 */""".split())


def macro_body(text):
    """A macro's parameters and replacement as tokens, comments left out: `((void *)0)` is `((void*)0)`."""
    return " ".join(t.text for t in ctop.tokens(text.replace("\\\n", " ")) if t.kind not in ("lc", "bc"))
# libultra's and the microcode's types: the headers' own, whatever a file wrote for them
SDK_TYPES = {"OSMesg", "OSMesgQueue", "OSThread", "__OSThreadContext", "OSTime", "OSPri", "OSId", "OSTimer", "OSTask",
             "OSTask_t", "OSPfs", "OSContStatus", "OSContPad", "OSViMode", "OSPiHandle", "OSIoMesg", "OSIoMesgHdr",
             "OSScTask", "OSScClient", "OSSched", "Gfx", "Mtx", "Vtx", "Vtx_t", "Gwords", "OSEvent", "OSIntMask",
             "OSViFieldRegs", "OSViCommonRegs", "__OSInode", "__OSInodeUnit", "__OSDir", "OSPifRam",
             "__OSViContext", "OSDevMgr", "OSPiInfo", "OSTranxInfo", "__OSBlockInfo", "__OSTranxInfo", "OSScMsg",
             "OSYieldResult", "M2C_UNK"}
BASIC = {"s8", "u8", "s16", "u16", "s32", "u32", "s64", "u64", "f32", "f64", "vu8", "vu16", "vu32", "vu64", "vs8",
         "vs16", "vs32", "vs64"}
BASIC_C = {"s8": "signed char", "u8": "unsigned char", "s16": "short", "u16": "unsigned short", "s32": "int",
           "u32": "unsigned int", "s64": "long long", "u64": "unsigned long long", "f32": "float", "f64": "double"}
DECL_BEFORE = {"int", "char", "short", "long", "float", "double", "unsigned", "signed", ",", "*", "s8", "u8", "s16",
               "u16", "s32", "u32", "s64", "u64", "f32", "f64"}


def game_files(root):
    """The files to convert: the game's C but the boot segment's (src/rom has its own headers: rom_tu.h)."""
    out = sorted(str(p.relative_to(root)) for p in (root / "src").rglob("*.c") if not
                 str(p.relative_to(root)).startswith("src/rom/"))
    out += sorted(str(p.relative_to(root)) for p in (root / "nonmatching").rglob("*.c"))
    return out


# ---------------------------------------------------------------- the headers
class Headers:
    """What include/game declares: data, functions, types (typedef names and tags), and the tags it only names."""

    def __init__(self, root):
        self.data, self.funcs, self.types, self.tags, self.opaque = {}, set(), set(), set(), set()
        self.kinds = collections.defaultdict(set)
        self.macros = {}
        for p in sorted((root / "include" / "game").glob("*.h")):
            for it in ctop.split(p.read_text()):
                if it.kind == "pp":
                    m = re.match(r"#\s*define\s+([A-Za-z_]\w*)(.*)", it.text(), flags=re.S)
                    if m:
                        self.kinds[m.group(1)].add("macro")
                        self.macros[m.group(1)] = macro_body(m.group(2))
                    continue
                d = it.decl() if it.kind == "decl" else None
                if not d:
                    continue
                if "typedef" in d.storage:
                    for dc in d.declarators:
                        self.types.add(dc.name)
                if d.tag:
                    if d.tag_body:
                        self.tags.add(f"{d.tag_kind} {d.tag}")
                    elif not d.declarators:
                        self.opaque.add(f"{d.tag_kind} {d.tag}")
                if "typedef" in d.storage:
                    continue
                for dc in d.declarators:
                    if dc.func:
                        self.funcs.add(dc.name)
                    else:
                        self.data[dc.name] = ctop.norm(it.toks)


# ---------------------------------------------------------------- DWARF: each file's types of what it declares
def unit_of(obj):
    m = re.search(r"CMakeFiles/[^/]+\.dir/(?:\./)?(.*)\.o(?:bj)?$", obj.replace(os.sep, "/"))
    return m.group(1) if m else obj


def ctype(dies, off, memo, depth=0):
    """A type as a value that two objects agree on when the C types are the same: structs by their name (typedef or
    tag) and members, typedefs of other things resolved, qualifiers kept."""
    if off is None:
        return ("void",)
    if off in memo:
        return memo[off]
    d = dies.get(off)
    if d is None or depth > 40:
        return ("?",)
    t = d.tag
    nm = G._name(d.attrs["DW_AT_name"]) if "DW_AT_name" in d.attrs else None
    if t == "DW_TAG_typedef":
        inner = dies.get(G._ref(d))
        if inner is not None and inner.tag in ("DW_TAG_structure_type", "DW_TAG_union_type") and \
                "DW_AT_name" not in inner.attrs:
            r = ("S~", nm) + ctype(dies, G._ref(d), memo, depth + 1)[2:]   # anonymous: not a tagged one
        else:
            r = ctype(dies, G._ref(d), memo, depth + 1)
    elif t in ("DW_TAG_const_type", "DW_TAG_volatile_type"):
        r = (t[7], ctype(dies, G._ref(d), memo, depth + 1))
    elif t == "DW_TAG_base_type":
        r = ("b", nm, G._num(d.attrs.get("DW_AT_byte_size")))
    elif t == "DW_TAG_pointer_type":
        # a pointer to a struct: the struct by its name only (its tag, or an anonymous one's typedef name)
        tgt = dies.get(G._ref(d)) if G._ref(d) else None
        hint = None
        quals = []
        while tgt is not None and tgt.tag in ("DW_TAG_typedef", "DW_TAG_const_type", "DW_TAG_volatile_type"):
            if tgt.tag == "DW_TAG_typedef":
                hint = hint or G._name(tgt.attrs.get("DW_AT_name", "?"))
            else:
                quals.append(tgt.tag[7])
            nxt = dies.get(G._ref(tgt)) if G._ref(tgt) else None
            if nxt is None or nxt.tag not in ("DW_TAG_typedef", "DW_TAG_const_type", "DW_TAG_volatile_type",
                                              "DW_TAG_structure_type", "DW_TAG_union_type"):
                tgt = None
                break
            tgt = nxt
        if tgt is not None and tgt.tag in ("DW_TAG_structure_type", "DW_TAG_union_type"):
            name = G._name(tgt.attrs["DW_AT_name"]) if "DW_AT_name" in tgt.attrs else hint
            r = ("p", tuple(quals) + ("S" if "DW_AT_name" in tgt.attrs else "S~", name))
        else:
            r = ("p", ctype(dies, G._ref(d), memo, depth + 1))
    elif t == "DW_TAG_enumeration_type":
        r = ("e", nm, G._num(d.attrs.get("DW_AT_byte_size", "4")))
    elif t in ("DW_TAG_structure_type", "DW_TAG_union_type"):
        mem = tuple((G._name(m.attrs.get("DW_AT_name", "")), G._num(m.attrs.get("DW_AT_data_member_location", "0")),
                     ctype(dies, G._ref(m), memo, depth + 1)) for m in d.children if m.tag == "DW_TAG_member")
        r = ("S", nm, mem)
    elif t == "DW_TAG_array_type":
        r = ctype(dies, G._ref(d), memo, depth + 1)
        for s in reversed([c for c in d.children if c.tag == "DW_TAG_subrange_type"] or [None]):
            n = None
            if s is not None and "DW_AT_upper_bound" in s.attrs:
                ub = G._num(s.attrs["DW_AT_upper_bound"])
                n = ub + 1 if ub is not None and 0 <= ub < 1 << 40 else None
            elif s is not None and "DW_AT_count" in s.attrs:
                n = G._num(s.attrs["DW_AT_count"])
            r = ("a", r, n)
    elif t == "DW_TAG_subroutine_type":
        r = ("f", ctype(dies, G._ref(d), memo, depth + 1),
             tuple(ctype(dies, G._ref(c), memo, depth + 1) for c in d.children if c.tag == "DW_TAG_formal_parameter"),
             "DW_AT_prototyped" in d.attrs)
    else:
        r = ("?", t)
    memo[off] = r
    return r


def members(t):
    """A struct's ctype without its own name (an anonymous struct and a tagged one with one body are the same)."""
    return t[2:] if t and t[0] in ("S", "S~") else t


def same_named_type(key, ftypes, canon):
    """The file's type of this name is the headers' (or the file's DWARF doesn't have it: assumed so)."""
    if ftypes is None or ("type", key) not in ftypes:
        return True
    mine = ftypes[("type", key)]
    for k in (key, sdk_name(key), "struct " + sdk_name(key), "union " + sdk_name(key)):
        if ("type", k) in canon:
            return members(mine) == members(canon[("type", k)])
    return True


def view_name(name, hdr, used):
    """A name for a file's own type whose name the headers give another type: `Gfx` -> `GfxView`."""
    n = name + "View"
    k = 2
    while n in hdr.types or n in hdr.data or n in hdr.funcs or n in used:
        n = f"{name}View{k}"
        k += 1
    return n


def sdk_name(key):
    """libultra's typedef name for one of its tags: `struct OSThread_s` -> OSThread."""
    return re.sub(r"_s$", "", key.split(" ", 1)[-1])


def struct_names(t, tags):
    """The names of the structs a ctype reaches whose tag is in `tags` (as `struct N` or `union N`): N and the tag."""
    out = set()
    stack = [t]
    while stack:
        x = stack.pop()
        if not isinstance(x, tuple):
            continue
        if len(x) >= 2 and x[0] in ("S", "S~") and isinstance(x[1], str):
            for kw in ("struct", "union"):
                if f"{kw} {x[1]}" in tags:
                    out |= {x[1], f"{kw} {x[1]}"}
        stack.extend(y for y in x if isinstance(y, tuple))
    return out


def same_type(mine, theirs):
    """The file's declaration of a datum is the headers' as the file sees it. The headers' DWARF (game_decls.c's) has
    an array's extent, which a game file doesn't see (EXTENT: `T x[]`): a file's open array of the same elements is
    the headers' declaration; a file's bounded one is a view (the bound changes what the optimizer may assume)."""
    if mine and theirs and mine[0] == theirs[0] == "a":
        return mine[2] is None and mine[1] == theirs[1]
    return mine == theirs


def read_types(objects, readelf):
    """unit -> {datum name: ctype} for the external variables each object declares or defines."""
    out = {}
    for i in range(0, len(objects), 50):
        batch = objects[i:i + 50]
        text = subprocess.run([*G.dwarf_info_command(readelf), *batch], capture_output=True, text=True,
                              check=True).stdout
        for obj, dies in G.parse_readelf(text, batch[0] if len(batch) == 1 else None).items():
            memo, vars_ = {}, {}
            cu = next((d for d in dies.values() if d.tag == "DW_TAG_compile_unit"), None)
            for d in cu.children if cu is not None else ():
                if d.tag != "DW_TAG_variable":
                    continue
                spec = dies.get(G._ref(d, "DW_AT_specification")) if "DW_AT_specification" in d.attrs else None
                src = spec or d
                if "DW_AT_name" not in src.attrs or "DW_AT_external" not in src.attrs:
                    continue
                toff = G._ref(d) if "DW_AT_type" in d.attrs else G._ref(src)
                vars_[G._name(src.attrs["DW_AT_name"])] = ctype(dies, toff, memo)
            # the named types: a typedef name, or `struct T` for a tag
            for off, d in dies.items():
                if "DW_AT_name" not in d.attrs or "DW_AT_declaration" in d.attrs:
                    continue
                n = G._name(d.attrs["DW_AT_name"])
                if d.tag == "DW_TAG_typedef":
                    vars_.setdefault(("type", n), ctype(dies, G._ref(d), memo))
                elif d.tag in ("DW_TAG_structure_type", "DW_TAG_union_type"):
                    kw = "struct" if d.tag == "DW_TAG_structure_type" else "union"
                    vars_.setdefault(("type", f"{kw} {n}"), ctype(dies, off, memo))
            out[unit_of(obj)] = vars_
    return out


def load_dwarf(build, readelf, jobs):
    objs = (Path(build) / "types_objects.txt").read_text().split()
    decls = sorted(Path(build).glob("CMakeFiles/game_decls.dir/**/game_decls.c.o*"))
    if not decls:
        sys.exit(f"to_headers: no game_decls object in {build} (build the tree first)")
    batches = [objs[i:i + 50] for i in range(0, len(objs), 50)]
    files = {}
    with concurrent.futures.ProcessPoolExecutor(max_workers=jobs) as ex:
        for r in ex.map(read_types, batches, [readelf] * len(batches)):
            files.update(r)
    canon = next(iter(read_types([str(decls[0])], readelf).values()))
    return files, canon


# ---------------------------------------------------------------- one file
class Plan:
    def __init__(self, rel):
        self.rel = rel
        self.counts = collections.Counter()
        self.views, self.kept_decls, self.kept_types, self.problems = [], [], [], []
        self.text = None


def fmt_type(toks):
    """A type name from its tokens, spaced as C is written: `Camera *[4]`, `void (*)(s32)`."""
    s = " ".join(toks)
    s = re.sub(r"\s*([\[\]\),])", r"\1", s)
    s = re.sub(r"([\(\[])\s*", r"\1", s)
    s = re.sub(r"\*\s+(?=[\*\)\[])", "*", s)
    s = re.sub(r"\)\s+\(", ")(", s)
    return s.replace(",", ", ")


def convert(rel, src, hdr, ftypes, canon):
    """The file's new text and what was done (a Plan)."""
    plan = Plan(rel)
    items = ctop.split(src)
    # a storage class the file spells with a macro (`#define WEAPON_STATE extern`): the declaration is that
    storage_macros = {}
    for it in items:
        m = re.fullmatch(r"#\s*define\s+([A-Za-z_]\w*)\s+(extern|static)\s*(?:/\*.*\*/)?\s*", it.text(), flags=re.S) \
            if it.kind == "pp" else None
        if m:
            storage_macros[m.group(1)] = m.group(2)
    for it in items:
        if it.kind == "decl" and storage_macros:
            toks = [t for t in it.toks if t.kind not in ("lc", "bc")]
            if toks and toks[0].text in storage_macros:
                it._decl = ctop.parse_decl(toks[1:])
                it._decl.storage.add(storage_macros[toks[0].text])
    keep = [True] * len(items)
    replace = {}              # item index -> replacement text (declarations partly kept, views)
    type_items = {}           # key -> item index
    for i, it in enumerate(items):
        d = it.decl() if it.kind == "decl" else None
        if d and ("typedef" in d.storage or (d.tag and d.tag_body and not d.declarators)):
            for dc in d.declarators:
                if dc.name and "typedef" in d.storage:
                    type_items.setdefault(dc.name, i)
            if d.tag and d.tag_body:
                type_items.setdefault(f"{d.tag_kind} {d.tag}", i)

    # the M2C block and the IDO flags line
    in_m2c = False
    for i, it in enumerate(items):
        text = " ".join(it.text().split())
        if it.kind == "comment" and i == 0 and re.fullmatch(r"/\* flags: .*\*/", text):
            keep[i] = False
            plan.counts["flags line"] += 1
            continue
        if it.kind == "comment" and text == M2C_COMMENT:
            keep[i] = False
            plan.counts["m2c lines"] += 1
            continue
        if it.kind == "pp" and text == "#ifndef M2C_MACROS_H":
            in_m2c = True
        if in_m2c:
            lead = " ".join(" ".join(t.text.split()) for t in it.lead)
            standard = text in M2C_LINES and all(" ".join(t.text.split()) in M2C_LINES for t in it.lead)
            if it.kind == "comment":
                standard = all(" ".join(t.text.split()) in M2C_LINES for t in it.toks)
            if standard:
                keep[i] = False
                plan.counts["m2c lines"] += 1
            if it.kind == "pp" and text == "#endif":
                in_m2c = False
            del lead

    # what the file's own code uses
    def ids_of(it):
        code = it.code()
        return set(ID.findall(code)) | {f"{k} {n}" for k, n in TAGREF.findall(code)}

    own = set()
    decl_items = []
    for i, it in enumerate(items):
        if not keep[i]:
            continue
        d = it.decl() if it.kind == "decl" else None
        if it.kind in ("func", "pp", "other"):
            own |= ids_of(it)
        elif it.kind == "decl":
            if d is None or (it.toks and it.toks[0].text == "VIEW"):
                own |= ids_of(it)     # a view this tool wrote before: the file's own
            elif "typedef" in d.storage or (d.tag and d.tag_body and not d.declarators):
                pass
            elif "static" in d.storage or any(dc.init is not None for dc in d.declarators) or \
                    ("extern" not in d.storage and d.declarators and not any(dc.func for dc in d.declarators)):
                own |= ids_of(it)     # a definition: the file's own
            else:
                decl_items.append(i)

    renames = {}              # a file's own type -> the name it has in the file, where the headers have that name
    # a macro of the headers' that the file defines its own way (its own M2C_FIELD): undefined first, so the file's
    # definition is the one its code sees, as before
    for i, it in enumerate(items):
        if it.kind == "pp" and keep[i]:
            m = re.match(r"#\s*define\s+([A-Za-z_]\w*)(.*)", it.text(), flags=re.S)
            if m and m.group(1) in hdr.macros and macro_body(m.group(2)) == hdr.macros[m.group(1)]:
                keep[i] = False   # the headers' own definition (NULL, TRUE, FALSE)
                plan.counts["macros the headers define"] += 1
            elif m and "macro" in hdr.kinds.get(m.group(1), ()) and \
                    not any(" ".join(p.text().split()) == f"#undef {m.group(1)}" for p in items[max(0, i - 2):i]):
                replace[i] = f"#undef {m.group(1)}\n" + it.text()
                plan.counts["header macros redefined"] += 1

    # the file's object-like macros, by name
    file_macros = {}
    for i, it in enumerate(items):
        if it.kind == "pp" and keep[i]:
            m = re.match(r"#\s*define\s+([A-Za-z_]\w*)(?!\()", it.text())
            if m:
                file_macros.setdefault(m.group(1), i)
    # the macros that are another name of one identifier: `#define A B`
    alias = {}
    for n, j in file_macros.items():
        m = re.fullmatch(r"#\s*define\s+%s\s+([A-Za-z_]\w*)\s*(?:/\*.*\*/)?\s*" % re.escape(n), items[j].text(),
                         flags=re.S)
        if m:
            alias[n] = m.group(1)

    # basic type names a function uses for a local or a parameter (`int u8, u9;`)
    shadowed = set()
    for it in items:
        if it.kind != "func":
            continue
        toks = [t.text for t in it.toks if t.kind not in ("lc", "bc")]
        for k, t in enumerate(toks[1:-1], 1):
            if t in BASIC_C and toks[k - 1] in DECL_BEFORE and toks[k + 1] in (",", ";", "=", "[", ")"):
                shadowed.add(t)

    # declarations: drop, keep, or a view
    needed_from_decls = set()
    for i in decl_items:
        it = items[i]
        d = it.decl()
        toks = [t for t in it.toks if t.kind not in ("lc", "bc")]
        spec = " ".join(t.text for t in d.spec)
        kept, views, struct_lines = [], [], []
        for dc in d.declarators:
            n = dc.name
            if not n:
                kept.append(dc)
                continue
            if dc.func:
                if n in hdr.funcs:
                    plan.counts["function declarations the headers make"] += 1
                elif n in own:
                    kept.append(dc)
                    plan.kept_decls.append(n)
                else:
                    plan.counts["unused declarations"] += 1
                continue
            if n not in own:
                plan.counts["unused declarations"] += 1
                continue
            # a macro that names the datum under another name (`#define ObjInstanceListAlias ObjInstanceList`)
            real = alias[n] if alias.get(n) in hdr.data else n
            if real in hdr.data:
                mine = ftypes.get(real) if ftypes is not None else None
                theirs = canon.get(real)
                if mine is not None and theirs is not None and same_type(mine, theirs):
                    plan.counts["data declarations the headers make"] += 1
                    # a struct the headers name only by its tag: the file's own definition completes it
                    needed_from_decls |= struct_names(mine, hdr.opaque)
                    continue
                spec_toks = [t.text for t in d.spec]
                if "{" in spec_toks:
                    # a struct defined in the declaration: on a line of its own, and named in the view (VIEW's
                    # argument can't hold its commas)
                    body = fmt_type(spec_toks)
                    if d.tag:
                        struct_lines.append(f"{body};")
                        spec_toks = [d.tag_kind, d.tag]
                    else:
                        struct_lines.append(f"typedef {body} {n}_type;")
                        spec_toks = [f"{n}_type"]
                toks_ty = spec_toks + [t.text for t in dc.toks if t.kind not in ("lc", "bc")]
                if "=" in toks_ty:
                    toks_ty = toks_ty[:toks_ty.index("=")]
                toks_ty.remove(n)
                # a basic type's name a function of the file uses for a local: spelled out (the view's type is
                # resolved where it is used)
                toks_ty = [BASIC_C.get(t, t) if t in shadowed else t for t in toks_ty]
                views.append((n, real, fmt_type(toks_ty)))
                continue
            kept.append(dc)
            plan.kept_decls.append(n)
        lines = list(struct_lines)
        if kept and len(kept) == len([dc for dc in d.declarators]):
            pass   # the item stays whole
        else:
            if kept:
                storage = " ".join(s for s in ("extern",) if s in d.storage)
                lines.append(f"{storage + ' ' if storage else ''}{spec} " +
                             ", ".join(ctop.norm(dc.toks) for dc in kept) + ";")
            for n, real, ty in views:
                plan.views.append(real)
                lines.append(f"VIEW({ty}, {real});")
                if n in file_macros:
                    # the file defines the name as a macro already (`#define gCheckpoints (gCheckpoints[0])`, or an
                    # alias of the datum): that macro names the view
                    j = file_macros[n]
                    body = replace.get(j, items[j].text())
                    head, _, rest = body.partition(n)
                    replace[j] = head + n + re.sub(r"\b%s\b" % re.escape(real), f"{real}_view", rest)
                    continue
                lines.append(f"#define {n} {n}_view")
            replace[i] = "\n".join(lines)
            if not lines:
                keep[i] = False
        if kept:
            needed_from_decls |= ids_of(it)
        for n, real, ty in views:
            needed_from_decls |= set(ID.findall(ty)) | {f"{k} {m}" for k, m in TAGREF.findall(ty)}

    # the structs the headers name only by tag in the data the file's code uses (`extern union InputSequence *gEgg`):
    # the file's own definition completes them
    for n in own:
        if n in hdr.data and n in canon:
            needed_from_decls |= struct_names(canon[n], hdr.opaque)

    # types: what the file's own code, its kept declarations and its views use, and what those types use
    needed = set()
    work = [k for k in (own | needed_from_decls) if k in type_items]
    enum_items = {}
    for i, it in enumerate(items):
        d = it.decl() if it.kind == "decl" else None
        if d and d.tag_kind == "enum" and d.tag_body:
            body = it.code().split("{", 1)[1] if "{" in it.code() else ""
            for m in re.finditer(r"(?:^|[{,])\s*([A-Za-z_]\w*)", body):
                enum_items[m.group(1)] = i
    work_items = set()
    for k in own | needed_from_decls:
        if k in enum_items:
            work_items.add(enum_items[k])
    item_keys = collections.defaultdict(set)
    by_key = collections.defaultdict(list)      # a name -> every item that defines it (`typedef T FAR a; ... FAR b;`)
    for i, it in enumerate(items):
        d = it.decl() if it.kind == "decl" else None
        if d and ("typedef" in d.storage or (d.tag and d.tag_body and not d.declarators)):
            if "typedef" in d.storage:
                item_keys[i] |= {dc.name for dc in d.declarators if dc.name}
            if d.tag and d.tag_body:
                item_keys[i].add(f"{d.tag_kind} {d.tag}")
            for k in item_keys[i]:
                by_key[k].append(i)
    seen = set()
    while work or work_items:
        if work:
            k = work.pop()
            if k in seen:
                continue
            seen.add(k)
            needed.add(k)
            for i in by_key.get(k, ()):
                work_items.add(i)
            continue
        i = work_items.pop()
        if ("item", i) in seen:
            continue
        seen.add(("item", i))
        needed.add(("item", i))
        for k2 in ids_of(items[i]) - item_keys[i]:
            if k2 in type_items and k2 not in seen:
                work.append(k2)
            if k2 in enum_items and ("item", enum_items[k2]) not in seen:
                work_items.add(enum_items[k2])
    for i, it in enumerate(items):
        if not keep[i]:
            continue
        d = it.decl() if it.kind == "decl" else None
        if not d or not ("typedef" in d.storage or (d.tag and d.tag_body and not d.declarators)):
            continue
        keys = [dc.name for dc in d.declarators if dc.name] if "typedef" in d.storage else []
        if d.tag and d.tag_body:
            keys.append(f"{d.tag_kind} {d.tag}")
        enum_used = d.tag_kind == "enum" and ("item", i) in needed
        if not enum_used and not any(k in needed for k in keys):
            keep[i] = False
            plan.counts["unused types"] += 1
            continue
        names = [k.split(" ", 1)[-1] for k in keys]
        # the names of it the file uses: when the headers define all of them, the headers' definition serves
        # (`typedef struct OSThread_s {...} OSThread;` where the code says only OSThread)
        used_keys = [k for k in keys if k in needed] or keys
        if all(k in BASIC or k in hdr.types or k in hdr.tags or (sdk_name(k) in SDK_TYPES and sdk_name(k) in hdr.types)
               for k in used_keys) and not (d.tag_kind == "enum" and d.tag_body):
            # the headers' type, if it is this one (what the file's DWARF says, where it says anything)
            # (a name this tool renamed before, `#define Gfx GfxView`, is the file's own)
            differs = [k for k in used_keys if not same_named_type(k, ftypes, canon) or
                       re.fullmatch(re.escape(k.split(" ", 1)[-1]) + r"View\d*", alias.get(k.split(" ", 1)[-1], ""))]
            if not differs:
                keep[i] = False
                plan.counts["types the headers define"] += 1
                continue
        clash = [k for k in keys if k in hdr.types or k in hdr.tags or
                 (sdk_name(k) in SDK_TYPES and sdk_name(k) in hdr.types) or
                 # a union where the headers name a struct of that tag, or the other way round
                 (" " in k and (("struct " if k.startswith("union") else "union ") + k.split(" ", 1)[1]) in hdr.opaque)]
        for k in clash:
            # the file's own type of a name the headers give another: renamed in this file by a macro after the
            # include, so its code goes on naming it
            n0 = k.split(" ", 1)[-1]
            if n0 not in renames and n0 not in file_macros:
                renames[n0] = view_name(n0, hdr, own)
                plan.counts["own types renamed"] += 1
        # an anonymous struct the headers name by its typedef name as a tag: give it that tag
        if "typedef" in d.storage and d.tag_kind in ("struct", "union") and d.tag_body and not d.tag:
            for n0 in names:
                if f"{d.tag_kind} {n0}" in hdr.opaque and f"{d.tag_kind} {n0}" not in type_items:
                    t0 = next(t for t in it.toks if t.text == d.tag_kind)
                    replace[i] = src[it.start:t0.end] + " " + n0 + src[t0.end:it.end]
                    plan.counts["tags given"] += 1
                    break
        plan.kept_types.append("/".join(names))

    # names the headers declare that the file gives another kind
    for i, it in enumerate(items):
        if not keep[i]:
            continue
        d = it.decl() if it.kind == "decl" else None
        if it.kind == "func" and it.names and it.names[0] in (hdr.funcs | set(hdr.data)):
            if any(t.text == "static" for t in it.toks[:4]):
                plan.problems.append(f"{rel}: static {it.names[0]} has the name of a function include/game declares")
        if d and "typedef" in d.storage:
            for dc in d.declarators:
                if dc.name in hdr.funcs or dc.name in hdr.data:
                    plan.problems.append(f"{rel}: typedef {dc.name} has the name of something include/game declares")

    # the text: the kept items as they were, the whitespace between them at most one blank line where something
    # went; the include after the file's leading comments (unless the file has it)
    has_include = any(it.kind == "pp" and " ".join(it.text().split()) == INCLUDE for it in items)
    inc_at = None if has_include else \
        next((i for i, it in enumerate(items) if keep[i] and it.kind != "comment"), len(items))
    rename_text = "".join(f"#define {n} {v}   /* this file's own {n}: include/game's {n} is another type */\n"
                          for n, v in sorted(renames.items()))
    if rename_text and has_include:
        j = next(i for i, it in enumerate(items) if it.kind == "pp" and " ".join(it.text().split()) == INCLUDE)
        replace[j] = items[j].text() + "\n" + rename_text.rstrip("\n")
    out, prev, gap, removed, directive = [], 0, "", False, False
    for i, it in enumerate(items):
        s, e = it.span(with_lead=True)
        gap += src[prev:s]
        prev = e
        if i == inc_at:
            out.append(squeeze(gap, True) if out else "")
            out.append(INCLUDE + "\n" + (rename_text if rename_text else "") + "\n")
            gap, removed = "", False
        if not keep[i]:
            removed = True
            continue
        g = squeeze(gap, removed) if out else ""
        text_i = replace.get(i, src[s:e])
        if out and (text_i.lstrip().startswith("#") or directive) and "\n" not in g:
            g = "\n"   # a directive starts a line, and nothing may follow one on its line
        out.append(g)
        out.append(text_i)
        directive = i in replace and "#define" in text_i
        gap, removed = "", False
    if inc_at == len(items):
        out.append("\n\n" + INCLUDE + "\n")
    out.append(squeeze(gap + src[prev:], removed) if out else src[prev:])
    text = "".join(out).lstrip("\n")
    if not text.endswith("\n"):
        text += "\n"
    plan.text = text
    return plan


def squeeze(gap, removed):
    """The whitespace between two kept items: as it was, or at most one blank line where items between went."""
    if not removed:
        return gap
    n = gap.count("\n")
    return "\n" * min(max(n, 1), 2)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="*", help="only these files (default: every game file but src/rom)")
    ap.add_argument("--build", type=Path, required=True, help="a built tree's build directory (build/pc)")
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--readelf", default="readelf")
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--apply", action="store_true")
    mode.add_argument("--dry-run", action="store_true")
    mode.add_argument("--check", action="store_true")
    ap.add_argument("-j", "--jobs", type=int, default=min(8, os.cpu_count() or 2))
    ap.add_argument("--report", type=Path, help="write each file's views, kept declarations and types here")
    a = ap.parse_args(argv)
    root = a.root.resolve()
    hdr = Headers(root)
    ftypes_all, canon = load_dwarf(a.build, a.readelf, a.jobs)
    files = a.files or game_files(root)
    totals = collections.Counter()
    problems, changed = [], []
    lines_before = lines_after = 0
    rep = []
    for rel in files:
        p = root / rel
        src = p.read_text()
        plan = convert(rel, src, hdr, ftypes_all.get(rel), canon)
        totals.update(plan.counts)
        totals["views"] += len(plan.views)
        problems += plan.problems
        lines_before += src.count("\n")
        lines_after += plan.text.count("\n")
        if plan.text != src:
            changed.append(rel)
            if a.apply:
                p.write_text(plan.text)
        rep.append(f"{rel}: {src.count(chr(10))} -> {plan.text.count(chr(10))} lines; views: "
                   f"{' '.join(plan.views) or '-'}; kept declarations: {' '.join(plan.kept_decls) or '-'}; "
                   f"kept types: {' '.join(plan.kept_types) or '-'}")
    if a.report:
        a.report.write_text("\n".join(rep) + "\n")
    for line in problems:
        print("to_headers: " + line)
    print(f"to_headers: {len(files)} files, {len(changed)} {'changed' if a.apply else 'to change'}; lines "
          f"{lines_before} -> {lines_after}; " + ", ".join(f"{k} {v}" for k, v in sorted(totals.items())))
    if a.check:
        for rel in changed:
            print(f"to_headers: would change {rel}")
        return 1 if changed else 0
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
