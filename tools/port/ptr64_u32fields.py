#!/usr/bin/env python3
"""Keep the N64 layout of structs whose pointer fields hold 32-bit N64 addresses (docs/port/POINTERS.md).

    python3 tools/port/ptr64_u32fields.py --types Block,Heap,HandleTable FILE...

For records that live in the 32-bit N64 world (the game heap's block and heap headers in the RDRAM window,
records loaded from the ROM), a native 8-byte pointer field would move every field after it and break the
offsets other code uses. On PC this keeps such a field 4 bytes, as an N64 address:

1. Each `typedef struct ... } T;` named in --types gets an #ifdef TARGET_PC copy in which every pointer field
   (`struct X *p`, `X *p`, `void *p`, `u32 *p`, arrays of them) is `enum n64p_X p` instead. An enum is a 4-byte
   unsigned integer type that `_Generic` can tell apart, so one macro knows what each field points to:
       N64P(x)   PC: the field as a host pointer, `(X *)(uintptr_t)(x)` (the RDRAM window is identity-mapped);
       N64W(x)   PC: a pointer stored into such a field, `(u32)(uintptr_t)(x)`.
   On the N64 both are `x`, so the token stream IDO sees is unchanged.
2. The file is compiled with the PC build's flags (build/pc/compile_commands.json) and each error the change
   causes is fixed in place: `->`, unary `*` or `[]` applied to such a field wraps the operand in N64P();
   a pointer assigned to one wraps the value in N64W(); a comparison with a pointer wraps the field in N64P().
   Repeats until the file compiles cleanly.
Pointer arithmetic on such a field (`p->next + 1`) would silently become integer arithmetic: the tool lists
any `->field +/-/++/--` it finds for review. Stdlib only, Python 3.9+.
"""
import argparse
import json
import re
import shlex
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ptr64_edit  # noqa: E402

ROOT = ptr64_edit.ROOT
_ID = r"[A-Za-z_]\w*"


def split_top(s, sep):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch in "({[":
            depth += 1
        elif ch in ")}]":
            depth -= 1
        if ch == sep and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    out.append(cur)
    return out


def find_typedefs(text, names):
    """(start, end, tag, name) of `typedef struct [tag] { ... } name;` for each name."""
    found = []
    for m in re.finditer(r"typedef\s+struct\s*(" + _ID + r")?\s*\{", text):
        i = m.end()
        depth = 1
        while depth and i < len(text):
            depth += {"{": 1, "}": -1}.get(text[i], 0)
            i += 1
        tail = re.match(r"\s*(" + _ID + r")\s*;[^\n]*\n?", text[i:])
        if tail and tail.group(1) in names:
            found.append((m.start(), i + tail.end(), m.group(1), tail.group(1), m.end(), i - 1))
    return found


def convert_body(body, pointees):
    """Pointer fields -> enum n64p_X. Returns the new body; adds pointee kinds to `pointees`."""
    out = []
    for decl in split_top(body, ";"):
        if not decl.strip():
            out.append(decl)
            continue
        cm = re.match(r"^(\s*(?:/\*.*?\*/\s*)*)", decl, re.S)   # leading space and comments
        lead = cm.group(1)
        d = decl[len(lead):].strip()
        m = re.match(r"((?:(?:const|volatile|struct|unsigned|signed)\s+)*" + _ID + r")(\s*)(.*)$", d, re.S)
        if not m or d.startswith(("struct {", "union", "struct\n")) or "{" in d:
            out.append(decl)
            continue
        base, rest = m.group(1), m.group(3)
        if m.group(2) == "" and not rest.startswith("*"):
            out.append(decl)
            continue
        decls = [x.strip() for x in split_top(rest, ",")]
        if not any(x.startswith("*") or x.startswith("(*") for x in decls):
            out.append(decl)
            continue
        kind = re.sub(r"\b(const|volatile|struct)\s+", "", base).strip()
        new = []
        plain = []
        for x in decls:
            fm = re.match(r"^\(\s*\*\s*(" + _ID + r")\s*\)\s*\(.*\)$", x, re.S)
            pm = re.match(r"^(\*+)\s*(" + _ID + r")\s*((?:\[[^\]]*\]\s*)*)$", x)
            if fm:
                pointees.add("fn")
                new.append(f"enum n64p_fn {fm.group(1)}")
            elif pm:
                k = kind if len(pm.group(1)) == 1 else "ptr"
                k = re.sub(r"\W", "_", k)
                pointees.add(k)
                new.append(f"enum n64p_{k} {pm.group(2)}{pm.group(3).strip()}")
            else:
                plain.append(x)
        if plain:
            new.insert(0, f"{base} {', '.join(plain)}")
        out.append(lead + "; ".join(new))
    return ";".join(out)


def pc_block(text, typedefs, tags):
    """The PC copies of the typedefs, the enums and the N64P/N64W macros."""
    pointees = set()
    copies = []
    for (s, e, tag, name, bs, be) in typedefs:
        body = text[bs:be]
        copies.append((s, e, text[s:bs] + convert_body(body, pointees) + text[be:e]))
    enums = "".join(f"enum n64p_{k} {{ n64p_{k}_ }};\n" for k in sorted(pointees))
    branches = []
    for k in sorted(pointees):
        if k == "void":
            tgt = "void *"
        elif k in ("fn", "ptr"):
            tgt = "void *" if k == "fn" else "void **"
        else:
            tgt = (f"struct {tags[k]} *" if tags.get(k) else f"{k} *")
        branches.append(f"enum n64p_{k}: ({tgt})(uintptr_t)(x)")
    macros = ("#define N64P(x) _Generic((x), " + ", ".join(branches) + ", default: (x))\n"
              "#define N64W(x) ((u32)(uintptr_t)(x))\n")
    return copies, enums, macros


_ERR = re.compile(r"^(\S+?):(\d+):(\d+): (error|warning): (.*)$")


def compile_errors(path, cc):
    e = cc[str(path)]
    r = subprocess.run(e["args"], cwd=e["dir"], capture_output=True, text=True)
    errs = []
    for l in r.stderr.splitlines():
        m = _ERR.match(l)
        if m and Path(e["dir"], m.group(1)).resolve() == path.resolve():
            errs.append((int(m.group(2)), int(m.group(3)), m.group(4), m.group(5)))
    return errs


def _match_back(line, i):
    """Index of the bracket that opens the one closing at line[i]."""
    close = line[i]
    open_ = "(" if close == ")" else "["
    depth = 0
    while i >= 0:
        if line[i] == close:
            depth += 1
        elif line[i] == open_:
            depth -= 1
            if depth == 0:
                return i
        i -= 1
    return 0


def left_operand(line, pos):
    """Start of the postfix expression (a, a->b, a.b, a[i], f(x), (e)) that ends just before `pos`."""
    i = pos
    while i > 0 and line[i - 1] == " ":
        i -= 1
    while True:
        if i > 0 and line[i - 1] == "]":
            i = _match_back(line, i - 1)
            continue                        # a subscript: its base is to the left
        if i > 0 and line[i - 1] == ")":
            i = _match_back(line, i - 1)
            k = i
            while k > 0 and re.match(r"\w", line[k - 1]):
                k -= 1
            if k == i:
                return i                    # a parenthesized primary expression
            i = k                           # a call: the callee name
        else:
            k = i
            while k > 0 and re.match(r"\w", line[k - 1]):
                k -= 1
            if k == i:
                return i
            i = k
        if i >= 2 and line[i - 2:i] == "->":
            i -= 2
            continue
        if i >= 1 and line[i - 1] == ".":
            i -= 1
            continue
        return i


def right_postfix_end(line, start):
    """End of the postfix expression that starts at `start` (identifier, then ->x, .x, [..], (..))."""
    i = start
    while i < len(line) and line[i] == " ":
        i += 1
    if i < len(line) and line[i] == "(":
        depth = 0
        while i < len(line):
            depth += {"(": 1, ")": -1}.get(line[i], 0)
            i += 1
            if depth == 0:
                break
    else:
        while i < len(line) and re.match(r"\w", line[i]):
            i += 1
    while i < len(line):
        if line.startswith("->", i):
            i += 2
            while i < len(line) and re.match(r"\w", line[i]):
                i += 1
        elif line[i] == "." and i + 1 < len(line) and re.match(r"[A-Za-z_]", line[i + 1]):
            i += 1
            while i < len(line) and re.match(r"\w", line[i]):
                i += 1
        elif line[i] in "[(":
            open_, close = line[i], "]" if line[i] == "[" else ")"
            depth = 0
            while i < len(line):
                if line[i] == open_:
                    depth += 1
                elif line[i] == close:
                    depth -= 1
                    if depth == 0:
                        i += 1
                        break
                i += 1
        else:
            break
    return i


def right_operand_end(line, start):
    """End of the expression starting at `start`: up to `;`, `,` or an unmatched `)` at depth 0."""
    depth = 0
    i = start
    while i < len(line):
        ch = line[i]
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            if depth == 0:
                return i
            depth -= 1
        elif ch in ";," and depth == 0:
            return i
        i += 1
    return i


def fix_once(path, cc):
    errs = compile_errors(path, cc)
    todo = []
    for ln, col, sev, msg in errs:
        if sev == "error" and ("invalid type argument of" in msg and "n64p" in msg or
                               "invalid type argument of ‘->’" in msg or "invalid type argument of '->'" in msg or
                               "invalid type argument of unary" in msg or "subscripted value is neither" in msg):
            todo.append(("P", ln, col))
        elif sev == "error" and re.search(r"incompatible types when (assigning to|initializing) type ‘enum n64p_", msg):
            todo.append(("W", ln, col))
        elif sev == "error" and re.search(r"incompatible types? (when (assigning to|initializing|returning)|for argument).*", msg):
            todo.append(("R", ln, col))
        elif sev == "warning" and "comparison between pointer and integer" in msg:
            todo.append(("C", ln, col))
        elif sev == "error":
            todo.append(("?", ln, col, msg))
    if not todo:
        return 0, []
    lines = path.read_text().split("\n")
    done = 0
    unknown = []
    # right to left within a line so columns stay valid
    for item in sorted(todo, key=lambda t: (t[1], -t[2])):
        kind, ln, col = item[0], item[1], item[2]
        line = lines[ln - 1]
        c = col - 1
        if kind == "P":
            if line[c:c + 2] == "->" or line[c:c + 1] == "[":
                s = left_operand(line, c)
                e = c
            elif line[c:c + 1] == "*":
                s = c + 1
                e = right_postfix_end(line, s)
            else:   # the error points at the expression: up to its first `[` or `->`
                e = right_postfix_end(line, c)
                sub = line[c:e]
                m = re.search(r"\[|->(?=\w+\s*$)", sub)
                s, e = c, (c + m.start() if m else e)
            if s >= e:
                unknown.append(f"{ln}:{col} {line.strip()[:100]}")
                continue
            lines[ln - 1] = line[:s] + "N64P(" + line[s:e].rstrip() + ")" + line[e:]
            done += 1
        elif kind in ("W", "R"):   # a pointer into an N64-address field / a field read as a pointer
            s = c
            e = right_operand_end(line, s)
            wrap = "N64W(" if kind == "W" else "N64P("
            if line[s:e].startswith(wrap):
                unknown.append(f"{ln}:{col} {line.strip()[:100]}")
                continue
            lines[ln - 1] = line[:s] + wrap + line[s:e].rstrip() + ")" + line[e:]
            done += 1
        elif kind == "C":
            # wrap the side of the comparison that is a field access
            ls = left_operand(line, c)
            left = line[ls:c]
            op = re.match(r"\s*(==|!=|<=|>=|<|>)\s*", line[c:])
            rs = c + op.end() if op else c + 2
            re_ = right_operand_end(line, rs)
            right = line[rs:re_]
            m = re.search(r"&&|\|\|", right)
            if m:
                re_ = rs + m.start()
                right = line[rs:re_]
            if "->" in left or "." in left:
                lines[ln - 1] = line[:ls] + "N64P(" + left.rstrip() + ")" + line[ls + len(left.rstrip()):]
            elif "->" in right or "." in right:
                r = right.rstrip()
                lines[ln - 1] = line[:rs] + "N64P(" + r + ")" + line[rs + len(r):]
            else:
                unknown.append(f"{ln}:{col} {line.strip()[:100]}")
                continue
            done += 1
        else:
            unknown.append(f"{ln}:{col} {item[3][:80]} | {line.strip()[:80]}")
    path.write_text("\n".join(lines))
    return done, unknown


def load_cc():
    cc = {}
    for e in json.loads((ROOT / "build/pc/compile_commands.json").read_text()):
        args = e["arguments"] if "arguments" in e else shlex.split(e["command"])
        out, skip = [], False
        for x in args:
            if skip:
                skip = False
                continue
            if x in ("-o", "-MF", "-MT", "-MQ"):
                skip = True
                continue
            if x not in ("-MD", "-MMD"):
                out.append("-fsyntax-only" if x == "-c" else x)
        cc[str(Path(e["file"]).resolve())] = {"args": out, "dir": e["directory"]}
    return cc


def convert_file(path, types, cc):
    text = path.read_text()
    if "N64P(" in text:
        print(f"{ptr64_edit.rel(path)}: already converted")
        return
    tds = find_typedefs(text, types)
    if not tds:
        print(f"{ptr64_edit.rel(path)}: none of {sorted(types)} defined here")
        return
    tags = {}
    for (s, e, tag, name, bs, be) in tds:
        if tag:
            tags[name] = tag
            tags[tag] = tag
    copies, enums, macros = pc_block(text, tds, tags)
    # replace from the end so offsets stay valid; the enums and macros go after the last typedef
    last_end = max(e for _, e, _ in copies)
    new = text
    for s, e, pc in sorted(copies, key=lambda c: -c[0]):
        orig = text[s:e]
        block = "#ifdef TARGET_PC\n" + pc + ("" if pc.endswith("\n") else "\n") + "#else\n" + orig + \
                ("" if orig.endswith("\n") else "\n") + "#endif\n"
        if e == last_end:
            block += ("#ifdef TARGET_PC\n/* N64-address fields (tools/port/ptr64_u32fields.py) */\n" + macros +
                      "#else\n#define N64P(x) x\n#define N64W(x) x\n#endif\n")
        new = new[:s] + block + new[e:]
    first_start = min(s for s, _, _ in copies)
    new = new[:first_start] + "#ifdef TARGET_PC\n" + enums + "#endif\n" + new[first_start:]
    path.write_text(ptr64_edit.ensure_ptr_typedefs(new))
    total = 0
    for _ in range(12):
        n, unknown = fix_once(path, cc)
        total += n
        if unknown:
            for u in unknown:
                print(f"{ptr64_edit.rel(path)}: needs a hand edit: {u}")
            break
        if n == 0:
            break
    arith = []
    fields = set(re.findall(r"enum n64p_\w+ (\w+)", path.read_text()))
    for i, l in enumerate(path.read_text().split("\n"), 1):
        for f in fields:
            if re.search(r"(->|\.)" + f + r"\s*(\+\+|--|[+-](?!>)|\[)", l):
                arith.append(f"{i}: {l.strip()[:100]}")
    print(f"{ptr64_edit.rel(path)}: {total} accesses wrapped" +
          ("" if not arith else "; review pointer arithmetic: " + " | ".join(arith[:6])))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--types", required=True)
    ap.add_argument("files", nargs="+", type=Path)
    a = ap.parse_args()
    cc = load_cc()
    for f in a.files:
        convert_file(f.resolve(), set(a.types.split(",")), cc)


if __name__ == "__main__":
    main()
