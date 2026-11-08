#!/usr/bin/env python3
"""Check the name maps (tools/port/names/*.tsv, docs/port/PCONLY.md 3) before tools/port/rename.py applies them.

    python3 tools/port/names_check.py                          # every map in tools/port/names/
    python3 tools/port/names_check.py tools/port/names/cars.tsv [--root DIR] [--strict] [--quiet]

A map is tab separated (a run of tabs is one separator), `#` starts a comment line, and its rows are

    old  new  kind  source  confidence  note

kind is func, data, type, field (old = Struct.0xOFFSET) or file (old = a path from the top of the tree); source is
mat3 or code; confidence is sure or likely; the note is required. An optional header row names the columns.

Errors (exit status 1):
- the format: a row without six fields, an unknown kind, source or confidence, an old or new name that is no C
  identifier (a path for file rows, Struct.0xOFFSET for field rows), an empty note;
- an old name in two rows, or a new name used twice, across all the maps given (a field's new name twice in one
  struct; the same new path twice);
- a new name that collides with an identifier already in src/, include/, port/ or nonmatching/ (other than an old
  name a row renames away), or with a name in the symbol tables (asm/us/*/symbols.json, symbol_addrs.us.txt,
  undefined_syms_auto.us.txt, layout/*.json), with a C keyword, a reserved identifier (a leading underscore), or
  a name of the C library, POSIX, zlib or SDL;
- an address left in a new name (8 and seven hex digits, func_, D_8, sym_, jtbl_), or in a new file path;
- an old name that doesn't exist (neither in the C of those folders nor in the symbol tables); an old file that
  doesn't exist, or a new file that does; a field row whose struct is no type of the C;
- a kind that contradicts the C: a func row whose old name the C only declares as data or a type, a data row whose
  old name the C defines as a function, a type row whose old name is no typedef or struct, union or enum tag.

Warnings (errors with --strict): the naming style of PCONLY.md 3 for source `code` rows (functions and data in
lower_snake_case, types in CamelCase), the words it bans (unk, temp, tmp, data2, helper), the PC layer's prefixes
(pc_, PC_), a POSIX-reserved _t suffix, an old name only the symbol tables have, an old address name the C also
spells otherwise (func_800a3424 beside func_800A3424: rename.py renames the spelling given), a func or data row the C
gives no evidence of its kind for, and names Windows' headers define as macros.

The module is also the library rename.py uses: the map reader, the C lexer and the tree scan. Stdlib only,
Python 3.9+.
"""
import argparse
import collections
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAPS_DIR = Path("tools/port/names")

COLUMNS = ("old", "new", "kind", "source", "confidence", "note")
KINDS = ("func", "data", "type", "field", "file")
IDENT_KINDS = ("func", "data", "type")
SOURCES = ("mat3", "code")
CONFIDENCES = ("sure", "likely")

# The C of these folders is where a game name lives and where a new one must not collide (PCONLY.md 3).
SCAN_DIRS = ("src", "include", "port", "nonmatching")
C_EXTS = (".c", ".h", ".inc")

IDENT = re.compile(r"(?<![0-9A-Za-z_])[A-Za-z_][A-Za-z0-9_]*")   # not inside a number (0x27C)
IDENT_FULL = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
# A name that spells its N64 address. Two spellings of one address are one symbol (gen_stubs.py aliases
# func_8001fe58 to func_8001FE58), so the maps and the tools compare them by norm().
ADDR_NAME = re.compile(r"^(D|func|sym|jtbl)_([0-9A-Fa-f]{8})$")
ADDR_IN_NAME = re.compile(r"8[0-9A-Fa-f]{7}|(?:^|_)func_|(?:^|_)D_8|(?:^|_)sym_|(?:^|_)jtbl_", re.I)
FIELD_OLD = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\.(0x[0-9A-Fa-f]+)$")
SNAKE = re.compile(r"^[a-z][a-z0-9]*(?:_[a-z0-9]+)*$")
CAMEL = re.compile(r"^[A-Z][A-Za-z0-9]*$")
BANNED_WORDS = {"unk", "temp", "tmp", "data2", "helper"}


def norm(name):
    """The key a name is compared by: an address name with its hex digits in upper case, anything else itself."""
    m = ADDR_NAME.match(name)
    return f"{m.group(1)}_{m.group(2).upper()}" if m else name


def address_of_name(name):
    m = ADDR_NAME.match(name)
    return int(m.group(2), 16) if m else None


# ---------------------------------------------------------------------------------------------- reserved names

C_KEYWORDS = set("""
auto break case char const continue default do double else enum extern float for goto if inline int long register
restrict return short signed sizeof static struct switch typedef union unsigned void volatile while _Alignas _Alignof
_Atomic _Bool _Complex _Generic _Imaginary _Noreturn _Static_assert _Thread_local _BitInt _Decimal32 _Decimal64
_Decimal128 alignas alignof bool constexpr false nullptr static_assert thread_local true typeof typeof_unqual asm
__asm__ __asm __attribute__ __typeof__ __inline__ __inline __restrict__ __restrict __extension__ __volatile__
__label__ __builtin_va_list
""".split())

_MATH = """acos asin atan atan2 cos sin tan acosh asinh atanh cosh sinh tanh exp exp2 expm1 frexp ilogb ldexp log log10
log1p log2 logb modf scalbn scalbln cbrt fabs hypot pow sqrt erf erfc lgamma tgamma ceil floor nearbyint rint lrint
llrint round lround llround trunc fmod remainder remquo copysign nan nextafter nexttoward fdim fmax fmin fma""".split()
C_LIBRARY = set(_MATH) | {f + s for f in _MATH for s in "fl"} | set("""
fpclassify isfinite isinf isnan isnormal signbit isgreater isgreaterequal isless islessequal islessgreater isunordered
HUGE_VAL HUGE_VALF HUGE_VALL INFINITY NAN M_PI M_E float_t double_t
assert isalnum isalpha isblank iscntrl isdigit isgraph islower isprint ispunct isspace isupper isxdigit tolower toupper
errno EDOM ERANGE EILSEQ feclearexcept fegetexceptflag feraiseexcept fesetexceptflag fetestexcept fegetround
fesetround fegetenv feholdexcept fesetenv feupdateenv fenv_t fexcept_t imaxabs imaxdiv strtoimax strtoumax imaxdiv_t
CHAR_BIT CHAR_MAX CHAR_MIN INT_MAX INT_MIN LONG_MAX LONG_MIN SHRT_MAX SHRT_MIN UCHAR_MAX UINT_MAX ULONG_MAX USHRT_MAX
SCHAR_MAX SCHAR_MIN LLONG_MAX LLONG_MIN ULLONG_MAX MB_LEN_MAX setlocale localeconv lconv setjmp longjmp jmp_buf
signal raise sig_atomic_t SIGABRT SIGFPE SIGILL SIGINT SIGSEGV SIGTERM va_list va_start va_arg va_end va_copy NULL
offsetof ptrdiff_t size_t ssize_t wchar_t max_align_t int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t
uint64_t intptr_t uintptr_t intmax_t uintmax_t int_least8_t int_fast8_t FILE fpos_t EOF BUFSIZ stdin stdout stderr
remove rename tmpfile tmpnam fclose fflush fopen freopen setbuf setvbuf fprintf fscanf printf scanf snprintf sprintf
sscanf vfprintf vfscanf vprintf vscanf vsnprintf vsprintf vsscanf fgetc fgets fputc fputs getc getchar gets putc
putchar puts ungetc fread fwrite fgetpos fseek fsetpos ftell rewind clearerr feof ferror perror atof atoi atol atoll
strtod strtof strtold strtol strtoll strtoul strtoull rand srand aligned_alloc calloc free malloc realloc abort atexit
at_quick_exit exit quick_exit getenv system bsearch qsort abs labs llabs div ldiv lldiv div_t ldiv_t mblen mbtowc
wctomb mbstowcs wcstombs RAND_MAX EXIT_SUCCESS EXIT_FAILURE MB_CUR_MAX memcpy memmove memset memcmp memchr strcpy
strncpy strcat strncat strcmp strncmp strcoll strxfrm strchr strrchr strspn strcspn strpbrk strstr strtok strerror
strlen strdup strndup strnlen clock time difftime mktime asctime ctime gmtime localtime strftime timespec_get clock_t
time_t tm timespec CLOCKS_PER_SEC main
read write open close lseek unlink access stat fstat lstat mmap munmap mprotect msync getpid getppid fork execv execve
execvp kill sleep usleep nanosleep select poll socket bind listen accept connect send recv sendto recvfrom shutdown
getaddrinfo freeaddrinfo gethostbyname htons htonl ntohs ntohl inet_pton inet_ntop index rindex bzero bcopy bcmp
random srandom ffs strcasecmp strncasecmp fileno fdopen popen pclose dup dup2 pipe environ optarg optind getopt
alarm chdir getcwd mkdir rmdir opendir readdir closedir dirent sysconf isatty truncate ftruncate fsync link symlink
readlink realpath basename dirname gettimeofday clock_gettime timeval itimerval sigaction sigset_t sigemptyset
setenv unsetenv putenv mkstemp tmpfile64 off_t pid_t uid_t gid_t mode_t dev_t ino_t nlink_t
inflate deflate inflateInit inflateInit2 inflateEnd inflateReset deflateInit deflateInit2 deflateEnd crc32 adler32
compress compress2 uncompress zlibVersion Bytef uLong uInt uLongf voidpf voidp z_stream z_streamp zcalloc zcfree
""".split())
# Prefixes whose names belong to a library or the implementation.
LIBRARY_PREFIXES = ("SDL_", "SDLK_", "KMOD_", "AUDIO_", "pthread_", "sem_", "gz", "__builtin_", "PRI", "SCN",
                    "FLT_", "DBL_", "LDBL_", "SIG", "LC_", "E2BIG")
SDL_TYPES = re.compile(r"^[SU]int(8|16|32|64)$")
# Macros Windows' headers define (windef.h, rpcndr.h, minwindef.h): a game name equal to one breaks the Windows build
# of any file that includes <windows.h> before the game's headers.
WINDOWS_MACROS = set("""min max near far small interface pascal cdecl CALLBACK WINAPI APIENTRY TRUE FALSE IN OUT
OPTIONAL CONST VOID BOOL BYTE WORD DWORD ERROR DELETE""".split())
PC_PREFIXES = ("pc_", "PC_", "Pc")


def library_name(name):
    return (name in C_LIBRARY or name.startswith(LIBRARY_PREFIXES) or bool(SDL_TYPES.match(name)))


# ---------------------------------------------------------------------------------------------------- the maps

class Row:
    __slots__ = ("path", "line", "old", "new", "kind", "source", "confidence", "note")

    def __init__(self, path, line, old, new, kind, source, confidence, note):
        self.path, self.line, self.old, self.new = path, line, old, new
        self.kind, self.source, self.confidence, self.note = kind, source, confidence, note

    def where(self):
        return f"{self.path}:{self.line}"

    def __repr__(self):
        return f"Row({self.old!r} -> {self.new!r}, {self.kind}, {self.where()})"


def map_paths(root, given=None):
    """The maps: the files given, else every tools/port/names/*.tsv under root, sorted."""
    if given:
        return [Path(p) for p in given]
    d = Path(root) / MAPS_DIR
    return sorted(d.glob("*.tsv")) if d.is_dir() else []


def read_maps(paths):
    """(rows, errors): every row of the maps, and the format errors as (where, message)."""
    rows, errors = [], []
    for path in paths:
        try:
            text = Path(path).read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as e:
            errors.append((str(path), f"cannot read: {e}"))
            continue
        header_seen = False
        for n, line in enumerate(text.splitlines(), 1):
            where = f"{path}:{n}"
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            if "\t" not in line:
                errors.append((where, "no tab in the row: the maps are tab separated (PCONLY.md 3)"))
                continue
            f = [x.strip() for x in re.split(r"\t+", line.strip("\r\n"))]
            if not header_seen and [x.lower() for x in f[:6]] == list(COLUMNS[:len(f[:6])]) and f[0] == "old":
                header_seen = True
                continue
            header_seen = True
            if len(f) < 6:
                errors.append((where, f"{len(f)} fields, expected 6: {' | '.join(COLUMNS)}"))
                continue
            if len(f) > 6:
                f = f[:5] + [" ".join(f[5:])]
            rows.append(Row(str(path), n, *f))
    return rows, errors


def check_format(row):
    """The format errors of one row, as messages."""
    out = []
    if row.kind not in KINDS:
        out.append(f"kind {row.kind!r} is none of {', '.join(KINDS)}")
    if row.source not in SOURCES:
        out.append(f"source {row.source!r} is none of {', '.join(SOURCES)}")
    if row.confidence not in CONFIDENCES:
        out.append(f"confidence {row.confidence!r} is none of {', '.join(CONFIDENCES)}")
    if not row.note:
        out.append("no note: say what it is and the evidence")
    if row.kind in IDENT_KINDS:
        for col, v in (("old", row.old), ("new", row.new)):
            if not IDENT_FULL.match(v):
                out.append(f"{col} name {v!r} is no C identifier")
    elif row.kind == "field":
        if not FIELD_OLD.match(row.old):
            out.append(f"old {row.old!r}: a field row's old name is Struct.0xOFFSET (tModelData.0x5AC)")
        if not IDENT_FULL.match(row.new):
            out.append(f"new field name {row.new!r} is no C identifier")
    elif row.kind == "file":
        for col, v in (("old", row.old), ("new", row.new)):
            p = v.replace("\\", "/")
            if not p or p.startswith("/") or ".." in p.split("/") or p.endswith("/") or " " in p:
                out.append(f"{col} path {v!r}: a relative path from the top of the tree, without .. or spaces")
    if row.old == row.new:
        out.append("old and new are the same")
    return out


# ---------------------------------------------------------------------------------------------------- C lexing

# One token of C per match: comments, include lines (their path), string and character literals, numbers (the
# preprocessor's pp-numbers, so 0x80123456 and 1.5e3f are one token), identifiers and punctuation. Whitespace is
# skipped. An unterminated comment or literal ends at the end of the file (the line, for literals).
C_LEX = re.compile(r"""
    (?P<bc>/\*.*?(?:\*/|\Z))
   |(?P<lc>//(?:\\\r?\n|[^\n])*)
   |(?P<inc>\#[ \t]*include[ \t]*(?:"[^"\n]*"|<[^>\n]*>))
   |(?P<str>(?:u8|[uUL])?"(?:\\.|[^"\\\n])*"?)
   |(?P<chr>[uUL]?'(?:\\.|[^'\\\n])*'?)
   |(?P<num>\.?[0-9](?:[eEpP][+-]|[0-9A-Za-z_.])*)
   |(?P<id>[A-Za-z_][A-Za-z0-9_]*)
   |(?P<p>\#\#|->|\S)
""", re.S | re.X)


def c_tokens(text):
    """[(kind, start, end, text)] of a C file; kind is bc, lc, inc, str, chr, num, id or p."""
    return [(m.lastgroup, m.start(), m.end(), m.group()) for m in C_LEX.finditer(text)]


# ---------------------------------------------------------------------------------------------------- the tree

def tracked_files(root):
    """The files of the tree, relative to root: git's (tracked, and untracked but not ignored), else a walk."""
    root = Path(root)
    try:
        out = subprocess.run(["git", "-C", str(root), "ls-files", "-co", "--exclude-standard", "-z"],
                             capture_output=True, check=True).stdout.decode("utf-8", "replace")
        files = sorted({f for f in out.split("\0") if f and (root / f).is_file()})
        if files:
            return files
    except (OSError, subprocess.CalledProcessError):
        pass
    files = []
    for d, dirs, names in os.walk(root):
        dirs[:] = [x for x in dirs if x not in (".git", "build", "__pycache__", ".claude")]
        for name in names:
            files.append(os.path.relpath(os.path.join(d, name), root).replace(os.sep, "/"))
    return sorted(files)


def read_text(path):
    """A file's text, or None for a binary file (a NUL byte, or not UTF-8)."""
    try:
        data = Path(path).read_bytes()
    except OSError:
        return None
    if b"\0" in data[:65536]:
        return None
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        return None


_PREV_NOT_DECL = {"return", "sizeof", "case", "else", "do", "goto", "if", "while", "for", "switch"}
# The classifier's lexer: only code tokens matter, and findall is several times quicker than finditer here.
# Groups: an identifier, a punctuator, or a literal (a string, a character or a number); comments match no group.
_C_CODE = re.compile(r"""
    /\*.*?(?:\*/|\Z) | //(?:\\\r?\n|[^\n])* | \#[ \t]*include[^\n]*
   |([A-Za-z_][A-Za-z0-9_]*)
   |((?:u8|[uUL])?"(?:\\.|[^"\\\n])*"?|[uUL]?'(?:\\.|[^'\\\n])*'?|\.?[0-9](?:[eEpP][+-]|[0-9A-Za-z_.])*)
   |(\#\#|->|\S)
""", re.S | re.X)


_ASM_OPEN = re.compile(r"\b(?:__asm__|__asm|asm|alias|__alias__|weakref|__weakref__)\s*(?:(?:volatile|__volatile__)\s*)?\(")
_STRING = re.compile(r'"((?:\\.|[^"\\\n])*)"')


def asm_string_names(text):
    """The identifiers in the string literals of asm statements and alias/weakref attributes: linker symbols
    (`.set D_x, gScheduler + 0x40`)."""
    out = set()
    for m in _ASM_OPEN.finditer(text):
        depth, i = 1, m.end()
        while i < len(text) and depth and i - m.end() < 20000:
            c = text[i]
            if c == '"':
                s = _STRING.match(text, i)
                if s:
                    out.update(x for x in IDENT.findall(s.group(1).replace("\\n", " ").replace("\\t", " "))
                               if not x[0].isdigit())
                    i = s.end()
                    continue
            depth += c == "("
            depth -= c == ")"
            i += 1
    return out


def classify_c(text):
    """What one C file says of its names: (identifiers in the code and in asm strings, functions defined, functions
    declared, functions called or declared, variables declared at file scope or by extern, typedef names and struct,
    union and enum tags), as sets."""
    idents, func_def, func_decl, func_use, data_decl, types = set(), set(), set(), set(), set(), set()
    if "asm" in text or "alias" in text or "weakref" in text:
        idents |= asm_string_names(text)
    code = []   # (is identifier, text)
    for ident, lit, punct in _C_CODE.findall(text):
        if ident:
            code.append((True, ident))
        elif punct:
            code.append((False, punct))
        elif lit:
            code.append((False, "0"))
    n = len(code)
    brace = paren = 0
    in_typedef = in_extern = False   # the statement being read began with typedef / extern
    stmt_brace = 0                   # the brace depth that statement began at
    for i in range(n):
        is_id, v = code[i]
        if not is_id:
            if v == "{":
                brace += 1
                if not in_typedef and not in_extern:
                    stmt_brace = brace
            elif v == "}":
                brace = max(0, brace - 1)
                if not in_typedef and not in_extern:
                    stmt_brace = brace
            elif v == "(":
                paren += 1
            elif v == ")":
                paren = max(0, paren - 1)
            elif v == ";" and brace <= stmt_brace:
                in_typedef = in_extern = False
                stmt_brace = brace
                paren = 0
            continue
        idents.add(v)
        prev_id, prev = code[i - 1] if i else (False, "")
        prev2 = code[i - 2][1] if i > 1 else ""
        nxt = code[i + 1][1] if i + 1 < n else ""
        if v == "typedef" or v == "extern":
            if v == "typedef":
                in_typedef = True
            else:
                in_extern = True
            stmt_brace = brace
            continue
        if prev in ("struct", "union", "enum"):
            types.add(v)
            continue
        declarator = prev_id or prev == "*"
        if in_typedef:
            if brace == stmt_brace and ((nxt in (";", ",", "[") and paren == 0)
                                        or (prev == "*" and prev2 == "(" and nxt == ")")
                                        or (nxt == "(" and paren == 0 and prev_id)):
                types.add(v)
            continue
        file_scope = brace == 0 or in_extern
        if nxt == "(":
            func_use.add(v)
            if file_scope and paren == 0 and declarator and prev not in _PREV_NOT_DECL:
                # a declaration or a definition: is the parameter list followed by a body (or K&R parameters)?
                depth, j = 0, i + 1
                while j < n:
                    t = code[j][1]
                    if t == "(":
                        depth += 1
                    elif t == ")":
                        depth -= 1
                        if depth == 0:
                            break
                    j += 1
                a_id, after = code[j + 1] if j + 1 < n else (False, "")
                if brace == 0 and not in_extern and (after == "{" or (
                        a_id and after not in ("__attribute__", "asm", "__asm__", "__asm"))):
                    func_def.add(v)
                else:
                    func_decl.add(v)
            continue
        if file_scope and paren == 0 and nxt in (";", ",", "[", "=") and (declarator or prev == ",") \
                and prev not in _PREV_NOT_DECL:
            data_decl.add(v)
        elif file_scope and prev == "*" and prev2 == "(" and nxt == ")":
            data_decl.add(v)   # a function pointer variable: T (*name)(...)
    return idents, func_def, func_decl, func_use, data_decl, types


def _classify_path(path):
    text = read_text(path)
    return classify_c(text) if text is not None else None


def _classify_all(paths):
    """classify_c of every path, in worker processes when there are many (the tree's C is about 50 MB)."""
    if len(paths) > 64 and (os.cpu_count() or 1) > 1:
        try:
            import concurrent.futures
            import multiprocessing
            ctx = multiprocessing.get_context("fork") if "fork" in multiprocessing.get_all_start_methods() \
                else None
            with concurrent.futures.ProcessPoolExecutor(max_workers=min(16, os.cpu_count()),
                                                        mp_context=ctx) as ex:
                return list(ex.map(_classify_path, paths, chunksize=16))
        except (OSError, ImportError, RuntimeError):
            pass
    return [_classify_path(p) for p in paths]


class Tree:
    """What the tree says of its names: the C of SCAN_DIRS (identifiers in code, and the evidence of each name's
    kind) and the symbol tables."""

    def __init__(self, root, files=None):
        self.root = Path(root)
        self.files = files if files is not None else tracked_files(root)
        self.idents = set()
        self.func_def, self.func_decl, self.func_use = set(), set(), set()
        self.data_decl, self.types = set(), set()
        self.table_names = {}   # name -> where (a symbol table)
        self.table_kind = {}    # name -> func or data (symbol_addrs.us.txt's type:func, layout functions)
        paths = [self.root / f for f in self.files
                 if f.startswith(tuple(d + "/" for d in SCAN_DIRS)) and f.endswith(C_EXTS)]
        for res in _classify_all(paths):
            if res:
                for mine, theirs in zip((self.idents, self.func_def, self.func_decl, self.func_use, self.data_decl,
                                         self.types), res):
                    mine |= theirs
        self._tables()
        self.spellings = collections.defaultdict(set)   # norm(address name) -> its spellings in the C
        for x in self.idents:
            if ADDR_NAME.match(x):
                self.spellings[norm(x)].add(x)
        self.idents_norm = {norm(x) for x in self.idents}

    def _tables(self):
        r = self.root
        for p in sorted((r / "asm").glob("us/*/symbols.json")) if (r / "asm").is_dir() else []:
            try:
                for name in json.loads(p.read_text())["symbols"]:
                    self.table_names.setdefault(name, str(p.relative_to(r)))
            except (ValueError, KeyError, OSError):
                pass
        for name in ("symbol_addrs.us.txt", "undefined_syms_auto.us.txt", "undefined_funcs_auto.us.txt"):
            p = r / name
            if p.is_file():
                for line in p.read_text(errors="replace").splitlines():
                    m = re.match(r"^\s*([A-Za-z_]\w*)\s*=\s*0x[0-9A-Fa-f]+\s*;(.*)$", line)
                    if m:
                        self.table_names.setdefault(m.group(1), name)
                        kind = "func" if "type:func" in m.group(2) or name == "undefined_funcs_auto.us.txt" else None
                        if kind:
                            self.table_kind.setdefault(m.group(1), kind)
        for p in sorted((r / "layout").glob("*.json")) if (r / "layout").is_dir() else []:
            try:
                for region in json.loads(p.read_text())["regions"]:
                    for e in region["entries"]:
                        if e.get("kind") == "function":
                            self.table_names.setdefault(e["target_id"], str(p.relative_to(r)))
                            self.table_kind.setdefault(e["target_id"], "func")
            except (ValueError, KeyError, OSError):
                pass
        self.table_norm = {norm(x): x for x in self.table_names}

    def exists(self, name):
        k = norm(name)
        return k in self.idents_norm or k in self.table_norm

    def in_c(self, name):
        return norm(name) in self.idents_norm


# ---------------------------------------------------------------------------------------------------- the checks

def check_rows(rows, tree):
    """(errors, warnings) of rows that passed the format check, against each other and the tree: [(where, msg)]."""
    errors, warnings = [], []
    olds = collections.defaultdict(list)
    news = collections.defaultdict(list)
    field_news = collections.defaultdict(list)
    for r in rows:
        if r.kind in IDENT_KINDS:
            olds[("id", norm(r.old))].append(r)
            news[("id", r.new)].append(r)
        elif r.kind == "field":
            m = FIELD_OLD.match(r.old)
            olds[("field", m.group(1), int(m.group(2), 16))].append(r)
            field_news[(m.group(1), r.new)].append(r)
        else:
            olds[("file", r.old)].append(r)
            news[("file", r.new)].append(r)
    for key, rs in olds.items():
        if len(rs) > 1:
            for r in rs[1:]:
                errors.append((r.where(), f"old {r.old} is in another row too ({rs[0].where()})"))
    for key, rs in list(news.items()) + list(field_news.items()):
        if len(rs) > 1:
            for r in rs[1:]:
                errors.append((r.where(), f"new {r.new} is used by another row too ({rs[0].where()}: {rs[0].old})"))
    renamed_away = {k[1] for k in olds if k[0] == "id"}
    for r in rows:
        w = r.where()
        if r.kind in IDENT_KINDS or r.kind == "field":
            new = r.new
            if new in C_KEYWORDS:
                errors.append((w, f"new {new} is a C keyword"))
            if new.startswith("__") or re.match(r"^_[A-Z]", new):
                errors.append((w, f"new {new} is a reserved identifier (__ or _ and a capital)"))
            elif new.startswith("_") and r.kind != "field":
                errors.append((w, f"new {new} starts with an underscore: reserved at file scope"))
            if r.kind != "field":
                if library_name(new):
                    errors.append((w, f"new {new} is a name of the C library, POSIX, zlib or SDL"))
                if new in WINDOWS_MACROS:
                    warnings.append((w, f"new {new} is a macro of Windows' headers"))
                if new.endswith("_t"):
                    warnings.append((w, f"new {new}: POSIX reserves the _t suffix"))
                if new.startswith(PC_PREFIXES):
                    warnings.append((w, f"new {new} has the PC layer's prefix (pc_, PC_)"))
            if ADDR_IN_NAME.search(new):
                errors.append((w, f"new {new} still has an address in it (PCONLY.md 3: no address in any name)"))
            words = set(new.lower().split("_"))
            bad = sorted(words & BANNED_WORDS) or [x for x in BANNED_WORDS if re.search(
                r"(?:^|[a-z0-9])" + x.capitalize() + r"(?:[A-Z0-9]|$)", new)]
            if bad:
                warnings.append((w, f"new {new} uses {', '.join(bad)} (PCONLY.md 3: never unk, temp, data2, helper)"))
        if r.kind in IDENT_KINDS:
            new, old = r.new, r.old
            if tree.exists(new) and norm(new) not in renamed_away:
                where = "the C" if tree.in_c(new) else tree.table_names.get(tree.table_norm.get(norm(new)), "a table")
                errors.append((w, f"new {new} already exists in {where}"))
            if r.source == "code":
                if r.kind in ("func", "data") and not SNAKE.match(new):
                    warnings.append((w, f"new {new}: source code {r.kind} names are lower_snake_case"))
                if r.kind == "type" and not CAMEL.match(new):
                    warnings.append((w, f"new {new}: source code type names are CamelCase"))
            if not tree.exists(old):
                errors.append((w, f"old {old} exists nowhere in {', '.join(SCAN_DIRS)} or the symbol tables"))
                continue
            if not tree.in_c(old):
                warnings.append((w, f"old {old} is only in the symbol tables, not in the C"))
            others = sorted(tree.spellings.get(norm(old), set()) - {old})
            if others:
                warnings.append((w, f"old {old} is also spelled {', '.join(others)} in the C: another identifier, "
                                    f"which rename.py leaves (gen_stubs.py links it by address)"))
            spellings = {old} if old in tree.idents else set(tree.spellings.get(norm(old), ()))
            tk = tree.table_kind.get(tree.table_norm.get(norm(old), ""), None)
            is_func = any(x in tree.func_def or x in tree.func_decl or x in tree.func_use for x in spellings)
            is_data = any(x in tree.data_decl for x in spellings)
            is_type = any(x in tree.types for x in spellings)
            defined_func = any(x in tree.func_def for x in spellings)
            if r.kind == "func":
                if not is_func and tk != "func" and (is_data or is_type):
                    errors.append((w, f"old {old} is a {'type' if is_type else 'variable'} in the C, not a function"))
                elif not is_func and tk != "func" and tree.in_c(old):
                    warnings.append((w, f"old {old}: the C neither defines, declares nor calls it as a function"))
            elif r.kind == "data":
                if defined_func and not is_data:
                    errors.append((w, f"old {old} is defined as a function in the C, not data"))
                elif is_type and not is_data:
                    errors.append((w, f"old {old} is a type in the C, not data"))
                elif tk == "func" and not is_data:
                    errors.append((w, f"old {old} is a function in the symbol tables, not data"))
                elif not is_data and is_func:
                    warnings.append((w, f"old {old}: the C calls or declares it as a function"))
            elif r.kind == "type" and not is_type:
                errors.append((w, f"old {old} is no typedef name or struct, union or enum tag in the C"))
        elif r.kind == "field":
            struct = FIELD_OLD.match(r.old).group(1)
            if struct not in tree.types:
                errors.append((w, f"struct {struct} of {r.old} is no type of the C"))
        elif r.kind == "file":
            old, new = r.old.replace("\\", "/"), r.new.replace("\\", "/")
            if not (tree.root / old).is_file():
                errors.append((w, f"old file {old} doesn't exist"))
            if (tree.root / new).exists():
                errors.append((w, f"new file {new} exists already"))
            if ADDR_IN_NAME.search(Path(new).stem):
                errors.append((w, f"new file {new} still has an address in its name"))
            if Path(old).suffix != Path(new).suffix:
                warnings.append((w, f"file {old} changes its extension"))
    return errors, warnings


def run_checks(root, paths, tree=None):
    """(rows, errors, warnings) for the maps at paths."""
    rows, errors = read_maps(paths)
    warnings = []
    good = []
    for r in rows:
        bad = check_format(r)
        errors += [(r.where(), m) for m in bad]
        if not bad:
            good.append(r)
    if good:
        tree = tree or Tree(root)
        e, w = check_rows(good, tree)
        errors += e
        warnings += w
    return rows, errors, warnings


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("maps", nargs="*", help="maps to check (default: every tools/port/names/*.tsv)")
    ap.add_argument("--root", type=Path, default=ROOT, help="the top of the tree (default: this checkout)")
    ap.add_argument("--strict", action="store_true", help="warnings fail too")
    ap.add_argument("--quiet", action="store_true", help="the summary and the errors only")
    a = ap.parse_args(argv)
    paths = map_paths(a.root, a.maps)
    if not paths:
        print(f"names_check: no maps in {a.root / MAPS_DIR}")
        return 0
    rows, errors, warnings = run_checks(a.root, paths)
    for where, msg in sorted(errors, key=lambda x: _where_key(x[0])):
        print(f"{where}: error: {msg}")
    if not a.quiet:
        for where, msg in sorted(warnings, key=lambda x: _where_key(x[0])):
            print(f"{where}: warning: {msg}")
    kinds = collections.Counter(r.kind for r in rows)
    print(f"names_check: {len(paths)} map{'s' if len(paths) != 1 else ''}, {len(rows)} rows "
          f"({', '.join(f'{k} {kinds[k]}' for k in KINDS if kinds[k])}): {len(errors)} errors, "
          f"{len(warnings)} warnings")
    return 1 if errors or (a.strict and warnings) else 0


def _where_key(where):
    path, _, line = where.rpartition(":")
    return (path, int(line)) if line.isdigit() else (where, 0)


if __name__ == "__main__":
    sys.exit(main())
