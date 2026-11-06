/* port/ultra/statehash.c: the state hash (docs/port/LOCKSTEP.md 5, NETPLAY.md 1.6 and 5 phase 0.4).
 *
 * At each retrace, after the pads gate and while every N64 thread waits (loop.c's snapshot), the curated regions are
 * hashed: the model records, the car records, the roster, the random seed, the state words, the model tick, the race
 * time, the controllers as the game read them and the cameras. Every `full` retraces the whole RDRAM window, the
 * game's own storage (port/data's symbols kept outside the window because they hold pointers) and the decoupled data
 * (port/data/decoupled.txt) are hashed too.
 *
 * Host pointers. Game data holds host pointers where the N64 held 32-bit addresses (the window's N64 addresses are the
 * same in every process: it is mapped at 0x80000000). A host pointer's value differs from one process to the next
 * (ASLR) and between Windows and Linux (other image layouts), though both point at the same thing. So it is hashed as
 * what port/data's tables say it points to: an own-storage symbol's N64 address and N64 offset (the same on every
 * OS), a game function's N64 address, a decoupled item and offset, a runtime image's C data; a pointer elsewhere in
 * the image (the PC's buffers in game C) only as such. Which values are pointers depends on the bytes (scan()):
 *   - N64-layout data (the window, own storage's untyped bytes): a u64 equal to a named target's first byte or to a
 *     function, which no pair of numbers or texels hits by chance;
 *   - own storage's elements the tables type as pointers or as the N64's longs (8 bytes on the PC: intptr_t): any value
 *     but a 32-bit one (0, a window address, a number), one outside the image taken as "a heap pointer" (a table the
 *     loader copied, a converted file) without its address;
 *   - the PC's own C structs (the curated and decoupled records): an 8-byte-aligned u64 into a named target;
 *   - the game C's sites that keep a pointer cut to 32 bits (TRUNCATED) are left out (zeros).
 * libultra's SI scratch buffers, which hold the host stack's bytes, are taken as zeros (SCRATCH), and the default NaN
 * as the positive one (x86 and ARM make it with other signs). The ctests
 * replay_same_* and hash_aslr compare two processes' hashes, ASLR on, on every synthetic path.
 *
 * xxHash64 is written here from its specification (the XXH64 algorithm's description); no code is taken from the
 * reference implementation. */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "pc_data.h"
#include "pc_game_types.h"
#include "pc_lockstep.h"
#include "ultra.h"

/* ---------------------------------------------------------------- xxHash64 */

#define P1 0x9E3779B185EBCA87ull
#define P2 0xC2B2AE3D27D4EB4Full
#define P3 0x165667B19E3779F9ull
#define P4 0x85EBCA77C2B2AE63ull
#define P5 0x27D4EB2F165667C5ull

static uint64_t rotl(uint64_t x, int r) { return (x << r) | (x >> (64 - r)); }

static uint64_t rd64(const uint8_t *p)   /* little-endian, as the specification reads its lanes */
{
    return (uint64_t)p[0] | (uint64_t)p[1] << 8 | (uint64_t)p[2] << 16 | (uint64_t)p[3] << 24 |
           (uint64_t)p[4] << 32 | (uint64_t)p[5] << 40 | (uint64_t)p[6] << 48 | (uint64_t)p[7] << 56;
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t round64(uint64_t acc, uint64_t lane)
{
    acc += lane * P2;
    acc = rotl(acc, 31);
    return acc * P1;
}

static uint64_t merge64(uint64_t acc, uint64_t v)
{
    acc ^= round64(0, v);
    return acc * P1 + P4;
}

void pc_xxh64_init(PcXxh64 *s, uint64_t seed)
{
    memset(s, 0, sizeof *s);
    s->v[0] = seed + P1 + P2;
    s->v[1] = seed + P2;
    s->v[2] = seed;
    s->v[3] = seed - P1;
}

static void stripe(uint64_t v[4], const uint8_t *p)
{
    v[0] = round64(v[0], rd64(p));
    v[1] = round64(v[1], rd64(p + 8));
    v[2] = round64(v[2], rd64(p + 16));
    v[3] = round64(v[3], rd64(p + 24));
}

void pc_xxh64_update(PcXxh64 *s, const void *data, size_t size)
{
    const uint8_t *p = data;
    s->total += size;
    if (s->used) {   /* fill the stripe begun last time */
        size_t take = 32 - s->used < size ? 32 - s->used : size;
        memcpy(s->mem + s->used, p, take);
        s->used += (uint32_t)take, p += take, size -= take;
        if (s->used < 32)
            return;
        stripe(s->v, s->mem);
        s->used = 0;
    }
    for (; size >= 32; p += 32, size -= 32)
        stripe(s->v, p);
    memcpy(s->mem, p, size);
    s->used = (uint32_t)size;
}

uint64_t pc_xxh64_digest(const PcXxh64 *s)
{
    uint64_t h;
    const uint8_t *p = s->mem, *end = s->mem + s->used;
    if (s->total >= 32) {
        h = rotl(s->v[0], 1) + rotl(s->v[1], 7) + rotl(s->v[2], 12) + rotl(s->v[3], 18);
        for (int i = 0; i < 4; i++)
            h = merge64(h, s->v[i]);
    } else {
        h = s->v[2] + P5;   /* the seed */
    }
    h += s->total;
    for (; p + 8 <= end; p += 8) {
        h ^= round64(0, rd64(p));
        h = rotl(h, 27) * P1 + P4;
    }
    if (p + 4 <= end) {
        h ^= (uint64_t)rd32(p) * P1;
        h = rotl(h, 23) * P2 + P3;
        p += 4;
    }
    for (; p < end; p++) {
        h ^= *p * P5;
        h = rotl(h, 11) * P1;
    }
    h ^= h >> 33;
    h *= P2;
    h ^= h >> 29;
    h *= P3;
    h ^= h >> 32;
    return h;
}

uint64_t pc_xxh64(const void *data, size_t size, uint64_t seed)
{
    PcXxh64 s;
    pc_xxh64_init(&s, seed);
    pc_xxh64_update(&s, data, size);
    return pc_xxh64_digest(&s);
}

/* ---------------------------------------------------------------- host pointers */

#ifdef _WIN32
extern const unsigned char pc_image_base[] __asm__("__ImageBase");   /* the linker's: the image's first byte */
#else
extern char __executable_start[], _end[];
#endif

enum { K_OWN = 1, K_FUNC = 2, K_DECOUPLED = 3, K_CRANGE = 4, K_IMAGE = 5, K_HEAP = 6 };

/* The decoupled data (port/data/decoupled.txt, docs/port/DECOUPLE.md): game data the PC owns with no N64 address,
 * outside the window. Every name of decoupled.txt, in its order (ctest hash_decoupled_list checks): a pointer into
 * one is hashed as its index and offset, and the full hash takes them all. */
#define DECOUPLED(X) X(pc_links) X(pc_models) X(pc_cars) X(pc_tire_fx) X(pc_smoke_shade) X(pc_trail_pts) \
    X(pc_blasts) X(pc_blast_rings) X(pc_skids) X(pc_resurrect_mid) X(pc_engine_sounds) X(pc_screech_sounds) \
    X(pc_car_sounds_on) X(pc_engine_phase) X(pc_cp_last_pos) X(pc_cp_plane) X(pc_finish_cam_time) X(pc_finish_cam) \
    X(pc_drones) X(pc_our_drones) X(pc_mp_steer) X(pc_mp_brake) X(pc_mp_gas) X(pc_mp_dir) X(pc_mp_save_dir) \
    X(pc_carsetup_flags) X(pc_mx_voice) X(pc_mx_nodes) X(pc_mx_prio_links) X(pc_mx_free_links) X(pc_mx_fx_ctrl) \
    X(pc_mx_fx_chan_a) X(pc_mx_fx_chan_b) X(pc_mx_fx_legato) X(pc_mx_streams) X(pc_snd_msgs) X(pc_snd_start_cmds) \
    X(pc_snd_stop_cmds)
#define DECOUPLED_ROW(name) {#name, (const void *)name, (uint32_t)sizeof name},
static const struct {
    const char *name;
    const void *p;
    uint32_t size;
} g_decoupled[] = {DECOUPLED(DECOUPLED_ROW)};
#define N_DECOUPLED ((int)(sizeof g_decoupled / sizeof g_decoupled[0]))
#define TAG(kind, image) (0xFEED0000u | (uint32_t)(kind) << 8 | (uint32_t)(image))

typedef struct {
    uintptr_t lo, hi;
    uint32_t id;     /* the N64 address, or index << 24 */
    uint8_t kind, image;
    int32_t sym;     /* K_OWN: its index in the tables, for the N64 offset of a host one */
} Target;

static struct {
    int ready;
    uintptr_t img_lo, img_hi;
    Target *t;       /* sorted by lo */
    int nt;
    struct { uintptr_t host; uint32_t n64; uint8_t image; } *f;   /* functions, sorted by host */
    int nf;
    /* own storage's pointers by type (port/data's runs): each own symbol's first slot in slots[], and the count */
    uint32_t *own_first, *own_count, *slots;
    uintptr_t *starts;   /* every named target's first byte, and every function, sorted: the window's pointers */
    int nstarts;
} P;

static int by_lo(const void *a, const void *b)
{
    const Target *x = a, *y = b;
    return x->lo < y->lo ? -1 : x->lo > y->lo ? 1 : x->hi > y->hi ? -1 : x->hi < y->hi ? 1 : 0;
}

static int by_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : x > y;
}

static int by_host(const void *a, const void *b)
{
    uintptr_t x = *(const uintptr_t *)a, y = *(const uintptr_t *)b;
    return x < y ? -1 : x > y;
}

static void add_target(uintptr_t lo, size_t size, uint32_t id, int kind, int image, int32_t sym)
{
    if (!lo || !size)
        return;
    P.t = realloc(P.t, (size_t)(P.nt + 1) * sizeof *P.t);
    P.t[P.nt++] = (Target){lo, lo + size, id, (uint8_t)kind, (uint8_t)image, sym};
}

static void pointers_build(void);
static pthread_once_t g_pointers_once = PTHREAD_ONCE_INIT;

static void pointers_init(void)   /* any thread: pc_n64_identity is the game's */
{
    pthread_once(&g_pointers_once, pointers_build);
}

static void pointers_build(void)
{
    const PcDataTables *t = &pc_data_tables;
    P.ready = 1;
#ifdef _WIN32
    {
        const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)(const void *)pc_image_base;
        const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(const void *)(pc_image_base + dos->e_lfanew);
        P.img_lo = (uintptr_t)pc_image_base;
        P.img_hi = P.img_lo + nt->OptionalHeader.SizeOfImage;
    }
#else
    P.img_lo = (uintptr_t)__executable_start;
    P.img_hi = (uintptr_t)_end;
#endif
    for (uint32_t i = 0; i < t->n_syms; i++)
        if (t->syms[i].host && (t->syms[i].flags & (PC_SYM_OWN | PC_SYM_CDEF)))
            add_target((uintptr_t)t->syms[i].host, t->syms[i].host_size, t->syms[i].n64_addr, K_OWN, t->syms[i].image,
                       (int32_t)i);
    for (int i = 0; i < N_DECOUPLED; i++)
        add_target((uintptr_t)g_decoupled[i].p, g_decoupled[i].size, (uint32_t)i << 24, K_DECOUPLED, 0, -1);
    for (uint32_t i = 0; i < t->n_cranges; i++)
        add_target((uintptr_t)t->cranges[i].start, (size_t)(t->cranges[i].end - t->cranges[i].start), 0, K_CRANGE,
                   t->cranges[i].image, -1);
    qsort(P.t, (size_t)P.nt, sizeof *P.t, by_lo);
    P.starts = calloc((size_t)P.nt + t->n_funcs + 1, sizeof *P.starts);
    for (int i = 0; P.starts && i < P.nt; i++)
        P.starts[P.nstarts++] = P.t[i].lo;
    for (uint32_t i = 0; P.starts && i < t->n_funcs; i++)
        if (t->funcs[i].host)
            P.starts[P.nstarts++] = (uintptr_t)t->funcs[i].host;
    if (P.starts)
        qsort(P.starts, (size_t)P.nstarts, sizeof *P.starts, by_host);
    P.f = calloc(t->n_funcs ? t->n_funcs : 1, sizeof *P.f);
    for (uint32_t i = 0; i < t->n_funcs; i++)
        if (t->funcs[i].host) {
            P.f[P.nf].host = (uintptr_t)t->funcs[i].host;
            P.f[P.nf].n64 = t->funcs[i].n64_addr;
            P.f[P.nf++].image = t->funcs[i].image;
        }
    qsort(P.f, (size_t)P.nf, sizeof *P.f, by_host);
    /* Own storage's pointer elements, by their declared types (the loader's runs): there a value outside the image and
     * the window is a pointer into the host's heap (a table the loader copied, a converted file), which the hash
     * takes as such, without its address. Elsewhere bytes have no type here, and only image values are pointers. */
    P.own_first = calloc(t->n_syms + 1, sizeof *P.own_first);
    P.own_count = calloc(t->n_syms + 1, sizeof *P.own_count);
    size_t n = 0, cap = 0;
    for (uint32_t k = 0; P.own_first && P.own_count && k < t->n_syms; k++) {
        const PcDataSym *y = &t->syms[k];
        P.own_first[k] = (uint32_t)n;
        if (!y->host || !(y->flags & PC_SYM_OWN))
            continue;
        for (uint32_t q = 0; q < y->run_count; q++) {
            const PcDataRun *r = &t->runs[y->run_first + q];
            /* pointers, and the N64's longs, which are 8 bytes on the PC because they may hold one (intptr_t: a
             * blit's handle, D_801148CC) */
            if (r->kind != PC_K_PTR && r->kind != PC_K_S32_64 && r->kind != PC_K_U32_64)
                continue;
            for (uint32_t i = 0; i < r->rep; i++)
                for (uint32_t j = 0; j < r->count; j++) {
                    uint32_t ho = r->host_off + i * r->host_stride + j * (uint32_t)sizeof(void *);
                    if (ho + sizeof(void *) > y->host_size)
                        continue;
                    if (n == cap) {
                        cap = cap ? cap * 2 : 4096;
                        P.slots = realloc(P.slots, cap * sizeof *P.slots);
                    }
                    P.slots[n++] = ho;
                }
        }
        P.own_count[k] = (uint32_t)n - P.own_first[k];
        if (P.own_count[k])
            qsort(P.slots + P.own_first[k], P.own_count[k], sizeof *P.slots, by_u32);
    }
}

static int by_patch(const void *a, const void *b);

void pc_hash_image(uintptr_t *start, uintptr_t *end)
{
    pointers_init();
    *start = P.img_lo;
    *end = P.img_hi;
}

/* The N64 offset of host offset hoff in own symbol y, through the loader's runs (the inverse of its map): the same
 * on every OS where their layouts differ (a pointer's 8 bytes against 4, a long's); the host offset in padding. */
static uint32_t own_n64_offset(const PcDataSym *y, uint32_t hoff)
{
    static const uint8_t n64_size[PC_K_COUNT] = {0, 1, 2, 4, 4, 8, 8, 4, 4, 4};
    for (uint32_t k = 0; k < y->run_count; k++) {
        const PcDataRun *r = &pc_data_tables.runs[y->run_first + k];
        uint32_t en = r->kind < PC_K_COUNT ? n64_size[r->kind] : 1, eh, i, within, j;
        eh = r->kind == PC_K_PTR ? (uint32_t)sizeof(void *) : r->kind == PC_K_S32_64 || r->kind == PC_K_U32_64 ? 8 : en;
        if (hoff < r->host_off || !en)
            continue;
        i = r->host_stride ? (hoff - r->host_off) / r->host_stride : 0;
        if (i >= r->rep)
            continue;
        within = hoff - r->host_off - i * r->host_stride;
        j = within / eh;
        if (j < r->count)
            return r->n64_off + i * r->n64_stride + j * en + (within - j * eh < en ? within - j * eh : en - 1);
    }
    return hoff;
}

/* What a u64 that lies in the image points to, as the hash takes it. */
static uint64_t canonical(uintptr_t v)
{
    int lo = 0, hi = P.nf - 1;
    while (lo <= hi) {   /* a function, exactly */
        int m = (lo + hi) / 2;
        if (P.f[m].host == v)
            return (uint64_t)TAG(K_FUNC, P.f[m].image) << 32 | P.f[m].n64;
        if (P.f[m].host < v)
            lo = m + 1;
        else
            hi = m - 1;
    }
    lo = 0, hi = P.nt - 1;
    int at = -1;
    while (lo <= hi) {   /* the last target that starts at or before v */
        int m = (lo + hi) / 2;
        if (P.t[m].lo <= v)
            at = m, lo = m + 1;
        else
            hi = m - 1;
    }
    for (int k = at; k >= 0 && k > at - 8; k--)   /* nested ones (a name inside another's storage) come after it */
        if (v < P.t[k].hi) {
            const Target *g = &P.t[k];
            uint32_t off = (uint32_t)(v - g->lo);
            if (g->kind == K_OWN)
                off = own_n64_offset(&pc_data_tables.syms[g->sym], off);
            return (uint64_t)TAG(g->kind, g->image) << 32 | (uint32_t)(g->id + off);
        }
    /* storage of the program's own that no table names (the PC's buffers in game C: the frame's command lists, the
     * blits; the PC layer's): which one has no name the same on every OS, so only that it is one is hashed */
    return (uint64_t)TAG(K_IMAGE, 0) << 32;
}

uint64_t pc_hash_pointer(uint64_t v)
{
    pointers_init();
    return v - P.img_lo < P.img_hi - P.img_lo ? canonical((uintptr_t)v) : 0;
}

/* pc_prelude.h's: the 32-bit N64 address a pointer stands for where the game keeps one as a number (an id): a window
 * pointer's own; own storage's, its symbol's N64 address and offset, as on the N64; else the low half, as PC_N64A. */
unsigned int pc_n64_identity(const void *p)
{
    uint32_t a = p ? pc_n64_addr(p) : 0;
    uint64_t c;
    if (a || !p)
        return a;
    c = pc_hash_pointer((uintptr_t)p);
    if (c && (uint32_t)(c >> 32) == TAG(K_OWN, (uint32_t)(c >> 32) & 0xFF))
        return (uint32_t)c;
    return (unsigned int)(uintptr_t)p;
}

/* The game C's sites that keep a host pointer's low half in 32 bits (window addresses), found by comparing two
 * processes' windows (docs/port/LOCKSTEP.md 5). Such a word holds an N64 address at times (D_80156BCC at the boot) and a
 * host pointer's low half at others, and a low half completed with the image's high half can't be told from an N64
 * address the image's low range happens to cover in one process: the hash leaves these words out, as zeros. */
static const uint32_t TRUNCATED[] = {
    0x80156BCCu,   /* D_80156BCC = D_80149438, the display list's position (world_collision_response.c:143) */
};

/* libultra's SI scratch buffers, which its C fills from structs on the stack whose other bytes it never sets
 * (__osMakeMotorData's __OSContRamReadFormat: the 32 data bytes; the Pfs code's likewise), so they hold whatever the
 * host's stack held, and differ between processes until the next command overwrites them: device scratch, not the
 * game's state, hashed as zeros. Found by comparing two processes' windows (rumble2p, pak4p, ghost); a build with
 * RUSH_ZERO_LOCALS has zeros there anyway (docs/port/LOCKSTEP.md 5). */
static const struct {
    uint32_t n64, size;
} SCRATCH[] = {
    {0x80037B30u, 256},   /* __osMotorPifBuf */
    {0x80037F60u, 64},    /* __osPfsBuffer */
};

/* ---------------------------------------------------------------- normalised regions */

typedef struct {
    uint32_t off;
    uint8_t size;     /* 8: a host pointer; 4: a cut one, or a scratch word */
    uint8_t image;    /* 1: an image pointer no table names; 2: scratch (SCRATCH); 3: a NaN's sign */
    uint64_t value;
} Patch;

static Patch *g_patch;
static size_t g_npatch, g_cpatch;

static void patch_add(uint32_t off, int size, uint64_t value, int image)
{
    if (g_npatch == g_cpatch) {
        g_cpatch = g_cpatch ? g_cpatch * 2 : 256;
        g_patch = realloc(g_patch, g_cpatch * sizeof *g_patch);
    }
    g_patch[g_npatch++] = (Patch){off, (uint8_t)size, (uint8_t)image, value};
}

/* RUSH_HASH_UNNAMED=1 (diagnosis, docs/port/LOCKSTEP.md 5): each image pointer no table names, as the first full hash
 * finds it (where it is, and its offset in the image), for `nm` to name; the hash takes it without its offset. */
static int g_unnamed_log = -1;
static const char *g_scan_what;

static int is_start(uintptr_t v)
{
    int lo = 0, hi = P.nstarts - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (P.starts[m] == v)
            return 1;
        if (P.starts[m] < v)
            lo = m + 1;
        else
            hi = m - 1;
    }
    return 0;
}

/* How a region's bytes are read for pointers. Data the game keeps in N64 layout (the window, own storage's bytes the
 * tables don't type as pointers) holds numbers and texels too, and two words of them can make a value in the image's
 * range in one process and not in another (where the image's high half is a small number, 16-bit texel data can be
 * it): there only a pointer to a named target's first byte, or to a function, is one, which no pair of numbers hits by
 * chance. The PC's own C structs (the curated and decoupled data) keep their pointers at 8-byte offsets: there any
 * 8-byte-aligned value pointing into a named target is one. */
enum { SCAN_EXACT, SCAN_STRUCT };

/* The patches of the bytes [p, p + n), in order; n64 is p's N64 address in the window (0 elsewhere). */
static void scan(const uint8_t *p, size_t n, uint32_t n64, int mode)
{
    const uintptr_t lo = P.img_lo, span = P.img_hi - P.img_lo;
    size_t t = 0;
    g_npatch = 0;
    if (g_unnamed_log < 0)
        g_unnamed_log = getenv("RUSH_HASH_UNNAMED") != NULL;
    for (size_t o = 0; o + 8 <= n; o += 4) {
        uint64_t v;
        int scratch = 0;
        for (size_t k = 0; n64 && k < sizeof SCRATCH / sizeof *SCRATCH; k++)
            scratch |= n64 + o - SCRATCH[k].n64 < SCRATCH[k].size;
        if (scratch) {
            patch_add((uint32_t)o, 4, 0, 2);
            continue;
        }
        if (n64)   /* a cut pointer at a listed site first: its bytes aren't a u64's */
            while (t < sizeof TRUNCATED / sizeof *TRUNCATED && TRUNCATED[t] < n64 + o)
                t++;
        if (n64 && t < sizeof TRUNCATED / sizeof *TRUNCATED && TRUNCATED[t] == n64 + o) {
            patch_add((uint32_t)o, 4, 0, 2);   /* left out: see TRUNCATED */
            continue;
        }
        memcpy(&v, p + o, 8);
        if (v - lo < span && (mode == SCAN_EXACT ? is_start((uintptr_t)v) : ((uintptr_t)(p + o) & 7) == 0)) {
            uint64_t c = canonical((uintptr_t)v);
            if (mode == SCAN_EXACT || (uint32_t)(c >> 32) != TAG(K_IMAGE, 0)) {   /* else a number's bytes */
                patch_add((uint32_t)o, 8, c, (uint32_t)(c >> 32) == TAG(K_IMAGE, 0));
                if (g_unnamed_log > 0 && g_scan_what && (uint32_t)(c >> 32) == TAG(K_IMAGE, 0))
                    fprintf(stderr, "rush2049pc: hash: unnamed image pointer in %s+0x%zx (%#x): image+0x%llx\n",
                            g_scan_what, o, n64 ? n64 + (uint32_t)o : 0u, (unsigned long long)(v - lo));
                o += 4;   /* its high half is no pointer's start */
                continue;
            }
        }
        /* The default NaN that 0/0 and inf - inf make: x86's SSE sets its sign bit, ARM's FPU doesn't (the game makes
         * some in matrices it never reads). Hashed as the positive one, a float's and a double's high half, so an
         * aarch64 PC and an x86-64 one agree; no other value is touched. */
        uint32_t w = (uint32_t)v;
        if (w == 0xFFC00000u || w == 0xFFF80000u)
            patch_add((uint32_t)o, 4, w & 0x7FFFFFFFu, 3);
    }
}

/* Hash [p, p + n) with the patches of scan(); the normalised bytes to out when given. */
static uint64_t hash_patched(const uint8_t *p, size_t n, uint8_t *out)
{
    PcXxh64 s;
    size_t at = 0;
    pc_xxh64_init(&s, 0);
    for (size_t i = 0; i < g_npatch; i++) {
        const Patch *q = &g_patch[i];
        uint8_t b[8];
        if (q->off < at)   /* overlaps the one before (never seen): that one's bytes stand */
            continue;
        for (int k = 0; k < q->size; k++)
            b[k] = (uint8_t)(q->value >> (8 * k));
        pc_xxh64_update(&s, p + at, q->off - at);
        pc_xxh64_update(&s, b, q->size);
        at = q->off + q->size;
    }
    pc_xxh64_update(&s, p + at, n - at);
    if (out) {
        memcpy(out, p, n);
        for (size_t i = 0, end = 0; i < g_npatch; i++) {
            if (g_patch[i].off < end)
                continue;
            for (int k = 0; k < g_patch[i].size; k++)
                out[g_patch[i].off + k] = (uint8_t)(g_patch[i].value >> (8 * k));
            end = g_patch[i].off + g_patch[i].size;
        }
    }
    return pc_xxh64_digest(&s);
}

static int by_patch(const void *a, const void *b)
{
    const Patch *x = a, *y = b;
    return x->off < y->off ? -1 : x->off > y->off;
}

/* Own symbol k's typed pointers that point into the host's heap (not 0, not the window, not the image): patched as
 * one constant, sorted in among scan()'s. */
static void typed_pointers(const uint8_t *p, size_t n, uint32_t k)
{
    const uintptr_t lo = P.img_lo, span = P.img_hi - P.img_lo;
    size_t before = g_npatch, at = 0;
    uint32_t *slot = P.slots + P.own_first[k];
    if (!P.own_count || !P.own_count[k])
        return;
    for (uint32_t i = 0; i < P.own_count[k]; i++) {
        uint64_t v, c;
        if (slot[i] + 8 > n)
            continue;
        while (at < before && g_patch[at].off + g_patch[at].size <= slot[i])
            at++;
        if (at < before && g_patch[at].off < slot[i] + 8)
            continue;   /* scan() has it (a named target's first byte) */
        memcpy(&v, p + slot[i], 8);
        if (!(v >> 32) || v >> 32 == 0xFFFFFFFFu)
            continue;   /* NULL, a window address, or a long's number (a negative one sign-extended) */
        c = v - lo < span ? canonical((uintptr_t)v) : (uint64_t)TAG(K_HEAP, 0) << 32;
        patch_add(slot[i], 8, c, (uint32_t)(c >> 32) == TAG(K_IMAGE, 0));
    }
    if (g_npatch != before)
        qsort(g_patch, g_npatch, sizeof *g_patch, by_patch);
}

/* own: own storage's symbol index (typed pointers, the rest read as N64 data), -1 for the PC's C structs (curated,
 * decoupled), -2 for the window. */
static uint64_t hash_region_typed(const void *p, size_t n, int own, uint32_t n64, uint32_t *pointers,
                                  uint32_t *image, uint8_t *out)
{
    uint64_t h;
    scan(p, n, n64, own == -1 ? SCAN_STRUCT : SCAN_EXACT);
    if (own >= 0)
        typed_pointers(p, n, (uint32_t)own);
    h = hash_patched(p, n, out);
    for (size_t i = 0; i < g_npatch; i++) {
        *pointers += g_patch[i].image < 2;
        *image += g_patch[i].image == 1;
    }
    return h;
}

/* A region in the window is N64 data (the window's rules); outside, the PC's own C structs. */
static uint64_t hash_region(const void *p, size_t n, uint32_t n64, uint32_t *pointers, uint32_t *image, uint8_t *out)
{
    return hash_region_typed(p, n, n64 ? -2 : -1, n64, pointers, image, out);
}

/* ---------------------------------------------------------------- the regions */

extern int32_t D_8011735C;      /* the random seed: libc's LCG (NETPLAY.md 1.1) */
extern uint32_t state_word_a;   /* gGameState at 0x801174B4, state_word_b after it */
extern int32_t D_80143FF4;      /* the model thread's tick */
extern float D_801543CC;        /* the race time */
extern uint8_t gControllerData[];   /* 4 x 16 bytes: the controllers as ControllerUpdate read them */
extern uint8_t gCamera[];       /* 4 x 0x98: each window's camera */

typedef struct {
    const char *name;
    const void *p;
    uint32_t size;
} Curated;

static const Curated *curated(int *n)
{
    static Curated c[9];
    if (!c[0].name) {
        c[0] = (Curated){"models", pc_models, (uint32_t)sizeof pc_models};
        c[1] = (Curated){"cars", pc_cars, (uint32_t)sizeof pc_cars};
        c[2] = (Curated){"links", pc_links, (uint32_t)sizeof pc_links};
        c[3] = (Curated){"rng", &D_8011735C, 4};
        c[4] = (Curated){"words", &state_word_a, 8};   /* the state words: state_word_a and state_word_b */
        c[5] = (Curated){"tick", &D_80143FF4, 4};
        c[6] = (Curated){"time", &D_801543CC, 4};
        c[7] = (Curated){"pads", gControllerData, 64};
        c[8] = (Curated){"camera", gCamera, 4 * 0x98};
    }
    *n = 9;
    return c;
}

static uint32_t window_n64(const void *p)
{
    uintptr_t a = (uintptr_t)p, w = (uintptr_t)pc_rdram();
    return w && a - w < pc_rdram_size() ? 0x80000000u + (uint32_t)(a - w) : 0;
}

/* ---------------------------------------------------------------- the per-retrace work */

static struct {
    int on;
    uint32_t full_every;
    uint32_t log_every;     /* --log-hash N: print every Nth (0: none) */
    PcStateHash last;
    int have_last;
    /* kept curated bytes: keep entries of curated_bytes each, ring by retrace */
    uint32_t keep, curated_bytes;
    uint8_t *ring;
    PcStateHash *ring_hash;
    /* RUSH_SIM_DESYNC */
    int sim_read;
    uint32_t sim_retrace, sim_addr, sim_xor;
    int sim_on;
    /* --hash-report N:PATH */
    uint32_t report_at;
    const char *report_path;
    void (*source_hash)(void *, uint32_t, const PcStateHash *);
    void *source_user;
} H;

void pc_hash_enable(uint32_t full_every)
{
    H.on = 1;
    if (full_every)
        H.full_every = full_every;
}

int pc_hash_enabled(void) { return H.on; }

const PcStateHash *pc_hash_last(void) { return H.have_last ? &H.last : NULL; }

void pc_hash_keep(uint32_t retraces)
{
    int n;
    const Curated *c = curated(&n);
    H.curated_bytes = 0;
    for (int i = 0; i < n; i++)
        H.curated_bytes += c[i].size;
    free(H.ring), free(H.ring_hash);
    H.ring = retraces ? calloc(retraces, H.curated_bytes) : NULL;
    H.ring_hash = retraces ? calloc(retraces, sizeof *H.ring_hash) : NULL;
    H.keep = H.ring && H.ring_hash ? retraces : 0;
}

/* lockstep.c's: the source's hash callback, and --log-hash */
void pc_hash_set_listener(void (*fn)(void *, uint32_t, const PcStateHash *), void *user)
{
    H.source_hash = fn;
    H.source_user = user;
}

void pc_hash_log(uint32_t every, uint32_t full_every)
{
    H.log_every = every;
    H.full_every = full_every;
    H.on = 1;
}

void pc_hash_report_at(uint32_t n, const char *path)
{
    H.report_at = n;
    H.report_path = path;
    H.on = 1;
}

static void sim_desync_read(void)
{
    const char *e = getenv("RUSH_SIM_DESYNC");
    unsigned long r, a, x = 1;
    char *end;
    H.sim_read = 1;
    if (!e || !*e)
        return;
    r = strtoul(e, &end, 0);
    if (*end == ':') {
        a = strtoul(end + 1, &end, 16);
        if (*end == ':')
            x = strtoul(end + 1, &end, 16);
        if (!*end && pc_n64_ptr((uint32_t)a) && (a & 3) == 0) {
            H.sim_retrace = (uint32_t)r, H.sim_addr = (uint32_t)a, H.sim_xor = (uint32_t)x;
            H.sim_on = 1;
            fprintf(stderr, "rush2049pc: test: the word at 0x%08X changes (xor 0x%X) at retrace %u (RUSH_SIM_DESYNC)\n",
                    H.sim_addr, H.sim_xor, H.sim_retrace);
            return;
        }
    }
    fprintf(stderr, "rush2049pc: RUSH_SIM_DESYNC takes RETRACE:ADDRESS[:XOR] (a word of the window, hex), not '%s'\n",
            e);
}

static void compute(uint32_t n, int full, uint8_t *keep_out)
{
    PcStateHash *h = &H.last;
    int nc;
    const Curated *c = curated(&nc);
    uint64_t sums[PC_HASH_REGIONS];
    pointers_init();
    if (!H.have_last)   /* a process's own: ctest hash_aslr checks two runs had their images apart */
        fprintf(stderr, "rush2049pc: hash: the program's image at %#llx-%#llx\n", (unsigned long long)P.img_lo,
                (unsigned long long)P.img_hi);
    memset(h, 0, sizeof *h);
    h->retrace = n;
    for (int i = 0; i < nc; i++) {
        uint32_t ptrs = 0, img = 0;
        PcHashRegion *r = &h->region[h->n++];
        r->name = c[i].name;
        r->bytes = c[i].size;
        r->hash = sums[i] = hash_region(c[i].p, c[i].size, window_n64(c[i].p), &ptrs, &img, keep_out);
        r->pointers = ptrs;
        r->unnamed = img;
        if (keep_out)
            keep_out += c[i].size;
    }
    h->state = pc_xxh64(sums, (size_t)nc * sizeof sums[0], 0);
    if (full) {
        const PcDataTables *t = &pc_data_tables;
        uint32_t ptrs = 0, img = 0, bytes = 0;
        PcHashRegion *w = &h->region[h->n++], *o = &h->region[h->n++];
        PcXxh64 s;
        g_scan_what = g_unnamed_log > 0 ? "window" : NULL;
        w->name = "window";
        w->bytes = pc_rdram_size();
        w->hash = hash_region_typed(pc_rdram(), pc_rdram_size(), -2, 0x80000000u, &ptrs, &img, NULL);
        w->pointers = ptrs;
        w->unnamed = img;
        ptrs = img = 0;
        pc_xxh64_init(&s, 0);
        for (uint32_t i = 0; i < t->n_syms; i++) {   /* own storage, in N64 order */
            const PcDataSym *y = &t->syms[i];
            uint64_t one;
            if (!y->host || !(y->flags & PC_SYM_OWN))
                continue;
            g_scan_what = g_unnamed_log > 0 ? y->name : NULL;
            one = hash_region_typed(y->host, y->host_size, (int)i, 0, &ptrs, &img, NULL);
            pc_xxh64_update(&s, &one, sizeof one);
            bytes += y->host_size;
        }
        o->name = "own";
        o->bytes = bytes;
        o->hash = pc_xxh64_digest(&s);
        o->pointers = ptrs;
        o->unnamed = img;
        PcHashRegion *d = &h->region[h->n++];
        ptrs = img = bytes = 0;
        pc_xxh64_init(&s, 0);
        for (int i = 0; i < N_DECOUPLED; i++) {
            g_scan_what = g_unnamed_log > 0 ? g_decoupled[i].name : NULL;
            uint64_t one = hash_region(g_decoupled[i].p, g_decoupled[i].size, 0, &ptrs, &img, NULL);
            pc_xxh64_update(&s, &one, sizeof one);
            bytes += g_decoupled[i].size;
        }
        if (g_unnamed_log > 0)
            g_unnamed_log = 0;   /* once */
        g_scan_what = NULL;
        d->name = "decoupled";
        d->bytes = bytes;
        d->hash = pc_xxh64_digest(&s);
        d->pointers = ptrs;
        d->unnamed = img;
        h->full = 1;
        sums[0] = w->hash, sums[1] = o->hash, sums[2] = d->hash;
        h->full_hash = pc_xxh64(sums, 3 * sizeof sums[0], 0);
    }
    H.have_last = 1;
}

static void log_line(const PcStateHash *h)
{
    char line[1024];
    int len = snprintf(line, sizeof line, "rush2049pc: hash vi=%u: %016llx", h->retrace, (unsigned long long)h->state);
    for (int i = 0; i < h->n && len > 0 && (size_t)len < sizeof line; i++) {
        const PcHashRegion *r = &h->region[i];
        len += snprintf(line + len, sizeof line - (size_t)len, " %s %016llx", r->name, (unsigned long long)r->hash);
        if (r->pointers && (size_t)len < sizeof line)
            len += snprintf(line + len, sizeof line - (size_t)len, " ptrs=%u", r->pointers);
        if (r->unnamed && (size_t)len < sizeof line)
            len += snprintf(line + len, sizeof line - (size_t)len, " unnamed=%u", r->unnamed);
    }
    fprintf(stderr, "%s\n", line);
}

void pc_hash_retrace(uint32_t n)
{
    if (!H.sim_read)
        sim_desync_read();
    if (H.sim_on && n == H.sim_retrace) {   /* the mutation check: before this retrace's hash */
        uint32_t *w = pc_n64_ptr(H.sim_addr);
        *w ^= H.sim_xor;
        fprintf(stderr, "rush2049pc: test: retrace %u: the word at 0x%08X is now 0x%08X (RUSH_SIM_DESYNC)\n", n,
                H.sim_addr, *w);
    }
    if (!H.on)
        return;
    int full = (H.full_every && n % H.full_every == 0) || (H.report_path && n == H.report_at);
    uint8_t *keep = H.keep ? H.ring + (size_t)(n % H.keep) * H.curated_bytes : NULL;
    compute(n, full, keep);
    if (H.keep)
        H.ring_hash[n % H.keep] = H.last;
    if (H.log_every && n % H.log_every == 0)
        log_line(&H.last);
    if (H.source_hash)
        H.source_hash(H.source_user, n, &H.last);
    if (H.report_path && n == H.report_at) {
        if (pc_hash_report(H.report_path, n) == 0)
            fprintf(stderr, "rush2049pc: the state hash's report at retrace %u: %s\n", n, H.report_path);
        else
            fprintf(stderr, "rush2049pc: the state hash's report at retrace %u: can't write %s\n", n, H.report_path);
    }
}

/* ---------------------------------------------------------------- the report */

static void hexdump(FILE *f, const uint8_t *p, size_t n)
{
    for (size_t o = 0; o < n; o += 32) {
        fprintf(f, "%06zx:", o);
        for (size_t k = o; k < o + 32 && k < n; k++)
            fprintf(f, "%s%02x", (k - o) % 4 ? "" : " ", p[k]);
        fputc('\n', f);
    }
}

static int write_file(const char *path, const char *ext, const void *p, size_t n)
{
    char name[1024];
    FILE *f;
    snprintf(name, sizeof name, "%s%s", path, ext);
    f = fopen(name, "wb");
    if (!f)
        return -1;
    size_t w = fwrite(p, 1, n, f);
    return fclose(f) == 0 && w == n ? 0 : -1;
}

int pc_hash_report(const char *path, uint32_t n)
{
    int nc, kept = H.keep && H.ring_hash[n % H.keep].retrace == n && H.ring_hash[n % H.keep].n;
    const Curated *c = curated(&nc);
    const PcStateHash *h = kept ? &H.ring_hash[n % H.keep] : H.have_last ? &H.last : NULL;
    uint8_t *bytes = NULL;
    uintptr_t lo, hi;
    FILE *f;
    if (!h)
        return -1;
    f = fopen(path, "w");
    if (!f)
        return -1;
    pc_hash_image(&lo, &hi);
    fprintf(f, "rush2049pc state hash report (docs/port/LOCKSTEP.md 5)\nretrace %u: the hashes and bytes of retrace %u%s\n",
            n, h->retrace, h->retrace == n ? "" : " (the last kept: this retrace's are gone)");
    fprintf(f, "image %#llx-%#llx\nstate %016llx\n", (unsigned long long)lo, (unsigned long long)hi,
            (unsigned long long)h->state);
    for (int i = 0; i < h->n; i++)
        fprintf(f, "region %s %016llx bytes %u pointers %u unnamed %u\n", h->region[i].name,
                (unsigned long long)h->region[i].hash, h->region[i].bytes, h->region[i].pointers,
                h->region[i].unnamed);
    if (kept) {
        bytes = H.ring + (size_t)(n % H.keep) * H.curated_bytes;
    } else {   /* the last retrace's, normalised again (nothing has run since) */
        uint32_t total = 0, ptrs = 0, img = 0;
        for (int i = 0; i < nc; i++)
            total += c[i].size;
        bytes = malloc(total);
        for (int i = 0, at = 0; bytes && i < nc; at += (int)c[i].size, i++)
            hash_region(c[i].p, c[i].size, window_n64(c[i].p), &ptrs, &img, bytes + at);
    }
    if (bytes) {
        const uint8_t *q = bytes;
        for (int i = 0; i < nc; q += c[i].size, i++) {
            fprintf(f, "\n== %s (%u bytes, as hashed)\n", c[i].name, c[i].size);
            hexdump(f, q, c[i].size);
        }
        if (!kept)
            free(bytes);
    }
    if (h->full && h->retrace == n && H.last.retrace == n) {   /* the window and own storage, as hashed, beside it */
        const PcDataTables *t = &pc_data_tables;
        uint32_t ptrs = 0, img = 0, total = 0;
        uint8_t *w = malloc(pc_rdram_size());
        if (w) {
            hash_region_typed(pc_rdram(), pc_rdram_size(), -2, 0x80000000u, &ptrs, &img, w);
            write_file(path, ".window", w, pc_rdram_size());
            free(w);
        }
        fprintf(f, "\n== own storage in %s.own: name, N64 address, offset in the file, bytes\n", path);
        for (uint32_t i = 0; i < t->n_syms; i++)
            if (t->syms[i].host && (t->syms[i].flags & PC_SYM_OWN))
                total += t->syms[i].host_size;
        uint8_t *o = malloc(total ? total : 1), *q = o;
        for (uint32_t i = 0; o && i < t->n_syms; i++) {
            const PcDataSym *y = &t->syms[i];
            if (!y->host || !(y->flags & PC_SYM_OWN))
                continue;
            fprintf(f, "own %s %08X %zu %u\n", y->name, y->n64_addr, (size_t)(q - o), y->host_size);
            hash_region_typed(y->host, y->host_size, (int)i, 0, &ptrs, &img, q);
            q += y->host_size;
        }
        if (o)
            write_file(path, ".own", o, total);
        free(o);
        fprintf(f, "\n== decoupled data in %s.decoupled: name, offset in the file, bytes\n", path);
        total = 0;
        for (int i = 0; i < N_DECOUPLED; i++)
            total += g_decoupled[i].size;
        o = malloc(total), q = o;
        for (int i = 0; o && i < N_DECOUPLED; i++) {
            fprintf(f, "decoupled %s %zu %u\n", g_decoupled[i].name, (size_t)(q - o), g_decoupled[i].size);
            hash_region(g_decoupled[i].p, g_decoupled[i].size, 0, &ptrs, &img, q);
            q += g_decoupled[i].size;
        }
        if (o)
            write_file(path, ".decoupled", o, total);
        free(o);
    }
    return fclose(f) == 0 ? 0 : -1;
}
