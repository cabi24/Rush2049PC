/* port/assets/tests/wad_game.c: the game's own WAD loader reads a converted synthetic WAD (wad_build.h).
 *
 * The game's C is built with its PC flags (CMakeLists.txt, test_wad_game): _ReadWave and SetupWaveModel
 * (waves/waves.c), MBSetupWad, MBGetFromWad, MBFindInWad, MBOffsetWad, the TXLD rebase
 * (relocate_texture_loads), the display-list relocation (relocate_model_dl) and its vertex pass (mirror_vertex_texcoords), the
 * mirror pass (MirrorWaveInstance, display_list_traverse, flip_triangle_winding), MBOX_FindTexture_Sub (MBOX_FindTexture_WORK),
 * MBOX_FindPalette_WORK and their slot test (ModelActive), and the texture animation: its set-up
 * (InitTextureAnimation) and per-frame update (TextureAnimation) with the TXLD helpers displaylist_get_texture_image, displaylist_set_texture_image and
 * displaylist_set_tile_size. This file stands in for the rest: the wave and animation tables, a PI DMA from a
 * synthetic cartridge, and the RDRAM window at host 0x80000000, which the game's 32-bit relocation needs. The window
 * is 64 MiB, game.memory's (docs/port/MEMORY.md 3, 6), and the WAD is loaded past 16 MiB too: there the relocated
 * display-list words are K0 addresses (port/ultra/rcpaddr.c), which the walkers follow. */
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
Pair gRomObjDefPtrs[64], gTexDefPtrs[64], gPalDefPtrs[64], gTargetPaths;
int16_t gNumPlayers;
int8_t gBackwards, gMirrorWaves;
int32_t wave_storage_kinds[183], wave_rom_offsets[183], gWaveSizes[183];
uint8_t rom_dma_queue[64];
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
void ValidateWaveInstance(int32_t a, int32_t b, int32_t c) { (void)a, (void)b, (void)c; }
void *game_bsearch(void *key, void *base, uint32_t n, uint32_t size, int (*cmp)(const void *, const void *))
{
    return bsearch(key, base, n, size, cmp);
}

/* The texture-animation tables, in the host layout both of their game views agree on (InitTextureAnimation's
 * Row20/Sequence20/Item12, TextureAnimation's PaletteAnimation20/ObjectAnimation20/AnimationFrame12). */
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
AnimRow *PaletteLists[20];
AnimSequence *TextureSwapLists[20];
AnimItem texture_swap_shared_frames[4];
uint32_t gGameState, gGameState;
int8_t expansion_pak_present, gRestartGame, gTrackNum;
volatile float gFrameSeconds;
volatile int gLastFrameCount;
char *game_strcpy(char *dst, const char *src) { return strcpy(dst, src); }

#define WINDOW (64u << 20)   /* game.memory = 64 */
static uint8_t *g_rdram;
void *pc_n64_ptr(uint32_t a)
{
    uint32_t phys = a >= 0x80000000u && a < 0xC0000000u ? a & 0x1FFFFFFFu : a;
    return phys < WINDOW ? g_rdram + phys : NULL;
}
uint32_t pc_n64_addr(const void *p) { return 0x80000000u + (uint32_t)((const uint8_t *)p - g_rdram); }
uint32_t pc_rdram_size(void) { return WINDOW; }

void ReadWave(int32_t slot);   /* _ReadWave */
void MBSetupWad(void *wad, void *file);
uint32_t MBGetFromWad(void *wad, int32_t tag, int32_t *count);
void MBOffsetWad(void *wad, int32_t file, int32_t offset);   /* MBOffsetWad */
void *MBOX_FindTexture_WORK(char *name, int16_t *out, int8_t lo, int8_t hi, int32_t err);   /* MBOX_FindTexture_Sub */
void InitTextureAnimation(void);   /* the texture animation's set-up */
void TextureAnimation(void);         /* its per-frame update */

/* ---------------------------------------------------------------- checks */

static uint32_t n32(uint32_t a) { uint32_t v; memcpy(&v, pc_n64_ptr(a), 4); return v; }
static uint32_t b32(uint32_t a) { return pc_be32(pc_n64_ptr(a)); }
static uint16_t b16(uint32_t a) { return pc_be16(pc_n64_ptr(a)); }
#define PHYS(a) ((a) & 0xFFFFFFu)
/* A relocated display-list address: physical (the N64's 24 bits) below 16 MiB, past it K0 (docs/port/MEMORY.md 3). */
#define RCP(a) (((a) & 0x1FFFFFFFu) >= 0x01000000u ? (a) : PHYS(a))

/* Loads the synthetic WAD as wave 101 (a track: it has PTHD) into slot at base, as LoadWave would. */
static void load(int slot, uint32_t base)
{
    memset(&gWaveInstance[slot], 0, sizeof gWaveInstance[slot]);
    gWaveInstance[slot].model = 1;
    gWaveInstance[slot].wave = 101;
    gWaveInstance[slot].ptr = pc_n64_ptr(base);
    gWaveInstance[slot].size = 0x1000;
    ReadWave(slot);
}

static void check_tables(int slot, uint32_t base)
{
    uint32_t imag = base + W_IMAG;
    CHECK(gRomObjDefPtrs[slot].p == pc_n64_ptr(base + W_OBHD) && gRomObjDefPtrs[slot].count == 2);
    CHECK(gTexDefPtrs[slot].p == pc_n64_ptr(base + W_TXHD) && gTexDefPtrs[slot].count == 3);
    CHECK(gPalDefPtrs[slot].p == pc_n64_ptr(base + W_PLHD) && gPalDefPtrs[slot].count == 2);
    CHECK(gTargetPaths.p == pc_n64_ptr(base + W_PTHD) && gTargetPaths.count == 3);
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
    CHECK(b32(base + W_TXLD + 4) == RCP(imag + 0x10) && b32(base + W_TXLD + 20) == RCP(imag + 0x20));
    CHECK(b32(base + W_TXLD + 12) == 0x07000000u && b32(base + W_TXLD + 36) == 0x0007C03Cu);
    /* the display lists: relocated w1, big-endian */
    CHECK(b32(base + W_DL0) == 0x01003006u && b32(base + W_DL0 + 4) == RCP(base + W_VTX));
    CHECK(b32(base + W_DL0 + 28) == RCP(base + W_DL_SUB));
    CHECK(pc_mem_is_be(pc_n64_ptr(base + W_DL0)) && pc_mem_is_be(pc_n64_ptr(base + W_TLUT)));
}

int main(void)
{
    g_rdram = mmap((void *)(uintptr_t)PC_RDRAM_BASE, WINDOW, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (g_rdram != (uint8_t *)(uintptr_t)PC_RDRAM_BASE) {
        fprintf(stderr, "wad_game: no RDRAM window at 0x%X; skipped\n", PC_RDRAM_BASE);
        return 77;
    }
    size_t size = wad_build(g_rom + ROM_WAD);
    wave_rom_offsets[101] = ROM_WAD;
    gWaveSizes[101] = (int32_t)size;

    /* One player, forward, mirrored: LOD bit 1 kept, bit 2 becomes a branch; s and the windings flip. */
    uint32_t base = 0x80200000u;
    gNumPlayers = 1, gBackwards = 0, gMirrorWaves = 1;
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
        CHECK(MBOX_FindTexture_WORK("bravo", &out, -1, -1, 0) == pc_n64_ptr(base + W_TXHD + 0x24));
        CHECK(out == (3 << 10 | 1));
        CHECK(MBOX_FindTexture_WORK("zulu", &out, -1, -1, 0) == NULL && out == 0);
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
        PaletteLists[0] = rows;
        TextureSwapLists[0] = seqs;
        gTrackNum = 0, expansion_pak_present = 1, gRestartGame = 0;
        InitTextureAnimation();
        CHECK(rows[0].data == pc_n64_ptr(tlut1) && rows[1].data == pc_n64_ptr(txld));
        CHECK(flip_txld[0].resource == pc_n64_ptr(base + W_TXHD + 0x24));
        CHECK(flip_txld[0].output == (void *)(uintptr_t)PHYS(imag + 0x10));   /* bravo's SETTIMG */
        CHECK(flip_txld[1].output == pc_n64_ptr(imag) && flip_texels[1].output == pc_n64_ptr(imag + 0x20));

        gFrameSeconds = 0.05f, gLastFrameCount = 1;
        TextureAnimation();
        CHECK(b16(tlut1 + 8) == 1985 && b16(tlut1 + 6) == 10561 && b16(tlut1 + 4) == 10241);
        CHECK(b16(tlut1 + 2) == 0xF012 && b16(tlut1 + 10) == 0xF016);              /* untouched */
        CHECK(rows[1].first == 8 && b32(txld + 32) == 0xF2002000u && b32(txld + 36) == 0x0007C03Cu);
        CHECK(b32(txld + 4) == imag);                                              /* SETTIMG = alpha's texels */
        CHECK(n32(base + W_TXHD + 0x18) == imag + 0x20);                           /* alpha = charlie's texels */
        CHECK(n32(base + W_TXHD + 0x1C) == 0x08010000u);                           /* flags kept */
        PaletteLists[0] = NULL, TextureSwapLists[0] = NULL;
    }

    /* Two players, no mirror: bit 1 becomes the branch, bit 2 stays; nothing flips. */
    uint32_t base2 = 0x80300000u;
    gNumPlayers = 2, gMirrorWaves = 0;
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
        MBOffsetWad(&wad, (int32_t)base2, 0x40000);
        CHECK(wad.base == pc_n64_ptr(base2 + 0x40000) && wad.count == W_TOC_N);
        CHECK(wad.toc == pc_n64_ptr(base2 + 0x40000 + W_TOC));
        CHECK(MBGetFromWad(&wad, 0x54584844, &n) == base2 + 0x40000 + W_TXHD && n == 3);
        CHECK(MBGetFromWad(&wad, 0x12345678, &n) == 0 && n == 0);
    }

    /* game.memory: the same WAD past 16 MiB, one player, mirrored, as at base. Its relocated words are K0 addresses,
     * which the mirror pass (display_list_traverse) follows to the called list and the branch, and the vertex pass
     * (mirror_vertex_texcoords) to the vertices; with the N64's 24 bits they would be 16 MiB lower. */
    uint32_t base3 = 0x81200000u;
    gNumPlayers = 1, gBackwards = 0, gMirrorWaves = 1;
    load(5, base3);
    check_tables(5, base3);
    CHECK(gWaveInstance[5].mirrored == 1);
    CHECK(b32(base3 + W_DL0 + 4) == base3 + W_VTX && b32(base3 + W_DL0 + 28) == base3 + W_DL_SUB);   /* K0 */
    CHECK(b32(base3 + W_DL0 + 16) == 0xE0010001u && b32(base3 + W_DL0 + 20) == W_DL1);
    CHECK(b32(base3 + W_DL0 + 32) == 0xDE010000u && b32(base3 + W_DL0 + 36) == base3 + W_DL_TAIL);
    CHECK(b32(base3 + W_TXLD + 4) == base3 + W_IMAG + 0x10);
    CHECK(b32(base3 + W_DL0 + 8) == 0x05000402u);                                          /* windings */
    CHECK(b32(base3 + W_DL_SUB) == 0x06000402u && b32(base3 + W_DL_SUB + 4) == 0x00020604u); /* the call */
    CHECK(b32(base3 + W_DL_TAIL) == 0x05020604u);                                          /* the branch */
    for (int v = 0; v < 3; v++) {
        CHECK(b16(base3 + W_VTX + 16 * v + 6) == ((v < 2 ? 0xC000 : 0x4000) | 32));
        CHECK(b16(base3 + W_VTX + 16 * v + 8) == 1024 - v * 256);
    }

    /* The heap frees the blocks: their registrations go. */
    pc_heap_block_freed(pc_n64_ptr(base), 0x1000);
    pc_heap_block_freed(pc_n64_ptr(base2), 0x1000);
    pc_heap_block_freed(pc_n64_ptr(base3), 0x1000);
    CHECK(!pc_mem_is_be(pc_n64_ptr(base + W_DL0)) && !pc_mem_is_be(pc_n64_ptr(base2 + W_DL0)));
    CHECK(test_be_range_count() == 0);
    return TEST_DONE();
}
