/* port/assets/tests/test_musyx.c: the MusyX converter (port/assets/musyx.c, docs/port/MUSYX_DATA.md).
 *
 * 1. Synthetic big-endian files (project, pool, sample directory, song) are built field by field, each field
 *    logged with its width and value. After the conversion every logged field must read natively as its value,
 *    and every other byte must be unchanged. Shared lists and patterns are swapped once.
 * 2. Damaged files: truncated, garbage, self-overlapping, empty. Nothing is written past the block.
 * 3. The game's own readers (src/rom/lib_15a10.c, lib_16320.c, lib_17dc0.c, lib_1f340.c, lib_1f5b0.c, lib_22300.c,
 *    built with the game's flags) run on the converted files, placed below 4 GiB as the heap is in the RDRAM
 *    window: sndPushGroup and its registries, the resource lookups, the sample lookup with the sample base, the
 *    song start, and the macro steps that read envelopes, start samples and wait in milliseconds.
 * 4. The library's own state on LP64 (docs/port/MUSYX_DATA.md 8): the song plays for 60 frames through the
 *    sequencer's per-frame units, each with its own view of the slot (notes, programs, controllers, pitch bend,
 *    modulation, tempo, loop, note-offs); voices start macros, step them through the interpreter (envelope,
 *    sample, end, a subroutine, a wait, a trap on key-off) and free their sequence nodes; the DMA service
 *    (struct_init_and_call, func_8008ABE4) walks a request list as the driver builds it. */
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
    lists[0] = b16(B, 1), b16(B, 2), b16(B, 6), b16(B, 0xFFFF); /* macros */
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
    size_t macro[3], table[2], keymap[2], layer;   /* entry offsets */
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
    P->macro[2] = b32(B, 8 + 3 * 8);
    b16(B, 6), b16(B, 0);
    b32(B, 1u << 16 | 0x28), b32(B, 0);                /* SETTRAP: key-off goes to macro 1 */
    b32(B, 2u << 16 | 0x06), b32(B, 0);                /* GOSUB macro 2 */
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
    b32(B, 120), b32(B, 0x180);                        /* tempo, loop time: event 1's */
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
    b16(B, 0x10), b8(B, 0x80), b8(B, 0x80 | 104);      /* controller 104: a deferred request is carried out */
    b16(B, 0x20), b8(B, 0xFF), b8(B, 0xFF);
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
int func_8001558C(uint16_t id, uint16_t program, uintptr_t stream, uintptr_t options, uint8_t unlocked);
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
void func_800198C8(void);
unsigned short func_80018B8C(unsigned int identifier);
unsigned char func_800193C8(unsigned int identifier);
uint32_t func_80024988(uint32_t packed, uint16_t allocation, uint8_t key, uint8_t volume, uint8_t panning,
                       uint8_t channel, uint8_t set, uint16_t offset, uint16_t section, uint8_t start, uint8_t group);
void func_80023E9C(void *voice);
int func_80021700(uint32_t identifier);
void func_8001F864(void);
void func_8001E9B0(void);
void func_800218CC(void);
int func_8001EDF4(uint32_t key);
int func_8001FA18(uint32_t key);
void func_80019490(void *request, unsigned int *result, unsigned char unlocked);
/* func_80019490's request (lib_17dc0.c's SequenceRequest). */
typedef struct { uint32_t identifier; uint16_t duration; uint8_t unknown006[2]; uint32_t nextIdentifier;
    uint16_t nextDuration; uint8_t unknown00E[2]; void *stream; uint16_t group, program; uint8_t channel;
    uint8_t unknown019[3]; uint32_t first, second; uint16_t value; uint8_t flags; uint8_t unknown027; } Request;
int32_t struct_init_and_call(intptr_t list, int32_t count, intptr_t done);
int32_t func_8008ABE4(void);
/* The request list's header, in its own storage on the PC (func_8008ABE4.c's DmaList). */
extern struct { uint16_t pad0, count, next, pad6; void *reqs; void *done; } pc_D_80153F10;

/* func_800146B4's view (lib_11640.c, lib_1cf90.c): the PC layout every sample-info writer must match. */
#pragma pack(1)
typedef struct { uint32_t frequency; void *data; uint32_t offset, length, loopStart, loopLength; uint8_t format; }
    SampleInfo;
#pragma pack()

/* A sequence slot as the game lays it out on the PC (include/sequence_context.h, which every view of a slot in
 * lib_17dc0.c follows), with the header's storage names moved out of the way of this test's own. */
#define TARGET_PC 1
#define D_80043EB0 header_D_80043EB0
#define D_80043EB8 header_D_80043EB8
#define D_8004BE80 header_D_8004BE80
#define D_8004BE78 header_D_8004BE78
#define D_8004BE7C header_D_8004BE7C
#define D_8004F808 header_D_8004F808
#include "../../../include/sequence_context.h"
#undef D_80043EB0
#undef D_80043EB8
#undef D_8004BE80
#undef D_8004BE78
#undef D_8004BE7C
#undef D_8004F808
#undef TARGET_PC

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
/* lib_1f5b0.c's: the voice lists, the channel and priority links, the sequence nodes. */
STORAGE(D_8004BE94, 1);
STORAGE(D_8004FA19, 1);
STORAGE(D_8004FA1A, 1);
STORAGE(D_80050440, 32 * 4);
STORAGE(D_800504C0, 1);
STORAGE(D_800504C1, 1);
STORAGE(D_800504C2, 1);
STORAGE(D_800504C3, 1);
STORAGE(D_800504C8, 32 * 4);
STORAGE(D_80050548, 256);
STORAGE(D_80050648, 256 * 4);
STORAGE(D_80050A48, 2);
STORAGE(D_80050A4C, 4);
STORAGE(D_80050A50, 32 * 24);
void *D_80050C50, *D_80050C54;

/* What the units call outside themselves: recorded, or nothing. */
static uint32_t seen_bpm, seen_controls[16][4], seen_bend[16][2], seen_mod[16][2];
static uint32_t seen_tempi[16];
static unsigned seen_ntempi;
static int frame;
static struct { int frame; uint32_t program, id; uint8_t key, velocity, channel, track; } seen_notes[32];
static unsigned seen_nnotes;
static struct { int frame; uint32_t id; } seen_offs[32];
static unsigned seen_noffs;
static uint16_t seen_envelope[4];
static SampleInfo seen_sample;
static unsigned seen_samples;

void func_80014594(void) {}
void func_800145DC(void) {}
void *func_80014CAC(void *address) { return address; }   /* the sample base is a cartridge address */
void func_80014CF4(void) {}
void func_80014CFC(void **descriptor, void **data) {}
void func_80014D08(void *descriptor, unsigned int data) {}
void func_80019A60(unsigned int value, unsigned char index)
{
    seen_bpm = value;
    if (seen_ntempi < 16)
        seen_tempi[seen_ntempi++] = value;
}
void func_80020610(uint8_t controller, uint8_t channel, uint8_t set, uint8_t value)
{
    int k = controller == 7 ? 0 : controller == 10 ? 1 : controller == 91 ? 2 : controller == 93 ? 3 : -1;
    if (k >= 0 && channel < 16)
        seen_controls[channel][k] = value;
    if (channel < 16 && (controller == 128 || controller == 129))
        seen_bend[channel][controller - 128] = value;
    if (channel < 16 && (controller == 1 || controller == 33))
        seen_mod[channel][controller == 33] = value;
}
void func_800148F8(uint32_t index, const uint16_t *envelope) { memcpy(seen_envelope, envelope, 8); }
void func_800146B4(uint32_t index, const SampleInfo *info, uint8_t reset) { seen_sample = *info, seen_samples++; }
uint32_t func_80019AA8(unsigned char *state) { return 0x10000; }   /* ticks: (t << 16) / rate * 1000 >> 5 */
void func_80020820(uint8_t channel, uint8_t set) {}
void func_8001C19C(uint8_t a, uint8_t b) {}
void func_8001B9F8(uint8_t a, uint16_t b, uint8_t c, uint8_t d, uint32_t e) {}
uint32_t func_800201D0(uint32_t v) { return v; }
int func_8001B8C4(uint32_t key)   /* note off */
{
    if (seen_noffs < 32)
        seen_offs[seen_noffs].frame = frame, seen_offs[seen_noffs++].id = key;
    return 0;
}
/* Note on (program, key, velocity, pan, channel, slot, ?, track, volume group, ?): a voice identifier. */
uint32_t func_8001A270(uint32_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, uint16_t g, uint16_t h,
                       uint8_t i, int16_t j)
{
    if (seen_nnotes >= 32)
        return 0xFFFFFFFFu;
    seen_notes[seen_nnotes].frame = frame, seen_notes[seen_nnotes].program = a;
    seen_notes[seen_nnotes].key = b, seen_notes[seen_nnotes].velocity = c, seen_notes[seen_nnotes].channel = e;
    seen_notes[seen_nnotes].track = (uint8_t)h, seen_notes[seen_nnotes].id = 0x1000 + seen_nnotes;
    return seen_notes[seen_nnotes++].id;
}
int func_8001BDB8(uint8_t channel) { return 0; }
uint8_t func_8001467C(int index) { return 0; }
void func_80014AF0(int index) {}
void func_8002106C(void *voice) {}
uint8_t func_80020F98(uint8_t channel, uint8_t set) { return 0xFF; }
void func_80020F4C(uint8_t a, uint8_t b, uint8_t c) {}
void func_80014B3C(int index) {}
/* The PI: the requests func_8008ABE4 starts. */
STORAGE(D_80153E68, 24);
STORAGE(D_80161438, 24);
static struct { uint32_t dev, size; void *vaddr, *queue; } seen_dma[8];
static unsigned seen_ndma;
int32_t osPiStartDma(void *mb, int32_t priority, int32_t direction, uint32_t devAddr, void *vAddr, uint32_t nbytes,
                     void *mq)
{
    if (seen_ndma < 8)
        seen_dma[seen_ndma].dev = devAddr, seen_dma[seen_ndma].size = nbytes, seen_dma[seen_ndma].vaddr = vAddr,
        seen_dma[seen_ndma++].queue = mq;
    return 0;
}
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

/* The song plays: func_800198C8 runs the sequencer's units, each reading the slot through its own view. 60 frames
 * of 384 samples at 22050 Hz: 13.4 ticks a frame at 120 bpm, 15.6 at 140 (from tick 0x100), one loop at 0x300.
 * Track 0 (channel 0, transposed +5, velocity -2) plays pattern 0 at 0, then pattern 1 at 0x180, and loops to that
 * event; track 5 shares its events on channel 5, which has a program only after pattern 0's program change;
 * track 1 plays pattern 1 on channel 1, which has none. */
static void test_play(const SequenceContext *seq)
{
    static const struct { uint8_t key, velocity, channel, track; } want[] = {
        {65, 98, 0, 0},                   /* pattern 0's first note */
        {127, 48, 0, 0}, {127, 48, 5, 5}, /* key bit 7 with a plain velocity: a note, key 0x85 + 5 clamped */
        {64, 90, 0, 0}, {64, 90, 5, 5},   /* pattern 1 at 0x180 */
        {64, 90, 0, 0}, {64, 90, 5, 5},   /* and after the loop to it */
    };
    unsigned i, k;
    int offs_ok;
    uint32_t rate = 384;
    int32_t frequency = 22050;
    Request request;
    static unsigned int result;

    memcpy(D_8004F808, &rate, 4);
    memcpy(D_8004F800, &frequency, 4);
    seen_nnotes = seen_noffs = seen_ntempi = 0;
    memset(seen_controls, 0, sizeof seen_controls);
    /* A request deferred to the song's controller 104 (flags 4): the slot keeps a copy and the result word's
     * address; then (flags 2) it would start a next song that doesn't exist. */
    memset(&request, 0, sizeof request);
    request.identifier = 0, request.flags = 4 | 2, request.nextIdentifier = 0x1234, request.channel = 7;
    request.first = 11, request.value = 0x2222, request.stream = &request;
    func_80019490(&request, &result, 1);
    CHECK(result == 0x80000000u && seq->pendingFF0 == (uintptr_t)&result && seq->unknownFF4[0] == 1);
    CHECK(seq->valueFE0 == 7 && seq->firstFE4 == 11 && seq->valueFEC == 0x2222 && seq->flagsFEE == 2 &&
          seq->streamFD8 == &request);
    for (frame = 0; frame < 60; frame++)
        func_800198C8();
    CHECK(result == 0xFFFFFFFFu && seq->unknownFF4[0] == 0);       /* carried out at tick 0x190 */
    CHECK(seen_nnotes == sizeof want / sizeof want[0]);
    for (i = 0; i < seen_nnotes && i < sizeof want / sizeof want[0]; i++)
        if (seen_notes[i].key != want[i].key || seen_notes[i].velocity != want[i].velocity ||
            seen_notes[i].channel != want[i].channel || seen_notes[i].track != want[i].track ||
            seen_notes[i].program != (2u << 16 | 0x30 << 8 | 2)) {
            fprintf(stderr, "note %u: key %u velocity %u channel %u track %u program %08x\n", i, seen_notes[i].key,
                    seen_notes[i].velocity, seen_notes[i].channel, seen_notes[i].track, seen_notes[i].program);
            CHECK(!"a note the song doesn't play");
        }
    if (seen_nnotes == 7) {
        /* When: tick 0, 0x38, 0x180 and the loop at 0x300 (the lookahead is half a frame's ticks). */
        CHECK(seen_notes[0].frame == 0);
        CHECK(seen_notes[1].frame == seen_notes[2].frame && seen_notes[1].frame >= 3 && seen_notes[1].frame <= 4);
        CHECK(seen_notes[3].frame == seen_notes[4].frame && seen_notes[3].frame >= 25 && seen_notes[3].frame <= 28);
        CHECK(seen_notes[5].frame == seen_notes[6].frame && seen_notes[5].frame >= 49 && seen_notes[5].frame <= 53);
    }
    /* Every note has ended (durations 0x60, 0x22 and 0x30 ticks: 7, 3 and 3 frames), each after it started. */
    offs_ok = seen_noffs == 7;
    for (i = 0; i < seen_noffs; i++) {
        for (k = 0; k < seen_nnotes && seen_notes[k].id != seen_offs[i].id; k++)
            ;
        if (k == seen_nnotes || seen_offs[i].frame <= seen_notes[k].frame)
            offs_ok = 0;
    }
    CHECK(offs_ok);
    /* Tempo: the tempo track's first entry in frame 0, its second at tick 0x100, and both again when the loop
     * restarts the track at its time (0x180). */
    CHECK(seen_ntempi == 4 && seen_tempi[0] == 120 && seen_tempi[1] == 140 && seen_tempi[2] == 120 &&
          seen_tempi[3] == 140 && seq->rate124 == 140);
    CHECK(func_80018B8C(0) == 1);                                 /* one loop */
    /* Controllers: pattern 0's volume 10 on channels 0 and 5, then the 0x180 event's 0x60. */
    CHECK(seen_controls[0][0] == 0x60 && seen_controls[5][0] == 0x60);
    /* Pitch bend: 0x2000 + 16; modulation: 0 + 127 (pattern 1, channels 0, 1 and 5). */
    CHECK(seen_bend[0][0] == 0x40 && seen_bend[0][1] == 0x10 && seen_bend[5][0] == 0x40);
    CHECK(seen_mod[0][1] == 127 && seen_mod[1][1] == 127 && seen_mod[5][1] == 127 && seen_mod[1][0] == 0);
    /* Track 1 ended at 0x200; tracks 0 and 5 still run, both at the loop's events again. */
    CHECK(seq->unknown128[1].cursor == NULL && seq->unknown128[0].cursor != NULL &&
          seq->unknown128[5].cursor == seq->unknown128[0].cursor);
    CHECK(seq->activeFC0 == 1);
}

/* Voices: func_80024988 starts a macro on a voice of D_8004BEB8 and func_80023E9C steps it. The voice keeps its
 * N64 layout on the PC: the macro pointers at +0, +4, +8, +0xC, +0x1C and +0x20 as 32-bit window addresses, the
 * sequence node at +0x18 as its N64 address (docs/port/MUSYX_DATA.md 8.3). */
static void test_voices(uint8_t *pl)
{
    uint8_t *v0 = D_8004BEB8, *v1 = D_8004BEB8 + 0x1A0;
    uint32_t m1 = (uint32_t)(uintptr_t)(pl + pool.macro[0] + 8), m2 = (uint32_t)(uintptr_t)(pl + pool.macro[1] + 8);
    uint32_t m6 = (uint32_t)(uintptr_t)(pl + pool.macro[2] + 8), key, id;
    int i;

    /* As the synth's init leaves them: 32 voices, all free, their identifiers clear. */
    D_8004FA18[0] = 32, D_8004FA19[0] = 32, D_8004FA1A[0] = 32;
    memset(D_8004BEB8, 0, 32 * 0x1A0);
    for (i = 0; i < 32; i++)
        memset(D_8004BEB8 + i * 0x1A0 + 0x10, 0xFF, 8), memset(D_8004BEB8 + i * 0x1A0 + 0x60, 0xFF, 4);
    func_8001F864();
    func_8001E9B0();
    func_800218CC();

    /* Macro 1 (envelope, sample, end) on voice 0, key 60. */
    seen_samples = 0;
    memset(seen_envelope, 0, sizeof seen_envelope);
    key = func_80024988(1u << 16 | 0x40 << 8 | 4, 0x100, 60, 100, 64, 0, 0, 0, 0, 1, 23);
    id = 1u << 16 | 60 << 8 | 0;
    CHECK(key == 0 && func_8001EDF4(key) == (int)id);
    CHECK(rd32(v0 + 0) == m1 && rd32(v0 + 4) == m1 && rd32(v0 + 8) == 0 && rd32(v0 + 0x60) == id);
    CHECK(rd32(v0 + 0x18) == 0x80050A50u && rd32(v0 + 0x10) == 0xFFFFFFFFu && rd32(v0 + 0x14) == 0xFFFFFFFFu);
    CHECK(D_80050C50 == (void *)D_80050A50);
    func_80023E9C(v0);
    CHECK(seen_envelope[0] == 0x1234 && seen_envelope[3] == 0x0ABC);
    CHECK(seen_samples == 1 && seen_sample.data == (void *)(uintptr_t)SAMPLE_BASE && seen_sample.offset == 100);
    /* The end freed the voice and its node, after three steps. */
    CHECK(rd32(v0 + 4) == m1 + 24 && rd32(v0 + 0x60) == 0xFFFFFFFFu && rd32(v0 + 0) == 0);
    CHECK(D_80050C50 == NULL && D_80050C54 == (void *)D_80050A50 && func_8001EDF4(key) == -1);

    /* Macro 6 on voice 1: a trap for key-off, then a subroutine (macro 2) that waits 250 ms. */
    key = func_80024988(6u << 16 | 0x40 << 8 | 4, 0x100, 62, 100, 64, 0, 0, 0, 0, 1, 23);
    id = 6u << 16 | 62 << 8 | 1;
    CHECK(key == 1 && func_8001EDF4(key) == (int)id && rd32(v1 + 0x60) == id && rd32(v1 + 0x18) == 0x80050A50u);
    func_80023E9C(v1);
    CHECK(rd32(v1 + 0) == m2 && rd32(v1 + 4) == m2 + 8);            /* in the subroutine, after its wait */
    CHECK(rd32(v1 + 8) == m6 && rd32(v1 + 0xC) == m6 + 16);         /* where it returns */
    CHECK(rd32(v1 + 0x1C) == m1 && rd32(v1 + 0x20) == m1);          /* the trap */
    CHECK(rd32(v1 + 0x9C) == 250 * 256);
    func_80023E9C(v1);                                              /* still waiting */
    CHECK(rd32(v1 + 4) == m2 + 8 && seen_samples == 1);
    /* Key-off: the trap runs macro 1, which starts its sample and ends the voice. */
    CHECK(func_80021700(id) == 0);
    func_80023E9C(v1);
    CHECK(seen_samples == 2 && rd32(v1 + 0x1C) == 0 && rd32(v1 + 0x20) == 0 && rd32(v1 + 4) == m1 + 24);
    CHECK(rd32(v1 + 0x60) == 0xFFFFFFFFu && D_80050C50 == NULL && func_8001EDF4(key) == -1);

    /* A voice stopped by its key (func_8001FA18) while it waits. */
    key = func_80024988(2u << 16 | 0x40 << 8 | 4, 0x100, 64, 100, 64, 0, 0, 0, 0, 1, 23);
    CHECK(key == 2 && D_80050C50 != NULL);
    func_80023E9C(D_8004BEB8 + 2 * 0x1A0);
    CHECK(func_8001FA18(key) == 0 && D_80050C50 == NULL && func_8001EDF4(key) == -1);
}

/* The DMA service (slot 4 of the service table) on a request list as func_80012234 and func_80012730 build it:
 * {length, source, destination}, 16 bytes on the PC. struct_init_and_call starts the first, func_8008ABE4 each
 * next one as the PI finishes, and the header keeps the queue to tell when all are done (docs/port/MUSYX_DATA.md
 * 8.1). */
static void test_dma(void)
{
    static struct { int32_t length; uint32_t source; unsigned char *destination; } list[3];
    static unsigned char buf[3][0x600];
    static uint8_t done;

    CHECK(sizeof list[0] == 16);
    for (int i = 0; i < 3; i++)
        list[i].length = 0x100 << i, list[i].source = SAMPLE_BASE + 0x1000 * (uint32_t)i, list[i].destination = buf[i];
    seen_ndma = 0;
    CHECK(struct_init_and_call((intptr_t)list, 3, (intptr_t)&done) == 1);
    CHECK(seen_ndma == 1 && seen_dma[0].dev == SAMPLE_BASE && seen_dma[0].size == 0x100 && seen_dma[0].vaddr == buf[0] &&
          seen_dma[0].queue == (void *)D_80153E68);
    CHECK(func_8008ABE4() == 1 && func_8008ABE4() == 1 && func_8008ABE4() == 0);
    CHECK(seen_ndma == 3 && seen_dma[2].dev == SAMPLE_BASE + 0x2000 && seen_dma[2].size == 0x400 &&
          seen_dma[2].vaddr == buf[2]);
    CHECK(pc_D_80153F10.reqs == (void *)list && pc_D_80153F10.done == &done && pc_D_80153F10.count == 3 &&
          pc_D_80153F10.next == 3 && (pc_D_80153F10.pad0 & 0xFF) == 1);
    CHECK(struct_init_and_call((intptr_t)list, 0, (intptr_t)&done) == 1 && seen_ndma == 3);   /* nothing to do */
}

static void test_game(void)
{
    uint8_t *w = low_window(0x20000), *prj, *pl, *sd, *sg, *base_ptr;
    uint8_t state[0x200], step[8];
    uint16_t count;
    SampleInfo si;
    const SequenceContext *seq;
    const uint32_t *prg;

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
    test_voices(pl);

    /* The song group: its pages and MIDI setup start the song through func_800178B0. */
    CHECK(func_8001536C(prj, 20, base_ptr, sd, pl) == 1);
    func_800199F4();
    memset(seen_controls, 0, sizeof seen_controls);
    /* Started, as the first song (identifier 0) in slot 0, and active. */
    CHECK(func_8001558C(20, 0, (uintptr_t)sg, 0, 1) == 0);
    CHECK(func_8001558C(20, 1, (uintptr_t)sg, 0, 1) == -1);
    seq = (const SequenceContext *)D_80043EB8;
    prg = (const uint32_t *)seq->unknownF80;
    CHECK(seq->identifier == 0 && seq->activeFC0 == 1 && seq->inactiveFC1 == 0 && seq->channelFC4 == 23);
    CHECK((uint8_t *)seq->normtab004 == prj + project.norm_page && (uint8_t *)seq->drumtab088 == prj + project.drum_page);
    CHECK((uint8_t *)seq->song10C == sg && seq->rate124 == 120 && seen_bpm == 120);
    CHECK((uint8_t *)seq->timingStartF68 == sg + song.tempo);
    CHECK(seq->unknown128[0].start == sg + song.track[0] && seq->unknown128[0].cursor == sg + song.track[0] &&
          seq->unknown128[1].start == sg + song.track[1] && seq->unknown128[5].start == sg + song.track[0] &&
          seq->unknown128[2].start == NULL);
    CHECK(seq->normTrans008[0] == 0 && seq->normTrans008[5] == 1 && seq->normTrans008[1] == 0xFF &&
          seq->drumTrans08C[36] == 0);
    CHECK(prg[3] == (1u << 16 | 0x40 << 8 | 4));                  /* page 0, slot 3 */
    CHECK(prg[9] == (2u << 16 | 0x50 << 8 | 1));                  /* drum page, slot 9; and drum 36 */
    CHECK(prg[1] == 0xFFFFFFFFu);
    CHECK(prg[0] == (2u << 16 | 0x30 << 8 | 2));                  /* the setup's program 5 (func_80017720) */
    CHECK(func_800193C8(0) == 23);                                /* through an integer slot address */
    CHECK(seen_controls[0][0] == 100 && seen_controls[0][1] == 64 && seen_controls[15][0] == 115 &&
          seen_controls[15][1] == 49 && seen_controls[7][2] == 10 && seen_controls[7][3] == 20);
    /* What the sequencer reads from the song, natively (func_80018634, func_80017D38, func_80018A30). */
    CHECK(rd32(sg + song.track[0] + 12) == 0x180 && rd16(sg + song.track[0] + 20) == 1 &&
          rd16(sg + song.track[0] + 34) == 1 && sg[song.track[0] + 10] == 5 && sg[song.track[0] + 11] == 0xFE);
    CHECK(rd32(sg + rd32(sg + 4) + 4) == song.pattern[1] && rd16(sg + song.pattern[0] + 12 + 4) == 0x60);
    CHECK(seq->timingStartF68[1].time == 0x100 && seq->timingStartF68[1].value == 140);
    test_play(seq);

    /* sndPopGroup, twice: the shared macro goes with the second, the samples with their last reference. */
    CHECK(func_800154A4() == 1);
    CHECK(func_80016C20(1) == pl + pool.macro[0] + 8 && func_80016C20(2) == pl + pool.macro[1] + 8 &&
          func_80016C20(6) == pl + pool.macro[2] + 8);
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
    test_dma();
    return TEST_DONE();
}
