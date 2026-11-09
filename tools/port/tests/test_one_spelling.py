#!/usr/bin/env python3
"""one_spelling.py on a made-up tree: another spelling becomes the header's name (checked by value), a file's copy of
a header's constant goes with its comment, a copy with another value stays and is reported, a file's abbreviation of
a shared conversion is inlined token for token, a number that stands for a named value becomes the name, and a second
run changes nothing (--check).

    python3 tools/port/tests/test_one_spelling.py

Stdlib only."""
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
TOOL = HERE.parent / "one_spelling.py"

FILES = {
    "port/include/pc_prelude.h": """\
#define PC_N64W(x) ((unsigned int)(unsigned long)(x))
""",
    "include/game/game.h": """\
#ifndef GAME_GAME_H
#define GAME_GAME_H
/* gGameState's bits */
#define GS_DEMO 0x8          /* the demo */
#define GS_RACING 0x400000
/* gRace */
enum { kOneRace = 0, kPractice = 1, kBattle = 6 };
extern unsigned int gGameState;
extern int gRace;
#endif
""",
    "src/blob/a.c": """\
/* a.c: a file with its own copies. */
#include "game/game.h"

/* gGameState: the demo. */
#define GAME_STATE_ATTRACT 0x8
#define kBattle 6     /* the race kinds (MAT3) */

#define LOCAL_ONLY 3

#define N64W(x) PC_N64W(x)

struct S { unsigned int f; };

int a(struct S *s, void *q)
{
    s->f = N64W(q);
    if (gGameState & GAME_STATE_ATTRACT) return LOCAL_ONLY;
    if (gRace == 6) return 1;   /* kBattle */
    return (gGameState & 0x400008) != 0;
}
""",
    "src/blob/b.c": """\
/* b.c: a copy that disagrees. */
#include "game/game.h"

#define GS_DEMO 0x9

int b(void) { return GS_DEMO; }
""",
    "tables/spellings.tsv": "old\tnew\told_value\tnote\nGAME_STATE_ATTRACT\tGS_DEMO\t0x8\ta.c\n",
    "tables/inline.tsv": "name\tbody_regex\tcomment_name\tnote\nN64W\tPC_N64W\\(x\\)\tPC_N64W\tthe prelude's\n",
    "tables/literals.tsv": "pattern\tnames\tnote\n"
                           "\\bgRace\\s*(?:==|!=)\\s*(?P<lit>[0-9]+)\\b\tk(?:OneRace|Practice|Battle)\tgRace\n"
                           "\\bgGameState\\s*&\\s*(?P<lit>0x[0-9A-Fa-f]+|[0-9]+)\\b\tGS_\\w+\tgGameState\n",
}

WANT_A = """\
/* a.c: a file with its own copies. */
#include "game/game.h"

#define LOCAL_ONLY 3

struct S { unsigned int f; };

int a(struct S *s, void *q)
{
    s->f = PC_N64W(q);
    if (gGameState & GS_DEMO) return LOCAL_ONLY;
    if (gRace == kBattle) return 1;
    return (gGameState & (GS_RACING | GS_DEMO)) != 0;
}
"""


def run(root, *args):
    return subprocess.run([sys.executable, str(TOOL), "--root", str(root), "--tables", str(root / "tables"), *args],
                          capture_output=True, text=True)


def main():
    fails = []
    with tempfile.TemporaryDirectory() as d:
        root = Path(d)
        for rel, text in FILES.items():
            (root / rel).parent.mkdir(parents=True, exist_ok=True)
            (root / rel).write_text(text)
        r = run(root, "--check")
        if r.returncode != 1:
            fails.append(f"--check before: exit {r.returncode}, not 1")
        r = run(root, "--apply")
        if r.returncode != 0:
            fails.append(f"--apply: exit {r.returncode}\n{r.stdout}{r.stderr}")
        got = (root / "src/blob/a.c").read_text()
        if got != WANT_A:
            fails.append("a.c:\n" + got)
        if (root / "src/blob/b.c").read_text() != FILES["src/blob/b.c"]:
            fails.append("b.c changed: its GS_DEMO is another value")
        if "left: src/blob/b.c:4: GS_DEMO is `0x9`" not in r.stdout:
            fails.append("the disagreeing copy isn't reported:\n" + r.stdout)
        r = run(root, "--check")
        if r.returncode != 0:
            fails.append(f"--check after: exit {r.returncode}\n{r.stdout}")
    for f in fails:
        print("FAIL:", f)
    print("test_one_spelling:", "FAIL" if fails else "ok")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
