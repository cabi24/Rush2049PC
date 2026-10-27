#!/usr/bin/env python3
"""Find the function prototypes the game's files disagree on, and what each disagreement costs on a 64-bit host.

    python3 tools/port/proto_check.py [--build build/pc] [--jobs 3] [--no-calls] [--doc docs/port/PROTOTYPES.md]

Every game C file carries its own declarations of what it calls. MIPS o32 forgave most disagreements with the
definition (an int and a pointer share a register, K&R calls put floats in GPRs). On aarch64 and x86-64 SysV
they are real bugs. This tool compares, for every external function the build defines:

- the definition's prototype (the strong definition the linker keeps; the PC layer's for replaced libultra
  units; the host's for the C library names the game calls), with
- every other file's view of it: its declaration (DW_TAG_subprogram with DW_AT_declaration in that object's
  DWARF), a weak context copy of the definition (calls in that file go through the copy's prototype), and for
  unprototyped and implicit declarations, the argument types each call site actually passes.

Views are read from the Debug build's DWARF (`readelf --debug-dump=info`, as gen_data.py does). Call sites come
from compiling the files with live callers of a suspect view again, with the build's own command plus
`-fsyntax-only -fdump-tree-original-raw`: the GENERIC tree has each call's promoted argument types (what an
unprototyped call passes), the operand each argument had before the prototype converted it, and whether the
result is used.

Which code runs comes from each object's relocations: weak context copies never run (every reference binds to
the owner), so a declaration only they use is `dead`, and their calls are left out. The same graph orders the
report: functions nearest the boot roots (the game's main, game_init, game_late_init, AudioInitSys, LoadWave...)
first.

Each disagreement is classified by what it does under AAPCS64 and x86-64 SysV (worst first):

    class      float vs integer/pointer argument: different register files, so this and every later
               argument of either class lands in the wrong register
    fwidth     f32 vs f64 argument (an unprototyped call promotes float to double)
    width      pointer or 64-bit integer vs a 32-bit-or-smaller integer argument: the upper half is lost or
               garbage (unless every caller passes a small constant, which is zero-extended: harmless)
    aggregate  a struct passed by value that travels differently (size, eightbyte classes, float aggregate)
    count      the caller passes fewer arguments than the definition reads
    return     return class differs and the caller uses the value (float vs int, pointer vs s32, a void
               definition read as a value)
    -- benign --
    narrow     integers of 32 bits or less with different width or signedness (s8 vs s32): same register,
               the extension may differ
    extra      the caller passes more arguments than the definition reads
    unused     the return types differ but no call reads the value
    reviewed   read by hand and known not to matter (REVIEWED below, docs/port/PROTOTYPES.md)
    dead       only weak copies refer to the function in that file
    harmless   same class and width (s32 vs u32, T * vs void *)

Outputs: build/pc/proto_report.txt (every disagreement), build/pc/proto_report.json, and with --doc the summary
table between the `<!-- proto_check:begin -->` / `<!-- proto_check:end -->` markers of that file.
Stdlib, binutils and the build's compiler. Python 3.9+.
"""
import argparse
import collections
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import n64syms  # noqa: E402
from gen_data import _name, _num, _ref, display, parse_readelf  # noqa: E402

ROOT = n64syms.ROOT
HARMFUL = ("stub", "class", "fwidth", "width", "aggregate", "count", "return")
EMPTY_FN = 8   # bytes of code an empty `void f(void) {}` compiles to (aarch64 `nop; ret`, x86-64 -O0 7)
BENIGN = ("narrow", "extra", "unused", "reviewed", "dead", "harmless")
CLASSES = HARMFUL + BENIGN
RANK = {c: i for i, c in enumerate(CLASSES)}
DEFAULT_ROOTS = "n64_main,game_init,game_late_init,AudioInitSys,LoadWave,InitWaves,LoadFrontEndOverlay"
# Disagreements that are read and known not to matter on the PC (docs/port/PROTOTYPES.md): function, or
# function@view file, -> why.
_HEAP = "heap block addresses are RDRAM-window addresses below 4 GB, which 32 bits hold exactly (POINTERS.md 4.1)"
_ROMDL = ("a word of a display list loaded from the ROM (N64 form): a 32-bit window address of loaded data "
          "(POINTERS.md 2)")
_SAME32 = "compared with 32-bit slots that hold the same truncation of the pointer (POINTERS.md 4.4)"
_INDEX = "a model index carried in a void * variable"
REVIEWED = {
    "func_800C1B60": "old-style definition; `hits` is a third parameter no caller passes, written before it is read",
    "PurgeableMemory": _HEAP, "func_80095EF4": _HEAP, "func_80095F8C": _HEAP, "audio_reverb_update": _HEAP,
    "audio_effect_process": _HEAP, "menu_item_select": _HEAP, "audio_buffer_sync": _HEAP,
    "AllocateMemoryHeap@src/blob/groups/initgraphics/initgfx.c": _HEAP,
    "sound_play_menu@src/blob/groups/initgraphics/initgfx.c": _HEAP,
    "format_string_parse@src/blob/groups/menu_save_options/ctx_slot.c": "the buffer is a heap block: " + _HEAP,
    "inflate_entry_alt@src/blob/groups/menu_save_options/ctx_slot.c": "the buffer is a heap block: " + _HEAP,
    "inflate_decompress": "the source is a ROM offset from the file table D_8011B5BC, not a host pointer",
    "lzss_decompress": "the source is a ROM offset from the file table D_8011B5BC, not a host pointer",
    "entity_cull_check@src/blob/groups/audio_frame_sync/waves.c":
        "the WAD loader (an N64_WORLD file) passes its loaded block's window addresses as 32-bit integers",
    "func_800BD080": _ROMDL, "func_800BDA24": _ROMDL,
    "state_utility@src/ovl_a/func_803AE51C.c":
        "the text comes from the loaded text bank, whose fields are 32-bit window addresses (POINTERS.md 4.2)",
    "audio_channel_priority": _SAME32, "func_800956BC": _SAME32,
    "string_copy_format@src/ovl_a/groups/runtime_a_availability_refresh/group.c": _INDEX,
    "sign_extend_call@src/ovl_a/groups/runtime_a_availability_refresh/group.c": _INDEX,
    "func_80014D08": "the definition is empty, as in retail: its arguments are never read",
}

VOID = ("void",)
PTR = ("p",)
QUAL_TAGS = ("DW_TAG_typedef", "DW_TAG_const_type", "DW_TAG_volatile_type", "DW_TAG_restrict_type",
             "DW_TAG_atomic_type")

# The host C library functions the game calls under their own names (n64syms.HOST_LIBC; memcpy, memset, strlen,
# strchr, sinf and cosf compile as n64_* and are the game's own). (return, [params], varargs).
_I32, _SZ, _F, _D = ("i", 4, True), ("i", 8, False), ("f", 4), ("f", 8)
HOST_PROTOS = {
    "bcopy": (VOID, [PTR, PTR, _SZ], False), "bzero": (VOID, [PTR, _SZ], False),
    "bcmp": (_I32, [PTR, PTR, _SZ], False), "memmove": (PTR, [PTR, PTR, _SZ], False),
    "memcmp": (_I32, [PTR, PTR, _SZ], False), "strcpy": (PTR, [PTR, PTR], False),
    "strcmp": (_I32, [PTR, PTR], False), "strncpy": (PTR, [PTR, PTR, _SZ], False),
    "sqrtf": (_F, [_F], False), "sqrt": (_D, [_D], False), "fabsf": (_F, [_F], False), "fabs": (_D, [_D], False),
    "abort": (VOID, [], False), "printf": (_I32, [PTR], True), "sprintf": (_I32, [PTR, PTR], True),
    "vsprintf": (_I32, [PTR, PTR, ("any",)], False), "modff": (_F, [_F, PTR], False),   # va_list: the host's
}
HOST_DISPLAY = {"bcopy": "void bcopy(const void *, void *, size_t)", "bzero": "void bzero(void *, size_t)",
                "bcmp": "int bcmp(const void *, const void *, size_t)",
                "memmove": "void *memmove(void *, const void *, size_t)",
                "memcmp": "int memcmp(const void *, const void *, size_t)",
                "strcpy": "char *strcpy(char *, const char *)", "strcmp": "int strcmp(const char *, const char *)",
                "strncpy": "char *strncpy(char *, const char *, size_t)", "sqrtf": "float sqrtf(float)",
                "sqrt": "double sqrt(double)", "fabsf": "float fabsf(float)", "fabs": "double fabs(double)",
                "abort": "void abort(void)", "printf": "int printf(const char *, ...)",
                "sprintf": "int sprintf(char *, const char *, ...)",
                "vsprintf": "int vsprintf(char *, const char *, va_list)", "modff": "float modff(float, float *)"}


# ---------------------------------------------------------------- types by ABI

def abi_of(dies, off, depth=0):
    """ABI view of a DWARF type: ('void',), ('i', size, signed or None), ('f', size), ('p',),
    ('a', size, scalars) for structs and unions (scalars: ((offset, kind), ...) for those of 16 bytes or less),
    ('?',) when unknown."""
    if off is None:
        return VOID
    d = dies.get(off)
    if d is None or depth > 12:
        return ("?",)
    tag = d.tag
    if tag in QUAL_TAGS:
        return abi_of(dies, _ref(d), depth + 1)
    if tag == "DW_TAG_base_type":
        enc, size = _num(d.attrs.get("DW_AT_encoding", "0")), _num(d.attrs.get("DW_AT_byte_size", "0"))
        if enc == 4:
            return ("f", size)
        if enc == 3:   # complex float: two floats
            return ("a", size, ((0, ("f", size // 2)), (size // 2, ("f", size // 2))))
        return ("i", size, enc in (5, 6))
    if tag in ("DW_TAG_pointer_type", "DW_TAG_reference_type", "DW_TAG_array_type"):
        return PTR
    if tag == "DW_TAG_enumeration_type":
        return ("i", _num(d.attrs.get("DW_AT_byte_size", "4")), None)
    if tag in ("DW_TAG_structure_type", "DW_TAG_union_type"):
        if "DW_AT_declaration" in d.attrs:
            return ("a", None, None)
        size = _num(d.attrs.get("DW_AT_byte_size", "0"))
        scalars = []
        if size is not None and size <= 16:
            flatten(dies, off, 0, scalars, 0)
        return ("a", size, tuple(sorted(set(scalars))) if size is not None and size <= 16 else None)
    if tag == "DW_TAG_subroutine_type":
        return ("fn",)
    return ("?",)


def flatten(dies, off, base, out, depth):
    """(offset, scalar ABI type) of every scalar inside the aggregate at off."""
    d = dies.get(off)
    if d is None or depth > 12:
        return
    if d.tag in QUAL_TAGS:
        flatten(dies, _ref(d), base, out, depth + 1)
    elif d.tag in ("DW_TAG_structure_type", "DW_TAG_union_type"):
        for m in d.children:
            if m.tag == "DW_TAG_member":
                flatten(dies, _ref(m), base + (_num(m.attrs.get("DW_AT_data_member_location", "0")) or 0), out,
                        depth + 1)
    elif d.tag == "DW_TAG_array_type":
        n = 1
        for s in d.children:
            if s.tag == "DW_TAG_subrange_type":
                ub = _num(s.attrs["DW_AT_upper_bound"]) if "DW_AT_upper_bound" in s.attrs else None
                n *= (ub + 1) if ub is not None and 0 <= ub < 1 << 20 else 0
        e = abi_of(dies, _ref(d), depth + 1)
        esz = size_of(e)
        for i in range(min(n, 16)):
            if e[0] == "a":
                flatten(dies, _ref(d), base + i * esz, out, depth + 1)
            else:
                out.append((base + i * esz, e))
    else:
        out.append((base, abi_of(dies, off, depth + 1)))


def size_of(t):
    if t[0] == "i" or t[0] == "f":
        return t[1] or 0
    if t[0] == "p":
        return 8
    if t[0] == "a":
        return t[1] or 0
    return 0


def short(t):
    """A short spelling of an ABI type for the report."""
    k = t[0]
    if k == "i":
        return {1: "8", 2: "16", 4: "32", 8: "64"}.get(t[1], str(t[1])).join(
            ["s" if t[2] else "u" if t[2] is not None else "e", ""])
    if k == "f":
        return {4: "f32", 8: "f64"}.get(t[1], f"f{t[1] * 8}")
    if k == "p":
        return "ptr"
    if k == "a":
        return f"struct({t[1]})"
    return {"void": "void", "fn": "fn", "?": "?"}.get(k, k)


def promote(t):
    """The default argument promotions (an unprototyped call, or a K&R definition's parameters)."""
    if t[0] == "f" and t[1] == 4:
        return ("f", 8)
    if t[0] == "i" and t[1] < 4:
        return ("i", 4, True)
    return t


def compare_types(d, v):
    """Classify a definition type d against a view type v (parameters; returns use compare_returns)."""
    if d == v:
        return None
    kd, kv = d[0], v[0]
    if "any" in (kd, kv):
        return None
    if "?" in (kd, kv) or "fn" in (kd, kv):
        return "harmless" if kd == kv else "class"
    if kd == "a" or kv == "a":
        if kd == kv and d[1] == v[1] and (d[1] is None or d[1] > 16 or agg_class(d) == agg_class(v)):
            return "harmless"
        return "aggregate"
    if (kd == "f") != (kv == "f"):
        return "class"
    if kd == "f":
        return "fwidth"
    wide_d = kd == "p" or d[1] == 8
    wide_v = kv == "p" or v[1] == 8
    if wide_d != wide_v:
        return "width"
    if wide_d:
        return "harmless"
    if d[1] == v[1] and (d[1] == 4 or d[2] == v[2] or None in (d[2], v[2])):
        return "harmless"
    return "narrow"


def agg_class(t):
    """How a struct of 16 bytes or less travels: x86-64 SysV eightbyte classes (SSE if all floats, else INTEGER)
    and the AAPCS64 homogeneous float aggregate (element type, count), if it is one."""
    scalars = t[2] or ()
    eight = []
    for i in range(0, max(t[1] or 0, 1), 8):
        part = [s for o, s in scalars if i <= o < i + 8]
        eight.append("sse" if part and all(s[0] == "f" for s in part) else "int")
    kinds = {s for _, s in scalars}
    hfa = (next(iter(kinds)), len(scalars)) if len(kinds) == 1 and next(iter(kinds))[0] == "f" \
        and len(scalars) <= 4 else None
    return tuple(eight), hfa


def compare_returns(d, v):
    """(class, why) for a definition's return type d read through a view's v; None if the same."""
    if d == v:
        return None
    if v == VOID:
        return ("unused", "the view ignores the value")
    if d == VOID:
        return ("return", "the definition returns nothing")
    c = compare_types(d, v)
    if c is None or c == "harmless":
        return ("harmless", "") if c else None
    if c == "narrow":
        return ("narrow", "")
    return ("return", c)


# ---------------------------------------------------------------- DWARF

class Fn:
    """One file's prototype of an external function."""
    __slots__ = ("name", "obj", "src", "line", "kind", "proto", "varargs", "ret", "params", "text", "size")

    def __init__(self, **kw):
        for k in self.__slots__:
            setattr(self, k, kw.get(k))


def rel(p):
    try:
        return str(Path(p).resolve().relative_to(ROOT))
    except ValueError:
        return str(p)


def obj_key(readelf_name):
    """readelf names archive members `lib.a(m.o)`, nm `lib.a:m.o`."""
    m = re.match(r"^(.*\.a)\((.*)\)$", readelf_name)
    return f"{m.group(1)}:{m.group(2)}" if m else readelf_name


def read_prototypes(objects, readelf):
    """[Fn] for every external subprogram (definitions and declarations) in the objects' DWARF, and
    object -> source path."""
    fns, sources = [], {}
    for i in range(0, len(objects), 100):
        batch = objects[i:i + 100]
        out = subprocess.run([readelf, "--debug-dump=info", *batch], capture_output=True, text=True,
                             check=True).stdout
        for obj, dies in parse_readelf(out, batch[0] if len(batch) == 1 else None).items():
            obj = obj_key(obj)
            cu = next((d for d in dies.values() if d.tag == "DW_TAG_compile_unit"), None)
            src = _name(cu.attrs.get("DW_AT_name", obj)) if cu else obj
            sources[obj] = rel(src)
            if cu is None:
                continue
            specified = {id(dies[o]) for o in (_ref(d, "DW_AT_specification") for d in cu.children
                                                 if "DW_AT_specification" in d.attrs) if o in dies}
            for d in cu.children:
                if d.tag != "DW_TAG_subprogram":
                    continue
                spec = dies.get(_ref(d, "DW_AT_specification")) if "DW_AT_specification" in d.attrs else None
                src_d = spec or d
                if "DW_AT_name" not in src_d.attrs:
                    continue
                if "DW_AT_external" not in src_d.attrs and "DW_AT_external" not in d.attrs:
                    continue
                is_def = "DW_AT_low_pc" in d.attrs
                if not is_def and id(d) in specified:
                    continue   # the declaration half of a definition in this file
                name = _name(src_d.attrs["DW_AT_name"])
                proto = "DW_AT_prototyped" in d.attrs or "DW_AT_prototyped" in src_d.attrs
                pdies = [c for c in d.children if c.tag == "DW_TAG_formal_parameter"]
                if not pdies and spec is not None:
                    pdies = [c for c in spec.children if c.tag == "DW_TAG_formal_parameter"]
                varargs = any(c.tag == "DW_TAG_unspecified_parameters"
                              for c in d.children + (spec.children if spec else []))
                rt = _ref(d) if "DW_AT_type" in d.attrs else _ref(src_d) if "DW_AT_type" in src_d.attrs else None
                ret = abi_of(dies, rt)
                params = [abi_of(dies, _ref(p)) for p in pdies]
                ptext = [display(dies, _ref(p)) for p in pdies]
                if proto:
                    args = ", ".join(ptext + (["..."] if varargs else [])) or "void"
                else:
                    args = ""
                text = f"{display(dies, rt)} {name}({args})"
                line = _num(d.attrs.get("DW_AT_decl_line", src_d.attrs.get("DW_AT_decl_line", "0")))
                dfile = _num(d.attrs.get("DW_AT_decl_file", src_d.attrs.get("DW_AT_decl_file", "1")))
                fns.append(Fn(name=name, obj=obj, src=(sources[obj], dfile), line=line,
                              kind="def" if is_def else "decl", proto=proto, varargs=varargs and proto,
                              ret=ret, params=params, text=text,
                              size=_num(d.attrs.get("DW_AT_high_pc", "0")) if is_def else None))
    return fns, sources


_FILE_ROW = re.compile(r"^\s+(\d+)\s+(\d+)\s+(?:\(indirect line string, offset: 0x[0-9a-f]+\): )?(.*)$")
_DIR_ROW = re.compile(r"^\s+(\d+)\s+(?:\(indirect line string, offset: 0x[0-9a-f]+\): )?(.*)$")


def file_tables(objects, readelf):
    """object -> {file index: path} from the line table headers, for declarations in headers."""
    tables = {}
    for i in range(0, len(objects), 50):
        batch = objects[i:i + 50]
        out = subprocess.run([readelf, "--debug-dump=rawline", *batch], capture_output=True, text=True).stdout
        obj = batch[0] if len(batch) == 1 else None
        mode, dirs = None, {}
        for line in out.splitlines():
            if line.startswith("File: "):
                obj, mode, dirs = obj_key(line[6:].strip()), None, {}
                continue
            if "The Directory Table" in line:
                mode, dirs = "dir", {}
                continue
            if "The File Name Table" in line:
                mode = "file"
                continue
            if "Line Number Statements" in line or not line.strip():
                if mode == "file":
                    mode = None
                continue
            if mode == "dir":
                m = _DIR_ROW.match(line)
                if m:
                    dirs[int(m.group(1))] = m.group(2).strip()
            elif mode == "file":
                m = _FILE_ROW.match(line)
                if m and obj:
                    d = dirs.get(int(m.group(2)), "")
                    tables.setdefault(obj, {})[int(m.group(1))] = rel(os.path.join(d, m.group(3).strip()))
    return tables


# ---------------------------------------------------------------- call sites (GENERIC dump)

_NODE = re.compile(r"^@(\d+)\s+(\w+)\s*(.*)$")
_FIELD = re.compile(r"(?:^|\s)([a-z]+(?: \d+)?|\d+)\s*:\s(\S+)")
_IDENT = re.compile(r"strg:\s(.*?)\s+lngt:\s*(\d+)")
_IMPLICIT = re.compile(r"implicit declaration of function [‘'`](\w+)[’']")
# Where an expression's value is discarded: statements, the arms of an if, the left of a comma, (void) casts.
UNUSED_PARENTS = {"statement_list", "bind_expr", "loop_expr", "switch_expr", "case_label_expr", "label_expr"}


def parse_dump(text):
    """Yield (function name, {node: (kind, fields)}) for each function in a -fdump-tree-original-raw file."""
    fname, nodes, cur = None, {}, None
    for line in text.split("\n"):
        if line.startswith(";; Function "):
            if fname is not None:
                yield fname, nodes
            fname, nodes, cur = line.split()[2], {}, None
            continue
        m = _NODE.match(line)
        if m:
            cur = [m.group(2), m.group(3)]
            nodes[int(m.group(1))] = cur
        elif cur is not None and line.startswith(" "):
            cur[1] += " " + line.strip()
    if fname is not None:
        yield fname, nodes


def _fields(entry):
    kind, text = entry
    if isinstance(text, dict):
        return text
    if kind == "identifier_node":
        m = _IDENT.search(text)
        f = {"strg": m.group(1) if m else ""}
    elif kind == "string_cst":
        f = {k: v for k, v in _FIELD.findall(text.split(" strg:")[0])}
    else:
        f = {k: v for k, v in _FIELD.findall(text)}
    entry[1] = f
    return f


def tree_abi(nodes, ref):
    """ABI view of a GENERIC type node."""
    if not ref or not ref.startswith("@"):
        return ("?",)
    e = nodes.get(int(ref[1:]))
    if e is None:
        return ("?",)
    kind, f = e[0], _fields(e)
    prec = int(f.get("prec", "0") or 0)
    if kind == "integer_type":
        return ("i", prec // 8, f.get("sign") == "signed")
    if kind == "boolean_type":
        return ("i", 1, False)
    if kind == "enumeral_type":
        return ("i", prec // 8 or 4, None)
    if kind == "real_type":
        return ("f", prec // 8)
    if kind in ("pointer_type", "reference_type", "array_type"):
        return PTR
    if kind in ("record_type", "union_type", "complex_type", "vector_type"):
        size = f.get("size")
        cst = nodes.get(int(size[1:])) if size and size.startswith("@") else None
        bits = _fields(cst).get("int") if cst else None
        return ("a", int(bits) // 8 if bits else None, None)
    if kind == "void_type":
        return VOID
    return ("?",)


def statement_position(nodes, pkind, key, pf):
    """True if a child at `key` of a node of kind `pkind` (fields pf) has its value discarded."""
    if pkind in UNUSED_PARENTS:
        return key != "cond"
    void = tree_abi(nodes, pf.get("type")) == VOID
    if pkind == "cond_expr":
        return void and key in ("op 1", "op 2")
    if pkind == "compound_expr":
        return key == "op 0" or void
    if pkind in ("nop_expr", "convert_expr"):
        return void
    return False


def calls_in_dump(text, wanted):
    """{callee: [(caller, [arg ABI types], result used)]} for calls to the names in `wanted`."""
    out = collections.defaultdict(list)
    for caller, nodes in parse_dump(text):
        calls = [n for n, e in nodes.items() if e[0] == "call_expr"]
        if not calls:
            continue
        targets = {}
        for n in calls:
            f = _fields(nodes[n])
            ref = f.get("fn")
            name = None
            for _ in range(6):
                if not ref or not ref.startswith("@") or int(ref[1:]) not in nodes:
                    break
                e = nodes[int(ref[1:])]
                if e[0] == "function_decl":
                    nm = _fields(e).get("name")
                    if nm and nm.startswith("@") and int(nm[1:]) in nodes:
                        name = _fields(nodes[int(nm[1:])]).get("strg")
                    break
                if e[0] not in ("addr_expr", "nop_expr", "convert_expr", "non_lvalue_expr"):
                    break
                ref = _fields(e).get("op 0")
            if name in wanted:
                targets[n] = name
        if not targets:
            continue
        parents = collections.defaultdict(list)
        for n, e in nodes.items():
            f = _fields(e)
            for k, v in f.items():
                if k in ("type", "name", "scpe", "size", "algn", "chan", "purp", "unql", "retn", "prms", "args"):
                    continue
                if v.startswith("@") and int(v[1:]) in targets:
                    parents[int(v[1:])].append((e[0], k, f))
        for n, name in targets.items():
            f = _fields(nodes[n])
            args, srcs = [], []
            k = 0
            while str(k) in f:
                a = nodes.get(int(f[str(k)][1:]))
                args.append(arg_abi(nodes, a))
                for _ in range(8):   # what the caller had before the prototype converted it
                    if not a or a[0] not in ("nop_expr", "convert_expr", "non_lvalue_expr", "view_convert_expr"):
                        break
                    op = _fields(a).get("op 0")
                    a = nodes.get(int(op[1:])) if op and op.startswith("@") else None
                srcs.append(arg_abi(nodes, a))
                k += 1
            used = any(not statement_position(nodes, pkind, key, pf) for pkind, key, pf in parents.get(n, []))
            out[name].append((caller, args, used, srcs))
    return out


def arg_abi(nodes, a):
    """ABI type of an argument node; ('k', ...) marks a non-negative integer constant (zero-extended into its
    register)."""
    if a is None:
        return ("?",)
    t = tree_abi(nodes, _fields(a).get("type"))
    if a[0] == "integer_cst" and t[0] in ("i", "p") and int(_fields(a).get("int", "-1") or -1) >= 0:
        return ("k",) + (t if t[0] == "i" else ("i", 8, False))   # a null pointer constant is a 64-bit 0
    return t


def syntax_command(entry, dump_path):
    args = []
    skip = False
    for a in entry["args"]:
        if skip:
            skip = False
            continue
        if a in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if a in ("-MD", "-MMD", "-w"):
            continue
        args.append("-fsyntax-only" if a == "-c" else a)
    return args + ["-Wimplicit-function-declaration", f"-fdump-tree-original-raw={dump_path}"]


def collect_calls(jobs_list, entries, jobs):
    """{object: ({callee: [(caller, args, used)]}, {implicitly declared names})}."""
    tmp = Path(tempfile.mkdtemp(prefix="proto_check-"))

    def one(item):
        obj, wanted = item
        e = entries.get(obj)
        if e is None:
            return obj, None, set()
        dump = tmp / (re.sub(r"[^\w.]", "_", obj)[-120:] + ".original")
        r = subprocess.run(syntax_command(e, dump), cwd=e["dir"], capture_output=True, text=True)
        implicit = set(_IMPLICIT.findall(r.stderr))
        try:
            text = dump.read_text(errors="replace")
            dump.unlink()
        except OSError:
            return obj, None, implicit
        return obj, calls_in_dump(text, wanted), implicit

    try:
        with ThreadPoolExecutor(jobs) as pool:
            return {obj: (calls, implicit) for obj, calls, implicit in pool.map(one, jobs_list)}
    finally:
        for p in tmp.glob("*"):
            p.unlink()
        tmp.rmdir()


def load_entries(build):
    entries = {}
    path = build / "compile_commands.json"
    if not path.exists():
        return entries
    for e in json.loads(path.read_text()):
        args = e["arguments"] if "arguments" in e else shlex.split(e["command"])
        out = e.get("output") or (args[args.index("-o") + 1] if "-o" in args else None)
        if out:
            entries[str((Path(e["directory"]) / out).resolve())] = {"args": args, "dir": e["directory"]}
    return entries


# ---------------------------------------------------------------- call graph (for ordering)

_SECHDR = re.compile(r"^\s*\[\s*(\d+)\]\s+(\S+)")
_SYMROW = re.compile(r"^\s*\d+:\s+([0-9a-f]+)\s+(\d+)\s+(\w+)\s+(\w+)\s+\w+\s+(\w+)\s+(\S+)")
_RELROW = re.compile(r"^([0-9a-f]+)\s+[0-9a-f]+\s+(\S+)\s+[0-9a-f]+\s+(\S+)(?:\s*\+\s*([0-9a-f]+))?")


def call_graph(objects, readelf):
    """The references between functions, from each object's relocations. Returns (graph, refs, live):
    graph {caller node: {callee node}} over the code that runs; refs {object: {callee name: {caller node}}}
    for the live callers in each object; live {object: {node}}. A node is a global name, or (object, name) for
    a static function. Weak copies are not live: every reference binds to the owner, so their code never runs.
    A static function is live when live code or the object's data refers to it."""
    graph = collections.defaultdict(set)
    refs, live = {}, {}
    for i in range(0, len(objects), 100):
        batch = objects[i:i + 100]
        out = subprocess.run([readelf, "-W", "-S", "-s", "-r", *batch], capture_output=True, text=True).stdout
        for obj, text in _split_files(out, batch):
            sections, funcs, syms_by_name, weak = {}, collections.defaultdict(list), {}, set()
            relocs, cur = [], None
            for line in text.splitlines():
                m = _SECHDR.match(line)
                if m and not line.lstrip().startswith("Num"):
                    sections[int(m.group(1))] = m.group(2)
                    continue
                m = _SYMROW.match(line)
                if m:
                    val, size, typ, bind, ndx, name = m.groups()
                    if typ == "FUNC" and ndx.isdigit():
                        node = name if bind in ("GLOBAL", "WEAK") else (obj, name)
                        funcs[int(ndx)].append((int(val, 16), int(size), node))
                        syms_by_name[name] = node
                        if bind == "WEAK":
                            weak.add(node)
                    continue
                if line.startswith("Relocation section"):
                    sec = line.split("'")[1]
                    cur = sec[5:] if sec.startswith(".rela") else None
                    continue
                m = _RELROW.match(line)
                if m and cur:
                    relocs.append((cur, int(m.group(1), 16), m.group(3), int(m.group(4) or "0", 16)))
            text_idx = next((k for k, v in sections.items() if v == ".text"), None)
            fl = sorted(funcs.get(text_idx, []), key=lambda x: (x[0], x[1]))

            def at(off):
                return next((n for v, s, n in fl if v <= off < v + max(s, 1)), None)
            edges, data_refs = collections.defaultdict(set), set()
            for sec, off, target, addend in relocs:
                dst = at(addend) if target == ".text" else syms_by_name.get(target, target)
                if dst is None:
                    continue
                if sec != ".text":
                    data_refs.add(dst)
                    continue
                src = at(off)
                if src is not None:
                    edges[src].add(dst)
            nodes = {n for _, _, n in fl}
            alive = {n for n in nodes if isinstance(n, str) and n not in weak}
            alive |= {n for n in data_refs if n in nodes and not isinstance(n, str)}
            todo = list(alive)
            while todo:
                for m in edges.get(todo.pop(), ()):
                    if m in nodes and not isinstance(m, str) and m not in alive:
                        alive.add(m)
                        todo.append(m)
            live[obj] = alive
            orefs = refs.setdefault(obj, collections.defaultdict(set))
            for src, dsts in edges.items():
                if src not in alive:
                    continue
                for dst in dsts:
                    graph[src].add(dst)
                    orefs[dst if isinstance(dst, str) else dst[1]].add(src)
    return graph, refs, live


def _split_files(out, batch):
    if len(batch) == 1:
        yield batch[0], out
        return
    cur, buf = None, []
    for line in out.splitlines():
        if line.startswith("File: "):
            if cur is not None:
                yield cur, "\n".join(buf)
            cur, buf = obj_key(line[6:].strip()), []
        else:
            buf.append(line)
    if cur is not None:
        yield cur, "\n".join(buf)


def distances(graph, roots):
    """BFS depth of every node from the roots (global names)."""
    dist = {r: 0 for r in roots}
    frontier = list(roots)
    while frontier:
        nxt = []
        for n in frontier:
            for m in graph.get(n, ()):
                if m not in dist:
                    dist[m] = dist[n] + 1
                    nxt.append(m)
        frontier = nxt
    return dist


# ---------------------------------------------------------------- comparison

def compare_view(defn, view, calls):
    """[(class, detail)] for one view against the definition. `calls` are its call sites (or None)."""
    issues = []
    dparams = [promote(p) for p in defn.params] if not defn.proto else defn.params
    if view.proto:
        for i, (dp, vp) in enumerate(zip(dparams, view.params)):
            c = compare_types(dp, vp)
            if c:
                srcs_i = [srcs[i] for _, _, _, srcs in calls or () if i < len(srcs)]
                passed = sorted({short(s[1:]) + " const" if s[0] == "k" else short(s) for s in srcs_i})
                gpr = sum(1 for p in dparams[:i] if p[0] != "f")
                if c == "width" and srcs_i and all(s[0] == "k" for s in srcs_i) and (gpr < 6 or size_of(vp) == 8):
                    c = "harmless"   # every caller passes a small constant: its register is zero-extended
                issues.append((c, f"arg {i + 1}: {short(dp)} vs {short(vp)}"
                                  + (f" (callers pass {', '.join(passed)})" if passed and c in HARMFUL + ("harmless",)
                                     else "")))
        nd, nv = len(dparams), len(view.params)
        if nv < nd and not view.varargs:
            issues.append(("count", f"{nv} args for {nd}"))
        elif nv > nd and not defn.varargs:
            issues.append(("extra", f"{nv} args for {nd}"))
        if calls is not None and not calls:   # only its address is taken: the prototype shapes no call
            issues = [("harmless" if c in HARMFUL else c, w + " (no direct calls)") for c, w in issues]
        if view.varargs and nv < nd and calls is not None:
            for caller, args, _, _ in calls:
                for i in range(nv, min(nd, len(args))):
                    c = compare_types(dparams[i], args[i])
                    if c:
                        issues.append((c, f"arg {i + 1} (variadic, in {caller}): {short(dparams[i])} vs "
                                          f"{short(args[i])}"))
    else:
        if calls is None:
            issues.append(("harmless" if not dparams else "narrow", "unprototyped; no call sites read"))
        for caller, args, used, _ in calls or ():
            gpr = 0
            for i, (dp, a) in enumerate(zip(dparams, args)):
                const = a[0] == "k"
                a = a[1:] if const else a
                c = compare_types(dp, a)
                if c == "width" and const and (gpr < 6 or size_of(a) == 8):
                    c = "harmless"   # 0 or a small constant for a pointer: the register is zero-extended
                if c:
                    issues.append((c, f"arg {i + 1} in {caller}: {short(dp)} vs {short(a)}"
                                      + (" (constant)" if const else "")))
                gpr += a[0] != "f"
            if len(args) < len(dparams) and not defn.varargs:
                issues.append(("count", f"{len(args)} args for {len(dparams)} in {caller}"))
            elif len(args) > len(dparams) and not defn.varargs and defn.proto:
                issues.append(("extra", f"{len(args)} args for {len(dparams)} in {caller}"))
    r = compare_returns(defn.ret, view.ret)
    if r:
        c, why = r
        if c == "return" and calls is not None and not any(u for _, _, u, _ in calls):
            c = "unused"
        issues.append((c, f"returns {short(defn.ret)}, read as {short(view.ret)}" + (f" ({why})" if why else "")))
    return issues


def worst(issues):
    return min((c for c, _ in issues), key=RANK.get) if issues else None


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--build", type=Path, default=ROOT / "build" / "pc")
    ap.add_argument("--readelf", default="readelf")
    ap.add_argument("--nm", default="nm")
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--no-calls", action="store_true", help="skip the call-site pass (unprototyped views unchecked)")
    ap.add_argument("--roots", default=DEFAULT_ROOTS, help="comma-separated boot roots for the ordering")
    ap.add_argument("--out", type=Path, help="report (default <build>/proto_report.txt)")
    ap.add_argument("--doc", type=Path, help="update the summary table between the proto_check markers")
    ap.add_argument("-q", "--quiet", action="store_true")
    a = ap.parse_args()
    build = a.build.resolve()
    out_txt = a.out or build / "proto_report.txt"

    def lines(p):
        return [str(Path(x).resolve()) for x in (build / p).read_text().split() if x] if (build / p).exists() else []
    game_objs = lines("game_objects.txt")
    port_objs = [p for p in lines("port_objects.txt") if Path(p).exists()]
    if not game_objs:
        sys.exit(f"proto_check: {build}/game_objects.txt not found; build first")
    all_objs = game_objs + port_objs
    game_set = set(game_objs)

    fns, sources = read_prototypes(all_objs, a.readelf)
    defs, _ = n64syms.nm_table(all_objs, a.nm)
    strong = {(o, n) for n, lst in defs.items() for o, t in lst if t == "T"}
    order = {o: i for i, o in enumerate(all_objs)}

    # Declarations in headers (src/rom): their file names.
    need_tables = sorted({f.obj for f in fns if f.src[1] not in (0, 1)})
    tables = file_tables(need_tables, a.readelf) if need_tables else {}
    for f in fns:
        src, idx = f.src
        f.src = tables.get(f.obj, {}).get(idx, src) if idx not in (0, 1) else src

    # Aliases the link resolves to another name (data.c thunks, aliases.ld).
    alias = {}
    datac = build / "pc-data" / "data.c"
    if datac.exists():
        alias.update(re.findall(r'PC_THUNK\("(\w+)", "(\w+)"\)', datac.read_text()))
    aliases_ld = build / "pc-data" / "aliases.ld"
    if aliases_ld.exists():
        alias.update(re.findall(r"PROVIDE\(\s*(\w+)\s*=\s*(\w+)\s*\)", aliases_ld.read_text()))

    by_name = collections.defaultdict(list)
    for f in fns:
        by_name[f.name].append(f)
    owners = {}
    for name, lst in by_name.items():
        ds = [f for f in lst if f.kind == "def"]
        if not ds:
            continue
        st = [f for f in ds if (f.obj, name) in strong]
        pick = sorted(st or ds, key=lambda f: order.get(f.obj, 1 << 30))
        owners[name] = pick[0]
    game_defined = set(owners)
    for name, (ret, params, va) in HOST_PROTOS.items():
        if name not in game_defined:
            owners[name] = Fn(name=name, obj="host", src="host C library", line=0, kind="host", proto=True,
                              varargs=va, ret=ret, params=params, text=HOST_DISPLAY[name])

    # Views: every other prototype of a defined name, in a game object.
    views = []
    for name, lst in by_name.items():
        owner = owners.get(alias.get(name, name))
        if owner is None:
            continue
        for f in lst:
            if f is owner or f.obj not in game_set:
                continue
            f.kind = "copy" if f.kind == "def" else f.kind
            views.append((owner, f))

    # Which code runs (weak copies don't), who refers to what, and the order.
    graph, refs, live = call_graph(game_objs, a.readelf)
    roots = [r for r in a.roots.split(",") if r]
    dist = distances(graph, roots)
    live_names = {o: {n if isinstance(n, str) else n[1] for n in s} for o, s in live.items()}

    def callers(v):
        return refs.get(v.obj, {}).get(v.name, set())

    # Call sites: unprototyped and variadic views, and views whose return type differs, where live code calls.
    entries = load_entries(build)
    need = collections.defaultdict(set)
    for owner, v in views:
        if callers(v) and (not v.proto or (v.varargs and len(v.params) < len(owner.params))
                           or compare_returns(owner.ret, v.ret) or worst(compare_view(owner, v, None)) in HARMFUL):
            need[v.obj].add(v.name)
    calls = {}
    if not a.no_calls and need:
        calls = collect_calls(sorted(need.items()), entries, a.jobs)

    # Empty owners (deleted-procedure stubs: the retail symbol is `jr ra`, the body was inlined into its group)
    # whose name also has a weak copy with a body: that group's calls bind to the stub on the PC.
    stub_bodies = {}
    for name, owner in owners.items():
        if owner.kind == "def" and owner.size is not None and owner.size <= EMPTY_FN:
            bodies = {f.src for f in by_name[name] if f is not owner and f.kind in ("def", "copy")
                      and (f.size or 0) > EMPTY_FN}
            if bodies:
                stub_bodies[name] = bodies

    results = []
    for owner, v in views:
        site = calls.get(v.obj)
        vcalls = site[0].get(v.name, []) if site and site[0] is not None else None
        if vcalls is not None:
            vcalls = [c for c in vcalls if c[0] in live_names.get(v.obj, ())]
        implicit = bool(site and v.name in site[1])
        issues = compare_view(owner, v, vcalls)
        bodies = stub_bodies.get(owner.name, ())
        if bodies and v.src not in bodies and os.path.dirname(v.src) in {os.path.dirname(b) for b in bodies} \
                or bodies and v.src in bodies and callers(v):
            issues.append(("stub", f"the owner is an empty deleted-procedure stub; the body is in "
                                   f"{', '.join(sorted(bodies))}"))
        cls = worst(issues)
        if cls is None:
            continue
        note = ""
        reviewed = REVIEWED.get(f"{owner.name}@{v.src}", REVIEWED.get(owner.name))
        if not callers(v):
            cls, note = "dead", "only weak copies (code the link never runs) refer to it here"
        elif reviewed and cls in HARMFUL:
            cls, note = "reviewed", reviewed
        results.append({"name": v.name, "class": cls, "issues": issues, "owner": owner, "view": v, "note": note,
                        "implicit": implicit, "calls": len(vcalls) if vcalls is not None else None,
                        "dist": dist.get(owner.name, dist.get(v.name))})

    # Per function: the worst view.
    per_fn = {}
    for r in results:
        p = per_fn.get(r["owner"].name)
        if p is None or RANK[r["class"]] < RANK[p]:
            per_fn[r["owner"].name] = r["class"]
    fn_counts = collections.Counter(per_fn.values())
    view_counts = collections.Counter(r["class"] for r in results)
    n_harm_views = sum(view_counts[c] for c in HARMFUL)
    n_harm_fns = sum(fn_counts[c] for c in HARMFUL)
    checked = len({o.name for o, _ in views})

    # Report.
    def loc(f):
        return f"{f.src}:{f.line}" if f.line else f.src

    def sort_key(r):
        d = r["dist"]
        return (d is None, d if d is not None else 0, RANK[r["class"]], r["owner"].name, loc(r["view"]))

    L = [f"proto_check: {len(owners)} defined functions, {checked} with views in other files, "
         f"{len(views)} views ({sum(1 for _, v in views if not v.proto)} unprototyped).",
         f"Harmful: {n_harm_views} views of {n_harm_fns} functions. Roots for the order: {', '.join(roots)}.", "",
         f"{'class':10} {'functions':>9} {'views':>6}"]
    for c in CLASSES:
        L.append(f"{c:10} {fn_counts[c]:9} {view_counts[c]:6}" + ("   (benign)" if c in BENIGN else ""))
    L.append("")
    for title, keep in (("Harmful", HARMFUL), ("Benign", BENIGN)):
        L += [f"== {title} (by distance from the roots, then class) ==", ""]
        cur = None
        for r in sorted((r for r in results if r["class"] in keep), key=sort_key):
            o, v = r["owner"], r["view"]
            if o.name != cur:
                cur = o.name
                d = r["dist"]
                L.append(f"{o.name}  [{'depth ' + str(d) if d is not None else 'not reached'}]  definition "
                         f"{loc(o)}: {o.text}")
            tag = " implicit" if r["implicit"] else ""
            ncalls = f", {r['calls']} calls" if r["calls"] is not None and not v.proto else ""
            L.append(f"  {r['class']:9} {v.kind}{tag} {loc(v)}{ncalls}: {v.text}")
            if r["note"]:
                L.append(f"      ({r['note']})")
            seen = set()
            for c, why in sorted(r["issues"], key=lambda x: RANK[x[0]]):
                if (c, why) in seen or (title == "Harmful" and c not in HARMFUL and c != "narrow"):
                    continue
                seen.add((c, why))
                L.append(f"      {c}: {why}")
        L.append("")
    out_txt.parent.mkdir(parents=True, exist_ok=True)
    out_txt.write_text("\n".join(L) + "\n")
    js = {"functions": len(owners), "checked": checked, "views": len(views),
          "by_class": {c: {"functions": fn_counts[c], "views": view_counts[c]} for c in CLASSES},
          "results": [{"name": r["name"], "owner": r["owner"].name, "class": r["class"],
                       "definition": {"file": r["owner"].src, "line": r["owner"].line, "text": r["owner"].text},
                       "view": {"file": r["view"].src, "line": r["view"].line, "text": r["view"].text,
                                "kind": r["view"].kind, "prototyped": r["view"].proto},
                       "implicit": r["implicit"], "issues": r["issues"], "depth": r["dist"], "note": r["note"],
                       "callers": sorted(n if isinstance(n, str) else n[1] for n in callers(r["view"]))}
                      for r in sorted(results, key=sort_key)]}
    out_txt.with_suffix(".json").write_text(json.dumps(js, indent=1))

    if a.doc:
        table = ["| Class | Functions | Views | |", "|---|---:|---:|---|"]
        for c in CLASSES:
            table.append(f"| `{c}` | {fn_counts[c]} | {view_counts[c]} | {'benign' if c in BENIGN else 'harmful'} |")
        table.append(f"| **harmful** | **{n_harm_fns}** | **{n_harm_views}** | |")
        doc = a.doc.read_text() if a.doc.exists() else ""
        begin, end = "<!-- proto_check:begin -->", "<!-- proto_check:end -->"
        block = begin + "\n" + "\n".join(table) + "\n" + end
        if begin in doc and end in doc:
            doc = doc[:doc.index(begin)] + block + doc[doc.index(end) + len(end):]
        else:
            doc += ("\n" if doc else "") + block + "\n"
        a.doc.write_text(doc)
    if not a.quiet:
        print(f"proto_check: {len(views)} views of {checked} functions; harmful {n_harm_views} views of "
              f"{n_harm_fns} functions -> {rel(out_txt)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
