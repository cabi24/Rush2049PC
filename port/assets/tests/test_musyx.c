/* port/assets/tests/test_musyx.c: the MusyX converter (port/assets/musyx.c, docs/port/MUSYX_DATA.md).
 *
 * 1. Synthetic big-endian files (project, pool, sample directory, song) are built field by field, each field
 *    logged with its width and value. After the conversion every logged field must read natively as its value,
 *    and every other byte must be unchanged. Shared lists and patterns are swapped once.
 * 2. Damaged files: truncated, garbage, self-overlapping, empty. Nothing is written past the block.
 * 3. The game's own readers (src/rom/lib_15a10.c, lib_16320.c, lib_17dc0.c, lib_1f340.c, lib_22300.c, built with
 *    the game's flags) run on the converted files, placed below 4 GiB as the heap is in the RDRAM window:
 *    sndPushGroup and its registries, the resource lookups, the sample lookup with the sample base, the song
 *    start, and the macro steps that read envelopes, start samples and wait in milliseconds. */
#define _GNU_SOURCE
#include <string.h>
#include <sys/mman.h>

#include "assets.h"
#include "support.h"

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

/* ---------------------------------------------------------------- building big-endian files */

typedef struct {
    uint8_t b[0x3000];
    size_t n;
    struct { uint32_t off, value; uint8_t width; } f[2048];
    size_t nf;
} Build;

static void b_log(Build *B, size_t off, int width, uint32_t value)
{
    for (size_t i = 0; i < B->nf; i++)
        if (B->f[i].off == off) {
            B->f[i].value = value, B->f[i].width = (uint8_t)width;
            return;
        }
    B->f[B->nf].off = (uint32_t)off, B->f[B->nf].value = value, B->f[B->nf].width = (uint8_t)width;
    B->nf++;
}

static size_t b8(Build *B, uint32_t v)
{
    b_log(B, B->n, 1, v & 0xFF);
    B->b[B->n] = (uint8_t)v;
    return B->n++;
}

static size_t b16(Build *B, uint32_t v)
{
    b_log(B, B->n, 2, v & 0xFFFF);
    test_put16(B->b + B->n, (uint16_t)v);
    B->n += 2;
    return B->n - 2;
}

static size_t b32(Build *B, uint32_t v)
{
    b_log(B, B->n, 4, v);
    test_put32(B->b + B->n, v);
    B->n += 4;
    return B->n - 4;
}

static void set32(Build *B, size_t off, uint32_t v)
{
    b_log(B, off, 4, v);
    test_put32(B->b + off, v);
}

/* Padding the converter must leave alone. */
static size_t b_align(Build *B, size_t a)
{
    while (B->n % a)
        B->b[B->n++] = 0xEE;
    return B->n;
}

static uint32_t rd16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

/* Every logged field reads natively as built; every byte outside the multi-byte fields is unchanged. */
static int check_converted(const Build *B, const uint8_t *conv, const char *what)
{
    int bad = 0;
    static uint8_t inside[sizeof ((Build *)0)->b];
    memset(inside, 0, sizeof inside);
    for (size_t i = 0; i < B->nf; i++) {
        uint32_t off = B->f[i].off, got;
        if (B->f[i].width == 1)
            continue;
        got = B->f[i].width == 2 ? rd16(conv + off) : rd32(conv + off);
        if (got != B->f[i].value) {
            if (bad++ < 5)
                fprintf(stderr, "%s: field at 0x%x: 0x%x, expected 0x%x\n", what, off, got, B->f[i].value);
        }
        memset(inside + off, 1, B->f[i].width);
    }
    for (size_t i = 0; i < B->n; i++)
        if (!inside[i] && conv[i] != B->b[i] && bad++ < 5)
            fprintf(stderr, "%s: byte at 0x%zx changed: 0x%02x, was 0x%02x\n", what, i, conv[i], B->b[i]);
    return bad;
}

/* ---------------------------------------------------------------- the four files */

typedef struct {
    Build B;
    size_t sfx_group, song_group;
    size_t sfx_entry[2];
    size_t norm_page, drum_page, setups;
} Project;

/* The offsets in a group are relative to the group + 8. */
static void project_build(Project *P)
{
    Build *B = &P->B;
    size_t g, lists[5], k, shared;

    memset(P, 0, sizeof *P);
    /* Group 12: sfx (type 1). */
    g = P->sfx_group = b32(B, 0);
    b16(B, 12), b16(B, 1);
    for (k = 0; k < 6; k++)
        b32(B, 0);
    lists[0] = b16(B, 1), b16(B, 2), b16(B, 0xFFFF);           /* macros */
    lists[1] = b16(B, 5), b16(B, 7), b16(B, 0xFFFF);           /* samples */
    lists[2] = b16(B, 3), b16(B, 4), b16(B, 0xFFFF);           /* tables */
    lists[3] = b16(B, 0x4001), b16(B, 0x4002), b16(B, 0xFFFF);  /* keymaps */
    lists[4] = b16(B, 0x8001), b16(B, 0xFFFF);                 /* layers */
    for (k = 0; k < 5; k++)
        set32(B, g + 8 + 4 * k, (uint32_t)(lists[k] - g - 8));
    set32(B, g + 8 + 20, (uint32_t)(b_align(B, 4) - g - 8));   /* sfx table */
    b16(B, 2), b16(B, 0x0506);
    P->sfx_entry[0] = b16(B, 0x100);
    b16(B, 0x0001), b8(B, 2), b8(B, 0x40), b8(B, 0x7F), b8(B, 0x40), b8(B, 60), b8(B, 0x11), b8(B, 0), b8(B, 0);
    P->sfx_entry[1] = b16(B, 0x101);
    b16(B, 0x4001), b8(B, 1), b8(B, 0x50), b8(B, 0x64), b8(B, 0x20), b8(B, 64), b8(B, 0x22), b8(B, 0), b8(B, 0);
    set32(B, g, (uint32_t)(b_align(B, 4) - g));

    /* Group 20: song (type 0), with three of its lists shared. */
    g = P->song_group = b32(B, 0);
    b16(B, 20), b16(B, 0);
    for (k = 0; k < 8; k++)
        b32(B, 0);
    lists[0] = b16(B, 1), b16(B, 0xFFFF);
    lists[1] = b16(B, 5), b16(B, 0xFFFF);
    shared = b16(B, 0xFFFF);
    set32(B, g + 8, (uint32_t)(lists[0] - g - 8));
    set32(B, g + 12, (uint32_t)(lists[1] - g - 8));
    for (k = 2; k < 5; k++)
        set32(B, g + 8 + 4 * k, (uint32_t)(shared - g - 8));
    /* Pages: {u16 macro; prio; max voices; slot; program; 2 bytes}, ended by program 0xFF. */
    P->norm_page = b_align(B, 4);
    set32(B, g + 8 + 20, (uint32_t)(P->norm_page - g - 8));
    b16(B, 1), b8(B, 0x40), b8(B, 4), b8(B, 3), b8(B, 0), b8(B, 0), b8(B, 0);
    b16(B, 2), b8(B, 0x30), b8(B, 2), b8(B, 0xFF), b8(B, 5), b8(B, 0), b8(B, 0);
    b8(B, 0xAA), b8(B, 0xBB), b8(B, 0), b8(B, 0), b8(B, 0), b8(B, 0xFF), b8(B, 0), b8(B, 0);   /* end: u16 not read */
    P->drum_page = B->n;
    set32(B, g + 8 + 24, (uint32_t)(P->drum_page - g - 8));
    b16(B, 2), b8(B, 0x50), b8(B, 1), b8(B, 9), b8(B, 36), b8(B, 0), b8(B, 0);
    b8(B, 0xCC), b8(B, 0xDD), b8(B, 0), b8(B, 0), b8(B, 0), b8(B, 0xFF), b8(B, 0), b8(B, 0);
    /* MIDI setups: {u16 id; u16; 16 x {program, volume, pan, reverb, chorus, 3 bytes}}, ended by id 0xFFFF. */
    P->setups = B->n;
    set32(B, g + 8 + 28, (uint32_t)(P->setups - g - 8));
    b16(B, 0), b16(B, 0x1234);
    for (k = 0; k < 16; k++) {
        b8(B, k == 0 ? 5 : k == 9 ? 36 : 0x7F), b8(B, 100 + (uint32_t)k), b8(B, 64 - (uint32_t)k), b8(B, 10),
            b8(B, 20), b8(B, 0), b8(B, 0), b8(B, 0);
    }
    b16(B, 0xFFFF), b16(B, 0);
    set32(B, g, (uint32_t)(b_align(B, 4) - g));
    b32(B, 0xFFFFFFFF);
}

typedef struct {
    Build B;
    size_t macro[2], table[2], keymap[2], layer;   /* entry offsets */
} Pool;

static void pool_build(Pool *P)
{
    Build *B = &P->B;

    memset(P, 0, sizeof *P);
    for (int k = 0; k < 4; k++)
        b32(B, 0);
    /* Macros: {u32 size; u16 id; u16; steps of two u32}. */
    set32(B, 0, (uint32_t)B->n);
    P->macro[0] = b32(B, 8 + 3 * 8);
    b16(B, 1), b16(B, 0x0A0B);
    b32(B, 3 << 8 | 0x0C), b32(B, 0);                  /* SETADSR table 3 */
    b32(B, 5 << 8 | 0x10), b32(B, 100);                /* STARTSAMPLE 5 at 100, mode 0 */
    b32(B, 0), b32(B, 0);                              /* END */
    P->macro[1] = b32(B, 8 + 2 * 8);
    b16(B, 2), b16(B, 0);
    b32(B, 0x07), b32(B, 250u << 16);                  /* WAIT 250, the flag byte clear: WAITMS sets it */
    b32(B, 0), b32(B, 0);
    b32(B, 0xFFFFFFFF);
    /* Tables: an envelope (four u16 stored little-endian: the N64 reads 0x3412 and swaps it to 0x1234) and a
     * curve (bytes). */
    set32(B, 4, (uint32_t)B->n);
    P->table[0] = b32(B, 16);
    b16(B, 3), b16(B, 0);
    b16(B, 0x3412), b16(B, 0x5604), b16(B, 0x8907), b16(B, 0xBC0A);
    P->table[1] = b32(B, 8 + 128);
    b16(B, 4), b16(B, 0);
    for (int i = 0; i < 128; i++)
        b8(B, (uint32_t)(i * 2));
    b32(B, 0xFFFFFFFF);
    /* Keymaps: 128 x {u16 id; s8 transpose; u8 pan; s16 priority; 2 bytes}. */
    set32(B, 8, (uint32_t)B->n);
    for (int k = 0; k < 2; k++) {
        P->keymap[k] = b32(B, 8 + 128 * 8);
        b16(B, 0x4001 + (uint32_t)k), b16(B, 0);
        for (int i = 0; i < 128; i++)
            b16(B, i & 1 ? 0xFFFF : 1 + (uint32_t)k), b8(B, (uint32_t)(i - 64)), b8(B, i % 3 ? 0x40 : 0x80),
                b16(B, (uint32_t)(i - 64)), b8(B, 0), b8(B, 0);
    }
    b32(B, 0xFFFFFFFF);
    /* Layers: header {u32 size; u16 id; u16; u16; u16 count}, 12-byte entries
     * {u16 id; u8 low, high; s8 transpose; u8 volume; s16 priority; u8 pan; 3 bytes}. */
    set32(B, 12, (uint32_t)B->n);
    P->layer = b32(B, 12 + 2 * 12);
    b16(B, 0x8001), b16(B, 0x0C0D), b16(B, 0x0E0F), b16(B, 2);
    b16(B, 1), b8(B, 0), b8(B, 63), b8(B, 0xF4), b8(B, 100), b16(B, 0xFFFB), b8(B, 64), b8(B, 0), b8(B, 0), b8(B, 0);
    b16(B, 2), b8(B, 64), b8(B, 127), b8(B, 12), b8(B, 90), b16(B, 7), b8(B, 0x80), b8(B, 0), b8(B, 0), b8(B, 0);
    b32(B, 0xFFFFFFFF);
}

#define SAMPLE_BASE 0xB0039B40u

static void sdir_build(Build *B)
{
    memset(B, 0, sizeof *B);
    b16(B, 5), b16(B, 0), b32(B, 0), b32(B, 0);
    b32(B, 60u << 24 | 22050), b32(B, 3u << 24 | 0x1000), b32(B, 0x200), b32(B, 0xE00);
    b16(B, 7), b16(B, 0), b32(B, 0x2B40), b32(B, 0);
    b32(B, 64u << 24 | 16000), b32(B, 0u << 24 | 0x800), b32(B, 0), b32(B, 0);
    b16(B, 0xFFFF);
}

typedef struct {
    Build B;
    size_t track[2], pattern[2], tempo, bend, mod;
} Song;

static void song_build(Song *S)
{
    Build *B = &S->B;
    size_t tracks, patterns, channels;

    memset(S, 0, sizeof *S);
    for (int k = 0; k < 4; k++)
        b32(B, 0);
    b32(B, 120), b32(B, 0x300);                        /* tempo, loop time */
    tracks = B->n;
    set32(B, 0, (uint32_t)tracks);
    for (int j = 0; j < 64; j++)
        b32(B, 0);
    patterns = B->n;
    set32(B, 4, (uint32_t)patterns);
    b32(B, 0), b32(B, 0);
    channels = B->n;
    set32(B, 8, (uint32_t)channels);
    for (int j = 0; j < 64; j++)
        b8(B, (uint32_t)(j % 16));
    /* Track events: {u32 time; u8 program; u8 controller; u16; u16 code; u16 index or two bytes}. Track 0
     * plays pattern 0, then 1, and loops to its second event; tracks 5 and 0 share their events. */
    S->track[0] = b32(B, 0);
    b8(B, 0xFF), b8(B, 0xFF), b16(B, 0x0102), b16(B, 0), b8(B, 5), b8(B, 0xFE);
    b32(B, 0x180), b8(B, 3), b8(B, 0x60), b16(B, 0), b16(B, 1), b8(B, 0), b8(B, 0);
    b32(B, 0x300), b8(B, 0xFF), b8(B, 0xFF), b16(B, 0x0304), b16(B, 0xFFFE), b16(B, 1);
    for (int k = 0; k < 12; k++)   /* not events: the loop ends what the sequencer reads */
        b8(B, 0x10 + (uint32_t)k);
    S->track[1] = b32(B, 0);
    b8(B, 0xFF), b8(B, 0xFF), b16(B, 0), b16(B, 1), b8(B, 0), b8(B, 0);
    b32(B, 0x200), b8(B, 0xFF), b8(B, 0xFF), b16(B, 0), b16(B, 0xFFFF), b8(B, 0x77), b8(B, 0x66);
    set32(B, tracks + 0, (uint32_t)S->track[0]);
    set32(B, tracks + 4, (uint32_t)S->track[1]);
    set32(B, tracks + 20, (uint32_t)S->track[0]);
    /* Byte streams: pitch bend and modulation. */
    S->bend = b8(B, 0x05), b8(B, 0x10), b8(B, 0x80), b8(B, 0x00);
    S->mod = b8(B, 0x02), b8(B, 0x7F), b8(B, 0x80), b8(B, 0x00);
    /* Patterns: {u32; u32 bend; u32 modulation} and note events {u16 delta; key; velocity; [u16 duration]}. */
    S->pattern[0] = b_align(B, 4);
    set32(B, patterns, (uint32_t)S->pattern[0]);
    b32(B, 0x40), b32(B, (uint32_t)S->bend), b32(B, 0);
    b16(B, 0), b8(B, 60), b8(B, 100), b16(B, 0x60);    /* note */
    b16(B, 0x10), b8(B, 0x85), b8(B, 0);               /* program change */
    b16(B, 0x10), b8(B, 0x8A), b8(B, 0x87);            /* controller */
    b16(B, 0x10), b8(B, 0x85), b8(B, 1);               /* volume */
    b16(B, 0x08), b8(B, 0x85), b8(B, 50), b16(B, 0x22); /* key bit 7 with another velocity: a note */
    b16(B, 0x08), b8(B, 0), b8(B, 0);                  /* nothing */
    b16(B, 0x20), b8(B, 0xFF), b8(B, 0xFF);            /* end: its delta is read too */
    S->pattern[1] = b_align(B, 4);
    set32(B, patterns + 4, (uint32_t)S->pattern[1]);
    b32(B, 0x30), b32(B, 0), b32(B, (uint32_t)S->mod);
    b16(B, 0), b8(B, 64), b8(B, 90), b16(B, 0x30);
    b16(B, 0x30), b8(B, 0xFF), b8(B, 0xFF);
    /* Tempo track: {u32 time; u32 tempo} until time 0xFFFFFFFF. */
    S->tempo = b_align(B, 4);
    set32(B, 12, (uint32_t)S->tempo);
    b32(B, 0), b32(B, 120), b32(B, 0x100), b32(B, 140), b32(B, 0xFFFFFFFF);
    b8(B, 0x99), b8(B, 0x99), b8(B, 0x99), b8(B, 0x99);   /* the end's tempo is not read */
}

/* Convert a copy the way _ReadWave hands it over: the read registered the block as big-endian. */
static void convert(int wave, uint8_t *dst, const Build *B, size_t block)
{
    memcpy(dst, B->b, B->n);
    test_be_reset();
    pc_mem_register_be(dst, B->n);
    pc_wave_loaded(wave, dst, block);
}

/* ---------------------------------------------------------------- part 1: formats */

static Project project;
static Pool pool;
static Build sdir;
static Song song;

static void test_formats(void)
{
    static uint8_t buf[0x3100];

    convert(6, buf, &project.B, sizeof project.B.b);
    CHECK(check_converted(&project.B, buf, "project") == 0);
    CHECK(test_be_range_count() == 0);
    convert(7, buf, &pool.B, sizeof pool.B.b);
    CHECK(check_converted(&pool.B, buf, "pool") == 0);
    /* The envelope's u16 are swapped, the curve's bytes are not. */
    CHECK(rd16(buf + pool.table[0] + 8) == 0x3412 && buf[pool.table[1] + 8 + 5] == 10);
    convert(8, buf, &sdir, sizeof sdir.b);
    CHECK(check_converted(&sdir, buf, "sample directory") == 0);
    convert(10, buf, &song.B, sizeof song.B.b);
    CHECK(check_converted(&song.B, buf, "song") == 0);
    convert(21, buf, &song.B, song.B.n);   /* the last song wave, block = file */
    CHECK(check_converted(&song.B, buf, "song 11") == 0);

    /* Not a MusyX file: wave 9 (sample data, never loaded) and the neighbors are left alone. */
    memcpy(buf, song.B.b, song.B.n);
    pc_wave_loaded(9, buf, song.B.n);
    CHECK(memcmp(buf, song.B.b, song.B.n) == 0);
}

/* ---------------------------------------------------------------- part 2: damaged files */

static void test_damaged(void)
{
    static uint8_t buf[0x3100];
    static const int waves[] = {6, 7, 8, 10};
    const Build *files[] = {&project.B, &pool.B, &sdir, &song.B};
    uint32_t seed = 12345;

    /* Truncated: everything up to the cut is converted, and nothing at or after it is touched. */
    for (int w = 0; w < 4; w++)
        for (size_t cut = 0; cut < files[w]->n; cut += files[w]->n / 7 + 1) {
            memset(buf, 0x5A, sizeof buf);
            memcpy(buf, files[w]->b, cut);
            pc_wave_loaded(waves[w], buf, cut);
            for (size_t i = cut; i < sizeof buf; i++)
                if (buf[i] != 0x5A) {
                    CHECK(!"write past a truncated block");
                    break;
                }
        }
    /* Garbage: no crash, no write past the block. */
    for (int round = 0; round < 64; round++) {
        size_t size = 64 + (seed % 2048);
        for (size_t i = 0; i < size; i++) {
            seed = seed * 1103515245u + 12345u;
            buf[i] = (uint8_t)(seed >> 16);
            if (round & 1)   /* small offsets, so the walks get somewhere */
                buf[i] &= i % 4 == 3 ? 0xFF : 0x01;
        }
        memset(buf + size, 0x5A, sizeof buf - size);
        pc_wave_loaded(waves[round % 4], buf, size);
        for (size_t i = size; i < sizeof buf; i++)
            if (buf[i] != 0x5A) {
                CHECK(!"write past a garbage block");
                break;
            }
    }
    /* A group whose macro list offset is 0 points at its own offsets: no field is swapped twice. */
    {
        Build B;
        memset(&B, 0, sizeof B);
        b32(&B, 0x20), b16(&B, 1), b16(&B, 1);
        for (int k = 0; k < 6; k++)
            b32(&B, k == 1 ? 0x18 : 0);
        b16(&B, 0xFFFF), b16(&B, 0);
        b32(&B, 0xFFFFFFFF);
        memcpy(buf, B.b, B.n);
        pc_wave_loaded(6, buf, B.n);
        CHECK(rd32(buf + 8) == 0 && rd32(buf + 12) == 0x18 && rd16(buf + 4) == 1);
    }
    /* Empty (a zero-size block). */
    pc_wave_loaded(7, buf, 0);
    /* A blank cartridge's files (--no-rom, port/main.c): all zero, which no real file is. They become empty
     * files; a zero song stays zero. A file with one nonzero byte is parsed as usual. */
    memset(buf, 0, 0x200);
    pc_wave_loaded(6, buf, 0x200);
    CHECK(rd32(buf) == 0xFFFFFFFFu && buf[4] == 0);
    memset(buf, 0, 0x200);
    pc_wave_loaded(7, buf, 0x200);
    CHECK(rd32(buf) == 16 && rd32(buf + 12) == 16 && rd32(buf + 16) == 0xFFFFFFFFu);
    memset(buf, 0, 0x200);
    pc_wave_loaded(8, buf, 0x200);
    CHECK(rd16(buf) == 0xFFFF && buf[2] == 0);
    memset(buf, 0, 0x200);
    pc_wave_loaded(12, buf, 0x200);
    for (int i = 0; i < 0x200; i++)
        if (buf[i]) {
            CHECK(!"a zero song changed");
            break;
        }
    memset(buf, 0, 0x200);
    buf[0x1FF] = 1;
    pc_wave_loaded(6, buf, 0x200);
    CHECK(rd32(buf) == 0);
}

/* ---------------------------------------------------------------- part 3: the game's readers */

/* The game's functions (PC prototypes). */
int func_8001536C(uint8_t *groups, uint16_t id, void *address, void *records, void *resources);
int func_800154A4(void);
int func_8001558C(uint16_t id, uint16_t program, unsigned long stream, unsigned long options, uint8_t unlocked);
void func_80017108(void);
void *func_80016C20(uint16_t id);
void *func_80016E68(uint16_t id);
void *func_80016EE0(uint16_t id);
void *func_80016F80(uint16_t id, uint16_t *count);
void *func_80017040(uint16_t id);
int func_80016CF0(uint16_t id, void *sample);
void func_800199F4(void);
uint8_t func_80022F24(void *state, void *step);
uint8_t func_80022874(void *state, void *step);
uint8_t func_80021B9C(void *state, void *step);
uint8_t func_8002193C(void *state, void *step);

/* func_800146B4's view (lib_11640.c, lib_1cf90.c): the PC layout every sample-info writer must match. */
#pragma pack(1)
typedef struct { uint32_t frequency; void *data; uint32_t offset, length, loopStart, loopLength; uint8_t format; }
    SampleInfo;
#pragma pack()

/* func_800178B0's view of a sequence slot (lib_17dc0.c). */
#pragma pack(1)
typedef struct Page_800178B0 { uint16_t macro; uint8_t prio, maxVoices, slot, index, reserved[2]; } Page_800178B0;
#pragma pack()
typedef struct SequenceNode SequenceNode;
typedef struct Arr_800178B0 { uint32_t tTab, unknown04, unknown08, mTrack, info; } Arr_800178B0;
typedef struct Track_800178B0 { uint8_t *addr, *base; uint32_t pattern08, pattern0C; } Track_800178B0;
typedef struct TrackState_800178B0 { uint32_t unknown00[3]; uint32_t active0C, active10, active14; uint8_t unknown18[16]; }
    TrackState_800178B0;
typedef struct Seq_800178B0 { uint32_t identifier; Page_800178B0 *normtab; uint8_t normTrans[128]; Page_800178B0 *drumtab;
    uint8_t drumTrans[128]; Arr_800178B0 *arrbase; uint32_t trackMute[2]; uint32_t fraction118, whole11C, half120;
    uint32_t bpm; Track_800178B0 track[64]; uint8_t trackVolGroup[64]; TrackState_800178B0 trackState[64];
    uint32_t *mTrackBase, *mTrackAddr; uint32_t lowF70, highF74; SequenceNode *noteUsed, *noteKeyOff;
    uint32_t prgState[16]; uint8_t activeFC0, freeFC1; uint16_t speed; uint8_t defVGroup, disabledFC5;
    uint16_t valueFC6; uint8_t unknownFC8[0x2C]; uint8_t syncFF4; uint8_t unknownFF5[3]; } Seq_800178B0;
/* func_80017720's view of the same slot. */
typedef struct Seq_80017720 { uint32_t identifier; void *normtab; uint8_t normTrans[128]; void *drumtab;
    uint8_t drumTrans[128]; uint8_t unknown10C[0xE74]; uint32_t prgState[16]; } Seq_80017720;

/* The boot library's storage these units use (gen_bss defines it in the game). */
#define STORAGE(name, bytes) unsigned char name[bytes] __attribute__((aligned(16)))
uint8_t D_8002C630;
STORAGE(D_80038390, 16);
STORAGE(D_80038398, 128 * 8);
int D_800385A0, D_80038608, D_8003C610, D_8003CE18, D_8003DA20, D_80042228;
STORAGE(D_800385A8, 8 * 32);
STORAGE(D_80038610, 2048 * 8);
STORAGE(D_8003C618, 2048 * 8);
STORAGE(D_8003CE20, 256 * 12);
STORAGE(D_8003DA28, 512 * 4);
STORAGE(D_8003E228, 2048 * 8);
STORAGE(D_80042230, 128 * 16);
STORAGE(D_80043EB0, 8);
STORAGE(D_80043EB8, 0x10000);
STORAGE(D_800426B0, 256 * 64);
STORAGE(D_800443E0, 16);
STORAGE(D_8004BE78, 64);
STORAGE(D_8004BE7B, 1);
STORAGE(D_8004BE7C, 4);
STORAGE(D_8004BE80, 8);
STORAGE(D_8004BE84, 4);
STORAGE(D_8004BE90, 4);
STORAGE(D_8004BE98, 512);
STORAGE(D_8004BEB8, 32 * 0x200);
STORAGE(D_8004F2B8, 64);
STORAGE(D_8004F800, 4);
STORAGE(D_8004F808, 4);
STORAGE(D_8004FA18, 4);
STORAGE(D_80056160, 128);
STORAGE(D_800561E0, 32);
STORAGE(D_8002CC50, 4);
STORAGE(D_8002CC54, 2048);
STORAGE(D_8002D452, 2048);
void *D_80038018, *D_8003801C;

/* What the units call outside themselves: recorded, or nothing. */
static uint32_t seen_bpm, seen_controls[16][4];
static uint16_t seen_envelope[4];
static SampleInfo seen_sample;
static unsigned seen_samples;

void func_80014594(void) {}
void func_800145DC(void) {}
void *func_80014CAC(void *address) { return address; }   /* the sample base is a cartridge address */
void func_80014CF4(void) {}
void func_80014CFC(void **descriptor, void **data) {}
void func_80014D08(void *descriptor, unsigned int data) {}
void func_80019A60(unsigned int value, unsigned char index) { seen_bpm = value; }
void func_80020610(uint8_t controller, uint8_t channel, uint8_t set, uint8_t value)
{
    int k = controller == 7 ? 0 : controller == 10 ? 1 : controller == 91 ? 2 : controller == 93 ? 3 : -1;
    if (k >= 0 && channel < 16)
        seen_controls[channel][k] = value;
}
void func_800148F8(uint32_t index, const uint16_t *envelope) { memcpy(seen_envelope, envelope, 8); }
void func_800146B4(uint32_t index, const SampleInfo *info, uint8_t reset) { seen_sample = *info, seen_samples++; }
uint32_t func_80019AA8(unsigned char *state) { return 0x10000; }   /* ticks: (t << 16) / rate * 1000 >> 5 */
void func_80020820(uint8_t channel, uint8_t set) {}
void func_8001C19C(uint8_t a, uint8_t b) {}
void func_8001B9F8(uint8_t a, uint16_t b, uint8_t c, uint8_t d, uint32_t e) {}
uint32_t func_800201D0(uint32_t v) { return v; }
int func_8001FA18(uint32_t key) { return 0; }
int func_8001B8C4(uint32_t key) { return 0; }
uint32_t func_8001A270(uint32_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, uint16_t g, uint16_t h,
                       uint8_t i, int16_t j) { return 0xFFFFFFFFu; }
int func_8001BDB8(uint8_t channel) { return 0; }
uint8_t func_8001467C(int index) { return 0; }
void func_80014AF0(int index) {}
void func_8002106C(void *voice) {}
uint8_t func_80020F98(uint8_t channel, uint8_t set) { return 0xFF; }
void func_80020F4C(uint8_t a, uint8_t b, uint8_t c) {}
void func_8001EB10(void *state) {}
void func_8001F6EC(void *state) {}
void func_8001EF8C(void *state, uint8_t channel) {}
uint32_t func_8001ECE0(void *state) { return 0; }
int func_8001F13C(uint8_t priority, uint8_t max, uint16_t alloc, uint8_t fx) { return -1; }
void func_8001489C(int index, uint8_t release) {}
uint32_t func_80014D30(uint32_t rate) { return rate; }
uint16_t func_800214C8(void *voice) { return 0; }
uint8_t func_80021548(uint8_t controller) { return controller; }
uint16_t func_800215A8(void *voice, uint8_t controller) { return 0; }
void func_8002165C(void *voice, uint8_t controller, int16_t value) {}
void func_80020FDC(uint8_t a, uint8_t b, uint8_t c) {}
void func_80019BE4(void *state) {}
void func_8001B744(void *a, void *b) {}

/* The heap is in the RDRAM window at 0x80000000; the registries keep 32-bit addresses into the files there. */
static uint8_t *low_window(size_t size)
{
    void *p = mmap((void *)0x80000000UL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == MAP_FAILED)
        return NULL;
    if ((uintptr_t)p + size > 0x100000000ull) {
        munmap(p, size);
        return NULL;
    }
    return p;
}

static void test_game(void)
{
    uint8_t *w = low_window(0x20000), *prj, *pl, *sd, *sg, *base_ptr;
    uint8_t state[0x200], step[8];
    uint16_t count;
    SampleInfo si;
    const Seq_800178B0 *seq;

    if (!w) {
        fprintf(stderr, "test_musyx: no mapping below 4 GiB; the game's readers were not run\n");
        return;
    }
    prj = w + 0x1000, pl = w + 0x5000, sd = w + 0x9000, sg = w + 0xA000;
    convert(6, prj, &project.B, project.B.n);
    convert(7, pl, &pool.B, pool.B.n);
    convert(8, sd, &sdir, sdir.n);
    convert(10, sg, &song.B, song.B.n);

    /* sndPushGroup with the sample base as AudioInitSys passes it: (s32) of the cartridge address. */
    func_80017108();
    D_8002C630 = 1;
    base_ptr = (uint8_t *)(intptr_t)(int32_t)SAMPLE_BASE;
    {
        /* A blank cartridge's project: sndPushGroup finds nothing, and stops. */
        uint8_t *blank = w + 0x10000;
        memset(blank, 0, 0x200);
        pc_wave_loaded(6, blank, 0x200);
        CHECK(func_8001536C(blank, 12, base_ptr, sd, pl) == 0);
    }
    CHECK(func_8001536C(prj, 12, base_ptr, sd, pl) == 1);
    CHECK(func_8001536C(prj, 99, base_ptr, sd, pl) == 0);

    /* The registries hold window addresses into the pool, found by id. */
    CHECK(func_80016C20(1) == pl + pool.macro[0] + 8);
    CHECK(func_80016C20(2) == pl + pool.macro[1] + 8);
    CHECK(func_80016C20(3) == NULL);
    CHECK(func_80016E68(3) == pl + pool.table[0] + 8);
    CHECK(func_80016E68(4) == pl + pool.table[1] + 8);
    CHECK(func_80016EE0(0x4001) == pl + pool.keymap[0] + 8);
    CHECK(func_80016EE0(0x4002) == pl + pool.keymap[1] + 8);
    CHECK(func_80016EE0(0x4003) == NULL);
    count = 0;
    CHECK(func_80016F80(0x8001, &count) == pl + pool.layer + 12 && count == 2);
    /* The sfx entries, in the project, with the state byte set. */
    CHECK(func_80017040(0x100) == prj + project.sfx_entry[0]);
    CHECK(func_80017040(0x101) == prj + project.sfx_entry[1]);
    CHECK(func_80017040(0x102) == NULL);
    CHECK(rd16(prj + project.sfx_entry[1] + 2) == 0x4001 && prj[project.sfx_entry[1] + 9] == 31);
    /* Samples: the directory's records hold base + offset, 32 bits; the lookup hands them on. */
    CHECK(rd16(sd + 2) == 1 && rd32(sd + 8) == SAMPLE_BASE);
    CHECK(rd16(sd + 0x1C + 2) == 1 && rd32(sd + 0x1C + 8) == SAMPLE_BASE + 0x2B40);
    memset(&si, 0xAB, sizeof si);
    CHECK(func_80016CF0(7, &si) == 0);
    CHECK(si.data == (void *)(uintptr_t)(SAMPLE_BASE + 0x2B40) && si.offset == 0 && si.length == 0x800 &&
          si.format == 0 && si.frequency == (64u << 24 | 16000) && si.loopStart == 0 && si.loopLength == 0);
    CHECK(func_80016CF0(5, &si) == 0 && si.length == 0x1000 && si.format == 3 && si.loopStart == 0x200 &&
          si.loopLength == 0xE00);
    CHECK(func_80016CF0(6, &si) == -1);

    /* Macro steps, copied word by word as the interpreter does (func_80023E9C). */
    memset(state, 0, sizeof state);
    state[0x60] = 3;   /* the voice index */
    memcpy(step, pl + pool.macro[0] + 8, 8);
    CHECK((rd32(step) & 0x7F) == 0x0C);
    func_80022F24(state, step);
    CHECK(seen_envelope[0] == 0x1234 && seen_envelope[1] == 0x0456 && seen_envelope[2] == 0x0789 &&
          seen_envelope[3] == 0x0ABC);
    memcpy(step, pl + pool.macro[0] + 16, 8);
    seen_samples = 0;
    func_80022874(state, step);
    CHECK(seen_samples == 1 && seen_sample.data == (void *)(uintptr_t)SAMPLE_BASE && seen_sample.offset == 100 &&
          seen_sample.length == 0x1000 && seen_sample.format == 3 && seen_sample.loopStart == 0x200 &&
          seen_sample.loopLength == 0xE00 && seen_sample.frequency == (60u << 24 | 22050));
    CHECK(rd32(state + 0x58) == SAMPLE_BASE && rd32(state + 0x5C) == (60u << 24 | 22050));
    /* WAITMS: 250 ms is 250 * 256 in the voice's clock (func_8001E930); as ticks it would be 7812. */
    memcpy(step, pl + pool.macro[1] + 8, 8);
    CHECK(func_80021B9C(state, step) == 1 && rd32(state + 0x9C) == 250 * 256);
    memcpy(step, pl + pool.macro[1] + 8, 8);
    memset(state + 0x9C, 0, 12);
    CHECK(func_8002193C(state, step) == 1 && rd32(state + 0x9C) == (((250u << 16) / 0x10000) * 1000) >> 5);

    /* The song group: its pages and MIDI setup start the song through func_800178B0. */
    CHECK(func_8001536C(prj, 20, base_ptr, sd, pl) == 1);
    func_800199F4();
    memset(seen_controls, 0, sizeof seen_controls);
    /* Started (the identifier itself depends on lib_17dc0.c's views, which overlap on LP64). */
    CHECK(func_8001558C(20, 0, (unsigned long)sg, 0, 1) != -1);
    CHECK(func_8001558C(20, 1, (unsigned long)sg, 0, 1) == -1);
    seq = (const Seq_800178B0 *)D_80043EB8;
    CHECK((uint8_t *)seq->normtab == prj + project.norm_page && (uint8_t *)seq->drumtab == prj + project.drum_page);
    CHECK((uint8_t *)seq->arrbase == sg && seq->bpm == 120 && seen_bpm == 120);
    CHECK((uint8_t *)seq->mTrackBase == sg + song.tempo);
    CHECK(seq->track[0].base == sg + song.track[0] && seq->track[1].base == sg + song.track[1] &&
          seq->track[5].base == sg + song.track[0] && seq->track[2].base == NULL);
    CHECK(seq->normTrans[0] == 0 && seq->normTrans[5] == 1 && seq->normTrans[1] == 0xFF && seq->drumTrans[36] == 0);
    CHECK(seq->prgState[3] == (1u << 16 | 0x40 << 8 | 4));        /* page 0, slot 3 */
    CHECK(seq->prgState[9] == (2u << 16 | 0x50 << 8 | 1));        /* drum page, slot 9 */
    CHECK(seq->prgState[1] == 0xFFFFFFFFu);
    /* The setup's programs, through func_80017720's own view: on LP64 its prgState is not func_800178B0's
     * (lib_17dc0.c's views of the slot disagree; docs/port/MUSYX_DATA.md). */
    CHECK(((const Seq_80017720 *)D_80043EB8)->prgState[0] == (2u << 16 | 0x30 << 8 | 2));   /* program 5 */
    CHECK(((const Seq_80017720 *)D_80043EB8)->prgState[9] == (2u << 16 | 0x50 << 8 | 1));   /* drum 36 */
    CHECK(seen_controls[0][0] == 100 && seen_controls[0][1] == 64 && seen_controls[15][0] == 115 &&
          seen_controls[15][1] == 49 && seen_controls[7][2] == 10 && seen_controls[7][3] == 20);
    /* What the sequencer reads from the song, natively (func_80018634, func_80017D38, func_80018A30). */
    CHECK(rd32(sg + song.track[0] + 12) == 0x180 && rd16(sg + song.track[0] + 20) == 1 &&
          rd16(sg + song.track[0] + 34) == 1 && sg[song.track[0] + 10] == 5 && sg[song.track[0] + 11] == 0xFE);
    CHECK(rd32(sg + rd32(sg + 4) + 4) == song.pattern[1] && rd16(sg + song.pattern[0] + 12 + 4) == 0x60);
    CHECK(seq->mTrackBase[2] == 0x100 && seq->mTrackBase[3] == 140);

    /* sndPopGroup, twice: the shared macro goes with the second, the samples with their last reference. */
    CHECK(func_800154A4() == 1);
    CHECK(func_80016C20(1) == pl + pool.macro[0] + 8 && func_80016C20(2) == pl + pool.macro[1] + 8);
    CHECK(func_800154A4() == 1);
    CHECK(func_800154A4() == 0);
    CHECK(func_80016C20(1) == NULL && func_80016EE0(0x4001) == NULL && func_80016F80(0x8001, &count) == NULL);
    CHECK(func_80017040(0x100) == NULL && func_80016CF0(5, &si) == -1);
    munmap(w, 0x20000);
}

int main(void)
{
    project_build(&project);
    pool_build(&pool);
    sdir_build(&sdir);
    song_build(&song);
    test_formats();
    test_damaged();
    test_game();
    return TEST_DONE();
}
