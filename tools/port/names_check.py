#!/usr/bin/env python3
"""Check the name maps (tools/port/names/*.tsv) and the drop list (tools/port/names/drop.tsv), docs/port/PCONLY.md 3,
before tools/port/rename.py applies them.

    python3 tools/port/names_check.py                          # every map in tools/port/names/, and drop.tsv
    python3 tools/port/names_check.py tools/port/names/cars.tsv [drop.tsv] [--root DIR] [--strict] [--quiet]

A map is tab separated (a run of tabs is one separator), `#` starts a comment line (the namer's notes: never
checked), and its rows are

    old  new  kind  source  confidence  note

kind is func, data, type, field (old = Struct.0xOFFSET; new = a member, or a path into an array member such as
visuals[14].func) or file (old = a file or a folder, from the top of the tree); source is mat3 or code; confidence
is sure or likely; the note is required. An optional header row names the columns. A file named drop.tsv is the drop
list: `name  table  note`, the labels the loader's symbol tables lose (table blob, boot_tail, ovl_a, ovl_b, or * for
every one of those that has the label), or symbol_addrs for a line of symbol_addrs.us.txt (never by *: boot code finds
its addresses there). The drop list applies before the maps: a dropped label's name is free for a map to give.

Errors (exit status 1):
- the format: a row without six fields, an unknown kind, source or confidence, an old or new name that is no C
  identifier (a path for file rows, Struct.0xOFFSET and a member path for field rows), an empty note;
- an old name in two rows that disagree (two maps that agree on a shared datum are a warning), or a new name used
  twice, across all the maps given, unless the old names are aliases: names of one address in the symbol tables
  (D_801174B4 and state_word_a), which rename.py makes one symbol (a field's new name twice in one struct; one new
  path twice);
- a new name that collides with what the tree has already: an identifier of the C of src/, include/, port/ and
  nonmatching/ (other than an old name a row renames away), said with where it is (a static function in a file,
  defined in, used in), or a name in the symbol tables (asm/us/*/symbols.json, symbol_addrs.us.txt,
  undefined_syms_auto.us.txt, layout/*.json) at another address, less what the drop list removes. Not collisions,
  but warnings: a name of the old one's address already (an alias; any table that gives both one address is
  enough), and a name the C has only in a #define whose body names the old name (`#define gRace D_8014A110`,
  `#define HAL_GetTextWidth(s, n) object_manager_update(s, n)`: the same after the rename), in a header no .c file
  includes even through other headers (the arcade headers), as a struct member, as a local or a parameter, or
  declared extern but nowhere defined and in no table (rspbootTextStart: the rename gives it its address);
- a C keyword, a reserved identifier (__x, _X, or _x at file scope: a warning for source mat3 rows, whose names
  are kept verbatim, _PS_FlushFile), a name of the C library, POSIX, zlib or SDL;
- an address left in a new name (8 and seven hex digits, func_, D_8, sym_, jtbl_), or in a new file path;
- an old name that doesn't exist (neither in the C of those folders nor in the symbol tables; in comments and
  strings only is a warning); an old file or folder that doesn't exist, or a new one that does. A row the tree has
  had applied already is checked for its format, its new name's form and duplicates only (the maps stay in the tree
  after rename.py applied them): a row in tools/port/names/applied.tsv, the ledger rename.py --apply writes (a
  chain's rows can't be told by their names), or one whose new name is there and its old one in no table and
  declared nowhere at file scope;
- a kind that contradicts the C: a func row whose old name the C only declares as a type, or as data unless at a
  function's address (D_8008A6A4: its address taken); a data row whose old name the C defines as a function, a type
  row whose old name is no typedef or struct, union or enum tag (macros have no kind to check);
- the drop list: a label not in the table named, also renamed by a map, or still used by the C (referenced in a
  function body or an initialiser, or defined; not only declared, which is one warning for all: wave 2 deletes
  those declarations) with no symbol file keeping its address, or used by the tools' Python (code, and strings that
  are the name), unless a map gives the name to another symbol. A label in no table and used nowhere counts as
  dropped already.

Warnings (errors with --strict): the naming style of PCONLY.md 3 for source `code` rows (functions and data in
lower_snake_case, types in CamelCase), the words it bans (unk, temp, tmp, data2, helper), the PC layer's prefixes
(pc_, PC_), a POSIX-reserved _t suffix, an old name only the symbol tables have, an old address name the C also
spells otherwise (func_800a3424 beside func_800A3424: rename.py renames the spelling given), a func or data row the C
gives no evidence of its kind for, a field row's struct the C has under no such name (MAT3's tWindowInfo: the hand
edit finds it), names Windows' headers define as macros, and the benign overlaps above.

The module is also the library rename.py uses: the map and drop list readers, the C lexer and the tree scan. Stdlib
only, Python 3.9+.
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
# tools/port/names/drop.tsv: labels the loader's symbol tables lose, not renamed (stale guesses, labels inside other
# functions, second names nothing uses): name, table (blob, boot_tail, ovl_a, ovl_b, or * for every table that has
# it), note.
DROP_FILE = "drop.tsv"
DROP_COLUMNS = ("name", "table", "note")
DROP_TABLES = ("blob", "boot_tail", "ovl_a", "ovl_b", "*", "symbol_addrs")
SYMBOL_FILE_TABLE = "symbol_addrs"   # symbol_addrs.us.txt's line: only by name, never by *
# The ledger rename.py --apply writes of the rows it applied (kind, old, new): a chain's rows (A to B, B to C) can't
# be told applied by the names alone.
APPLIED_FILE = "applied.tsv"
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
# A field's new name: a member, or a path into an array member (tCarData.0x274 is visuals[14].func)
FIELD_PATH = re.compile(r"^[A-Za-z_]\w*(?:\[\d+\])*(?:\.[A-Za-z_]\w*(?:\[\d+\])*)*$")
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


def is_drop_file(path):
    return Path(path).name == DROP_FILE


def map_paths(root, given=None):
    """The maps and drop files: the files given, else every tools/port/names/*.tsv under root, sorted. (A file named
    drop.tsv is a drop file: split_paths.)"""
    if given:
        return [Path(p) for p in given]
    d = Path(root) / MAPS_DIR
    return sorted(d.glob("*.tsv")) if d.is_dir() else []


def split_paths(paths):
    """(maps, drop files) of a list of paths."""
    paths = [p for p in paths if Path(p).name != APPLIED_FILE]
    return [p for p in paths if not is_drop_file(p)], [p for p in paths if is_drop_file(p)]


class Drop:
    """A row of drop.tsv: a label to remove from a loader symbol table (asm/us/<table>/symbols.json), or from
    every table that has it (table *)."""
    __slots__ = ("path", "line", "name", "table", "note")

    def __init__(self, path, line, name, table, note):
        self.path, self.line, self.name, self.table, self.note = path, line, name, table, note

    def where(self):
        return f"{self.path}:{self.line}"


def read_drops(paths):
    """(drops, errors): every row of the drop files (name, table, note; tab separated, # comments), and the format
    errors as (where, message)."""
    drops, errors = [], []
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
            f = [x.strip() for x in re.split(r"\t+", line.strip("\r\n"))]
            if not header_seen and [x.lower() for x in f[:3]] == list(DROP_COLUMNS):
                header_seen = True
                continue
            header_seen = True
            if len(f) < 3 or "\t" not in line:
                errors.append((where, f"{len(f)} fields, expected 3, tab separated: {' | '.join(DROP_COLUMNS)}"))
                continue
            d = Drop(str(path), n, f[0], f[1], " ".join(f[2:]))
            if not IDENT_FULL.match(d.name):
                errors.append((where, f"name {d.name!r} is no C identifier"))
            elif d.table not in DROP_TABLES:
                errors.append((where, f"table {d.table!r} is none of {', '.join(DROP_TABLES)}"))
            elif not d.note:
                errors.append((where, "no note: say why the label goes"))
            else:
                drops.append(d)
    return drops, errors


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
        if not FIELD_PATH.match(row.new):
            out.append(f"new field name {row.new!r} is no member path (a name, or visuals[14].func)")
    elif row.kind == "file":
        for col, v in (("old", row.old), ("new", row.new)):
            p = v.replace("\\", "/").rstrip("/")   # a folder may end with /
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
# Groups: an #include's file name, an identifier, a literal (a string, a character or a number), a punctuator;
# comments match no group.
_C_CODE = re.compile(r"""
    /\*.*?(?:\*/|\Z) | //(?:\\\r?\n|[^\n])* | \#[ \t]*include[ \t]*["<]([^">\n]*)[">]?
   |([A-Za-z_][A-Za-z0-9_]*)
   |((?:u8|[uUL])?"(?:\\.|[^"\\\n])*"?|[uUL]?'(?:\\.|[^'\\\n])*'?|\.?[0-9](?:[eEpP][+-]|[0-9A-Za-z_.])*)
   |(\#\#|->|\S)
""", re.S | re.X)
# An object-like macro whose body is one identifier: #define gRace D_8014A110
# A #define and its body: #define gRace D_8014A110, #define HAL_GetTextWidth(s, n) object_manager_update(s, n)
_DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)(\([^)]*\))?((?:\\\r?\n|[^\n])*)", re.M)


def define_pairs(text):
    """{(macro, identifier)} for each identifier in each #define's body (not its parameters): a macro whose body
    names an old name that the maps rename to the macro's own name is the same thing after the rename."""
    out = set()
    for m in _DEFINE.finditer(text):
        params = set(IDENT.findall(m.group(2) or ""))
        body = re.sub(r"/\*.*?\*/|//.*", "", m.group(3))
        out |= {(m.group(1), x) for x in IDENT.findall(body) if x not in params}
    return out


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


CLASSES = ("idents", "nonmember", "func_def", "func_static", "func_decl", "func_use", "data_decl", "data_def",
           "types", "used", "local_decl", "macros")
_NOT_A_TYPE = _PREV_NOT_DECL | {"typedef", "extern", "struct", "union", "enum", "const", "volatile", "break",
                                "continue", "default"}
_PP_COND = re.compile(r"^[ \t]*#[ \t]*(if|ifdef|ifndef|elif|else|endif)\b")
_MACRO_NAME = re.compile(r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)", re.M)


def first_branches(text):
    """(active, inactive): the text with the lines of every #elif and #else branch blanked (each conditional's first
    branch kept: the PC branch of an #ifdef TARGET_PC, whose braces may not pair with its #else's), and those lines.
    The classifier reads the code structure of the first and only the identifiers of the second."""
    if "#" not in text or not re.search(r"^[ \t]*#[ \t]*(?:elif|else)\b", text, re.M):
        return text, ""
    active, inactive = [], []
    stack = []   # per open conditional: is the line in its first branch?
    for line in text.split("\n"):
        m = _PP_COND.match(line)
        if m:
            d = m.group(1)
            if d.startswith("if"):
                stack.append(True)
            elif d in ("elif", "else") and stack:
                stack[-1] = False
            elif d == "endif" and stack:
                stack.pop()
            active.append(line)
            continue
        if all(stack):
            active.append(line)
        else:
            active.append("")
            inactive.append(line)
    return "\n".join(active), "\n".join(inactive)


def classify_c(text):
    """What one C file says of its names: a dict of sets, CLASSES (identifiers in the code and in asm strings; those
    of them used otherwise than as a struct member (declared in a struct body, or after . or ->); functions defined,
    and those of them defined static; functions declared; functions called or declared; variables declared at file
    scope or by extern, and defined at file scope (not extern, not static); typedef names and struct, union and enum
    tags; names used: referenced in a function body or an initialiser, or defined; names declared as a local or a
    parameter; macros), plus `defines` ((macro, identifier) of each #define of one identifier) and `includes` (the
    base names of the files it includes). The structure is read from each conditional's first branch."""
    out = {c: set() for c in CLASSES}
    (idents, nonmember, func_def, func_static, func_decl, func_use, data_decl, data_def, types, used, local_decl,
     macros) = (out[c] for c in CLASSES)
    if "define" in text:
        out["defines"] = define_pairs(text)
        macros |= set(_MACRO_NAME.findall(text))
    else:
        out["defines"] = set()
    out["includes"] = includes = set()
    if "asm" in text or "alias" in text or "weakref" in text:
        idents |= asm_string_names(text)
        nonmember |= idents
        used |= idents
    text, inactive = first_branches(text)
    for inc, ident, lit, punct in _C_CODE.findall(inactive) if inactive else ():
        if ident:
            idents.add(ident)
            nonmember.add(ident)
    code = []   # (is identifier, text)
    for inc, ident, lit, punct in _C_CODE.findall(text):
        if ident:
            code.append((True, ident))
        elif punct:
            code.append((False, punct))
        elif lit:
            code.append((False, "0"))
        elif inc:
            includes.add(inc.rsplit("/", 1)[-1])
    n = len(code)
    brace = paren = 0
    in_typedef = in_extern = in_static = False   # the statement being read began with typedef / extern; has static
    stmt_brace = 0                               # the brace depth that statement began at
    kinds = []                                   # per open brace: is it a struct, union or enum body?
    stmt_at = 0                                  # where the statement being read began, in code
    for i in range(n):
        is_id, v = code[i]
        if not is_id:
            if v in ("{", "}", ";"):
                stmt_at = i + 1
            if v == "{":
                p1, p2 = code[i - 1][1] if i else "", code[i - 2][1] if i > 1 else ""
                kinds.append(p1 in ("struct", "union", "enum") or p2 in ("struct", "union", "enum"))
                brace += 1
                if not in_typedef and not in_extern:
                    stmt_brace = brace
            elif v == "}":
                if kinds:
                    kinds.pop()
                brace = max(0, brace - 1)
                if not in_typedef and not in_extern:
                    stmt_brace = brace
                    if brace == 0:
                        in_static = False
            elif v == "(":
                paren += 1
            elif v == ")":
                paren = max(0, paren - 1)
            elif v == ";" and brace <= stmt_brace:
                in_typedef = in_extern = False
                if brace == 0:
                    in_static = False
                stmt_brace = brace
                paren = 0
            continue
        idents.add(v)
        prev_id, prev = code[i - 1] if i else (False, "")
        prev2 = code[i - 2][1] if i > 1 else ""
        nxt = code[i + 1][1] if i + 1 < n else ""
        member = prev in (".", "->") or (kinds and kinds[-1] and paren == 0 and nxt in (";", ",", "[", ":"))
        if not member:
            nonmember.add(v)
        typeish = (prev_id and prev not in _NOT_A_TYPE) or prev == "*"
        if brace > 0 and not (kinds and kinds[-1]) and not member and not in_extern and not in_typedef:
            # in a function body or an initialiser: a local's declaration, or a use. A declaration statement starts
            # with a type and a name or a * (S16 total_drones[8], humans[8];), after static, const...
            j = stmt_at
            while j < n and code[j][1] in ("static", "const", "volatile", "register", "unsigned", "signed"):
                j += 1
            decl_stmt = (j + 1 < n and code[j][0] and code[j][1] not in _NOT_A_TYPE
                         and (code[j + 1][0] or code[j + 1][1] == "*"))
            if paren == 0 and nxt in (";", ",", "=", "[") and (typeish or (prev == "," and decl_stmt)) or (
                    prev == "*" and prev2 == "(" and nxt == ")" and paren == 1):
                local_decl.add(v)
            else:
                used.add(v)
        elif brace == 0 and paren >= 1 and typeish and nxt in (",", ")", "[") and not (
                prev == "*" and prev2 == "(" and paren == 1):
            local_decl.add(v)   # a parameter
        if v == "typedef" or v == "extern":
            if v == "typedef":
                in_typedef = True
            else:
                in_extern = True
            stmt_brace = brace
            continue
        if v == "static" and brace == 0:
            in_static = True
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
                    used.add(v)
                    if in_static:
                        func_static.add(v)
                else:
                    func_decl.add(v)
            continue
        if file_scope and paren == 0 and nxt in (";", ",", "[", "=") and (declarator or prev == ",") \
                and prev not in _PREV_NOT_DECL:
            data_decl.add(v)
            if brace == 0 and not in_extern and not in_static:
                data_def.add(v)
                used.add(v)
        elif file_scope and prev == "*" and prev2 == "(" and nxt == ")":
            data_decl.add(v)   # a function pointer variable: T (*name)(...)
            if brace == 0 and not in_extern and not in_static:
                data_def.add(v)
                used.add(v)
    return out


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


OVERLAY_BASE = 0x8038A400   # both runtime images load here: an address above it is one image's (n64syms.CODE)


class Tree:
    """What the tree says of its names: the C of SCAN_DIRS (identifiers in code, and the evidence of each name's
    kind) and the symbol tables. For the names in `watch` (the maps' new names, the labels to drop) also where they
    are: the files that use them (not as a member), use them in code (a reference or a definition), declare them as
    a local or a parameter, define them as functions (static or not), and the #defines that name them."""

    def __init__(self, root, files=None, watch=()):
        self.root = Path(root)
        self.files = files if files is not None else tracked_files(root)
        for c in CLASSES:
            setattr(self, c, set())
        self.includes_of = {}                            # file -> base names of the files it includes
        self.where = collections.defaultdict(list)       # watched name -> [file using it, not as a member]
        self.member_in = collections.defaultdict(list)   # watched name -> [file using it only as a struct member]
        self.used_in = collections.defaultdict(list)     # watched name -> [file referencing or defining it]
        self.local_in = collections.defaultdict(list)    # watched name -> [file declaring it a local or parameter]
        self.decl_in = collections.defaultdict(list)     # watched name -> [file declaring it at file scope]
        self.defined_in = collections.defaultdict(list)  # watched name -> [(file, static)] defining it
        self.defines = collections.defaultdict(set)      # file -> {(watched macro, identifier)}
        self.table_names = {}   # name -> where (a symbol table)
        self.table_kind = {}    # name -> func or data (symbol_addrs.us.txt's type:func, layout functions)
        self.table_addr = collections.defaultdict(set)   # name -> {(table, address)}
        self.func_addrs = set()  # the N64 addresses of functions (the layouts, the symbol file's type:func)
        self.symfile_names = set()   # the names symbol_addrs.us.txt defines
        watch = set(watch)
        rels = [f for f in self.files if f.startswith(tuple(d + "/" for d in SCAN_DIRS)) and f.endswith(C_EXTS)]
        for f, res in zip(rels, _classify_all([self.root / f for f in rels])):
            if not res:
                continue
            for c in CLASSES:
                getattr(self, c).update(res[c])
            self.includes_of[f] = res["includes"]
            if watch:
                for name in res["nonmember"] & watch:
                    self.where[name].append(f)
                for name in (res["idents"] - res["nonmember"]) & watch:
                    self.member_in[name].append(f)
                for name in res["used"] & watch:
                    self.used_in[name].append(f)
                for name in (res["func_decl"] | res["data_decl"] | res["func_def"]) & watch:
                    self.decl_in[name].append(f)
                for name in res["local_decl"] & watch:
                    self.local_in[name].append(f)
                for name in res["func_def"] & watch:
                    self.defined_in[name].append((f, name in res["func_static"]))
                for macro, target in res["defines"]:
                    if macro in watch:
                        self.defines[f].add((macro, target))
        self.included = self._reachable()
        self._tables()
        self.spellings = collections.defaultdict(set)   # norm(address name) -> its spellings in the C
        for x in self.idents:
            if ADDR_NAME.match(x):
                self.spellings[norm(x)].add(x)
        self.idents_norm = {norm(x) for x in self.idents}
        self._text_words = None

    def _reachable(self):
        """The base names of the headers some .c file includes, directly or through other headers."""
        by_base = collections.defaultdict(list)
        for f in self.includes_of:
            by_base[f.rsplit("/", 1)[-1]].append(f)
        seen, todo = set(), [b for f, inc in self.includes_of.items() if f.endswith(".c") for b in inc]
        while todo:
            b = todo.pop()
            if b in seen:
                continue
            seen.add(b)
            for f in by_base.get(b, ()):
                todo += self.includes_of.get(f, ())
        return seen

    def _tables(self):
        r = self.root
        for p in sorted((r / "asm").glob("us/*/symbols.json")) if (r / "asm").is_dir() else []:
            try:
                for name, addr in json.loads(p.read_text())["symbols"].items():
                    self.table_names.setdefault(name, str(p.relative_to(r)))
                    self.table_addr[name].add((p.parent.name, int(addr, 16)))
            except (ValueError, KeyError, OSError, TypeError):
                pass
        for name in ("symbol_addrs.us.txt", "undefined_syms_auto.us.txt", "undefined_funcs_auto.us.txt"):
            p = r / name
            if p.is_file():
                for line in p.read_text(errors="replace").splitlines():
                    m = re.match(r"^\s*([A-Za-z_]\w*)\s*=\s*0x([0-9A-Fa-f]+)\s*;(.*)$", line)
                    if m:
                        self.table_names.setdefault(m.group(1), name)
                        self.table_addr[m.group(1)].add(("symbol file", int(m.group(2), 16)))
                        if name == "symbol_addrs.us.txt":
                            self.symfile_names.add(m.group(1))
                        kind = "func" if "type:func" in m.group(3) or name == "undefined_funcs_auto.us.txt" else None
                        if kind:
                            self.table_kind.setdefault(m.group(1), kind)
                            self.func_addrs.add(int(m.group(2), 16))
        for p in sorted((r / "layout").glob("*.json")) if (r / "layout").is_dir() else []:
            try:
                for region in json.loads(p.read_text())["regions"]:
                    for e in region["entries"]:
                        if e.get("kind") == "function":
                            self.table_names.setdefault(e["target_id"], str(p.relative_to(r)))
                            self.table_kind.setdefault(e["target_id"], "func")
                            if isinstance(e.get("vaddr"), int):
                                self.table_addr[e["target_id"]].add((f"layout:{p.stem}", e["vaddr"]))
                                self.func_addrs.add(e["vaddr"])
            except (ValueError, KeyError, OSError):
                pass
        self.table_norm = {norm(x): x for x in self.table_names}

    def addresses(self, name, dropped=frozenset()):
        """{(table, N64 address)} the symbol tables give the name (less the (name, table) entries in dropped), with
        ("name", address) for an address name."""
        out = {(t, a) for t, a in self.table_addr.get(name, ()) if (name, t) not in dropped
               and not (t == "symbol file" and (name, SYMBOL_FILE_TABLE) in dropped)}
        a = address_of_name(name)
        if a is not None:
            out.add(("name", a))
        return out

    def aliases(self, a, b, dropped=frozenset()):
        """Are a and b names of one symbol? Some table (or an address name's spelling) gives both one address:
        below the runtime images, or in one image's table (both images load at 0x8038A400, so an address there
        alone says nothing). The tables don't always agree (draw_sprites at 0x8009539C in blob's, 0x800953A0 in
        boot_tail's); one address both share is enough."""
        x, y = self.addresses(a, dropped), self.addresses(b, dropped)
        for v in {v for _, v in x} & {v for _, v in y}:
            if v < OVERLAY_BASE or {t for t, w in x if w == v} & {t for t, w in y if w == v} - {"name", "symbol file"}:
                return True
        return False

    def address_of(self, name, dropped=frozenset()):
        """The address most of the tables give the name (for the reports)."""
        c = collections.Counter(v for _, v in self.addresses(name, dropped))
        return c.most_common(1)[0][0] if c else None

    def exists(self, name):
        k = norm(name)
        return k in self.idents_norm or k in self.table_norm

    def in_c(self, name):
        return norm(name) in self.idents_norm

    def has(self, name):
        """Is the name in the tree in this exact spelling (the C, or a symbol table)?"""
        return name in self.idents or name in self.table_names

    def in_text(self, name):
        """Is the name a word anywhere in the C (comments and strings too)? Read once, when first asked."""
        if self._text_words is None:
            self._text_words = set()
            for f in self.files:
                if f.startswith(tuple(d + "/" for d in SCAN_DIRS)) and f.endswith(C_EXTS):
                    text = read_text(self.root / f)
                    if text:
                        self._text_words.update(IDENT.findall(text))
        return name in self._text_words


def row_key(r):
    """What identifies a row in the ledger of applied rows: kind, old (an address name by norm), new."""
    old = r.old.replace("\\", "/").rstrip("/") if r.kind == "file" else (norm(r.old) if r.kind in IDENT_KINDS else r.old)
    return (r.kind, old, r.new.replace("\\", "/").rstrip("/") if r.kind == "file" else r.new)


def drop_key(d):
    """A drop row's key in the ledger: drop, label, table."""
    return ("drop", d.name, d.table)


def read_ledger(root):
    """The rows rename.py --apply applied (tools/port/names/applied.tsv): {row_key}."""
    p = Path(root) / MAPS_DIR / APPLIED_FILE
    out = set()
    if p.is_file():
        for line in p.read_text(encoding="utf-8").splitlines():
            f = line.split("\t")
            if len(f) >= 3 and not line.startswith("#"):
                out.add((f[0], f[1], f[2]))
    return out


def is_applied(r, tree, ledger=frozenset()):
    """Has the tree had this row applied already? It is in the ledger rename.py --apply writes (a chain's rows, A to
    B and B to C, can't be told by names); or its new name is there and its old one, in the spelling the row gives,
    is in no table and declared nowhere at file scope (a file row: the old file or folder is gone, the new one
    there). Such a row is checked for its format only: the maps stay in the tree after rename.py applied them."""
    if row_key(r) in ledger:
        return True
    if r.kind in IDENT_KINDS:
        # the old name may stay as another thing of the same spelling (a struct member, a local): applied when no
        # table has it and the C declares it nowhere at file scope
        declared = any(r.old in s for s in (tree.data_decl, tree.func_def, tree.func_decl, tree.types))
        return tree.has(r.new) and r.old not in tree.table_names and not declared
    if r.kind == "file":
        old, new = r.old.rstrip("/"), r.new.rstrip("/")
        return not (tree.root / old).exists() and (tree.root / new).exists()
    return False


def dropped_entries(drops, tree):
    """{(label, table)} the drop rows remove from the loader's tables."""
    out = set()
    for d in drops:
        tables = [t for t, _ in tree.table_addr.get(d.name, ())] if d.table == "*" else [d.table]
        out |= {(d.name, t) for t in tables if t in DROP_TABLES and t != "*"}
    return out


# ---------------------------------------------------------------------------------------------------- the checks

def check_rows(rows, tree, drops=(), ledger=frozenset()):
    """(errors, warnings, applied) of rows that passed the format check, against each other and the tree, with the
    drop rows applied first: [(where, msg)], and the rows the tree has had applied already (is_applied)."""
    errors, warnings = [], []
    applied = [r for r in rows if is_applied(r, tree, ledger)]
    done = {id(r) for r in applied}
    dropped = dropped_entries(drops, tree)
    olds = collections.defaultdict(list)
    news = collections.defaultdict(list)
    for r in rows:
        if r.kind in IDENT_KINDS:
            olds[("id", norm(r.old))].append(r)
            news[("id", r.new)].append(r)
        elif r.kind == "field":
            m = FIELD_OLD.match(r.old)
            olds[("field", m.group(1), int(m.group(2), 16))].append(r)
            news[("field", m.group(1), r.new)].append(r)
        else:
            olds[("file", r.old.rstrip("/"))].append(r)
            news[("file", r.new.rstrip("/"))].append(r)
    for key, rs in olds.items():
        for r in rs[1:]:
            if (r.new, r.kind) == (rs[0].new, rs[0].kind):   # two maps that agree (shared data): one rename
                warnings.append((r.where(), f"old {r.old} is in another row too ({rs[0].where()}), to the same "
                                            f"new name"))
            else:
                errors.append((r.where(), f"old {r.old} is in another row too ({rs[0].where()}: {rs[0].new})"))
    for key, rs in news.items():
        firsts = {}
        for r in rs:   # rows that agree count once
            firsts.setdefault(norm(r.old) if r.kind != "field" else r.old, r)
        rs = list(firsts.values())
        if len(rs) < 2:
            continue
        if key[0] == "id" and all(r.kind in ("func", "data") for r in rs):
            # many to one: names of one symbol (aliases at one address in the tables) become one name
            if any(id(r) in done for r in rs):
                continue
            if all(tree.aliases(rs[0].old, r.old, dropped) for r in rs[1:]):
                addr = tree.address_of(rs[0].old, dropped)
                warnings.append((rs[0].where(), f"new {rs[0].new}: {len(rs)} rows merge the aliases "
                                 f"{', '.join(r.old for r in rs)} (0x{addr:08X}) into one symbol "
                                 f"({', '.join(r.where() for r in rs[1:])})"))
                continue
        for r in rs[1:]:
            errors.append((r.where(), f"new {r.new} is used by another row too ({rs[0].where()}: {rs[0].old})"
                           + (", and the old names are no aliases at one address" if key[0] == "id" else "")))
    renamed_away = {k[1] for k in olds if k[0] == "id"}
    renames_to = {norm(r.old): r.new for r in rows if r.kind in IDENT_KINDS}
    new_names = {r.new for r in rows if r.kind in IDENT_KINDS}
    type_names = {x for r in rows if r.kind == "type" for x in (r.old, r.new)
                  if r.old in tree.types or r.new in tree.types}
    for r in rows:
        w = r.where()
        if r.kind in IDENT_KINDS or r.kind == "field":
            new = r.new if r.kind != "field" else re.match(r"\w+", r.new).group()
            if new in C_KEYWORDS:
                errors.append((w, f"new {new} is a C keyword"))
            # MAT3's names are kept verbatim (_PS_FlushFile): a reserved form is a warning there; libultra's own
            # __os names are its to have
            reserved = (errors if r.source == "code" else warnings)
            if new.startswith("__os"):
                pass
            elif new.startswith("__") or re.match(r"^_[A-Z]", new):
                reserved.append((w, f"new {new} is a reserved identifier (__ or _ and a capital)"))
            elif new.startswith("_") and r.kind != "field":
                reserved.append((w, f"new {new} starts with an underscore: reserved at file scope"))
            if r.kind != "field":
                if library_name(new):
                    errors.append((w, f"new {new} is a name of the C library, POSIX, zlib or SDL"))
                if new in WINDOWS_MACROS:
                    warnings.append((w, f"new {new} is a macro of Windows' headers"))
                if new.endswith("_t"):
                    warnings.append((w, f"new {new}: POSIX reserves the _t suffix"))
                if new.startswith(PC_PREFIXES):
                    warnings.append((w, f"new {new} has the PC layer's prefix (pc_, PC_)"))
            if ADDR_IN_NAME.search(r.new):
                errors.append((w, f"new {r.new} still has an address in it (PCONLY.md 3: no address in any name)"))
            words = set(re.split(r"[_\W]+", r.new.lower()))
            bad = sorted(words & BANNED_WORDS) or [x for x in BANNED_WORDS if re.search(
                r"(?:^|[a-z0-9])" + x.capitalize() + r"(?:[A-Z0-9]|$)", r.new)]
            if bad:
                warnings.append((w, f"new {r.new} uses {', '.join(bad)} (PCONLY.md 3: never unk, temp, data2, "
                                    f"helper)"))
        if id(r) in done:
            continue
        if r.kind in IDENT_KINDS:
            new, old = r.new, r.old
            if tree.exists(new) and norm(new) not in renamed_away:
                level, msg = collision(r, tree, renames_to, dropped)
                if level:
                    (errors if level == "error" else warnings).append((w, msg))
            if r.source == "code":
                if r.kind in ("func", "data") and not SNAKE.match(new):
                    warnings.append((w, f"new {new}: source code {r.kind} names are lower_snake_case"))
                if r.kind == "type" and not CAMEL.match(new):
                    warnings.append((w, f"new {new}: source code type names are CamelCase"))
            if not tree.exists(old):
                if tree.in_text(old):
                    warnings.append((w, f"old {old} is only in comments and strings"))
                else:
                    errors.append((w, f"old {old} exists nowhere in {', '.join(SCAN_DIRS)} or the symbol tables"))
                continue
            if not tree.in_c(old):
                warnings.append((w, f"old {old} is only in the symbol tables, not in the C"))
            others = sorted(tree.spellings.get(norm(old), set()) - {old})
            if others:
                warnings.append((w, f"old {old} is also spelled {', '.join(others)} in the C: another identifier, "
                                    f"which rename.py leaves (gen_stubs.py links it by address)"))
            if old in tree.macros:
                continue   # a macro: no kind of its own to check
            spellings = {old} if old in tree.idents else set(tree.spellings.get(norm(old), ()))
            tk = tree.table_kind.get(tree.table_norm.get(norm(old), ""), None)
            is_func = any(x in tree.func_def or x in tree.func_decl or x in tree.func_use for x in spellings)
            is_data = any(x in tree.data_decl for x in spellings)
            is_type = any(x in tree.types for x in spellings)
            defined_func = any(x in tree.func_def for x in spellings)
            at_function = bool({v for _, v in tree.addresses(old)} & tree.func_addrs)
            if r.kind == "func":
                if not is_func and tk != "func" and is_type:
                    errors.append((w, f"old {old} is a type in the C, not a function"))
                elif not is_func and tk != "func" and is_data and at_function:
                    warnings.append((w, f"old {old} is declared as data in the C at a function's address (its "
                                        f"address taken): renamed as the function"))
                elif not is_func and tk != "func" and is_data:
                    errors.append((w, f"old {old} is a variable in the C, not a function"))
                elif not is_func and tk != "func" and tree.in_c(old):
                    warnings.append((w, f"old {old}: the C neither defines, declares nor calls it as a function"))
            elif r.kind == "data":
                if defined_func and not is_data:
                    errors.append((w, f"old {old} is defined as a function in the C, not data"))
                elif is_type and not is_data:
                    errors.append((w, f"old {old} is a type in the C, not data"))
                elif tk == "func" and not is_data:
                    warnings.append((w, f"old {old} is a function in the symbol tables (a second label of a "
                                        f"function's address?), not data"))
                elif not is_data and is_func:
                    warnings.append((w, f"old {old}: the C calls or declares it as a function"))
            elif r.kind == "type" and not is_type:
                errors.append((w, f"old {old} is no typedef name or struct, union or enum tag in the C"))
        elif r.kind == "field":
            # the struct by its C tag or typedef, or by either name a type row gives it; a MAT3 name (tWindowInfo)
            # is the hand edit's to find
            struct = FIELD_OLD.match(r.old).group(1)
            if struct not in tree.types and struct not in type_names:
                warnings.append((w, f"struct {struct} of {r.old} is no type of the C (by that name)"))
        elif r.kind == "file":
            old, new = r.old.replace("\\", "/").rstrip("/"), r.new.replace("\\", "/").rstrip("/")
            if not (tree.root / old).exists():
                errors.append((w, f"old file or folder {old} doesn't exist"))
            if (tree.root / new).exists():
                errors.append((w, f"new file or folder {new} exists already"))
            if ADDR_IN_NAME.search(Path(new).stem):
                errors.append((w, f"new {new} still has an address in its name"))
            if Path(new).suffix in (".c", ".h") and norm(Path(new).stem) in renames_to \
                    and Path(new).stem not in new_names:   # a chain's middle (no_catchup) is a name again
                warnings.append((w, f"new file {new} is named after {Path(new).stem}, which a map renames to "
                                    f"{renames_to[norm(Path(new).stem)]}: rename.py would move it again"))
            if (tree.root / old).is_file() and Path(old).suffix != Path(new).suffix:
                warnings.append((w, f"file {old} changes its extension"))
    return errors, warnings, applied


def collision(r, tree, renames_to, dropped=frozenset()):
    """('error', 'warning' or None, message) for a row whose new name the tree has already. Not a collision: a name
    the drop list takes out of the tables (and nothing else has); the new name is already a name of the old one's
    address (an alias: one symbol afterwards); in the C only as `#define NEW OLD` (an identity after the rename), in
    a header no file includes, as a struct member, or as a local or parameter. Otherwise say where it is: the
    symbol table, the files that define it (static or not), the files that use it."""
    new, old = r.new, r.old
    if r.kind == "type" and new in tree.types and not ({new} & (tree.func_decl | tree.func_def | tree.data_decl)):
        # a view's type named after the record type other files define (Rec164 is eggs.c's Egg): types are each
        # file's own, so two definitions of one name are no collision unless one file sees both
        files = tree.where.get(new, [])
        return "warning", (f"new {new} is a type the C has already ({', '.join(files[:2])}"
                           + (f" +{len(files) - 2}" if len(files) > 2 else "") + "): one name for two definitions "
                           "in different files; check they agree, and that no file sees both")
    if tree.aliases(old, new, dropped):
        addr = tree.address_of(old, dropped)
        return "warning", f"new {new} is already a name of {old}'s address 0x{addr:08X} (an alias): one symbol afterwards"
    benign, bad = [], []
    for f in tree.where.get(new, ()):
        targets = {t for m, t in tree.defines.get(f, ()) if m == new}
        if any(t == new or renames_to.get(norm(t)) == new for t in targets):
            benign.append(f"#define {new} {sorted(targets)[0]} in {f}")
        elif f.endswith(".h") and Path(f).name not in tree.included:
            benign.append(f"{f}, which no .c file includes")
        elif f in tree.local_in.get(new, ()) and f not in tree.decl_in.get(new, ()):
            benign.append(f"a local or parameter in {f}")
        else:
            bad.append(f)
    benign += [f"a struct member in {f}" for f in tree.member_in.get(new, ())[:2]]
    tables = sorted({t for t, _ in tree.addresses(new, dropped) if t != "name"})
    if bad and not tables and new not in tree.func_def and new not in tree.data_def and new not in tree.types \
            and new not in tree.macros:
        return "warning", (f"new {new} is in the C already, declared but nowhere defined and in no table "
                           f"({', '.join(bad[:2])}{f' +{len(bad) - 2}' if len(bad) > 2 else ''}): after the rename "
                           f"it is {old}'s symbol (check the declarations agree)")
    if not bad and not tables:
        if not benign:
            return None, ""   # only a dropped label: the drop list frees the name
        return "warning", (f"new {new} is in the C only as {'; '.join(benign[:3])}"
                           + (f" (+{len(benign) - 3})" if len(benign) > 3 else "")
                           + " (not a collision; after the rename drop the #define, fix or drop the header)")
    where = []
    statics = [f for f, st in tree.defined_in.get(new, ()) if st]
    defs = [f for f, st in tree.defined_in.get(new, ()) if not st]
    if statics:
        where.append(f"a static function in {', '.join(statics[:4])}" + (f" (+{len(statics) - 4})" if len(statics) > 4
                                                                          else ""))
    if defs:
        where.append(f"defined in {', '.join(defs[:4])}" + (f" (+{len(defs) - 4})" if len(defs) > 4 else ""))
    used = [f for f in bad if f not in statics and f not in defs]
    if used:
        where.append(f"used in {', '.join(used[:3])}" + (f" (+{len(used) - 3} files)" if len(used) > 3 else ""))
    if tables:
        where.append(f"named in the tables ({', '.join(tables)}) at "
                     f"{', '.join(sorted({f'0x{v:08X}' for t, v in tree.addresses(new, dropped) if t != 'name'}))}")
    if benign:
        where.append(f"and harmlessly {'; '.join(benign[:2])}")
    return "error", f"new {new} already exists: {'; '.join(where) or 'in the C'}"


NAME_TOOLS = ("tools/port/rename.py", "tools/port/names_check.py", "tools/port/tests/test_rename.py",
              "tools/port/tests/test_names_check.py")   # these tools' own examples are no uses


def tool_names(root, files, names):
    """{name: first file} for the names the tools use: tools/**/*.py's identifiers and their string literals that
    are a name (lookups by name), not their comments, docstrings or messages (prose)."""
    import ast
    out = {}
    for f in files:
        if not (f.startswith("tools/") and f.endswith(".py")) or f in NAME_TOOLS or f.startswith("tools/port/tests/names/"):
            continue
        try:
            text = (Path(root) / f).read_text(encoding="utf-8")
            if not any(n in text for n in names):
                continue
            tree = ast.parse(text)
        except (OSError, SyntaxError, UnicodeDecodeError, ValueError):
            continue
        docstrings = set()
        for node in ast.walk(tree):
            if isinstance(node, (ast.Module, ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)) and node.body:
                first = node.body[0]
                if isinstance(first, ast.Expr) and isinstance(getattr(first, "value", None), ast.Constant):
                    docstrings.add(id(first.value))
        for node in ast.walk(tree):
            if isinstance(node, ast.Name):
                found = {node.id} & names
            elif isinstance(node, ast.Attribute):
                found = {node.attr} & names
            elif isinstance(node, ast.Constant) and isinstance(node.value, str) and id(node) not in docstrings:
                found = {node.value.strip()} & names   # a lookup by name: the string is the name, not prose
            else:
                continue
            for x in found:
                out.setdefault(x, f)
    return out


def check_drops(drops, rows, tree):
    """(errors, warnings, dropped) of the drop rows. Errors: a label in no row of a map's old names, in the table
    named (or in some table, for *), and not used: by the C of src/, include/, port/ and nonmatching/ (referenced in
    a function body or an initialiser, or defined: not declared only, not a member, not an identity #define) or by
    the tools' Python (code and strings). A label a map gives to another address is dropped first, so its name is
    free (NAMING.md 7). Declarations left of a dropped label are one warning (wave 2 deletes them). A label in no
    table at all and used nowhere was dropped already."""
    errors, warnings, dropped = [], [], []
    seen = {}
    old_names = {norm(r.old): r for r in rows if r.kind in IDENT_KINDS}
    new_names = {r.new: r for r in rows if r.kind in IDENT_KINDS}
    renames_to = {norm(r.old): r.new for r in rows if r.kind in IDENT_KINDS}
    used_by_tools = tool_names(tree.root, tree.files, {d.name for d in drops})
    declared_only = []
    for d in drops:
        w = d.where()
        tables = {t for t, _ in tree.table_addr.get(d.name, ()) if t in DROP_TABLES}
        if d.table == SYMBOL_FILE_TABLE:   # its own kind of table: the label's line, or none
            tables = {SYMBOL_FILE_TABLE} if d.name in tree.symfile_names else set()
        for key in ((d.name, d.table), (d.name, "*")) if d.table != "*" else [(d.name, t) for t in DROP_TABLES]:
            if key in seen:
                errors.append((w, f"{d.name} is dropped by another row too ({seen[key]})"))
                break
        seen[(d.name, d.table)] = w
        if norm(d.name) in old_names:
            errors.append((w, f"{d.name} is also renamed by a map ({old_names[norm(d.name)].where()})"))
        users = []
        for f in tree.used_in.get(d.name, ()):
            targets = {t for m, t in tree.defines.get(f, ()) if m == d.name}
            if not any(renames_to.get(norm(t)) == d.name or t == d.name for t in targets):
                users.append(f)
        drop_addrs = {v for t, v in tree.table_addr.get(d.name, ()) if t in DROP_TABLES}
        kept = set() if d.table == SYMBOL_FILE_TABLE else (
            {v for t, v in tree.table_addr.get(d.name, ()) if t == "symbol file"} & drop_addrs)
        renamed_to = d.name in new_names   # the maps give the name to another symbol: the uses will name that
        if users and (kept or renamed_to):
            warnings.append((w, f"{d.name} is used in the C ({', '.join(users[:2])}"
                                + (f" +{len(users) - 2}" if len(users) > 2 else "") + "), but "
                                + ("the symbol file keeps its address, which the tools read" if kept else
                                   f"a map gives the name to another symbol ({new_names[d.name].where()})")))
        elif users:
            errors.append((w, f"{d.name} is still used in the C: {', '.join(users[:3])}"
                              + (f" (+{len(users) - 3})" if len(users) > 3 else "")
                              + ", and no symbol file keeps its address: it can't leave the tables"))
        elif d.name in used_by_tools and not renamed_to:
            errors.append((w, f"{d.name} is still used by {used_by_tools[d.name]}: it can't leave the tables"))
        elif tree.where.get(d.name):
            declared_only.append((d, tree.where[d.name]))
        if not tables:
            if not users and d.name not in used_by_tools:
                dropped.append(d)
        elif d.table != "*" and d.table not in tables:
            errors.append((w, f"{d.name} is not in {d.table}'s table (it is in {', '.join(sorted(tables))})"))
    if declared_only:
        names = sorted({d.name for d, _ in declared_only})
        d, files = declared_only[0]
        warnings.append((d.where(), f"{len(names)} labels to drop are still declared (not used) in the C, e.g. "
                                    f"{d.name} in {files[0]} (+{len(files) - 1}): wave 2 deletes the declarations"))
    return errors, warnings, dropped


def run_checks(root, paths, tree=None):
    """(rows, errors, warnings, applied, drops, dropped) for the maps and drop files at paths: the map rows, the
    errors and warnings as (where, message), the rows applied already, the drop rows and those dropped already."""
    maps, drop_files = split_paths(paths)
    rows, errors = read_maps(maps)
    drops, drop_errors = read_drops(drop_files)
    errors += drop_errors
    warnings, applied, dropped = [], [], []
    good = []
    for r in rows:
        bad = check_format(r)
        errors += [(r.where(), m) for m in bad]
        if not bad:
            good.append(r)
    if good or drops:
        tree = tree or Tree(root, watch={r.new for r in good if r.kind in IDENT_KINDS} | {d.name for d in drops})
    if good:
        errors_, warnings_, applied = check_rows(good, tree, drops, read_ledger(root))
        errors += errors_
        warnings += warnings_
    ledger = read_ledger(root)
    done = [d for d in drops if drop_key(d) in ledger]   # dropped by an --apply: not checked again
    drops = [d for d in drops if drop_key(d) not in ledger]
    if drops:
        errors_, warnings_, dropped = check_drops(drops, good, tree)
        errors += errors_
        warnings += warnings_
    return rows, errors, warnings, applied, drops + done, dropped + done


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("maps", nargs="*", help="maps and drop files to check (default: every tools/port/names/*.tsv)")
    ap.add_argument("--root", type=Path, default=ROOT, help="the top of the tree (default: this checkout)")
    ap.add_argument("--strict", action="store_true", help="warnings fail too")
    ap.add_argument("--quiet", action="store_true", help="the summary and the errors only")
    a = ap.parse_args(argv)
    paths = map_paths(a.root, a.maps)
    if not paths:
        print(f"names_check: no maps in {a.root / MAPS_DIR}")
        return 0
    rows, errors, warnings, applied, drops, dropped = run_checks(a.root, paths)
    for where, msg in sorted(errors, key=lambda x: _where_key(x[0])):
        print(f"{where}: error: {msg}")
    if not a.quiet:
        for where, msg in sorted(warnings, key=lambda x: _where_key(x[0])):
            print(f"{where}: warning: {msg}")
    maps, drop_files = split_paths(paths)
    kinds = collections.Counter(r.kind for r in rows)
    print(f"names_check: {len(maps)} map{'s' if len(maps) != 1 else ''}, {len(rows)} rows "
          f"({', '.join(f'{k} {kinds[k]}' for k in KINDS if kinds[k])}"
          + (f"; {len(applied)} applied already" if applied else "") + ")"
          + (f", {len(drops)} labels to drop" + (f" ({len(dropped)} dropped already)" if dropped else "")
             if drop_files else "")
          + f": {len(errors)} errors, {len(warnings)} warnings")
    return 1 if errors or (a.strict and warnings) else 0


def _where_key(where):
    path, _, line = where.rpartition(":")
    return (path, int(line)) if line.isdigit() else (where, 0)


if __name__ == "__main__":
    sys.exit(main())
