/* port/assets/tests/wad_game.c: the game's own WAD loader reads a converted synthetic WAD (wad_build.h).
 *
 * The game's C is built with its PC flags (CMakeLists.txt, test_wad_game): _ReadWave and SetupWaveModel
 * (audio_frame_sync/waves.c), MBSetupWad, MBGetFromWad, MBFindInWad, MBOffsetWad, the TXLD rebase
 * (entity_cull_check), the display-list relocation (entity_lod_select) and its vertex pass (func_800966D8), the
 * mirror pass (entity_render_mode, display_list_traverse, func_8008AD6C), MBOX_FindTexture_Sub (func_800B24EC),
 * MBOX_FindPalette_WORK and their slot test (func_80092D80), and the texture animation: its set-up
 * (camera_shake_start) and per-frame update (race_setup_1) with the TXLD helpers func_800BDA24, func_800BD080 and
 * camera_smooth_follow. This file stands in for the rest: the wave and animation tables, a PI DMA from a
 * synthetic cartridge, and the RDRAM window at host 0x80000000, which the game's 32-bit relocation needs. */
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "assets.h"
#include "support.h"
#include "wad_build.h"

/* ---------------------------------------------------------------- what the game files reference */

typedef struct {   /* gWaveInstance, the host layout every game view agrees on */
    int8_t loaded, ready, locked, model;
    uint8_t mirrored;
    int8_t handle;
    uint8_t wave;
    uint32_t size;
    uint8_t *ptr, *lastPtr;
} WaveInstance;
typedef struct {
    void *p;
    int32_t count;
} Pair;

WaveInstance gWaveInstance[64];
Pair gRomObjDefPtrs[64], gTexDefPtrs[64], gPalDefPtrs[64], D_801392D0;
int16_t active_player_count;
int8_t D_80152570, D_80140A04;
int32_t D_80123564[183], D_8011B5BC[183], D_8002E580[183];
uint8_t D_80035410[64];
volatile uint8_t gNumModels;

static uint8_t g_rom[0x2000];
#define ROM_WAD 0x1000

void osInvalICache(void *p, int32_t n) { (void)p, (void)n; }
void osInvalDCache(void *p, int32_t n) { (void)p, (void)n; }
int32_t osRecvMesg(void *mq, void **msg, int32_t flags) { (void)mq, (void)msg, (void)flags; return 0; }
int32_t osPiStartDma(void *mb, int32_t pri, int32_t dir, uint32_t dev, void *dram, uint32_t size, void *mq)
{
    (void)mb, (void)pri, (void)dir, (void)mq;
    memcpy(dram, g_rom + dev, size);
    pc_mem_register_be(dram, size);   /* as port/ultra/pi.c does */
    return 0;
}
void lzss_decompress(int32_t src, void *dst) { (void)src, (void)dst; abort(); }
void inflate_decompress(int32_t src, void *dst, int32_t flag) { (void)src, (void)dst, (void)flag; abort(); }
void func_80096288(int32_t a, int32_t b, int32_t c) { (void)a, (void)b, (void)c; }
void *entity_name_copy(void *key, void *base, uint32_t n, uint32_t size, int (*cmp)(const void *, const void *))
{
    return bsearch(key, base, n, size, cmp);
}
int32_t func_8009508C(const void *a, const void *b) { return strcmp(a, b); }

/* The texture-animation tables, in the host layout both of their game views agree on (camera_shake_start's
 * Row20/Sequence20/Item12, race_setup_1's PaletteAnimation20/ObjectAnimation20/AnimationFrame12). */
typedef struct {
    const char *name;
    int16_t first, last;
    int8_t step, kind;
    int16_t ticks;
    float time;
    void *data;
} AnimRow;
typedef struct {
    const char *name;
    void *resource;
    void *output;
} AnimItem;
typedef struct {
    int16_t count, maximum, direction, current;
    float time, default_time;
    AnimItem *items;
} AnimSequence;
AnimRow *D_8011A840[20];
AnimSequence *D_8011A31C[20];
AnimItem D_8011905C[4];
uint32_t state_word_a, D_801174B4;
int8_t D_80156994, D_80114650, D_8014978C;
volatile float D_8002EB94;
volatile int D_8002EB98;
char *func_800A473C(char *dst, const char *src) { return strcpy(dst, src); }

static uint8_t *g_rdram;
void *pc_n64_ptr(uint32_t a)
{
    uint32_t phys = a >= 0x80000000u && a < 0xC0000000u ? a & 0x1FFFFFFFu : a;
    return phys < PC_RDRAM_SIZE ? g_rdram + phys : NULL;
}
uint32_t pc_n64_addr(const void *p) { return 0x80000000u + (uint32_t)((const uint8_t *)p - g_rdram); }

void func_80097164(int32_t slot);   /* _ReadWave */
void MBSetupWad(void *wad, void *file);
uint32_t MBGetFromWad(void *wad, int32_t tag, int32_t *count);
void func_80096BBC(void *wad, int32_t file, int32_t offset);   /* MBOffsetWad */
void *func_800B24EC(char *name, int16_t *out, int8_t lo, int8_t hi, int32_t err);   /* MBOX_FindTexture_Sub */
void camera_shake_start(void);   /* the texture animation's set-up */
void race_setup_1(void);         /* its per-frame update */

/* ---------------------------------------------------------------- checks */

static uint32_t n32(uint32_t a) { uint32_t v; memcpy(&v, pc_n64_ptr(a), 4); return v; }
static uint32_t b32(uint32_t a) { return pc_be32(pc_n64_ptr(a)); }
static uint16_t b16(uint32_t a) { return pc_be16(pc_n64_ptr(a)); }
#define PHYS(a) ((a) & 0xFFFFFFu)

/* Loads the synthetic WAD as wave 101 (a track: it has PTHD) into slot at base, as LoadWave would. */
static void load(int slot, uint32_t base)
{
    memset(&gWaveInstance[slot], 0, sizeof gWaveInstance[slot]);
    gWaveInstance[slot].model = 1;
    gWaveInstance[slot].wave = 101;
    gWaveInstance[slot].ptr = pc_n64_ptr(base);
    gWaveInstance[slot].size = 0x1000;
    func_80097164(slot);
}

static void check_tables(int slot, uint32_t base)
{
    uint32_t imag = base + W_IMAG;
    CHECK(gRomObjDefPtrs[slot].p == pc_n64_ptr(base + W_OBHD) && gRomObjDefPtrs[slot].count == 2);
    CHECK(gTexDefPtrs[slot].p == pc_n64_ptr(base + W_TXHD) && gTexDefPtrs[slot].count == 3);
    CHECK(gPalDefPtrs[slot].p == pc_n64_ptr(base + W_PLHD) && gPalDefPtrs[slot].count == 2);
    CHECK(D_801392D0.p == pc_n64_ptr(base + W_PTHD) && D_801392D0.count == 3);
    CHECK(gWaveInstance[slot].loaded && gWaveInstance[slot].ready);

    /* OBHD: display lists relocated to window addresses */
    CHECK(n32(base + W_OBHD + 0x20) == base + W_DL0 && n32(base + W_OBHD + 0x24) == base + W_VTX);
    CHECK(n32(base + W_OBHD + 0x30) == base + W_DL1);
    CHECK(n32(base + W_OBHD + 0x58 + 0x20) == base + W_DL_M1);
    /* TXHD texBase and PLHD palBase relocated by IMAG; flags native */
    CHECK(n32(base + W_TXHD + 0x18) == imag && n32(base + W_TXHD + 0x24 * 2 + 0x18) == imag + 0x20);
    CHECK(n32(base + W_TXHD + 0x24 + 0x18) == base + W_TXLD);   /* texture 1: its TXLD list */
    CHECK(n32(base + W_TXHD + 0x1C) == 0x08010000u);
    CHECK(n32(base + W_PLHD + 0x18 + 0x14) == base + W_TLUT + 0x20);
    /* PTHD pts and trigPtr relocated by the file; PATH native */
    CHECK(n32(base + W_PTHD + 0x24 + 0x1C) == base + W_PATH + 0x44 * 2);
    CHECK(n32(base + W_PTHD + 0x24 + 0x18) == base + W_PATH + 0x44 * 2 + 0x40);
    CHECK(n32(base + W_PATH + 0x44 * 3 + 0x40) == 0xF1A90003u);

    /* TXLD rebased by IMAG, big-endian */
    CHECK(b32(base + W_TXLD + 4) == PHYS(imag + 0x10) && b32(base + W_TXLD + 20) == PHYS(imag + 0x20));
    CHECK(b32(base + W_TXLD + 12) == 0x07000000u && b32(base + W_TXLD + 36) == 0x0007C03Cu);
    /* the display lists: relocated w1, big-endian */
    CHECK(b32(base + W_DL0) == 0x01003006u && b32(base + W_DL0 + 4) == PHYS(base + W_VTX));
    CHECK(b32(base + W_DL0 + 28) == PHYS(base + W_DL_SUB));
    CHECK(pc_mem_is_be(pc_n64_ptr(base + W_DL0)) && pc_mem_is_be(pc_n64_ptr(base + W_TLUT)));
}

int main(void)
{
    g_rdram = mmap((void *)(uintptr_t)PC_RDRAM_BASE, PC_RDRAM_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (g_rdram != (uint8_t *)(uintptr_t)PC_RDRAM_BASE) {
        fprintf(stderr, "wad_game: no RDRAM window at 0x%X; skipped\n", PC_RDRAM_BASE);
        return 77;
    }
    size_t size = wad_build(g_rom + ROM_WAD);
    D_8011B5BC[101] = ROM_WAD;
    D_8002E580[101] = (int32_t)size;

    /* One player, forward, mirrored: LOD bit 1 kept, bit 2 becomes a branch; s and the windings flip. */
    uint32_t base = 0x80200000u;
    active_player_count = 1, D_80152570 = 0, D_80140A04 = 1;
    load(3, base);
    check_tables(3, base);
    CHECK(gWaveInstance[3].mirrored == 1);
    CHECK(b32(base + W_DL0 + 16) == 0xE0010001u && b32(base + W_DL0 + 20) == W_DL1);   /* kept as it was */
    CHECK(b32(base + W_DL0 + 32) == 0xDE010000u && b32(base + W_DL0 + 36) == PHYS(base + W_DL_TAIL));
    CHECK(b32(base + W_DL0 + 8) == 0x05000402u);                                          /* windings */
    CHECK(b32(base + W_DL_SUB) == 0x06000402u && b32(base + W_DL_SUB + 4) == 0x00020604u);  /* the call */
    CHECK(b32(base + W_DL_TAIL) == 0x05020604u);                                          /* the branch */
    CHECK(b32(base + W_DL1) == 0x05000402u);                                              /* LOD 1 */
    for (int v = 0; v < 3; v++) {
        CHECK(b16(base + W_VTX + 16 * v + 6) == ((v < 2 ? 0xC000 : 0x4000) | 32));       /* done bit */
        CHECK(b16(base + W_VTX + 16 * v + 8) == 1024 - v * 256);                         /* s mirrored */
        CHECK(b16(base + W_VTX + 16 * v + 10) == v * 128);
    }

    /* MBOX_FindTexture_Sub over the loaded slots: record and (slot << 10) | index. */
    {
        int16_t out = -1;
        gNumModels = 4;
        CHECK(func_800B24EC("bravo", &out, -1, -1, 0) == pc_n64_ptr(base + W_TXHD + 0x24));
        CHECK(out == (3 << 10 | 1));
        CHECK(func_800B24EC("zulu", &out, -1, -1, 0) == NULL && out == 0);
    }

    /* The texture animation over slot 3: a palette light sequence (pal1, rewritten big-endian), a texture scroll
     * (bravo's TXLD SETTILESIZE), a flip-book on a TXLD texture (bravo: its SETTIMG) and on a texel texture
     * (alpha: its texBase). */
    {
        static AnimRow rows[3] = {
            {"pal1", 2, 15, 0, 6, 1, 0, NULL},
            {"bravo", 0, 1000, 8, 9, 1, 0, NULL},
            {NULL, 0, 0, 0, 0, 0, 0, NULL},
        };
        static AnimItem flip_txld[2] = {{"bravo", NULL, NULL}, {"alpha", NULL, NULL}};
        static AnimItem flip_texels[2] = {{"alpha", NULL, NULL}, {"charlie", NULL, NULL}};
        static AnimSequence seqs[3] = {
            {2, 0, 1, 0, 0, 0, flip_txld}, {2, 0, 1, 0, 0, 0, flip_texels}, {0, 0, 0, 0, 0, 0, NULL},
        };
        uint32_t imag = base + W_IMAG, txld = base + W_TXLD, tlut1 = base + W_TLUT + 0x20;
        D_8011A840[0] = rows;
        D_8011A31C[0] = seqs;
        D_8014978C = 0, D_80156994 = 1, D_80114650 = 0;
        camera_shake_start();
        CHECK(rows[0].data == pc_n64_ptr(tlut1) && rows[1].data == pc_n64_ptr(txld));
        CHECK(flip_txld[0].resource == pc_n64_ptr(base + W_TXHD + 0x24));
        CHECK(flip_txld[0].output == (void *)(uintptr_t)PHYS(imag + 0x10));   /* bravo's SETTIMG */
        CHECK(flip_txld[1].output == pc_n64_ptr(imag) && flip_texels[1].output == pc_n64_ptr(imag + 0x20));

        D_8002EB94 = 0.05f, D_8002EB98 = 1;
        race_setup_1();
        CHECK(b16(tlut1 + 8) == 1985 && b16(tlut1 + 6) == 10561 && b16(tlut1 + 4) == 10241);
        CHECK(b16(tlut1 + 2) == 0xF012 && b16(tlut1 + 10) == 0xF016);              /* untouched */
        CHECK(rows[1].first == 8 && b32(txld + 32) == 0xF2002000u && b32(txld + 36) == 0x0007C03Cu);
        CHECK(b32(txld + 4) == imag);                                              /* SETTIMG = alpha's texels */
        CHECK(n32(base + W_TXHD + 0x18) == imag + 0x20);                           /* alpha = charlie's texels */
        CHECK(n32(base + W_TXHD + 0x1C) == 0x08010000u);                           /* flags kept */
        D_8011A840[0] = NULL, D_8011A31C[0] = NULL;
    }

    /* Two players, no mirror: bit 1 becomes the branch, bit 2 stays; nothing flips. */
    uint32_t base2 = 0x80300000u;
    active_player_count = 2, D_80140A04 = 0;
    load(4, base2);
    check_tables(4, base2);
    CHECK(gWaveInstance[4].mirrored == 0);
    CHECK(b32(base2 + W_DL0 + 16) == 0xDE010000u && b32(base2 + W_DL0 + 20) == PHYS(base2 + W_DL1));
    CHECK(b32(base2 + W_DL0 + 32) == 0xE0010002u && b32(base2 + W_DL0 + 36) == W_DL_TAIL);
    CHECK(b32(base2 + W_DL0 + 8) == W_TRI1_A && b32(base2 + W_DL_SUB) == W_TRI2_0);
    CHECK(b16(base2 + W_VTX + 6) == 0xC020 && b16(base2 + W_VTX + 8) == 0);

    /* MBOffsetWad: the WAD at base2 moves up by 0x40000; the handle follows it. */
    {
        struct { uint32_t *base; int32_t count; void *toc; } wad;
        int32_t n = -1;
        memmove(pc_n64_ptr(base2 + 0x40000), pc_n64_ptr(base2), 0x1000);
        func_80096BBC(&wad, (int32_t)base2, 0x40000);
        CHECK(wad.base == pc_n64_ptr(base2 + 0x40000) && wad.count == W_TOC_N);
        CHECK(wad.toc == pc_n64_ptr(base2 + 0x40000 + W_TOC));
        CHECK(MBGetFromWad(&wad, 0x54584844, &n) == base2 + 0x40000 + W_TXHD && n == 3);
        CHECK(MBGetFromWad(&wad, 0x12345678, &n) == 0 && n == 0);
    }

    /* The heap frees the blocks: their registrations go. */
    pc_heap_block_freed(pc_n64_ptr(base), 0x1000);
    pc_heap_block_freed(pc_n64_ptr(base2), 0x1000);
    CHECK(!pc_mem_is_be(pc_n64_ptr(base + W_DL0)) && !pc_mem_is_be(pc_n64_ptr(base2 + W_DL0)));
    CHECK(test_be_range_count() == 0);
    return TEST_DONE();
}
