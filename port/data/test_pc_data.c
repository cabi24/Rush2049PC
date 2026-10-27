/* port/data/test_pc_data.c: the loader (pc_data.c) on a synthetic ROM, with hand-written tables.
 *
 * Four images as in the game: a raw boot slice, a DEFLATE game image, and two DEFLATE runtime images at one N64
 * base whose functions share an address (image B's data lies in image A's code range). Checks: byte order per
 * kind, a struct whose host layout differs (pointers, a 32-bit long), arrays of structs (run strides), pointers
 * into own storage (offset mapping), arenas, functions per image, functions with no C, ROM offsets kept raw,
 * unresolved pointers, an overlay reload restoring data, BSS and game-C data, and the address-like word report.
 *
 * It runs twice: with the arenas in host memory, and at their N64 addresses in an RDRAM window it maps at host
 * 0x80000000, as the game's are. There a pointer into an arena is its own N64 value, and a runtime image's arena,
 * which lies in the "heap", stays clear until the image is loaded. */
#define _GNU_SOURCE
#include "pc_data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <zlib.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void game_fn(void) {}
static void ovl_a_fn(void) {}
static void ovl_b_fn(void) {}

static uint8_t rom[0x8000];

static void put32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void put16(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = (uint8_t)v; }

static uint32_t deflate_to(uint32_t rom_off, const uint8_t *src, uint32_t size)
{
    z_stream z;
    memset(&z, 0, sizeof z);
    deflateInit2(&z, 9, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    z.next_in = (Bytef *)src;
    z.avail_in = size;
    z.next_out = rom + rom_off;
    z.avail_out = sizeof rom - rom_off;
    deflate(&z, Z_FINISH);
    uint32_t n = (uint32_t)z.total_out;
    deflateEnd(&z);
    return n;
}

/* Images: boot raw at ROM 0x1000 (0x80001000, 0x100 B); game at ROM 0x2000 (0x80010000, 0x200 B); A at 0x4000 and
 * B at 0x5000 (both 0x80300000). */
static PcDataImage images[4] = {
    {"boot", PC_IMG_RAW, 0x1000, 0x100, 0x100, 0x80001000},
    {"game", PC_IMG_DEFLATE, 0x2000, 0, 0x200, 0x80010000},
    {"ovl_a", PC_IMG_DEFLATE, 0x4000, 0, 0x100, 0x80300000},
    {"ovl_b", PC_IMG_DEFLATE, 0x5000, 0, 0x80, 0x80300000},
};

/* The arenas' host memory: these arrays, or the window. Image B's (0x80300040-0x80300080) is inside image A's
 * code; the two runtime images' arenas are disjoint, as the game's are. */
static uint8_t h_boot[0xC0], h_game[0x200], h_ovl_a[0x100], h_ovl_b[0x40];
static uint8_t *a_boot, *a_game, *a_ovl_a, *a_ovl_b;
static PcDataArena arenas[4] = {
    {0, 0x80001080, 0x80001100, 0x80001140, NULL, 0, 4},
    {1, 0x80010100, 0x80010200, 0x80010300, NULL, 4, 1},
    {2, 0x80300080, 0x80300100, 0x80300180, NULL, 5, 1},
    {3, 0x80300040, 0x80300060, 0x80300080, NULL, 0, 0},
};

/* Own storage. S: { u32 a; void *p; u16 h; void (*fp)(void); long l; } (N64 20 B, host 40 B).
 * P: void *[11]. Q: { u32 x; void *y; }[2] (N64 16 B, host 32 B). A1, B1: void *. */
typedef struct { uint32_t a; void *p; uint16_t h; void (*fp)(void); int64_t l; } S_t;
typedef struct { uint32_t x; void *y; } Q_t;
static S_t S;
static void *P[11];
static Q_t Q[2];
static void *A1, *B1;
static uint32_t cvar;   /* game-C data of image A */

static const PcDataRun runs[] = {
    /* 0-3 boot arena: u16[2] @0, u32 @4, f32 @8, f64 @0x10 */
    {0, 0, 2, 1, 0, 0, PC_K_U16}, {4, 4, 1, 1, 0, 0, PC_K_U32}, {8, 8, 1, 1, 0, 0, PC_K_F32},
    {0x10, 0x10, 1, 1, 0, 0, PC_K_F64},
    /* 4 game arena: the address-like words @0x80 */
    {0x80, 0x80, 2, 1, 0, 0, PC_K_U32},
    /* 5 A arena: u32 @0x10 */
    {0x10, 0x10, 1, 1, 0, 0, PC_K_U32},
    /* 6-10 S */
    {0, offsetof(S_t, a), 1, 1, 0, 0, PC_K_U32}, {4, offsetof(S_t, p), 1, 1, 0, 0, PC_K_PTR},
    {8, offsetof(S_t, h), 1, 1, 0, 0, PC_K_U16}, {12, offsetof(S_t, fp), 1, 1, 0, 0, PC_K_PTR},
    {16, offsetof(S_t, l), 1, 1, 0, 0, PC_K_S32_64},
    /* 11 P */
    {0, 0, 11, 1, 0, 0, PC_K_PTR},
    /* 12-13 Q: two elements, N64 stride 8, host stride sizeof(Q_t) */
    {0, offsetof(Q_t, x), 1, 2, 8, sizeof(Q_t), PC_K_U32}, {4, offsetof(Q_t, y), 1, 2, 8, sizeof(Q_t), PC_K_PTR},
    /* 14 A1, 15 B1 */
    {0, 0, 1, 1, 0, 0, PC_K_PTR}, {0, 0, 1, 1, 0, 0, PC_K_PTR},
};

static const PcDataSym syms[] = {
    {"S", 0x80010100, 20, sizeof S, &S, 6, 5, 1, PC_SYM_OWN},
    {"P", 0x80010120, 44, sizeof P, P, 11, 1, 1, PC_SYM_OWN},
    {"Q", 0x80010160, 16, sizeof Q, Q, 12, 2, 1, PC_SYM_OWN},
    {"B1", 0x80300040, 4, sizeof B1, &B1, 15, 1, 3, PC_SYM_OWN},
    {"A1", 0x80300080, 4, sizeof A1, &A1, 14, 1, 2, PC_SYM_OWN},
    {"cvar", 0x803000F0, 4, sizeof cvar, &cvar, 0, 0, 2, PC_SYM_CDEF},
};

static const PcDataFunc funcs[] = {
    {0x80010000, 1, (void *)game_fn, "game_fn"},
    {0x80010010, 1, NULL, "nocode_fn"},
    {0x80300010, 2, (void *)ovl_a_fn, "ovl_a_fn"},
    {0x80300010, 3, (void *)ovl_b_fn, "ovl_b_fn"},
};

static const PcDataName names[] = {{0x80001080, "boot_vals"}, {0x80010180, "game_word"}, {0x80300090, "a_word"}};

static PcDataState state;
/* The "heap" starts at 0x80020000, as the game's does at the end of its BSS. */
static const PcDataTables tables = {images, 4, arenas, 4, syms, 6, runs, sizeof runs / sizeof runs[0], funcs, 4,
                                    names, 3, 0x80020000, &state};

static void build_rom(void)
{
    uint8_t game[0x200] = {0}, ova[0x100] = {0}, ovb[0x80] = {0};
    uint8_t *boot = rom + 0x1000;

    /* boot: arena at image offset 0x80 */
    put16(boot + 0x80, 0x1234);
    put16(boot + 0x82, 0xABCD);
    put32(boot + 0x84, 0xDEADBEEF);
    put32(boot + 0x88, 0x3FC00000);   /* 1.5f */
    put32(boot + 0x90, 0x40020000);   /* 2.25 */
    put32(boot + 0x94, 0);
    memcpy(boot + 0x98, "HELLO", 6);
    /* game: S at 0x100 */
    put32(game + 0x100, 0x11223344);
    put32(game + 0x104, 0x80001084);   /* -> the boot u32 */
    put16(game + 0x108, 0x5566);
    put32(game + 0x10C, 0x80010000);   /* -> game_fn */
    put32(game + 0x110, 0xFFFFFFFB);   /* long -5 */
    /* P at 0x120 */
    put32(game + 0x120, 0x80010104);   /* S.p */
    put32(game + 0x124, 0x80010110);   /* S.l */
    put32(game + 0x128, 0x8001F000);   /* below the heap, in nothing */
    put32(game + 0x12C, 0x00B5C534);   /* a ROM offset */
    put32(game + 0x130, 0x80010010);   /* function with no C */
    put32(game + 0x134, 0x80300010);   /* both runtime images have a function here */
    put32(game + 0x138, 0x8001016C);   /* Q[1].y */
    put32(game + 0x13C, 0x80020040);   /* the heap: kept */
    put32(game + 0x140, 0x80300040);   /* image B's data, from game data: its own storage */
    put32(game + 0x144, 0x80300120);   /* image A's arena (its BSS) */
    put32(game + 0x148, 0x803FF000);   /* the heap above the images: kept */
    /* Q at 0x160 */
    put32(game + 0x160, 1);
    put32(game + 0x164, 0x80001084);
    put32(game + 0x168, 2);
    put32(game + 0x16C, 0x80010100);
    /* words that look like addresses, in u32s: of own storage, and of a boot arena word */
    put32(game + 0x180, 0x80010100);
    put32(game + 0x184, 0x80001088);
    /* image A: A1 at 0x80 -> its function; a u32 at 0x90 */
    put32(ova + 0x80, 0x80300010);
    put32(ova + 0x90, 0xCAFEF00D);
    /* image B: B1 at 0x40 -> its function; a byte at 0x50 */
    put32(ovb + 0x40, 0x80300010);
    ovb[0x50] = 0x42;

    images[1].rom_len = deflate_to(0x2000, game, sizeof game);
    images[2].rom_len = deflate_to(0x4000, ova, sizeof ova);
    images[3].rom_len = deflate_to(0x5000, ovb, sizeof ovb);
}

/* Drops the loader state of a run: the inflated images and the startup snapshot. */
static void reset_state(void)
{
    for (int i = 0; i < 8; i++)
        free(state.owned[i]);
    for (uint32_t i = 0; state.snapshot && i < tables.n_syms; i++)
        free(state.snapshot[i]);
    free(state.snapshot);
    memset(&state, 0, sizeof state);
}

static void run(int window)
{
    const char *where = window ? "window" : "host";
    a_boot = window ? (uint8_t *)(uintptr_t)arenas[0].n64_lo : h_boot;
    a_game = window ? (uint8_t *)(uintptr_t)arenas[1].n64_lo : h_game;
    a_ovl_a = window ? (uint8_t *)(uintptr_t)arenas[2].n64_lo : h_ovl_a;
    a_ovl_b = window ? (uint8_t *)(uintptr_t)arenas[3].n64_lo : h_ovl_b;
    arenas[0].host = a_boot, arenas[1].host = a_game, arenas[2].host = a_ovl_a, arenas[3].host = a_ovl_b;
    reset_state();
    memset(&S, 0, sizeof S), memset(P, 0, sizeof P), memset(Q, 0, sizeof Q);
    A1 = B1 = NULL;
    cvar = 7;
    images[3].size = 0x80;

    CHECK(pc_data_load_all(&tables, rom, sizeof rom, "pc_data_test_report.txt") == 0);
    const PcDataStats *st = pc_data_stats();

    /* boot arena */
    uint16_t h0, h1;
    uint32_t w;
    float f;
    double d;
    memcpy(&h0, a_boot, 2);
    memcpy(&h1, a_boot + 2, 2);
    memcpy(&w, a_boot + 4, 4);
    memcpy(&f, a_boot + 8, 4);
    memcpy(&d, a_boot + 0x10, 8);
    CHECK(h0 == 0x1234 && h1 == 0xABCD);
    CHECK(w == 0xDEADBEEF);
    CHECK(f == 1.5f);
    CHECK(d == 2.25);
    CHECK(strcmp((char *)a_boot + 0x18, "HELLO") == 0);

    /* own struct with a different host layout */
    CHECK(S.a == 0x11223344);
    CHECK(S.p == a_boot + 4);
    CHECK(S.h == 0x5566);
    CHECK(S.fp == game_fn);
    CHECK(S.l == -5);
    /* pointers into own storage, raw values, unresolved, no C, per-image functions */
    CHECK(P[0] == &S.p);
    CHECK(P[1] == &S.l);
    CHECK(P[2] == (void *)(uintptr_t)0x8001F000);
    CHECK(P[3] == (void *)(uintptr_t)0x00B5C534);
    CHECK(P[4] == NULL);
    CHECK(P[5] == (void *)ovl_a_fn);
    CHECK(P[6] == &Q[1].y);
    CHECK(P[7] == (void *)(uintptr_t)0x80020040);
    CHECK(P[8] == (void *)&B1);
    CHECK(P[9] == a_ovl_a + 0xA0);
    CHECK(P[10] == (void *)(uintptr_t)0x803FF000);
    /* array of structs: strides differ */
    CHECK(Q[0].x == 1 && Q[0].y == a_boot + 4);
    CHECK(Q[1].x == 2 && Q[1].y == &S);
    /* overlay functions by image */
    CHECK(A1 == (void *)ovl_a_fn);
    CHECK(B1 == (void *)ovl_b_fn);
    /* in the window, an arena pointer is its N64 address */
    if (window)
        CHECK(S.p == (void *)(uintptr_t)0x80001084 && P[9] == (void *)(uintptr_t)0x80300120);
    /* stats */
    CHECK(st->unresolved == 1 && st->raw == 1 && st->no_code == 1 && st->ambiguous == 1 && st->rdram == 2);
    CHECK(st->own == 5);   /* P[0], P[1], P[6], P[8], Q[1].y */
    CHECK(st->arena == 3);   /* S.p, Q[0].y, P[9] */
    CHECK(st->func == 4);   /* S.fp, P[5], A1, B1 */
    /* both address-like words; the one of own storage can't be reached through a u32, nor the arena word when
     * the arena is host memory */
    CHECK(st->addr_words == 2 && st->addr_words_host == (window ? 1u : 2u));

    /* In the window, the runtime images' arenas (the "heap") are clear until loaded; in host memory they keep
     * their data. */
    memcpy(&w, a_ovl_a + 0x10, 4);
    CHECK(window ? w == 0 && a_ovl_b[0x10] == 0 : w == 0xCAFEF00D && a_ovl_b[0x10] == 0x42);
    if (window) {
        CHECK(pc_data_load_image(&tables, 2) == 0 && pc_data_load_image(&tables, 3) == 0);
        memcpy(&w, a_ovl_a + 0x10, 4);
        CHECK(w == 0xCAFEF00D && a_ovl_b[0x10] == 0x42);
    }

    /* overlay reload: data back to its ROM values, BSS zeroed, game-C data back to its startup value */
    a_ovl_a[0x10] ^= 0xFF;
    a_ovl_a[0x90] = 0x55;   /* BSS */
    A1 = NULL;
    cvar = 99;
    a_ovl_b[0x10] = 0x77;
    CHECK(pc_data_load_image(&tables, 2) == 0);
    memcpy(&w, a_ovl_a + 0x10, 4);
    CHECK(w == 0xCAFEF00D);
    CHECK(a_ovl_a[0x90] == 0);
    CHECK(A1 == (void *)ovl_a_fn);
    CHECK(cvar == 7);
    CHECK(a_ovl_b[0x10] == 0x77);   /* the other image is untouched */
    CHECK(pc_data_load_image(&tables, 3) == 0);
    CHECK(a_ovl_b[0x10] == 0x42 && B1 == (void *)ovl_b_fn);

    /* lookups */
    CHECK(pc_data_host_of(&tables, 0x80010108, 1) == &S.h);
    CHECK(pc_data_host_of(&tables, 0x80300010, 3) == (void *)ovl_b_fn);
    CHECK(pc_data_host_of(&tables, 0x80001088, 0) == a_boot + 8);

    /* the report names the address-like words, with their targets, and the unresolved pointer */
    FILE *fp = fopen("pc_data_test_report.txt", "r");
    char buf[4096] = {0};
    CHECK(fp != NULL);
    if (fp) {
        fread(buf, 1, sizeof buf - 1, fp);
        fclose(fp);
    }
    CHECK(strstr(buf, "addr-word game_word+0x0 0x80010100 -> S+0x0 [own]") != NULL);
    CHECK(strstr(buf, "addr-word game_word+0x4 0x80001088 -> boot_vals+0x8 [arena]") != NULL);
    CHECK(strstr(buf, "unresolved P+0x8 0x8001F000") != NULL);
    CHECK(strstr(buf, "no-code P+0x10 0x80010010 nocode_fn") != NULL);

    /* a stream that does not inflate to the image size is an error */
    images[3].size = 0x81;
    state.image[3] = NULL;
    CHECK(pc_data_load_image(&tables, 3) != 0);
    fprintf(stderr, "pc_data_test: arenas in %s memory: %s\n", where, failures ? "failures so far" : "ok");
}

int main(void)
{
    build_rom();
    run(0);
    /* An RDRAM window like port/ultra's (4 MiB is enough here). */
    void *want = (void *)(uintptr_t)0x80000000u;
    void *p = mmap(want, 0x400000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == want) {
        run(1);
    } else {
        fprintf(stderr, "FAIL: cannot map a window at host 0x80000000\n");
        failures++;
    }
    reset_state();

    if (failures)
        fprintf(stderr, "pc_data_test: %d failures\n", failures);
    else
        printf("pc_data_test: all checks passed\n");
    return failures != 0;
}
