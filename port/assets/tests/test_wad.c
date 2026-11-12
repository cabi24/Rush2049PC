/* port/assets/tests/test_wad.c: the WAD converter (wad.c) on a synthetic big-endian WAD (wad_build.h): the OBHD, TXHD,
 * PLHD and PTHD records come out native beside the file, every field with host pointers where the file has offsets
 * (into the file, past IMAG, to another path record), path points shared by two paths are swapped once; the display
 * lists come out native beside the file too, command for command, with host pointers to the lists, vertices and
 * texels they name (the models' from the file's start, the texture loads' from IMAG's), and their vertices native
 * and registered as model data; the file's table of contents, records and big-endian payload are untouched and
 * registered, the release paths drop the records, the lists and the registrations, and files that aren't WADs are
 * left as they are. No window: the records hold host pointers into a block anywhere (docs/port/NATIVE.md steps 7
 * and 10). */
#include <string.h>

#include "assets.h"
#include "pc_game_types.h"
#include "support.h"
#include "wad_build.h"
#include "wad_lists_check.h"

static uint32_t n32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static float nf(const uint8_t *p) { float v; memcpy(&v, p, 4); return v; }
static int16_t ns16(const void *p) { int16_t v; memcpy(&v, p, 2); return v; }

static uint8_t g_buf[0x1000], g_orig[0x1000];

/* A level's or a listed texture's native list (the converter's: PcGfxWords, the game's Gfx). */
#define LIST(p) ((const PcGfxWords *)(const void *)(p))

/* The native display lists of the WAD at b: each command the file's, its addresses host pointers. */
static void check_lists(uint8_t *b)
{
    const struct pc_wad_records *w = pc_wad_records(b);
    const PcGfxWords *dl0 = LIST(w->models[0].lod[0].displayList), *dl1 = LIST(w->models[0].lod[1].displayList);
    const PcGfxWords *m1 = LIST(w->models[1].lod[0].displayList), *txld = LIST(w->textures[1].texBase);
    const PcGfxWords *sub, *tail;
    const uint8_t *v;

    CHECK(dl0 != NULL && dl1 != NULL && m1 != NULL && txld != NULL);
    if (!dl0 || !dl1 || !m1 || !txld)
        return;
    /* model 0, level 0: VTX, TRI1, a marker naming level 1's list, a call, a marker naming the tail, ENDDL */
    CHECK(dl0[0].w0 == 0x01003006u && dl0[1].w0 == W_TRI1_A && dl0[1].w1 == 0);
    CHECK(dl0[2].w0 == 0xE0010001u && dl0[2].w1 == (uintptr_t)dl1);
    CHECK(dl0[3].w0 == 0xDE000000u && dl0[4].w0 == 0xE0010002u && dl0[5].w0 == 0xDF000000u);
    sub = LIST(dl0[3].w1);
    tail = LIST(dl0[4].w1);
    CHECK(sub == dl0 + 6 && sub[0].w0 == W_TRI2_0 && sub[0].w1 == W_TRI2_1 && sub[1].w0 == 0xDF000000u);
    CHECK(tail == dl0 + 8 && tail[0].w0 == W_TRI1_B && tail[1].w0 == 0xDF000000u);
    CHECK(dl1 == dl0 + 10 && dl1[0].w0 == W_TRI1_A && dl1[1].w0 == 0xDF000000u);
    CHECK(m1 == dl0 + 12 && m1[0].w0 == 0xDF000000u);
    /* the vertices: native, beside the file, model data */
    v = (const uint8_t *)dl0[0].w1;
    CHECK(v != NULL && (v < b || v >= b + sizeof g_buf) && pc_mem_is_model_data(v) && pc_mem_is_model_data(v + 47));
    for (int k = 0; v && k < 3; k++) {
        const uint8_t *x = v + 16 * k;
        CHECK(ns16(x) == k * 10 && ns16(x + 2) == k * 20 && ns16(x + 4) == k * 30);
        CHECK((uint16_t)ns16(x + 6) == ((k < 2 ? 0x8000 : 0) | 32) && ns16(x + 8) == k * 256 && ns16(x + 10) == k * 128);
        CHECK(x[12] == 0x11 && x[13] == 0x22 && x[14] == 0x33 && x[15] == 0x44);
    }
    /* texture 1's load list: SETTIMG to its texels past IMAG, SETTILE, E1 + 04 to its own SETTILESIZE (before IMAG) */
    CHECK(txld[0].w0 == 0xFD100000u && txld[0].w1 == (uintptr_t)(b + W_IMAG + 0x10));
    CHECK(txld[1].w0 == 0xF5100000u && txld[1].w1 == 0x07000000u);
    CHECK(txld[2].w0 == 0xE1000000u && txld[2].w1 == (uintptr_t)(txld + 4) && txld[3].w0 == 0x04000000u);
    CHECK(txld[4].w0 == 0xF2000000u && txld[4].w1 == 0x0007C03Cu && txld[5].w0 == 0xDF000000u);
    CHECK(txld + 6 == dl0);   /* the file's order: TXLD, then OBJS */
    CHECK(dl0[13].w0 == 0xDF000000u);   /* after the last: one G_ENDDL */
}

static void check_converted(uint8_t *b)
{
    const struct pc_wad_records *w = pc_wad_records(b);
    uint8_t *imag = b + W_IMAG;

    /* The file keeps its table of contents and its records big-endian. */
    CHECK(pc_be32(b) == W_TOC && pc_be32(b + 4) == W_TOC_N);
    CHECK(memcmp(b + W_TOC, g_orig + W_TOC, W_TOC_N * 12) == 0);
    CHECK(memcmp(b + W_OBHD, g_orig + W_OBHD, W_PATH - W_OBHD) == 0);

    /* OBHD */
    CHECK(w->numModels == 2 && w->models != NULL);
    CHECK(strcmp(w->models[0].name, "model0") == 0);
    CHECK(w->models[0].radius == 100.0f);
    CHECK((uint16_t)w->models[0].unused == 0x1234 && w->models[0].lodCount == 2);
    CHECK(w->models[0].lod[0].texidx == 1 && w->models[0].lod[0].flags == 0x0300 && w->models[0].lod[0].lodRad == 50.0f);
    CHECK(w->models[0].lod[0].displayList != NULL && (uint8_t *)w->models[0].lod[0].vtxList == b + W_VTX);
    CHECK(w->models[0].lod[1].texidx == 2 && w->models[0].lod[1].lodRad == 500.0f);
    CHECK(w->models[0].lod[1].displayList != NULL);
    CHECK(w->models[0].lod[2].displayList == NULL && w->models[0].lod[3].vtxList == NULL);   /* past lodCount */
    CHECK(w->models[1].radius == 2.5f && w->models[1].lodCount == 1);
    CHECK(w->models[1].lod[0].displayList != NULL && w->models[1].lod[1].displayList == NULL);

    /* TXHD: texBase past IMAG, in 32 bits (texture 1's load list lies before IMAG: its native list, check_lists) */
    CHECK(w->numTextures == 3);
    for (int t = 0; t < 3; t++) {
        const struct tTexture *r = &w->textures[t];
        CHECK(r->width == 32 << t && r->height == 16 + t);
        CHECK(r->size == t + 1 && r->format == t * 2);
        CHECK(r->palIndex == (t == 2 ? 1 : -1));
        CHECK(t == 1 || (uint8_t *)r->texBase == imag + 0x10 * t);
        CHECK(r->flags == (t == 1 ? 0x00030000u : 0x08010000u));
        CHECK(r->xflags == 0xA0B0C0D0u + (uint32_t)t);
    }
    CHECK(strcmp(w->textures[2].name, "charlie") == 0);

    /* PLHD */
    CHECK(w->numPalettes == 2);
    CHECK(w->palettes[1].start == 16 && w->palettes[1].end == 31 && w->palettes[0].format == 0x0102);
    CHECK((uint8_t *)w->palettes[0].palBase == b + W_TLUT && (uint8_t *)w->palettes[1].palBase == b + W_TLUT + 0x20);

    /* PTHD: the trigger the next record, the points in the file, the key, no target yet */
    CHECK(w->numPaths == 3);
    CHECK(w->paths[1].flags == 0x00020000u && w->paths[1].numPts == 2 && w->paths[1].collIdx == 0);
    CHECK(w->paths[0].collIdx == -1);
    CHECK(w->paths[0].trigPtr == &w->paths[1] && w->paths[1].trigPtr == &w->paths[2] && w->paths[2].trigPtr == NULL);
    CHECK((uint8_t *)w->paths[1].pts == b + W_PATH + 0x44 * 2 && (uint8_t *)w->paths[0].pts == b + W_PATH);
    CHECK(w->paths[2].key == 0x702 && w->paths[0].parent == NULL);

    /* PATH: every point native once, including point 2, which both paths reach */
    for (int i = 0; i < 4; i++) {
        const uint8_t *pt = b + W_PATH + 0x44 * i;
        for (int k = 0; k < 16; k++)
            CHECK(nf(pt + 4 * k) == (float)(i * 100 + k));
        CHECK(n32(pt + 0x40) == 0xF1A90000u + (uint32_t)i);
    }

    /* the display lists, beside the file, and against the N64's relocation of the file; the file's own payload
     * (TXLD, OBJS's lists and Vtx, IMAG) untouched */
    check_lists(b);
    CHECK(wad_lists_check(b, g_orig, W_SIZE, w, "test WAD") == 0);
    CHECK(memcmp(b + W_TXLD, g_orig + W_TXLD, W_TOC - W_TXLD) == 0);
}

int main(void)
{
    /* A WAD in its block. */
    size_t size = wad_build(g_orig);
    memcpy(g_buf, g_orig, size);
    test_be_reset();
    pc_wave_loaded(101, g_buf, sizeof g_buf);
    check_converted(g_buf);
    CHECK(test_be_range_count() == 1 && test_model_data_count() == 1);
    CHECK(pc_mem_is_be(g_buf + W_DL0) && pc_mem_is_be(g_buf + W_TLUT) && pc_mem_is_be(g_buf + sizeof g_buf - 1));
    CHECK(!pc_mem_is_be(g_buf + sizeof g_buf));

    /* UnloadWaveInstance's release, and the heap's free: the records go too */
    pc_wave_released(101, g_buf);
    CHECK(test_be_range_count() == 0 && test_model_data_count() == 0);
    CHECK(pc_records_find(g_buf, PC_RECORDS_WAD) == NULL);
    memcpy(g_buf, g_orig, size);
    pc_wave_loaded(30, g_buf, sizeof g_buf);   /* a font texture WAD */
    check_converted(g_buf);
    pc_heap_block_freed(g_buf, sizeof g_buf);
    CHECK(test_be_range_count() == 0 && test_model_data_count() == 0);
    CHECK(pc_records_find(g_buf, PC_RECORDS_WAD) == NULL);

    /* The PI DMA registered the file already (port/ultra/pi.c): one range still, and the free drops it. */
    memcpy(g_buf, g_orig, size);
    pc_mem_register_be(g_buf, size);
    pc_wave_loaded(60, g_buf, sizeof g_buf);
    CHECK(test_be_range_count() == 1);
    pc_heap_block_freed(g_buf, sizeof g_buf);
    CHECK(test_be_range_count() == 0);

    /* Two WADs in a sub-heap that is freed whole. */
    {
        static uint8_t heap[0x3000];
        memcpy(heap + 0x100, g_orig, size);
        memcpy(heap + 0x1200, g_orig, size);
        pc_wave_loaded(88, heap + 0x100, 0x1000);
        pc_wave_loaded(89, heap + 0x1200, 0x1000);
        check_converted(heap + 0x100);
        check_converted(heap + 0x1200);
        CHECK(test_be_range_count() == 2 && test_model_data_count() == 2);
        pc_heap_block_freed(heap, sizeof heap);
        CHECK(test_be_range_count() == 0 && test_model_data_count() == 0);
        CHECK(pc_records_find(heap + 0x100, PC_RECORDS_WAD) == NULL && pc_records_find(heap + 0x1200, PC_RECORDS_WAD) == NULL);
    }

    /* A block reused for a new WAD without a release in between: the stale registration and records go. */
    memcpy(g_buf, g_orig, size);
    pc_wave_loaded(101, g_buf, sizeof g_buf);
    memcpy(g_buf + 0x20, g_orig, size);
    pc_wave_loaded(102, g_buf + 0x20, sizeof g_buf - 0x20);
    check_converted(g_buf + 0x20);
    CHECK(test_be_range_count() == 1 && test_model_data_count() == 1);
    CHECK(pc_records_find(g_buf, PC_RECORDS_WAD) == NULL);
    pc_wave_released(102, g_buf + 0x20);
    CHECK(test_be_range_count() == 0 && test_model_data_count() == 0);

    /* A world WAD keeps its WAD's records beside its world's (the same file's records of two kinds). */
    memcpy(g_buf, g_orig, size);
    pc_wave_loaded(120, g_buf, sizeof g_buf);
    CHECK(pc_records_find(g_buf, PC_RECORDS_WAD) != NULL && pc_records_find(g_buf, PC_RECORDS_WORLD) != NULL);
    pc_heap_block_freed(g_buf, sizeof g_buf);

    /* Not WADs: nothing changes, nothing is registered, and the game gets no records. */
    {
        uint8_t b[64];
        memset(b, 0, sizeof b);   /* a blank cartridge's file */
        pc_wave_loaded(54, b, 0);
        pc_wave_loaded(54, b, 4);
        test_put32(b, PC_RDRAM_BASE + 0x1000u);   /* relocated already (the N64 rebased the TOC in place) */
        test_put32(b + 4, 1);
        pc_wave_loaded(54, b, sizeof b);
        CHECK(pc_be32(b) == PC_RDRAM_BASE + 0x1000u);
        test_put32(b, 0x40);          /* the TOC runs past the block */
        test_put32(b + 4, 3);
        pc_wave_loaded(54, b, sizeof b);
        CHECK(pc_be32(b) == 0x40 && pc_be32(b + 4) == 3);
        CHECK(test_be_range_count() == 0);
        CHECK(pc_records_find(b, PC_RECORDS_WAD) == NULL);
        CHECK(pc_wad_records(b)->models == NULL && pc_wad_records(b)->numTextures == 0);
    }

    /* A chunk that runs past the block gives no records; the others are converted. */
    memcpy(g_buf, g_orig, size);
    test_put32(g_buf + W_TOC + 8, 0x1000);   /* OBHD: 0x1000 models */
    pc_wave_loaded(101, g_buf, sizeof g_buf);
    CHECK(pc_wad_records(g_buf)->numModels == 0 && pc_wad_records(g_buf)->models != NULL);
    CHECK(pc_wad_records(g_buf)->numTextures == 3 && pc_wad_records(g_buf)->textures[0].flags == 0x08010000u);
    pc_wave_released(101, g_buf);
    CHECK(test_be_range_count() == 0);

    /* A path whose trigger is no path record has none; one with a level of detail past 4 keeps 4. */
    memcpy(g_buf, g_orig, size);
    test_put32(g_buf + W_PTHD + 0x18, W_PATH);            /* path 0's trigger: a point, not a record */
    test_put16(g_buf + W_OBHD + 0x58 + 0x16, 7);          /* model 1: 7 levels */
    pc_wave_loaded(101, g_buf, sizeof g_buf);
    CHECK(pc_wad_records(g_buf)->paths[0].trigPtr == NULL && pc_wad_records(g_buf)->paths[1].trigPtr != NULL);
    CHECK(pc_wad_records(g_buf)->models[1].lodCount == 4);
    pc_wave_released(101, g_buf);

    /* A WAD without PTHD or TXLD: no paths (NULL with a count of 0, as MBGetFromWad gave them); texture 1's list is
     * still built, but with no TXLD the N64 never made its image an address, which pointed nowhere: none. */
    memcpy(g_buf, g_orig, size);
    test_put32(g_buf + W_TOC + 12 * 3, 0x58585858u);   /* PTHD -> XXXX */
    test_put32(g_buf + W_TOC + 12 * 4, 0x58585858u);   /* TXLD -> XXXX */
    pc_wave_loaded(101, g_buf, sizeof g_buf);
    CHECK(pc_wad_records(g_buf)->paths == NULL && pc_wad_records(g_buf)->numPaths == 0);
    CHECK(pc_wad_records(g_buf)->textures[1].texBase != NULL);
    CHECK(LIST(pc_wad_records(g_buf)->textures[1].texBase)[0].w0 == 0xFD100000u);
    CHECK(LIST(pc_wad_records(g_buf)->textures[1].texBase)[0].w1 == 0);
    pc_wave_released(101, g_buf);
    CHECK(test_model_data_count() == 0);
    return TEST_DONE();
}
