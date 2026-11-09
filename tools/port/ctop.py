#!/usr/bin/env python3
"""Split a C file into its top-level items: what headers.py and to_headers.py read the game's files with.

    import ctop
    for it in ctop.split(text):
        it.kind, it.start, it.end, it.text(), it.names

An item is one of:

    pp        a preprocessor line (with its continuations)
    comment   a comment or run of comments on lines of their own, not followed directly by an item
    decl      a declaration ending in `;` at file scope: typedefs, struct/union/enum definitions, data and function
              declarations, data definitions (with or without an initializer)
    func      a function definition (its header and its body)
    other     anything else (a stray `;`)

Each item's span starts at its first token and ends after its last (`;`, `}` or the end of the directive); the
comments directly above an item (no blank line between) are its `lead`, part of its span when it is removed with
`span(with_lead=True)`. Strings, character constants and comments are skipped when braces and parentheses are
counted. Stdlib only.

The parts of a declaration are found by `parse_decl`: its storage class (extern, static, typedef or none), the
type's specifier tokens, and each declarator's name and whether it declares a function (a parameter list next to the
name), with or without a prototype. That is enough for the header tools: they compare declarations as normalized
text, and leave the C compiler to judge what the text means.
"""
import re

TOKEN = re.compile(r"""
   (?P<nl>\n)
  |(?P<ws>[ \t\r\f\v]+|\\\n)
  |(?P<lc>//[^\n]*)
  |(?P<bc>/\*.*?\*/)
  |(?P<str>L?"(?:\\.|[^"\\\n])*")
  |(?P<chr>L?'(?:\\.|[^'\\\n])*')
  |(?P<id>[A-Za-z_]\w*)
  |(?P<num>\.?\d(?:[eEpP][+-]|[\w.])*)
  |(?P<punct>->|\+\+|--|<<=|>>=|<<|>>|<=|>=|==|!=|&&|\|\||\.\.\.|\#\#|[-+*/%&|^!~<>=?:;,.(){}\[\]#@$\\`])
""", re.S | re.X)

KEYWORDS = {
    "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern",
    "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short", "signed",
    "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Bool",
    "__inline", "__inline__", "__attribute__", "__extension__", "__restrict", "__restrict__", "__volatile__",
    "asm", "__asm__", "__asm", "_Complex", "__typeof__", "typeof", "__const",
}
QUALIFIERS = {"const", "volatile", "restrict", "__restrict", "__restrict__", "__const", "__volatile__"}
STORAGE = {"extern", "static", "typedef", "auto", "register"}


class Tok:
    __slots__ = ("kind", "text", "start", "end", "bol")

    def __init__(self, kind, text, start, end, bol):
        self.kind, self.text, self.start, self.end, self.bol = kind, text, start, end, bol

    def __repr__(self):
        return f"Tok({self.kind},{self.text!r})"


def tokens(src):
    """Every token but whitespace; comments are kept (kind lc/bc), newlines are not, and each token knows whether it
    is the first on its line (bol). A preprocessor directive is one token of kind pp, from its # to its end of line."""
    out = []
    pos, n, bol = 0, len(src), True
    while pos < n:
        m = TOKEN.match(src, pos)
        if not m:
            raise ValueError(f"cannot tokenize at {src[pos:pos + 40]!r} (offset {pos})")
        kind = m.lastgroup
        if kind == "nl":
            bol = True
            pos = m.end()
            continue
        if kind == "ws":
            pos = m.end()
            continue
        if kind == "punct" and m.group() == "#" and bol:
            # a directive: to the end of the line, across backslash-newlines and comments
            end = pos
            while True:
                m2 = re.compile(r"(?:\\\n|/\*.*?\*/|//[^\n]*|\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'|[^\n])*",
                                re.S).match(src, end)
                end = m2.end()
                break
            out.append(Tok("pp", src[pos:end], pos, end, True))
            pos = end
            continue
        out.append(Tok(kind, m.group(), pos, m.end(), bol))
        if kind == "bc" and "\n" in m.group():
            pass
        bol = False
        pos = m.end()
    return out


class Item:
    __slots__ = ("kind", "toks", "lead", "src", "names", "_decl")

    def __init__(self, kind, toks, src):
        self.kind, self.toks, self.lead, self.src = kind, toks, [], src
        self.names = []
        self._decl = None

    @property
    def start(self):
        return self.toks[0].start

    @property
    def end(self):
        return self.toks[-1].end

    def text(self):
        return self.src[self.start:self.end]

    def code(self):
        """The item's tokens without its comments, one space apart: a normalized form for comparing."""
        return norm(self.toks)

    def span(self, with_lead=False):
        s = self.lead[0].start if with_lead and self.lead else self.start
        return s, self.end

    def decl(self):
        if self._decl is None and self.kind == "decl":
            self._decl = parse_decl([t for t in self.toks if t.kind not in ("lc", "bc")])
        return self._decl

    def __repr__(self):
        return f"Item({self.kind}, {self.text()[:60]!r})"


def norm(toks):
    """Tokens as text, comments left out, one space between tokens."""
    return " ".join(t.text for t in toks if t.kind not in ("lc", "bc"))


def split(src):
    """The top-level items of a C file, in order (see the module's docstring)."""
    toks = tokens(src)
    items = []
    pending_comments = []   # comments since the last item
    cur = []
    depth = 0               # braces and parentheses and brackets
    body = False            # inside a function body (the item's first `{` at depth 0 after a `)`)
    i, n = 0, len(toks)

    def flush_comments(next_start=None):
        # comments directly above the next item (no blank line between) are its lead; the rest are items
        nonlocal pending_comments
        groups, g = [], []
        for c in pending_comments:
            if g and src.count("\n", g[-1].end, c.start) > 1:
                groups.append(g)
                g = []
            g.append(c)
        if g:
            groups.append(g)
        lead = []
        if groups and next_start is not None and src.count("\n", groups[-1][-1].end, next_start) <= 1:
            lead = groups.pop()
        for g in groups:
            it = Item("comment", g, src)
            items.append(it)
        pending_comments = []
        return lead

    while i < n:
        t = toks[i]
        if not cur:
            if t.kind in ("lc", "bc"):
                if items and not pending_comments and items[-1].kind != "comment" and \
                        "\n" not in src[items[-1].end:t.start]:
                    items[-1].toks.append(t)   # a comment after an item on its line is the item's
                else:
                    pending_comments.append(t)
                i += 1
                continue
            if t.kind == "pp":
                lead = flush_comments(t.start)
                it = Item("pp", [t], src)
                it.lead = lead
                items.append(it)
                i += 1
                continue
            lead = flush_comments(t.start)
            cur = [t]
            cur_lead = lead
            depth = 0
            body = False
            cond = []
            if t.text in "({[":
                depth = 1
            elif t.text == ";":
                it = Item("other", cur, src)
                it.lead = cur_lead
                items.append(it)
                cur = []
            i += 1
            continue
        if t.kind == "pp":
            # a directive inside an item (inside a function or an initializer): part of the item. Of a conditional's
            # branches only the first counts braces (`#ifdef A if (x) { #else if (y) { #endif`)
            cur.append(t)
            word = re.match(r"#\s*(\w+)", t.text)
            word = word.group(1) if word else ""
            if word in ("if", "ifdef", "ifndef"):
                cond.append(False)
            elif word in ("else", "elif") and cond:
                cond[-1] = True
            elif word == "endif" and cond:
                cond.pop()
            i += 1
            continue
        cur.append(t)
        if any(cond):
            i += 1
            continue
        x = t.text if t.kind == "punct" else None
        if x in ("(", "[", "{"):
            if x == "{" and depth == 0:
                prev = [p for p in cur[:-1] if p.kind not in ("lc", "bc", "pp")]
                if prev and (prev[-1].text == ")" or _knr_params(prev)) and not any(p.text == "=" for p in prev):
                    body = True
            depth += 1
        elif x in (")", "]", "}"):
            depth -= 1
            if x == "}" and depth == 0 and body:
                it = Item("func", cur, src)
                it.lead = cur_lead
                items.append(it)
                cur = []
        elif x == ";" and depth == 0 and not _knr_params(cur):
            it = Item("decl", cur, src)
            it.lead = cur_lead
            items.append(it)
            cur = []
        i += 1
    if cur:
        it = Item("other", cur, src)
        it.lead = cur_lead
        items.append(it)
    flush_comments()
    items.sort(key=lambda it: it.start)
    for it in items:
        if it.kind == "decl":
            d = it.decl()
            it.names = [x.name for x in d.declarators] if d else []
            if d and d.tag and not d.declarators:
                it.names = []
        elif it.kind == "func":
            it.names = [func_name(it)]
    return items


def _knr_params(cur):
    """A K&R definition's header so far: `[type] name(a, b) T a;` — its parameter declarations end in `;` before
    the body, so the item goes on to the body's `{`."""
    toks = [t for t in cur if t.kind not in ("lc", "bc", "pp")]
    for j, t in enumerate(toks):
        if t.text == "(" and j and toks[j - 1].kind == "id" and toks[j - 1].text not in KEYWORDS:
            k = _match(toks, j)
            inner = toks[j + 1:k]
            if not inner or not all(x.kind == "id" or x.text == "," for x in inner):
                return False
            rest = toks[k + 1:]
            # identifiers only, as a list: then declarations of them follow
            return bool(rest) and rest[0].kind == "id" and rest[-1].text == ";" and \
                any(x.kind == "id" and x.text in [y.text for y in inner] for x in rest)
        if t.text in ("=", "{", "["):
            return False
    return False


def _knr_header(prev):
    """A K&R definition's header: `f(a, b) int a; char *b;` (a `)` then declarations ending in `;`)."""
    if prev[-1].text != ";":
        return False
    depth = 0
    for j, p in enumerate(prev):
        if p.text == "(":
            depth += 1
        elif p.text == ")":
            depth -= 1
            if depth == 0:
                # the first top-level parenthesis closes, then only declarations follow
                rest = prev[j + 1:]
                return bool(rest) and rest[0].kind == "id" and all(r.text not in ("{", "=", "(") for r in rest)
    return False


def func_name(item):
    """A function definition's name: the identifier before the first top-level `(` that isn't inside the type."""
    toks = [t for t in item.toks if t.kind not in ("lc", "bc", "pp")]
    depth = 0
    for j, t in enumerate(toks):
        if t.text == "(":
            # skip __attribute__((...)) and a declarator in parentheses (`void (*f(void))(int)`)
            if j and toks[j - 1].kind == "id" and toks[j - 1].text not in KEYWORDS:
                return toks[j - 1].text
        if t.text == "{":
            break
    return None


class Declarator:
    __slots__ = ("name", "toks", "func", "proto", "init", "array", "pointer")

    def __init__(self):
        self.name, self.toks, self.func, self.proto, self.init, self.array, self.pointer = \
            None, [], False, False, None, False, False

    def __repr__(self):
        return f"Declarator({self.name}, func={self.func}, proto={self.proto})"


class Decl:
    __slots__ = ("storage", "spec", "tag", "tag_kind", "tag_body", "declarators", "attrs")

    def __init__(self):
        self.storage, self.spec, self.tag, self.tag_kind, self.tag_body, self.declarators, self.attrs = \
            set(), [], None, None, False, [], []

    def __repr__(self):
        return f"Decl({sorted(self.storage)}, {norm(self.spec)!r}, {self.declarators})"


def _match(toks, j):
    """Index of the bracket that closes toks[j]."""
    o = toks[j].text
    c = {"(": ")", "[": "]", "{": "}"}[o]
    depth = 0
    for k in range(j, len(toks)):
        if toks[k].text == o:
            depth += 1
        elif toks[k].text == c:
            depth -= 1
            if depth == 0:
                return k
    return len(toks) - 1


def parse_decl(toks):
    """Storage class, specifiers and declarators of a declaration's tokens (comments removed, `;` last)."""
    d = Decl()
    j, n = 0, len(toks)
    if n and toks[-1].text == ";":
        n -= 1
    # specifiers: storage classes, qualifiers, type names, struct/union/enum with or without a body, attributes
    seen_type = False
    while j < n:
        t = toks[j]
        if t.text in STORAGE:
            d.storage.add(t.text)
            j += 1
            continue
        if t.text in ("__attribute__", "__extension__"):
            if t.text == "__attribute__" and j + 1 < n and toks[j + 1].text == "(":
                k = _match(toks, j + 1)
                d.attrs.append(toks[j:k + 1])
                j = k + 1
            else:
                j += 1
            continue
        if t.text in ("struct", "union", "enum"):
            d.spec.append(t)
            d.tag_kind = t.text
            j += 1
            while j < n and toks[j].text == "__attribute__":
                k = _match(toks, j + 1)
                d.spec.extend(toks[j:k + 1])
                j = k + 1
            if j < n and toks[j].kind == "id":
                d.tag = toks[j].text
                d.spec.append(toks[j])
                j += 1
            if j < n and toks[j].text == "{":
                k = _match(toks, j)
                d.spec.extend(toks[j:k + 1])
                d.tag_body = True
                j = k + 1
            seen_type = True
            continue
        if t.text in QUALIFIERS or t.text in ("signed", "unsigned", "short", "long", "int", "char", "float", "double",
                                              "void", "_Bool", "inline", "__inline", "__inline__", "_Complex"):
            d.spec.append(t)
            if t.text not in QUALIFIERS and t.text not in ("inline", "__inline", "__inline__"):
                seen_type = True
            j += 1
            continue
        if t.text in ("__typeof__", "typeof"):
            k = _match(toks, j + 1)
            d.spec.extend(toks[j:k + 1])
            seen_type = True
            j = k + 1
            continue
        if t.kind == "id" and not seen_type and t.text not in KEYWORDS:
            d.spec.append(t)     # a typedef name
            seen_type = True
            j += 1
            continue
        break
    # declarators, separated by top-level commas
    while j < n:
        dc = Declarator()
        depth = 0
        k = j
        while k < n and not (toks[k].text == "," and depth == 0):
            if toks[k].text in ("(", "[", "{"):
                depth += 1
            elif toks[k].text in (")", "]", "}"):
                depth -= 1
            k += 1
        part = toks[j:k]
        dc.toks = part
        _declarator(dc, part)
        d.declarators.append(dc)
        j = k + 1
    return d


def _declarator(dc, part):
    """Name, function-ness and prototype of one declarator (`*name[4]`, `(*fp)(int)`, `f(void)`, `x = 3`)."""
    # cut the initializer
    for k, t in enumerate(part):
        if t.text == "=":
            dc.init = part[k + 1:]
            part = part[:k]
            break
    # cut trailing attributes and asm labels
    while part and part[-1].text == ")":
        # find an __attribute__ or asm before it
        k = len(part) - 1
        depth = 0
        while k >= 0:
            if part[k].text == ")":
                depth += 1
            elif part[k].text == "(":
                depth -= 1
                if depth == 0:
                    break
            k -= 1
        if k > 0 and part[k - 1].text in ("__attribute__", "__asm__", "asm", "__asm"):
            part = part[:k - 1]
        elif k > 1 and part[k - 1].text == "(" and part[k - 2].text == "__attribute__":
            part = part[:k - 2]
        else:
            break
    # the name: the first identifier that isn't a keyword or attribute, not inside a parameter list
    depth = 0
    name_at = None
    k = 0
    while k < len(part):
        t = part[k]
        if t.text == "__attribute__":
            k = _match(part, k + 1) + 1
            continue
        if t.text == "(":
            # a parameter list follows a name or a closing paren; a grouping paren follows * or starts the declarator
            if name_at is not None:
                break
            depth += 1
        elif t.text == ")":
            depth -= 1
        elif t.text == "[":
            if name_at is not None and depth == 0:
                break
            k = _match(part, k) + 1
            continue
        elif t.kind == "id" and t.text not in KEYWORDS and depth >= 0:
            # the last identifier before a parameter list: `FAR uLongf` (a macro before the name) names uLongf
            name_at = k
        k += 1
    if name_at is None:
        return
    dc.name = part[name_at].text
    dc.pointer = any(t.text == "*" for t in part[:name_at])
    # what follows the name (and the parentheses that close around it): ( → function, [ → array
    k = name_at + 1
    while k < len(part) and part[k].text == ")":
        k += 1
        # (*fp)(...) is a pointer to a function: a variable
        dc.pointer = dc.pointer or any(t.text == "*" for t in part[:name_at])
        if k < len(part) and part[k].text == "(":
            return
    if k < len(part) and part[k].text == "(" and not (name_at and part[name_at - 1].text == "(" and
                                                     any(t.text == "*" for t in part[:name_at])):
        dc.func = True
        close = _match(part, k)
        params = part[k + 1:close]
        dc.proto = bool(params)   # `f()` is no prototype; `f(void)` and `f(int)` are
    elif k < len(part) and part[k].text == "[":
        dc.array = True
