/* port/data/test_reach.c: the loader's rules for tables only pointers reach (pc_data.h's PcDataReach,
 * port/data/reached.txt, docs/port/DATA.md 7) on a synthetic image, with hand-written tables.
 *
 * One raw image at TEST_GAME whose arena (its first 0x200 bytes) holds, in N64 form, records no symbol describes:
 * sequences {u16 count; u16 id; f32 time; Frame *frames} (12 bytes, 16 on the host) ended by a count of 0, each
 * with `count` frames {char *name; u32 value} (8 bytes, 16 on the host), and arrays of s16 whose lengths are a
 * table of u16. Own storage: the table of sequence pointers `seqs` (3: two sequence tables, the second one twice, and
 * NULL), the table of s16 pointers `pts` (2), and `shared`, a Frame[2] that one sequence's frames point into.
 *
 * Checks: the sequences and frames are converted to host storage of their own, with the terminator included and
 * the pointers rewritten; a table two pointers reach is converted once; frames in `shared` resolve to it; the s16
 * arrays are swapped in place in the arena, by their lengths; the report lists the tables; a reload makes them
 * again. */
#include "pc_data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The synthetic layout's N64 addresses, KSEG0 addresses of the test's own: the image (GAME(o): o bytes into it) and
 * the heap's start. */
#define TEST_GAME (N64_KSEG0 + 0x1000u)
#define TEST_HEAP (N64_KSEG0 + 0x20000u)
#define GAME(o) (TEST_GAME + (o))

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

typedef struct Frame { const char *name; uint32_t value; } Frame;
typedef struct Seq { uint16_t count, id; float time; Frame *frames; } Seq;

static uint8_t rom[0x1000];
static uint8_t arena[0x200];
static Seq *seqs[3];
static int16_t *pts[2];
static Frame shared[2];

static void put32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = (uint8_t)v; }
static void put16(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = (uint8_t)v; }
static uint32_t fbits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

static PcDataImage images[1] = {{"game", PC_IMG_RAW, 0x0, 0x400, 0x400, TEST_GAME}};
static PcDataArena arenas[1] = {{0, TEST_GAME, GAME(0x200), GAME(0x200), arena, 0, 0}};

static const PcDataRun runs[] = {
    /* 0 seqs, 1 pts: pointers */
    {0, 0, 3, 1, 0, 0, PC_K_PTR}, {0, 0, 2, 1, 0, 0, PC_K_PTR},
    /* 2-3 shared: Frame[2] */
    {0, 0, 1, 2, 8, sizeof(Frame), PC_K_PTR}, {4, 8, 1, 2, 8, sizeof(Frame), PC_K_U32},
    /* 4-6 a Seq */
    {0, 0, 2, 1, 0, 0, PC_K_U16}, {4, 4, 1, 1, 0, 0, PC_K_F32}, {8, 8, 1, 1, 0, 0, PC_K_PTR},
    /* 7-8 a Frame */
    {0, 0, 1, 1, 0, 0, PC_K_PTR}, {4, 8, 1, 1, 0, 0, PC_K_U32},
    /* 9 an s16 */
    {0, 0, 1, 1, 0, 0, PC_K_U16},
};

static const PcDataSym syms[] = {
    {"seqs", GAME(0x200), 12, sizeof seqs, seqs, 0, 1, 0, PC_SYM_OWN},
    {"pts", GAME(0x210), 8, sizeof pts, pts, 1, 1, 0, PC_SYM_OWN},
    {"shared", GAME(0x220), 16, sizeof shared, shared, 2, 2, 0, PC_SYM_OWN},
};
static const uint32_t compat_frames[] = {2};
static const PcDataReach reach[] = {
    {"seqs -> Seq", 0, -1, 0, 0, 3, 12, sizeof(Seq), 4, 3, PC_REACH_UNTIL, 1, 0, 2, 0, 1, NULL, 0},
    {"Seq.frames -> Frame", -1, 0, 8, offsetof(Seq, frames), 0, 8, sizeof(Frame), 7, 2, PC_REACH_PARENT, 1, 0, 2, 0,
     1, compat_frames, 1},
    {"pts -> s16", 1, -1, 0, 0, 2, 2, 2, 9, 1, PC_REACH_TABLE, 1, 0, 2, GAME(0x230), 0, NULL, 0},
};

static PcDataState state;
static const PcDataTables tables = {images, 1, arenas, 1, syms, 3, runs, sizeof runs / sizeof runs[0], NULL, 0,
                                    NULL, 0, TEST_HEAP, &state, NULL, 0, reach, 3};

static uint8_t *at(uint32_t a) { return rom + (a - TEST_GAME); }

static void build_rom(void)
{
    /* Sequence table A at TEST_GAME: two sequences and the terminator. */
    put16(at(TEST_GAME), 2), put16(at(GAME(0x2)), 7), put32(at(GAME(0x4)), fbits(0.5f));
    put32(at(GAME(0x8)), GAME(0x100));                   /* frames of its own */
    put16(at(GAME(0xC)), 1), put16(at(GAME(0xE)), 8), put32(at(GAME(0x10)), fbits(1.5f));
    put32(at(GAME(0x14)), GAME(0x228));                   /* shared[1] */
    put16(at(GAME(0x18)), 0), put16(at(GAME(0x1A)), 9);   /* the end: count 0, the rest kept */
    /* Sequence table B at GAME(0x40): one sequence, the terminator. */
    put16(at(GAME(0x40)), 1), put16(at(GAME(0x42)), 3), put32(at(GAME(0x44)), fbits(2.0f));
    put32(at(GAME(0x48)), GAME(0x100));                   /* the same frames as A's first: converted once */
    /* Frames at GAME(0x100): two, named by strings at GAME(0x180). */
    put32(at(GAME(0x100)), GAME(0x180)), put32(at(GAME(0x104)), 0x11);
    put32(at(GAME(0x108)), GAME(0x188)), put32(at(GAME(0x10C)), 0x22);
    memcpy(at(GAME(0x180)), "ONE", 4), memcpy(at(GAME(0x188)), "TWO", 4);
    /* s16 arrays at GAME(0x140) (3) and GAME(0x150) (2), and their lengths at GAME(0x230) (u16[2]). */
    put16(at(GAME(0x140)), 0x0102), put16(at(GAME(0x142)), 0x0304), put16(at(GAME(0x144)), 0x0506);
    put16(at(GAME(0x148)), 0x7777);                       /* after the first array: not swapped */
    put16(at(GAME(0x150)), 0xFFFE), put16(at(GAME(0x152)), 0x0010);
    /* Own storage: seqs, pts, shared; the lengths. */
    put32(at(GAME(0x200)), TEST_GAME), put32(at(GAME(0x204)), GAME(0x40)), put32(at(GAME(0x208)), 0);
    put32(at(GAME(0x210)), GAME(0x140)), put32(at(GAME(0x214)), GAME(0x150));
    put32(at(GAME(0x220)), GAME(0x180)), put32(at(GAME(0x224)), 0x33);
    put32(at(GAME(0x228)), GAME(0x188)), put32(at(GAME(0x22C)), 0x44);
    put16(at(GAME(0x230)), 3), put16(at(GAME(0x232)), 2);
}

static void check_load(int pass)
{
    Seq *a = seqs[0], *b = seqs[1];
    CHECK(a != NULL && b != NULL && seqs[2] == NULL);
    if (!a || !b)
        return;
    /* converted, not the arena's bytes */
    CHECK((uint8_t *)a < arena || (uint8_t *)a >= arena + sizeof arena);
    CHECK(a[0].count == 2 && a[0].id == 7 && a[0].time == 0.5f);
    CHECK(a[1].count == 1 && a[1].id == 8 && a[1].time == 1.5f);
    CHECK(a[2].count == 0 && a[2].id == 9);   /* the terminator is part of the table */
    CHECK(a[1].frames == &shared[1] && shared[1].value == 0x44);   /* frames in `shared` resolve to it */
    Frame *f = a[0].frames;
    CHECK(f != NULL && f != shared);
    if (f) {
        CHECK(f[0].value == 0x11 && f[1].value == 0x22);
        CHECK((uint8_t *)f[0].name == arena + 0x180 && strcmp(f[0].name, "ONE") == 0);
        CHECK(f[1].name && strcmp(f[1].name, "TWO") == 0);
    }
    CHECK(b[0].count == 1 && b[0].id == 3 && b[0].time == 2.0f && b[1].count == 0);
    CHECK(b[0].frames == f);   /* one table, converted once */
    /* in place: the arrays' elements swapped, by their lengths, and nothing after them */
    CHECK((uint8_t *)pts[0] == arena + 0x140 && (uint8_t *)pts[1] == arena + 0x150);
    CHECK(pts[0][0] == 0x0102 && pts[0][1] == 0x0304 && pts[0][2] == 0x0506);
    CHECK(arena[0x148] == 0x77 && arena[0x149] == 0x77);
    CHECK(pts[1][0] == -2 && pts[1][1] == 0x10);
    const PcDataStats *st = pc_data_stats();
    if (pass == 0)
        CHECK(st->reached == 5 && st->reached_elems == 3 + 2 + 2 + 3 + 2);   /* A, B, the frames, two arrays */
}

/* Whether the report holds the line `fmt` gives with the N64 address a. */
static int report_has(const char *report, const char *fmt, uint32_t a)
{
    char want[128];
    snprintf(want, sizeof want, fmt, a);
    return strstr(report, want) != NULL;
}

int main(void)
{
    build_rom();
    CHECK(pc_data_load_all(&tables, rom, sizeof rom, "pc_reach_test_report.txt") == 0);
    check_load(0);
    FILE *fp = fopen("pc_reach_test_report.txt", "r");
    char buf[4096] = {0};
    if (fp) {
        fread(buf, 1, sizeof buf - 1, fp);
        fclose(fp);
    }
    CHECK(report_has(buf, "reached seqs -> Seq 0x%08X: 3 elements, converted to storage of its own", TEST_GAME));
    CHECK(report_has(buf, "reached Seq.frames -> Frame 0x%08X: 2 elements, converted", GAME(0x100)));
    CHECK(report_has(buf, "reached pts -> s16 0x%08X: 2 elements, swapped in place", GAME(0x150)));
    /* A reload (as an overlay's) makes them again from the image: what the game wrote into the copies goes. */
    seqs[0][0].count = 99;
    CHECK(pc_data_load_image(&tables, 0) == 0);
    check_load(1);
    if (failures)
        fprintf(stderr, "pc_reach_test: %d failures\n", failures);
    else
        fprintf(stderr, "pc_reach_test: all checks passed\n");
    return failures != 0;
}
