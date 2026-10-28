/* port/data/datacheck.c: run the data loader on the user's ROM with the game's tables and check known values.
 *
 *     rush2049pc_datacheck [baserom.us.z64]
 *
 * Linked with the game objects, the generated data and the link test's stubs, so every table entry is the real
 * host address. Without a ROM it says so and exits 0 (the build and the unit test still check the rest).
 * Checks: the ROM's SHA-1; the boot tail's build-info record; the inflate tables (public zlib constants); how
 * many floats are plausible (wrong byte order makes most of them garbage); function pointers; the element widths
 * at places where the game's reads fix them (known_widths); an overlay reload restoring its data.
 *
 *     rush2049pc_datacheck --synthetic
 *
 * needs no ROM: random data at the real image offsets (streams deflated into their retail slots), a known target
 * planted in every pointer field, then every arena byte and own-storage element is checked with an independent
 * decoder, known_widths against the images, and an overlay reload is checked. It tests the generated tables and
 * the loader together.
 *
 * The game's arenas are at their N64 addresses, so both map the RDRAM window at host 0x80000000 first, as
 * port/ultra does for the game. */
#define _GNU_SOURCE
#include "pc_data.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <zlib.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

int pc_data_init(const uint8_t *rom, size_t rom_size);
void pc_data_overlay_load(int overlay);

#ifndef PC_DATA_ROM
#define PC_DATA_ROM "baserom.us.z64"
#endif
#define ROM_SHA1 "3f99351d7bb61656614bdb2aa1a90cfe55d1922c"

static int failures;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); failures++; } \
                           else { printf("ok: " __VA_ARGS__); printf("\n"); } } while (0)

/* SHA-1 (FIPS 180-1). */
static uint32_t rol(uint32_t x, int n) { return x << n | x >> (32 - n); }

static void sha1(const uint8_t *msg, size_t len, char out[41])
{
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint64_t bits = (uint64_t)len * 8;
    size_t total = (len + 9 + 63) / 64 * 64;
    for (size_t off = 0; off < total; off += 64) {
        uint8_t blk[64];
        uint32_t w[80];
        for (int i = 0; i < 64; i++) {
            size_t k = off + i;
            blk[i] = k < len ? msg[k] : k == len ? 0x80 : 0;
        }
        if (off + 64 == total)
            for (int i = 0; i < 8; i++)
                blk[56 + i] = (uint8_t)(bits >> (56 - 8 * i));
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t)blk[4 * i] << 24 | blk[4 * i + 1] << 16 | blk[4 * i + 2] << 8 | blk[4 * i + 3];
        for (int i = 16; i < 80; i++)
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) f = (b & c) | (~b & d), k = 0x5A827999;
            else if (i < 40) f = b ^ c ^ d, k = 0x6ED9EBA1;
            else if (i < 60) f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
            else f = b ^ c ^ d, k = 0xCA62C1D6;
            uint32_t tmp = rol(a, 5) + f + e + k + w[i];
            e = d, d = c, c = rol(b, 30), b = a, a = tmp;
        }
        h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
    }
    for (int i = 0; i < 5; i++)
        sprintf(out + 8 * i, "%08x", h[i]);
}

static void *at(uint32_t n64, int image)
{
    return pc_data_host_of(&pc_data_tables, n64, image);
}

/* The RDRAM window (port/ultra/memory.c maps it for the game): 8 MiB at host 0x80000000. */
static int map_window(void)
{
    void *want = (void *)(uintptr_t)0x80000000u;
    void *p = mmap(want, 0x800000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p != want) {
        if (p != MAP_FAILED)
            munmap(p, 0x800000);
        printf("FAIL: cannot map the RDRAM window at host 0x80000000, where the game's data belongs\n");
        failures++;
        return -1;
    }
    return 0;
}

static int contains(const uint8_t *p, size_t n, const char *s)
{
    size_t k = strlen(s);
    for (size_t i = 0; i + k <= n; i++)
        if (memcmp(p + i, s, k) == 0)
            return 1;
    return 0;
}

/* Element widths the game's reads need (docs/port/ENDIAN.md 4, DATA.md 1-2), written down here apart from the
 * generated tables so that a regression in gen_data's layout rules shows: each element of `width` bytes in
 * [n64, n64 + len) must hold the image's bytes reversed (a byte as it is). The colour unions are bytes (a byte
 * member covers them; their words only copy them), D_801146BC's second colour too under D_801146C0's word view;
 * unions without such a member keep their widest member's layout (Vtx, MenuSlot); the boot's viewport offset
 * table is s16 up to gViewportPendingFrames, and the flags after it, which its s16[32] declarations ran over, are
 * bytes. */
static const struct {
    const char *what;
    uint32_t n64, len;
    uint8_t image, width;
} known_widths[] = {
    {"D_80118E28/2C, the menus' flashing text colours", 0x80118E28, 8, 1, 1},
    {"D_801146BC, their source pair (D_801146C0 the second)", 0x801146BC, 8, 1, 1},
    {"D_80116DB0, the options screen's button colour", 0x80116DB0, 4, 1, 1},
    {"D_8011AD8C, the shadow colour", 0x8011AD8C, 4, 1, 1},
    {"D_8011ADC0, the sheen colours", 0x8011ADC0, 16, 1, 1},
    {"D_8011AE58, track 1's IR colours", 0x8011AE58, 16, 1, 1},
    {"D_8011B558, the arrows' colours", 0x8011B558, 20, 1, 1},
    {"D_8011B578, the arrow colour", 0x8011B578, 4, 1, 1},
    {"D_8011EF10, a Vtx's ob, flag and tc (s16)", 0x8011EF10, 12, 1, 2},
    {"D_8011EF10, its cn (bytes)", 0x8011EF1C, 4, 1, 1},
    {"D_803B3054/58, image A's colours", 0x803B3054, 8, 2, 1},
    {"D_803B3458/5C", 0x803B3458, 8, 2, 1},
    {"D_803B8328", 0x803B8328, 4, 2, 1},
    {"D_803B8354", 0x803B8354, 4, 2, 1},
    {"D_803B8394/98", 0x803B8394, 8, 2, 1},
    {"D_803AF9A8, a MenuSlot's first float", 0x803AF9A8, 4, 2, 4},
    {"D_803AF9A8, its alpha byte", 0x803AF9E4, 1, 2, 1},
    {"D_80394270, image B's player colours", 0x80394270, 16, 3, 1},
    {"D_803942A0/A4", 0x803942A0, 8, 3, 1},
    {"D_803942E8", 0x803942E8, 4, 3, 1},
    {"gViewportOffsetXExtra..gViewportOffsetY, s16[3][4]", 0x8002AFF8, 24, 0, 2},
    {"gViewportPendingFrames", 0x8002B010, 2, 0, 2},
    {"gInitFlag and the byte after it", 0x8002B028, 2, 0, 1},
    {"gDmaInitialized and the byte after it", 0x8002B030, 2, 0, 1},
    /* The cheat codes (Egg: a step count, then u32 held and pressed button masks, 20 each; CheckForEgg reads
     * the masks as words). An opaque Rec164 view (func_8038FF90.c) once left them in the ROM's order. */
    {"D_803B3460, the first cheat code's step count", 0x803B3460, 4, 2, 1},
    {"D_803B3460, the first cheat code's button masks", 0x803B3464, 160, 2, 4},
    {"D_803B3460, the last cheat code's button masks", 0x803B4134, 160, 2, 4},
    {"D_803B41D4, the main menu's code for the cheat screen", 0x803B41D8, 160, 2, 4},
    /* The six car physics descriptors D_80110D08 points at (0xB4 bytes each: floats and the u32 tire_info words),
     * and the car select's stat factor tables (f32[][4], indexed by each car's parts): all 4-byte elements, so a
     * narrower declaration over either (the engine table D_8010FD80's kind of bug) shows here (QA.md round 3). */
    {"D_801108C8, the car physics descriptors", 0x801108D0, 6 * 0xB4, 1, 4},
    {"D_803B28C8..D_803B2A58, the car select's stat factors", 0x803B28C8, 0x1F0, 2, 4},
};

/* Checks known_widths against the images' bytes (after both runtime images are loaded); the number that differ. */
static uint32_t check_widths(const PcDataTables *t, uint8_t *const img[])
{
    uint32_t bad = 0;
    for (size_t i = 0; i < sizeof known_widths / sizeof known_widths[0]; i++) {
        uint32_t n = known_widths[i].len, w = known_widths[i].width, a = known_widths[i].n64;
        int im = known_widths[i].image;
        const uint8_t *host = at(a, im), *src = img[im] + (a - t->images[im].n64_base);
        int ok = host != NULL;
        for (uint32_t o = 0; ok && o < n; o++)
            ok = host[o] == src[o - o % w + (w - 1 - o % w)];
        if (!ok && bad++ < 8)
            printf("  %s: not %u-byte elements as the image holds them\n", known_widths[i].what, w);
    }
    return bad;
}

/* An image's bytes from the ROM, as the loader reads them: a slice, or a raw DEFLATE stream. */
static uint8_t *image_from_rom(const PcDataImage *im, const uint8_t *rom, size_t size)
{
    uint8_t *out = calloc(1, im->size);
    if (!out || (size_t)im->rom_off + im->rom_len > size) {
        free(out);
        return NULL;
    }
    if (im->storage == PC_IMG_RAW) {
        memcpy(out, rom + im->rom_off, im->size);
        return out;
    }
    z_stream z;
    memset(&z, 0, sizeof z);
    int rc = inflateInit2(&z, -15);
    z.next_in = (Bytef *)(rom + im->rom_off);
    z.avail_in = im->rom_len;
    z.next_out = out;
    z.avail_out = im->size;
    if (rc == Z_OK)
        rc = inflate(&z, Z_FINISH);
    inflateEnd(&z);
    if (rc != Z_STREAM_END || z.total_out != im->size) {
        free(out);
        return NULL;
    }
    return out;
}

/* ---------------------------------------------------------------- synthetic ROM, real tables */

static uint64_t rng = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(void)
{
    rng ^= rng << 13, rng ^= rng >> 7, rng ^= rng << 17;
    return (uint32_t)rng;
}

static void put32(uint8_t *p, uint32_t v) { p[0] = v >> 24, p[1] = v >> 16, p[2] = v >> 8, p[3] = (uint8_t)v; }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

static uint32_t esz(int kind, int host)
{
    switch (kind) {
    case PC_K_U8: return 1;
    case PC_K_U16: return 2;
    case PC_K_U32: case PC_K_F32: return 4;
    case PC_K_U64: case PC_K_F64: return 8;
    case PC_K_PTR: return host ? sizeof(void *) : 4;
    default: return host ? 8 : 4;
    }
}

typedef struct { void **field; void *want; } Expect;

/* A pointer target for data of image `img`: a value to plant and the host pointer the loader must produce. */
static uint32_t pick_target(const PcDataTables *t, int img, void **want)
{
    int ovl = img >= 2;
    for (int tries = 0; tries < 64; tries++) {
        switch (rnd() % 6) {
        case 0:
            *want = NULL;
            return 0;
        case 5: {   /* the game heap (the RDRAM window): kept as the N64 address */
            uint32_t a = t->heap_lo + rnd() % (0x8038A400u - t->heap_lo);
            *want = (void *)(uintptr_t)a;
            return a;
        }
        case 1:
            *want = (void *)(uintptr_t)0x00B5C534u;   /* a ROM offset stays as it is */
            return 0x00B5C534u;
        case 2: {   /* a function of this image, or a game/boot one with a single entry at its address */
            const PcDataFunc *f = &t->funcs[rnd() % t->n_funcs];
            int single = (f == t->funcs || f[-1].n64_addr != f->n64_addr)
                         && (f + 1 == t->funcs + t->n_funcs || f[1].n64_addr != f->n64_addr);
            if (!f->host || !single || (ovl ? f->image != img : f->image >= 2))
                continue;
            *want = f->host;
            return f->n64_addr;
        }
        case 3: {   /* the start of a symbol with its own storage */
            const PcDataSym *s = &t->syms[rnd() % t->n_syms];
            if (!(s->flags & PC_SYM_OWN) || (s->image >= 2 && s->image != img) || s->run_count == 0
                || t->runs[s->run_first].n64_off != 0 || t->runs[s->run_first].host_off != 0)
                continue;
            *want = s->host;
            return s->n64_addr;
        }
        default: {   /* a byte in an arena that no symbol's own storage covers */
            const PcDataArena *ar = &t->arenas[rnd() % t->n_arenas];
            if (ar->image >= 2 && ar->image != img)
                continue;
            if (!ovl && ar->image >= 2)
                continue;   /* image B's data range is image A's code: game data there is ambiguous */
            uint32_t a = ar->n64_lo + rnd() % (ar->n64_hi - ar->n64_lo);
            int covered = 0;
            for (uint32_t i = 0; i < t->n_syms && !covered; i++)
                covered = a >= t->syms[i].n64_addr && a < t->syms[i].n64_addr + (t->syms[i].n64_size ? t->syms[i].n64_size : 1);
            for (uint32_t i = 0; i < t->n_funcs && !covered; i++)
                covered = t->funcs[i].n64_addr == a;
            if (covered)
                continue;
            *want = ar->host + (a - ar->n64_lo);
            return a;
        }
        }
    }
    *want = NULL;
    return 0;
}

/* Expected arena contents: the image bytes, every described element byte-reversed, BSS zero. */
static int check_arena(const PcDataTables *t, const PcDataArena *ar, const uint8_t *img)
{
    uint32_t n = ar->n64_hi - ar->n64_lo, init = ar->n64_init - ar->n64_lo;
    uint8_t *want = calloc(1, n);
    memcpy(want, img + (ar->n64_lo - t->images[ar->image].n64_base), init);
    for (uint32_t k = 0; k < ar->run_count; k++) {
        const PcDataRun *r = &t->runs[ar->run_first + k];
        uint32_t e = esz(r->kind, 0);
        for (uint32_t i = 0; i < r->rep; i++)
            for (uint32_t j = 0; j < r->count; j++) {
                uint8_t *p = want + r->n64_off + i * r->n64_stride + j * e;
                for (uint32_t x = 0; x < e / 2; x++) {
                    uint8_t c = p[x];
                    p[x] = p[e - 1 - x], p[e - 1 - x] = c;
                }
            }
    }
    int bad = memcmp(want, ar->host, n) != 0;
    free(want);
    return bad;
}

/* Expected own storage: each element decoded from big-endian; pointers as planted. */
static int check_sym(const PcDataTables *t, const PcDataSym *s, const uint8_t *img, uint32_t img_size)
{
    uint8_t *want = calloc(1, s->host_size ? s->host_size : 1);
    uint32_t base = s->n64_addr - t->images[s->image].n64_base;
    for (uint32_t k = 0; k < s->run_count; k++) {
        const PcDataRun *r = &t->runs[s->run_first + k];
        uint32_t en = esz(r->kind, 0), eh = esz(r->kind, 1);
        for (uint32_t i = 0; i < r->rep; i++)
            for (uint32_t j = 0; j < r->count; j++) {
                uint32_t no = r->n64_off + i * r->n64_stride + j * en, ho = r->host_off + i * r->host_stride + j * eh;
                if (no + en > s->n64_size || base + no + en > img_size || ho + eh > s->host_size)
                    continue;
                const uint8_t *p = img + base + no;
                uint8_t *q = want + ho;
                if (r->kind == PC_K_PTR) {
                    memcpy(q, (uint8_t *)s->host + ho, eh);   /* compared against the planted targets */
                } else if (r->kind == PC_K_S32_64) {
                    int64_t v = (int32_t)get32(p);
                    memcpy(q, &v, 8);
                } else if (r->kind == PC_K_U32_64) {
                    uint64_t v = get32(p);
                    memcpy(q, &v, 8);
                } else {
                    for (uint32_t x = 0; x < en; x++)
                        q[x] = p[en - 1 - x];
                }
            }
    }
    int bad = memcmp(want, s->host, s->host_size) != 0;
    free(want);
    return bad;
}

static int synthetic(void)
{
    const PcDataTables *t = &pc_data_tables;
    size_t rom_size = 0xC00000;
    uint8_t *rom = calloc(1, rom_size), *img[8] = {0};
    size_t n_exp = 0, cap = 1 << 16;
    Expect *exp = malloc(cap * sizeof *exp);
    uint32_t bad_arena = 0, bad_sym = 0, bad_ptr = 0, n_own = 0;

    for (uint32_t i = 0; i < t->n_images; i++)
        img[i] = calloc(1, t->images[i].size);
    for (uint32_t i = 0; i < t->n_arenas; i++) {
        const PcDataArena *ar = &t->arenas[i];
        uint8_t *p = img[ar->image] + (ar->n64_lo - t->images[ar->image].n64_base);
        for (uint32_t k = 0; k < ar->n64_init - ar->n64_lo; k++)
            p[k] = (uint8_t)rnd();
    }
    /* Plant pointer targets in every pointer element of own storage. The tables of pointers that port/data/reached.txt
     * follows get NULL: what they reach is random data here (port/data/test_pc_data.c tests the rules). */
    for (uint32_t i = 0; i < t->n_syms; i++) {
        const PcDataSym *s = &t->syms[i];
        if (!(s->flags & PC_SYM_OWN) || (s->flags & PC_SYM_BSS))
            continue;
        int root = 0;
        for (uint32_t k = 0; k < t->n_reach; k++)
            root |= t->reach[k].sym == (int32_t)i;
        uint32_t base = s->n64_addr - t->images[s->image].n64_base;
        for (uint32_t k = 0; k < s->run_count; k++) {
            const PcDataRun *r = &t->runs[s->run_first + k];
            if (r->kind != PC_K_PTR)
                continue;
            for (uint32_t rep = 0; rep < r->rep; rep++)
                for (uint32_t j = 0; j < r->count; j++) {
                    uint32_t no = r->n64_off + rep * r->n64_stride + j * 4;
                    uint32_t ho = r->host_off + rep * r->host_stride + j * (uint32_t)sizeof(void *);
                    if (no + 4 > s->n64_size || base + no + 4 > t->images[s->image].size || ho + 8 > s->host_size)
                        continue;
                    void *want = NULL;
                    put32(img[s->image] + base + no, root ? 0 : pick_target(t, s->image, &want));
                    if (n_exp == cap)
                        exp = realloc(exp, (cap *= 2) * sizeof *exp);
                    exp[n_exp].field = (void **)((uint8_t *)s->host + ho);
                    exp[n_exp++].want = want;
                }
        }
    }
    /* Into the ROM at the real offsets; each stream must fit before the next image. */
    for (uint32_t i = 0; i < t->n_images; i++) {
        const PcDataImage *im = &t->images[i];
        if (im->storage == PC_IMG_RAW) {
            memcpy(rom + im->rom_off, img[i], im->size);
            continue;
        }
        uLongf n = compressBound(im->size);
        z_stream z;
        memset(&z, 0, sizeof z);
        deflateInit2(&z, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
        z.next_in = img[i];
        z.avail_in = im->size;
        z.next_out = rom + im->rom_off;
        z.avail_out = (uInt)(rom_size - im->rom_off < n ? rom_size - im->rom_off : n);
        deflate(&z, Z_FINISH);
        n = z.total_out;
        deflateEnd(&z);
        uint32_t room = im->rom_len;   /* the retail stream's length, which the tables give inflate */
        CHECK(n <= room, "synthetic image %s: %lu B stream fits its %u B slot", im->name, (unsigned long)n, room);
    }
    CHECK(pc_data_load_all(t, rom, rom_size, "") == 0, "pc_data_load_all on the synthetic ROM");
    /* The runtime images' arenas are heap memory until the game loads them. */
    uint32_t dirty = 0;
    for (uint32_t i = 0; i < t->n_arenas; i++)
        for (uint32_t k = 0; t->arenas[i].image >= 2 && k < t->arenas[i].n64_hi - t->arenas[i].n64_lo; k++)
            dirty += t->arenas[i].host[k] != 0;
    CHECK(dirty == 0, "the runtime images' arenas are clear until loaded (%u bytes set)", dirty);
    pc_data_overlay_load(0);
    pc_data_overlay_load(1);

    for (uint32_t i = 0; i < t->n_arenas; i++)
        bad_arena += check_arena(t, &t->arenas[i], img[t->arenas[i].image]);
    for (uint32_t i = 0; i < t->n_syms; i++) {
        const PcDataSym *s = &t->syms[i];
        if (!(s->flags & PC_SYM_OWN))
            continue;
        n_own++;
        if (s->flags & PC_SYM_BSS) {
            for (uint32_t k = 0; k < s->host_size; k++)
                if (((uint8_t *)s->host)[k]) {
                    bad_sym++;
                    break;
                }
        } else if (check_sym(t, s, img[s->image], t->images[s->image].size)) {
            bad_sym++;
            if (bad_sym < 5)
                printf("  own %s differs\n", s->name);
        }
    }
    for (size_t i = 0; i < n_exp; i++)
        if (*exp[i].field != exp[i].want && bad_ptr++ < 5)
            printf("  pointer at %p: %p, expected %p\n", (void *)exp[i].field, *exp[i].field, exp[i].want);
    uint32_t bad_width = check_widths(t, img), same = 0;
    uint8_t *from_rom[8] = {0};   /* the images as the ROM check reads them */
    for (uint32_t i = 0; i < t->n_images && i < 8; i++)
        same += (from_rom[i] = image_from_rom(&t->images[i], rom, rom_size)) != NULL
                && memcmp(from_rom[i], img[i], t->images[i].size) == 0;
    CHECK(same == t->n_images, "the ROM check's own inflate gives the %u images (%u do)", t->n_images, same);
    for (uint32_t i = 0; i < 8; i++)
        free(from_rom[i]);
    CHECK(bad_arena == 0, "%u arenas match the image with every described element byte-reversed", t->n_arenas);
    CHECK(bad_width == 0, "%zu places hold the element widths the game reads (colour unions, viewport table, flags; "
          "%u differ)", sizeof known_widths / sizeof known_widths[0], bad_width);
    CHECK(bad_sym == 0, "%u own-storage symbols decode element by element (%u differ)", n_own, bad_sym);
    CHECK(bad_ptr == 0, "%zu planted pointers resolve (%u wrong)", n_exp, bad_ptr);

    /* Overlay reload: scribble over image A, load it, check again. */
    const PcDataArena *a = &t->arenas[2];
    memset(a->host, 0xA5, a->n64_hi - a->n64_lo);
    for (uint32_t i = 0; i < t->n_syms; i++)
        if ((t->syms[i].flags & PC_SYM_OWN) && t->syms[i].image == a->image)
            memset(t->syms[i].host, 0xA5, t->syms[i].host_size);
    pc_data_overlay_load(0);
    uint32_t bad = check_arena(t, a, img[a->image]);
    for (uint32_t i = 0; i < t->n_syms; i++) {
        const PcDataSym *s = &t->syms[i];
        if ((s->flags & PC_SYM_OWN) && !(s->flags & PC_SYM_BSS) && s->image == a->image)
            bad += check_sym(t, s, img[s->image], t->images[s->image].size);
    }
    for (size_t i = 0; i < n_exp; i++)
        bad += *exp[i].field != exp[i].want;
    CHECK(bad == 0, "overlay A reload restores its arena, own storage and pointers");
    for (uint32_t i = 0; i < t->n_images; i++)
        free(img[i]);
    free(exp);
    free(rom);
    printf("datacheck --synthetic: %s\n", failures ? "FAILED" : "all checks passed");
    return failures != 0;
}

int main(int argc, char **argv)
{
    /* The arenas are at their N64 addresses, in the window. The renderer reads every other value below 4 GB as
     * an N64 address too, so the rest of the game's data must sit above it (PIE). */
    const PcDataTables *tab = &pc_data_tables;
    int placed = 1, high = 1;
    for (uint32_t i = 0; i < tab->n_arenas; i++)
        placed &= (uintptr_t)tab->arenas[i].host == tab->arenas[i].n64_lo;
    for (uint32_t i = 0; i < tab->n_syms; i++)
        high &= sizeof(void *) < 8 || !tab->syms[i].host || (uintptr_t)tab->syms[i].host >> 16 >> 16 != 0;
    CHECK(placed, "%u arenas at their N64 addresses (game: %p)", tab->n_arenas, (void *)tab->arenas[1].host);
    CHECK(high, "own storage and game-C data above 4 GB (%s at %p)", tab->syms[0].name, tab->syms[0].host);
    /* Both runtime images have a function at 0x8038A400 (image A's updateWarningScreen, image B's worlddam
     * entry), under one N64 name: each image's data must reach its own (docs/port/SYNTH.md). */
    const PcDataFunc *f_a = NULL, *f_b = NULL;
    for (uint32_t i = 0; i < tab->n_funcs; i++)
        if (tab->funcs[i].n64_addr == 0x8038A400u)
            *(tab->funcs[i].image == 2 ? &f_a : &f_b) = &tab->funcs[i];
    CHECK(f_a && f_b && f_a->host && f_b->host && f_a->host != f_b->host,
          "0x8038A400 is a function of its own in image A (%s) and in image B (%s)", f_a ? f_a->name : "none",
          f_b ? f_b->name : "none");
    if (argc > 1 && strcmp(argv[1], "--synthetic") == 0)
        return map_window() != 0 ? 1 : synthetic();
    const char *path = argc > 1 ? argv[1] : PC_DATA_ROM;
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        printf("datacheck: no ROM at %s; nothing to check\n", path);
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    uint8_t *rom = malloc((size_t)size);
    if (!rom || fread(rom, 1, (size_t)size, fp) != (size_t)size) {
        printf("datacheck: cannot read %s\n", path);
        return 1;
    }
    fclose(fp);
    char digest[41];
    sha1(rom, (size_t)size, digest);
    if (strcmp(digest, ROM_SHA1) != 0) {
        printf("datacheck: %s has SHA-1 %s, expected %s (San Francisco Rush 2049, USA, .z64)\n", path, digest,
               ROM_SHA1);
        return 1;
    }
    printf("ok: ROM SHA-1\n");

    if (map_window() != 0)
        return 1;
    CHECK(pc_data_init(rom, (size_t)size) == 0, "pc_data_init");
    const PcDataStats *st = pc_data_stats();
    /* Both runtime images loaded, as the game loads them, so their arenas hold their data too. */
    pc_data_overlay_load(0);
    pc_data_overlay_load(1);

    /* Boot tail build-info record, 0x8002E440..0x8002E8E0. */
    const uint8_t *info = at(0x8002E440, 0);
    CHECK(info && contains(info, 0x4A0, "07-17-00") && contains(info, 0x4A0, "MERRILL")
          && contains(info, 0x4A0, "BLACKOUT"), "build-info record (07-17-00, MERRILL, BLACKOUT)");

    /* Inflate tables (zlib's inflate.c constants: u32 border[], u16 cplens[], u16 mask_bits[]) in the boot data.
     * lib_5610 is one of the files port/ultra replaces (zlib inflates on the PC), so no PC declaration describes
     * them and they stay in N64 byte order: this checks where the boot data lands, read big-endian. */
    static const uint32_t border[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    static const uint16_t cplens[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
                                        67, 83, 99, 115, 131, 163, 195, 227, 258};
    const uint8_t *b = at(0x8002B040, 0), *cl = at(0x8002B08C, 0), *mb = at(0x8002B184, 0);
    int ok_border = b != NULL, ok_cplens = cl != NULL, masks = mb != NULL;
    for (int i = 0; ok_border && i < 19; i++)
        ok_border = ((uint32_t)b[4 * i] << 24 | (uint32_t)b[4 * i + 1] << 16 | b[4 * i + 2] << 8 | b[4 * i + 3]) == border[i];
    for (int i = 0; ok_cplens && i < 29; i++)
        ok_cplens = (uint16_t)(cl[2 * i] << 8 | cl[2 * i + 1]) == cplens[i];
    for (int i = 0; masks && i < 17; i++)
        masks = (uint16_t)(mb[2 * i] << 8 | mb[2 * i + 1]) == (uint16_t)((1u << i) - 1);
    CHECK(ok_border, "gInflateBorder (u32, N64 order)");
    CHECK(ok_cplens, "gInflateCplens (u16, N64 order)");
    CHECK(masks, "gInflateMaskBits (u16, N64 order)");

    /* Floats: of every f32 element the tables describe, most must be finite and of sane size. */
    uint32_t nf = 0, good = 0;
    const PcDataTables *t = &pc_data_tables;
    for (uint32_t i = 0; i < t->n_arenas; i++) {
        const PcDataArena *ar = &t->arenas[i];
        for (uint32_t k = 0; k < ar->run_count; k++) {
            const PcDataRun *r = &t->runs[ar->run_first + k];
            if (r->kind != PC_K_F32)
                continue;
            for (uint32_t rep = 0; rep < r->rep; rep++)
                for (uint32_t j = 0; j < r->count; j++) {
                    float f;
                    memcpy(&f, ar->host + r->n64_off + rep * r->n64_stride + j * 4, 4);
                    nf++;
                    good += isfinite(f) && (f == 0.0f || (fabsf(f) > 1e-6f && fabsf(f) < 1e7f));
                }
        }
    }
    printf("floats: %u of %u plausible\n", good, nf);
    CHECK(nf && good * 10 >= nf * 9, "at least 90%% of f32 values plausible");

    /* Function pointers resolved to host functions. */
    printf("pointers: %u own, %u C data, %u arena, %u functions, %u RDRAM, %u null, %u raw, %u unresolved, "
           "%u to functions with no C, %u inexact, %u ambiguous; %u address-like words (%u to host storage or "
           "code)\n", st->own, st->cdef, st->arena, st->func, st->rdram, st->null, st->raw, st->unresolved,
           st->no_code, st->inexact, st->ambiguous, st->addr_words, st->addr_words_host);
    CHECK(st->func > 0, "function pointers resolve to host functions");

    /* The file table (docs/port/ASSETS.md 2.2): 183 waves. Wrong byte order breaks every one of these. */
    const uint32_t *wrom = at(0x8011B5BC, 1), *wsize = at(0x8002E580, 0);
    const int32_t *wkind = at(0x80123564, 1);
    const int8_t *wmodel = at(0x801234AC, 1);
    int kinds = 1, models = 1, offsets = 1, sizes = 1, n_kind[3] = {0, 0, 0};
    for (int i = 0; i < 183; i++) {
        kinds &= wkind[i] >= 0 && wkind[i] <= 2;
        if (wkind[i] >= 0 && wkind[i] <= 2)
            n_kind[wkind[i]]++;
        models &= wmodel[i] == 0 || wmodel[i] == 1;
        offsets &= wrom[i] < (uint32_t)size;
        sizes &= wsize[i] < 0x800000u;
    }
    printf("file table: %d raw, %d LZSS, %d DEFLATE; wave 0 at ROM 0x%X, %u B\n", n_kind[0], n_kind[1], n_kind[2],
           wrom[0], wsize[0]);
    CHECK(kinds, "file table: storage kinds are 0, 1 or 2 (D_80123564)");
    CHECK(models, "file table: model flags are 0 or 1 (D_801234AC)");
    CHECK(offsets, "file table: ROM offsets inside the ROM (D_8011B5BC)");
    CHECK(sizes, "file table: sizes under 8 MB (D_8002E580)");

    /* Element widths at the places listed in known_widths, against the images as the ROM holds them. */
    uint8_t *img[8] = {0};
    int have = 1;
    for (uint32_t i = 0; i < t->n_images && i < 8; i++)
        have &= (img[i] = image_from_rom(&t->images[i], rom, (size_t)size)) != NULL;
    uint32_t bad_width = have ? check_widths(t, img) : 1;
    CHECK(bad_width == 0, "%zu places hold the element widths the game reads (colour unions, viewport table, "
          "flags; %u differ)", sizeof known_widths / sizeof known_widths[0], bad_width);
    for (uint32_t i = 0; i < 8; i++)
        free(img[i]);

    /* Overlay reload: change image A's and B's first data words, load, compare. */
    for (int ov = 0; ov < 2; ov++) {
        const PcDataArena *ar = &t->arenas[2 + ov];
        uint8_t keep[64];
        memcpy(keep, ar->host, sizeof keep);
        memset(ar->host, 0xA5, sizeof keep);
        ar->host[ar->n64_init - ar->n64_lo] = 0x5A;   /* first BSS byte */
        pc_data_overlay_load(ov);
        CHECK(memcmp(keep, ar->host, sizeof keep) == 0 && ar->host[ar->n64_init - ar->n64_lo] == 0,
              "overlay %c reload restores data and zeroes BSS", 'A' + ov);
    }
    printf("datacheck: %s\n", failures ? "FAILED" : "all checks passed");
    return failures != 0;
}
