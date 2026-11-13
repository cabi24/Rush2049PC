/* port/assets/tests/wad_game.c: the game's own WAD loader reads a converted synthetic WAD (wad_build.h).
 *
 * The game's C is built with its PC flags (CMakeLists.txt, test_wad_game): _ReadWave and SetupWaveModel
 * (waves/waves.c) on the WAD's records and native display lists (port/assets/wad.c), the race's set-up of the lists
 * (prepare_model_dl: the LOD markers' sections) and its vertex pass (mirror_vertex_texcoords), the mirror pass
 * (MirrorWaveInstance, display_list_traverse, flip_triangle_winding), MBOX_FindTexture_Sub (MBOX_FindTexture_WORK),
 * MBOX_FindPalette_WORK and their slot test (ModelActive), and the texture animation: its set-up
 * (InitTextureAnimation) and per-frame update (TextureAnimation) with the load-list helpers
 * displaylist_get_texture_image, displaylist_set_texture_image and displaylist_set_tile_size. This file stands in for
 * the rest: the wave and animation tables and a PI DMA from a synthetic cartridge. The WAD's heap blocks are ordinary
 * host memory: the lists hold host pointers, and nothing needs the RDRAM window (docs/port/NATIVE.md step 10). */
#include <stdlib.h>
#include <string.h>

#include "assets.h"
#include "pc_game_types.h"
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
    memcpy(dram, g_rom + dev, size);   /* the cartridge's bytes as they are, as port/ultra/pi.c copies them */
    return 0;
}
void lzss_decompress(int32_t src, void *dst) { (void)src, (void)dst; abort(); }
void inflate_decompress(int32_t src, void *dst, int32_t flag) { (void)src, (void)dst, (void)flag; abort(); }
void ValidateWaveInstance(int32_t a, int32_t b, int32_t c) { (void)a, (void)b, (void)c; }
void *game_bsearch(void *key, void *base, uint32_t n, uint32_t size, int (*cmp)(const void *, const void *))
{
    return bsearch(key, base, n, size, cmp);
}

/* The texture-animation tables, in the host layout both of their game views agree on (MAT3 tPaletteAnim,
 * tTextureSwap and tTextureBase: InitTextureAnimation's and TextureAnimation's views). */
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

void ReadWave(int32_t slot);   /* _ReadWave */
void *MBOX_FindTexture_WORK(char *name, int16_t *out, int8_t lo, int8_t hi, int32_t err);   /* MBOX_FindTexture_Sub */
void InitTextureAnimation(void);   /* the texture animation's set-up */
void TextureAnimation(void);         /* its per-frame update */

/* ---------------------------------------------------------------- checks */

#define BLOCK 0x1000
static _Alignas(32) uint8_t g_blocks[2][BLOCK];
static uint16_t b16(const uint8_t *p) { return pc_be16(p); }
static int16_t ns16(const void *p) { int16_t v; memcpy(&v, p, 2); return v; }

/* A level's or a listed texture's native list (the converter's: PcGfxWords, the game's Gfx). */
#define LIST(p) ((PcGfxWords *)(void *)(p))

/* Loads the synthetic WAD as wave 101 (a track: it has PTHD) into slot's heap block, as LoadWave would. */
static void load(int slot, uint8_t *block)
{
    memset(&gWaveInstance[slot], 0, sizeof gWaveInstance[slot]);
    gWaveInstance[slot].model = 1;
    gWaveInstance[slot].wave = 101;
    gWaveInstance[slot].ptr = block;
    gWaveInstance[slot].size = BLOCK;
    ReadWave(slot);
}

static const struct pc_wad_records *records(const uint8_t *block)
{
    return pc_wad_records(block);
}

/* The model's lists in the order the converter built them (the file's): model 0's level 0 (6 commands), the list it
 * calls (2), the tail its second marker names (2), level 1 (2), model 1's (1). */
static PcGfxWords *level0(const uint8_t *block)
{
    return LIST(records(block)->models[0].lod[0].displayList);
}

static void check_tables(int slot, uint8_t *b)
{
    const struct pc_wad_records *w = records(b);
    PcGfxWords *dl0 = level0(b);
    CHECK(gRomObjDefPtrs[slot].p == w->models && gRomObjDefPtrs[slot].count == 2);
    CHECK(gTexDefPtrs[slot].p == w->textures && gTexDefPtrs[slot].count == 3);
    CHECK(gPalDefPtrs[slot].p == w->palettes && gPalDefPtrs[slot].count == 2);
    CHECK(gTargetPaths.p == w->paths && gTargetPaths.count == 3);
    CHECK(gWaveInstance[slot].loaded && gWaveInstance[slot].ready);

    /* OBHD: the levels' native lists, in the file's order after texture 1's (6 commands) */
    CHECK(dl0 != NULL && LIST(w->models[0].lod[1].displayList) == dl0 + 10 && LIST(w->models[1].lod[0].displayList) == dl0 + 12);
    CHECK(w->models[0].lod[0].vtxList == b + W_VTX);
    /* TXHD texBase and PLHD palBase past IMAG (texture 1: its native load list); flags native */
    CHECK(w->textures[0].texBase == b + W_IMAG && w->textures[2].texBase == b + W_IMAG + 0x20);
    CHECK(LIST(w->textures[1].texBase) + 6 == dl0);
    CHECK(w->textures[0].flags == 0x08010000u);
    CHECK(w->palettes[1].palBase == b + W_TLUT + 0x20);
    /* PTHD: the points in the file, the trigger a record */
    CHECK((void *)w->paths[1].pts == b + W_PATH + 0x44 * 2 && w->paths[0].trigPtr == &w->paths[1]);

    /* The lists' addresses: host pointers (the call, the vertices native beside the file) */
    CHECK(dl0[0].w0 == 0x01003006u && pc_mem_is_model_data((void *)dl0[0].w1));
    CHECK(dl0[3].w0 == 0xDE000000u && dl0[3].w1 == (uintptr_t)(dl0 + 6));
    CHECK(LIST(w->textures[1].texBase)[0].w1 == (uintptr_t)(b + W_IMAG + 0x10));
    /* The file itself: not relocated (the game no longer reads its lists), its texels and TLUTs texel memory */
    CHECK(memcmp(b + W_TXLD, g_rom + ROM_WAD + W_TXLD, W_IMAG - W_TXLD) == 0);
    CHECK(pc_mem_is_texel_memory(b + W_TLUT) && !pc_mem_is_texel_memory(b + W_TXLD));
}

int main(void)
{
    uint8_t *block = g_blocks[0], *block2 = g_blocks[1];   /* two heap blocks, 32-byte aligned as the heap's are */
    size_t size = wad_build(g_rom + ROM_WAD);
    PcGfxWords *dl0;

    wave_rom_offsets[101] = ROM_WAD;
    gWaveSizes[101] = (int32_t)size;

    /* One player, forward, mirrored: LOD bit 1 kept, bit 2 becomes a branch; s and the windings flip. */
    gNumPlayers = 1, gBackwards = 0, gMirrorWaves = 1;
    load(3, block);
    check_tables(3, block);
    dl0 = level0(block);
    CHECK(gWaveInstance[3].mirrored == 1);
    CHECK(dl0[2].w0 == 0xE0010001u && dl0[2].w1 == (uintptr_t)(dl0 + 10));   /* kept as it was */
    CHECK(dl0[4].w0 == 0xDE010000u && dl0[4].w1 == (uintptr_t)(dl0 + 8));    /* a branch to the tail */
    CHECK(dl0[1].w0 == 0x05000402u);                                           /* windings */
    CHECK(dl0[6].w0 == 0x06000402u && dl0[6].w1 == 0x00020604u);               /* the call */
    CHECK(dl0[8].w0 == 0x05020604u);                                           /* the branch */
    CHECK(dl0[10].w0 == 0x05000402u);                                          /* LOD 1 */
    for (int v = 0; v < 3; v++) {
        const uint8_t *x = (const uint8_t *)dl0[0].w1 + 16 * v;
        CHECK((uint16_t)ns16(x + 6) == ((v < 2 ? 0xC000 : 0x4000) | 32));      /* done bit */
        CHECK(ns16(x + 8) == 1024 - v * 256);                                  /* s mirrored */
        CHECK(ns16(x + 10) == v * 128);
        CHECK(b16(block + W_VTX + 16 * v + 8) == v * 256);                     /* the file's: untouched */
    }

    /* MBOX_FindTexture_Sub over the loaded slots: record and (slot << 10) | index. */
    {
        int16_t out = -1;
        gNumModels = 4;
        CHECK(MBOX_FindTexture_WORK("bravo", &out, -1, -1, 0) == &records(block)->textures[1]);
        CHECK(out == (3 << 10 | 1));
        CHECK(MBOX_FindTexture_WORK("zulu", &out, -1, -1, 0) == NULL && out == 0);
    }

    /* The texture animation over slot 3: a palette light sequence (pal1, its colours written in the file's TLUT, in
     * its texel format), a texture scroll (bravo's load list: its SETTILESIZE), a flip-book on a listed texture
     * (bravo: its SETTIMG) and on a texel texture (alpha: its texBase). */
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
        uint8_t *imag = block + W_IMAG, *tlut1 = block + W_TLUT + 0x20;
        PcGfxWords *txld = LIST(records(block)->textures[1].texBase);
        PaletteLists[0] = rows;
        TextureSwapLists[0] = seqs;
        gTrackNum = 0, expansion_pak_present = 1, gRestartGame = 0;
        InitTextureAnimation();
        CHECK(rows[0].data == tlut1 && rows[1].data == txld);
        CHECK(flip_txld[0].resource == &records(block)->textures[1]);
        CHECK(flip_txld[0].output == imag + 0x10);                               /* bravo's SETTIMG */
        CHECK(flip_txld[1].output == imag && flip_texels[1].output == imag + 0x20);

        gFrameSeconds = 0.05f, gLastFrameCount = 1;
        TextureAnimation();
        CHECK(b16(tlut1 + 8) == 1985 && b16(tlut1 + 6) == 10561 && b16(tlut1 + 4) == 10241);
        CHECK(b16(tlut1 + 2) == 0xF012 && b16(tlut1 + 10) == 0xF016);              /* untouched */
        CHECK(rows[1].first == 8 && txld[4].w0 == 0xF2002000u && txld[4].w1 == 0x0007C03Cu);
        CHECK(txld[0].w1 == (uintptr_t)imag);                                     /* SETTIMG = alpha's texels */
        CHECK(records(block)->textures[0].texBase == imag + 0x20);                /* alpha = charlie's texels */
        CHECK(records(block)->textures[0].flags == 0x08010000u);                  /* flags kept */
        PaletteLists[0] = NULL, TextureSwapLists[0] = NULL;
    }

    /* Two players, no mirror: bit 1 becomes the branch, bit 2 stays; nothing flips. */
    gNumPlayers = 2, gMirrorWaves = 0;
    load(4, block2);
    check_tables(4, block2);
    dl0 = level0(block2);
    CHECK(gWaveInstance[4].mirrored == 0);
    CHECK(dl0[2].w0 == 0xDE010000u && dl0[2].w1 == (uintptr_t)(dl0 + 10));
    CHECK(dl0[4].w0 == 0xE0010002u && dl0[4].w1 == (uintptr_t)(dl0 + 8));
    CHECK(dl0[1].w0 == W_TRI1_A && dl0[6].w0 == W_TRI2_0);
    CHECK((uint16_t)ns16((const uint8_t *)dl0[0].w1 + 6) == 0xC020 && ns16((const uint8_t *)dl0[0].w1 + 8) == 0);

    /* The heap frees the blocks: their registrations and lists go. */
    pc_heap_block_freed(block, BLOCK);
    pc_heap_block_freed(block2, BLOCK);
    CHECK(!pc_mem_is_texel_memory(block + W_TLUT) && !pc_mem_is_texel_memory(block2 + W_TLUT));
    CHECK(test_texel_range_count() == 0 && test_model_data_count() == 0);
    return TEST_DONE();
}
