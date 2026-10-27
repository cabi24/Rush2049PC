/* port/assets/wad.c: the WAD container (docs/port/ASSETS.md §3.1, §5; docs/port/LOADERS.md): the font texture
 * waves 22..37, the model, car and track waves 54..119, and the world waves 120..138 (world.c converts their
 * own chunks after this).
 *
 * Swapped in place, with the N64 record sizes kept (the game indexes them by hard-coded strides):
 *   header      u32 TOC offset, u32 entry count
 *   TOC         per entry u32 tag, u32 offset, u32 length in bytes or record count
 *   OBHD 0x58   f32 radius @10; s16 @14, @16; 4 LODs @18 of 0x10: u16 @0, @2; u32 @4, @8, @C
 *   TXHD 0x24   u16 @10, @12, @16; u32 @18, @1C, @20
 *   PLHD 0x18   u16 @12; u32 @14
 *   PTHD 0x24   u32 @10; s16 @14, @16; u32 @18, @1C, @20
 *   PATH 0x44   the points a PTHD record's pts reaches, 17 x 32 bits each
 * The pointer fields among them (displayList, vtxList, texBase, palBase, trigPtr, pts, parent) are offsets in the
 * file: SetupWaveModel adds the file's or the IMAG chunk's address, so on PC they become 32-bit RDRAM-window
 * addresses, valid host pointers once widened (the game views declare them u32 under TARGET_PC).
 *
 * Everything else stays big-endian: the TXLD texture-load lists, the model display lists and their Vtx, IMAG
 * texels and TLUTs. port/gfx reads them in N64 form, and the game's own walkers (entity_lod_select,
 * entity_cull_check, display_list_traverse, func_8008AD6C, func_800966D8, the texture animation) use big-endian
 * accessors. The whole block is one pc_mem_register_be range; port/ultra's PI DMA and decompressors registered
 * the file already, so this only makes the range cover the block. Nothing in the converted records is read
 * through port/gfx.
 *
 * Release: the game's heap reports every block it frees (pc_heap_block_freed, from audio_reverb_update), and
 * pc_wave_released drops a wave's block on request. WAD blocks never move: heap compaction moves only handle
 * blocks, and the only waves loaded as handles are the font data (38..53). Only one N64 thread runs at a time
 * (docs/port/ULTRA.md), so the block list needs no lock. */
#include <stdio.h>
#include <stdlib.h>

#include "assets.h"

#define TAG(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))
#define TAG_OBHD TAG('O', 'B', 'H', 'D')
#define TAG_TXHD TAG('T', 'X', 'H', 'D')
#define TAG_PLHD TAG('P', 'L', 'H', 'D')
#define TAG_PTHD TAG('P', 'T', 'H', 'D')

#define OBHD_SIZE 0x58
#define TXHD_SIZE 0x24
#define PLHD_SIZE 0x18
#define PTHD_SIZE 0x24
#define PATH_POINT_SIZE 0x44

static int verbose(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("RUSH_TRACE_WAVES");
        on = e && *e && *e != '0';
    }
    return on;
}

/* ---------------------------------------------------------------- blocks this file registered */

typedef struct {
    uint8_t *buf;
    size_t size;
    int wave;
} WadBlock;

static WadBlock g_blocks[128];
static size_t g_nblocks;

static void drop_block(size_t i)
{
    pc_mem_unregister_be(g_blocks[i].buf);
    g_blocks[i] = g_blocks[--g_nblocks];
}

/* Drops the blocks inside [p, p + size): a block freed without a release (or a sub-heap freed whole) must not
 * leave a big-endian range over memory the game reuses for native data. */
static void drop_blocks_in(const uint8_t *p, size_t size)
{
    for (size_t i = 0; i < g_nblocks;)
        if (g_blocks[i].buf >= p && g_blocks[i].buf < p + size)
            drop_block(i);
        else
            i++;
}

static void add_block(int wave, uint8_t *buf, size_t size)
{
    for (size_t i = 0; i < g_nblocks;)   /* a stale block that overlaps this one */
        if (g_blocks[i].buf < buf + size && buf < g_blocks[i].buf + g_blocks[i].size)
            drop_block(i);
        else
            i++;
    if (g_nblocks == sizeof g_blocks / sizeof g_blocks[0]) {
        fprintf(stderr, "rush2049pc: wad: more than %zu WAD blocks registered\n", g_nblocks);
        abort();
    }
    g_blocks[g_nblocks++] = (WadBlock){buf, size, wave};
    pc_mem_register_be(buf, size);
}

void pc_assets_wave_released(int wave, uint8_t *buf)
{
    for (size_t i = 0; i < g_nblocks; i++)
        if (g_blocks[i].buf == buf) {
            drop_block(i);
            return;
        }
    (void)wave;
    pc_mem_unregister_be(buf);   /* another format's file, registered by the PI DMA or a decompressor */
}

void pc_assets_wad_block_freed(uint8_t *p, size_t size)
{
    drop_blocks_in(p, size);
}

/* ---------------------------------------------------------------- records */

static void swap_obhd(uint8_t *r)
{
    pc_swap32_at(r + 0x10);           /* radius */
    pc_swap16_n(r + 0x14, 2);         /* kind, lodCount */
    for (int k = 0; k < 4; k++) {
        uint8_t *lod = r + 0x18 + 0x10 * k;
        pc_swap16_n(lod, 2);          /* texidx, flags */
        pc_swap32_n(lod + 4, 3);      /* lodRad, displayList, vtxList */
    }
}

static void swap_txhd(uint8_t *r)
{
    pc_swap16_n(r + 0x10, 2);         /* width, height; size and format @14 are bytes */
    pc_swap16_at(r + 0x16);           /* palIndex */
    pc_swap32_n(r + 0x18, 3);         /* texBase, flags, xflags */
}

static void swap_plhd(uint8_t *r)
{
    pc_swap16_at(r + 0x12);           /* format; start and end @10 are bytes */
    pc_swap32_at(r + 0x14);           /* palBase */
}

static void swap_pthd(uint8_t *r)
{
    pc_swap32_at(r + 0x10);           /* flags */
    pc_swap16_n(r + 0x14, 2);         /* numPts, collIdx */
    pc_swap32_n(r + 0x18, 3);         /* trigPtr, pts, parent */
}

typedef struct {
    uint32_t start, end;
} Span;

static int span_cmp(const void *a, const void *b)
{
    const Span *x = a, *y = b;
    return x->start < y->start ? -1 : x->start > y->start;
}

/* The path points of the (already swapped) PTHD records. Paths may share points, so the spans are merged and
 * every word is swapped once. */
static void swap_paths(int wave, uint8_t *buf, size_t size, const uint8_t *pthd, uint32_t count)
{
    Span *spans = malloc(count * sizeof *spans);
    size_t n = 0;
    if (!spans)
        return;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *r = pthd + (size_t)i * PTHD_SIZE;
        int16_t pts_n;
        uint32_t pts;
        memcpy(&pts_n, r + 0x14, 2);
        memcpy(&pts, r + 0x1C, 4);
        if (pts_n <= 0)
            continue;
        uint64_t end = (uint64_t)pts + (uint64_t)pts_n * PATH_POINT_SIZE;
        if ((pts & 3) || end > size) {
            fprintf(stderr, "rush2049pc: wad %d: path %u's points (0x%X, %d) are outside the file; left as is\n",
                    wave, i, pts, pts_n);
            continue;
        }
        spans[n++] = (Span){pts, (uint32_t)end};
    }
    qsort(spans, n, sizeof *spans, span_cmp);
    uint32_t done = 0;   /* words below this offset are swapped */
    for (size_t i = 0; i < n; i++) {
        uint32_t s = spans[i].start > done ? spans[i].start : done;
        if (s < spans[i].end) {
            pc_swap32_n(buf + s, (spans[i].end - s) / 4);
            done = spans[i].end;
        }
    }
    free(spans);
}

/* ---------------------------------------------------------------- the container */

typedef struct {
    uint32_t tag, rec_size;
    void (*swap)(uint8_t *);
} RecordKind;

static const RecordKind g_kinds[] = {
    {TAG_OBHD, OBHD_SIZE, swap_obhd},
    {TAG_TXHD, TXHD_SIZE, swap_txhd},
    {TAG_PLHD, PLHD_SIZE, swap_plhd},
    {TAG_PTHD, PTHD_SIZE, swap_pthd},
};

/* The header and TOC are still big-endian here. Returns the entry count, or -1 if buf doesn't hold a WAD. */
static long check_toc(int wave, const uint8_t *buf, size_t size)
{
    if (size < 8)
        return -1;
    uint32_t toc = pc_be32(buf), n = pc_be32(buf + 4);
    if (toc & 0x80000000u) {
        fprintf(stderr, "rush2049pc: wad %d: already relocated (TOC 0x%08X); not converted\n", wave, toc);
        return -1;
    }
    if (toc < 8 || (toc & 3) || toc > size || n > (size - toc) / 12) {
        if (toc == 0 && n == 0)
            return -1;   /* an empty file (a blank cartridge's) */
        fprintf(stderr, "rush2049pc: wad %d: no WAD table of contents (offset 0x%X, %u entries, block 0x%zX); "
                "not converted\n", wave, toc, n, size);
        return -1;
    }
    return (long)n;
}

void pc_assets_wad(int wave, uint8_t *buf, size_t size)
{
    long n = check_toc(wave, buf, size);
    if (n < 0)
        return;
    pc_swap32_n(buf, 2);
    uint32_t toc;
    memcpy(&toc, buf, 4);
    uint8_t *entries = buf + toc;
    pc_swap32_n(entries, (size_t)n * 3);

    /* The game fetches each tag's first entry (MBFindInWad); later duplicates are never reached. */
    const uint8_t *pthd = NULL;
    uint32_t pthd_count = 0;
    for (size_t k = 0; k < sizeof g_kinds / sizeof g_kinds[0]; k++) {
        const RecordKind *kind = &g_kinds[k];
        for (long i = 0; i < n; i++) {
            uint32_t e[3];
            memcpy(e, entries + 12 * i, 12);
            if (e[0] != kind->tag)
                continue;
            uint32_t off = e[1], count = e[2];
            if ((off & 3) || off > size || count > (size - off) / kind->rec_size) {
                fprintf(stderr, "rush2049pc: wad %d: %c%c%c%c (0x%X, %u records) is outside the file; left as "
                        "is\n", wave, kind->tag >> 24, kind->tag >> 16 & 0xFF, kind->tag >> 8 & 0xFF,
                        kind->tag & 0xFF, off, count);
                break;
            }
            for (uint32_t j = 0; j < count; j++)
                kind->swap(buf + off + (size_t)j * kind->rec_size);
            if (kind->tag == TAG_PTHD)
                pthd = buf + off, pthd_count = count;
            break;
        }
    }
    if (pthd)
        swap_paths(wave, buf, size, pthd, pthd_count);
    add_block(wave, buf, size);
    if (verbose())
        fprintf(stderr, "rush2049pc: wad %d: %ld chunks converted at %p\n", wave, n, (void *)buf);
}
