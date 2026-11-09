/* port/assets/wad.c: the WAD container (docs/port/ASSETS.md 3.1, docs/port/NATIVE.md 4.4 and step 7): the font texture
 * waves 22..37, the model, car and track waves 54..119, and the world waves 120..138 (world.c reads their own chunks
 * after this).
 *
 * A WAD is a table of contents of chunks: the header's u32 TOC offset and entry count, then per entry a u32 tag, the
 * chunk's u32 offset in the file and a u32 count (records, or bytes). The game reads four chunks as records, which the
 * file holds in the N64's layout, big-endian, with 32-bit offsets where MAT3's records have pointers:
 *   OBHD 0x58   name[16]; f32 radius @10; s16 unused @14, lodCount @16; 4 LODs @18 of 0x10: u16 texidx @0, flags @2,
 *               f32 lodRad @4, u32 displayList @8 and vtxList @C (offsets in the file)
 *   TXHD 0x24   name[16]; u16 width @10, height @12; u8 size @14, format @15; s16 palIndex @16; u32 texBase @18 (an
 *               offset from IMAG, in 32-bit arithmetic: a texture's load list in TXLD lies before IMAG); u32 flags
 *               @1C, xflags @20
 *   PLHD 0x18   name[16]; u8 start @10, end @11; u16 format @12; u32 palBase @14 (an offset from IMAG)
 *   PTHD 0x24   name[16]; u32 flags @10; s16 numPts @14, collIdx @16; u32 trigPtr @18 (another PTHD record's offset,
 *               or 0), pts @1C (an offset in the file), parent @20 (the key the world's target groups know it by)
 *   PATH 0x44   the points a PTHD record's pts reaches, 17 x 32 bits each (TargetPathPt)
 * On the N64 SetupWaveModel (MBSetupWad, MBGetFromWad) added the file's or IMAG's address to those offsets in place,
 * making them 32-bit addresses. Here the converter builds the records beside the file instead, native (struct
 * pc_wad_records, port/include/pc_game_types.h), with host pointers where the file has offsets, and keeps them with
 * the file's heap block (records.c); SetupWaveModel takes them by the block (pc_wad_records). The file's table of
 * contents and its record chunks stay as the file has them: the game no longer reads them there. The paths' points
 * are read where they are, through the paths' pts, so they are swapped in place.
 *
 * Everything else stays big-endian: the TXLD texture-load lists, the model display lists and their Vtx, IMAG texels
 * and TLUTs. port/gfx reads them in N64 form, and the game's own walkers (relocate_model_dl, relocate_texture_loads,
 * display_list_traverse, flip_triangle_winding, mirror_vertex_texcoords, the texture animation) use big-endian
 * accessors, until docs/port/NATIVE.md step 10. The whole block is one pc_mem_register_be range; port/ultra's PI DMA
 * and decompressors registered the file already, so this only makes the range cover the block.
 *
 * Release: the game's heap reports every block it frees (pc_heap_block_freed, from DeallocateMemoryInternal), and
 * pc_wave_released drops a wave's block on request; the records go with it (records.c). WAD blocks never move: heap
 * compaction moves only handle blocks, and the only waves loaded as handles are the font data (38..53). Only one N64
 * thread runs at a time (docs/port/ULTRA.md), so the block list needs no lock. */
#include <stdio.h>
#include <stdlib.h>

#include "assets.h"
#include "pc_game_types.h"

#define TAG(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))
#define TAG_OBHD TAG('O', 'B', 'H', 'D')
#define TAG_TXHD TAG('T', 'X', 'H', 'D')
#define TAG_PLHD TAG('P', 'L', 'H', 'D')
#define TAG_PTHD TAG('P', 'T', 'H', 'D')
#define TAG_TXLD TAG('T', 'X', 'L', 'D')
#define TAG_IMAG TAG('I', 'M', 'A', 'G')

/* The records' sizes in the file */
#define OBHD_SIZE 0x58
#define TXHD_SIZE 0x24
#define PLHD_SIZE 0x18
#define PTHD_SIZE 0x24
#define PATH_POINT_SIZE 0x44
#define LOD_SIZE 0x10

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

/* ---------------------------------------------------------------- the table of contents */

typedef struct {
    uint32_t off, count;   /* the chunk's offset in the file and the TOC's count word */
    int found;
} Chunk;

static float be_f32(const uint8_t *p)
{
    uint32_t u = pc_be32(p);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

/* The header and TOC, big-endian. Returns the entry count, or -1 if buf doesn't hold a WAD. */
static long check_toc(int wave, const uint8_t *buf, size_t size)
{
    if (size < 8)
        return -1;
    uint32_t toc = pc_be32(buf), n = pc_be32(buf + 4);
    if (toc < 8 || (toc & 3) || toc > size || n > (size - toc) / 12) {
        if (toc == 0 && n == 0)
            return -1;   /* an empty file (a blank cartridge's) */
        fprintf(stderr, "rush2049pc: wad %d: no WAD table of contents (offset 0x%X, %u entries, block 0x%zX); "
                "not converted\n", wave, toc, n, size);
        return -1;
    }
    return (long)n;
}

/* The first entry tagged tag (the game fetched each tag's first, MBFindInWad; later duplicates were never reached). */
static Chunk find_chunk(const uint8_t *buf, long n, uint32_t tag)
{
    const uint8_t *e = buf + pc_be32(buf);
    for (long i = 0; i < n; i++, e += 12)
        if (pc_be32(e) == tag)
            return (Chunk){pc_be32(e + 4), pc_be32(e + 8), 1};
    return (Chunk){0, 0, 0};
}

/* A record chunk's count when its records fit the file, else 0, said so. */
static int records_in(int wave, Chunk c, uint32_t tag, size_t rec_size, size_t size)
{
    if (!c.found || !c.count)
        return 0;
    if ((c.off & 3) || c.off > size || c.count > (size - c.off) / rec_size || c.count > 0x7FFFFFFF) {
        fprintf(stderr, "rush2049pc: wad %d: %c%c%c%c (0x%X, %u records) is outside the file: none\n", wave,
                tag >> 24, tag >> 16 & 0xFF, tag >> 8 & 0xFF, tag & 0xFF, c.off, c.count);
        return 0;
    }
    return (int)c.count;
}

/* ---------------------------------------------------------------- the records */

/* The file at offset off (32 bits, as the N64's sum was), or NULL past its end. */
static void *in_file(uint8_t *buf, size_t size, uint32_t off)
{
    return off < size ? buf + off : NULL;
}

static void build_models(int wave, struct pc_wad_records *w, uint8_t *buf, size_t size, Chunk c)
{
    int n = records_in(wave, c, TAG_OBHD, OBHD_SIZE, size);
    if (!c.found)
        return;
    w->models = calloc(n ? (size_t)n : 1, sizeof *w->models);
    if (!w->models)
        abort();
    w->numModels = n;
    for (int i = 0; i < n; i++) {
        const uint8_t *r = buf + c.off + (size_t)i * OBHD_SIZE;
        struct tModelDef *m = &w->models[i];
        memcpy(m->name, r, sizeof m->name);
        m->radius = be_f32(r + 0x10);
        m->unused = (short)pc_be16(r + 0x14);
        m->lodCount = (short)pc_be16(r + 0x16);
        if (m->lodCount > 4) {   /* the N64 relocated the record after it as its levels past 4 */
            fprintf(stderr, "rush2049pc: wad %d: model %d ('%.16s') has %d levels of detail; 4 kept\n", wave, i,
                    m->name, m->lodCount);
            m->lodCount = 4;
        }
        for (int k = 0; k < 4; k++) {
            const uint8_t *l = r + 0x18 + LOD_SIZE * k;
            struct tLod *lod = &m->lod[k];
            uint32_t dl = pc_be32(l + 8);
            lod->texidx = pc_be16(l);
            lod->flags = pc_be16(l + 2);
            lod->lodRad = be_f32(l + 4);
            /* The N64 relocated the levels it has (lodCount) and left the rest offsets, which nothing reads. */
            if (k >= m->lodCount)
                continue;
            lod->displayList = in_file(buf, size, dl);
            lod->vtxList = in_file(buf, size, pc_be32(l + 12));   /* unread: no check of the file's */
            if (!lod->displayList || (dl & 7))
                fprintf(stderr, "rush2049pc: wad %d: model %d ('%.16s') LOD %d's display list 0x%X is not an 8-aligned "
                        "place in the file\n", wave, i, m->name, k, dl);
        }
    }
}

static void build_textures(int wave, struct pc_wad_records *w, uint8_t *buf, size_t size, Chunk c, Chunk imag)
{
    int n = records_in(wave, c, TAG_TXHD, TXHD_SIZE, size);
    if (!c.found)
        return;
    w->textures = calloc(n ? (size_t)n : 1, sizeof *w->textures);
    if (!w->textures)
        abort();
    w->numTextures = n;
    for (int i = 0; i < n; i++) {
        const uint8_t *r = buf + c.off + (size_t)i * TXHD_SIZE;
        struct tTexture *t = &w->textures[i];
        uint32_t base = pc_be32(r + 0x18);
        memcpy(t->name, r, sizeof t->name);
        t->width = pc_be16(r + 0x10);
        t->height = pc_be16(r + 0x12);
        t->size = r[0x14];
        t->format = r[0x15];
        t->palIndex = (short)pc_be16(r + 0x16);
        t->flags = pc_be32(r + 0x1C);
        t->xflags = pc_be32(r + 0x20);
        /* texBase is IMAG's offset plus the record's, in 32 bits (a load list before IMAG is a negative offset) */
        t->texBase = imag.found ? in_file(buf, size, imag.off + base) : NULL;
        if (!imag.found)
            fprintf(stderr, "rush2049pc: wad %d: texture %d ('%.16s') has no texels: the file has no IMAG\n", wave, i,
                    t->name);
        else if (!t->texBase)
            fprintf(stderr, "rush2049pc: wad %d: texture %d ('%.16s') at 0x%X past IMAG is outside the file\n", wave,
                    i, t->name, base);
    }
}

static void build_palettes(int wave, struct pc_wad_records *w, uint8_t *buf, size_t size, Chunk c, Chunk imag)
{
    int n = records_in(wave, c, TAG_PLHD, PLHD_SIZE, size);
    if (!c.found)
        return;
    w->palettes = calloc(n ? (size_t)n : 1, sizeof *w->palettes);
    if (!w->palettes)
        abort();
    w->numPalettes = n;
    for (int i = 0; i < n; i++) {
        const uint8_t *r = buf + c.off + (size_t)i * PLHD_SIZE;
        struct tPaletteDef *p = &w->palettes[i];
        uint32_t base = pc_be32(r + 0x14);
        memcpy(p->name, r, sizeof p->name);
        p->start = r[0x10];
        p->end = r[0x11];
        p->format = pc_be16(r + 0x12);
        p->palBase = imag.found ? in_file(buf, size, imag.off + base) : NULL;   /* IMAG's offset plus the record's */
        if (!p->palBase)
            fprintf(stderr, "rush2049pc: wad %d: palette %d ('%.16s') at 0x%X past IMAG is outside the file\n", wave,
                    i, p->name, base);
    }
}

typedef struct {
    uint32_t start, end;
} Span;

static int span_cmp(const void *a, const void *b)
{
    const Span *x = a, *y = b;
    return x->start < y->start ? -1 : x->start > y->start;
}

/* The paths, and their points swapped in place in the file. Paths may share points, so the spans are merged and every
 * word is swapped once. */
static void build_paths(int wave, struct pc_wad_records *w, uint8_t *buf, size_t size, Chunk c)
{
    int n = records_in(wave, c, TAG_PTHD, PTHD_SIZE, size);
    Span *spans;
    size_t nspans = 0;

    if (!c.found)
        return;
    spans = malloc((n ? (size_t)n : 1) * sizeof *spans);
    w->paths = calloc(n ? (size_t)n : 1, sizeof *w->paths);
    if (!w->paths || !spans)
        abort();
    w->numPaths = n;
    for (int i = 0; i < n; i++) {
        const uint8_t *r = buf + c.off + (size_t)i * PTHD_SIZE;
        struct tBasicTargetPath *p = &w->paths[i];
        uint32_t trig = pc_be32(r + 0x18), pts = pc_be32(r + 0x1C);
        memcpy(p->name, r, sizeof p->name);
        p->flags = pc_be32(r + 0x10);
        p->numPts = (short)pc_be16(r + 0x14);
        p->collIdx = (short)pc_be16(r + 0x16);
        p->key = (int)pc_be32(r + 0x20);
        p->parent = NULL;
        /* The trigger is another record of this chunk; 0 is none. */
        if (trig >= c.off && (trig - c.off) % PTHD_SIZE == 0 && (trig - c.off) / PTHD_SIZE < (uint32_t)n)
            p->trigPtr = &w->paths[(trig - c.off) / PTHD_SIZE];
        else if (trig)
            fprintf(stderr, "rush2049pc: wad %d: path %d ('%.16s') names 0x%X as its trigger, no path record: none\n",
                    wave, i, p->name, trig);
        if (p->numPts <= 0) {
            p->pts = in_file(buf, size, pts);   /* no points to read there */
            continue;
        }
        uint64_t end = (uint64_t)pts + (uint64_t)p->numPts * PATH_POINT_SIZE;
        if ((pts & 3) || end > size) {
            fprintf(stderr, "rush2049pc: wad %d: path %d's points (0x%X, %d) are outside the file: none\n", wave, i,
                    pts, p->numPts);
            continue;
        }
        p->pts = (struct TargetPathPt *)(buf + pts);
        spans[nspans++] = (Span){pts, (uint32_t)end};
    }
    qsort(spans, nspans, sizeof *spans, span_cmp);
    uint32_t done = 0;   /* words below this offset are swapped */
    for (size_t i = 0; i < nspans; i++) {
        uint32_t s = spans[i].start > done ? spans[i].start : done;
        if (s < spans[i].end) {
            pc_swap32_n(buf + s, (spans[i].end - s) / 4);
            done = spans[i].end;
        }
    }
    free(spans);
}

static void release_wad(void *records)
{
    struct pc_wad_records *w = records;

    free(w->models);
    free(w->textures);
    free(w->palettes);
    free(w->paths);
    free(w);
}

void pc_assets_wad(int wave, uint8_t *buf, size_t size)
{
    long n = check_toc(wave, buf, size);
    if (n < 0)
        return;
    Chunk imag = find_chunk(buf, n, TAG_IMAG), txld = find_chunk(buf, n, TAG_TXLD);
    struct pc_wad_records *w = calloc(1, sizeof *w);

    if (!w)
        abort();
    build_models(wave, w, buf, size, find_chunk(buf, n, TAG_OBHD));
    build_textures(wave, w, buf, size, find_chunk(buf, n, TAG_TXHD), imag);
    build_palettes(wave, w, buf, size, find_chunk(buf, n, TAG_PLHD), imag);
    build_paths(wave, w, buf, size, find_chunk(buf, n, TAG_PTHD));
    if (txld.found && txld.off <= size && txld.count <= size - txld.off && txld.count <= 0x7FFFFFFF) {
        w->textureLoads = buf + txld.off;
        w->textureLoadsSize = (int)txld.count;
    } else if (txld.found)
        fprintf(stderr, "rush2049pc: wad %d: TXLD (0x%X, 0x%X bytes) is outside the file: none\n", wave, txld.off,
                txld.count);
    if (imag.found)
        w->images = in_file(buf, size, imag.off);
    pc_records_keep(buf, size, PC_RECORDS_WAD, w, release_wad);
    add_block(wave, buf, size);
    if (verbose())
        fprintf(stderr, "rush2049pc: wad %d: %ld chunks, %d models, %d textures, %d palettes, %d paths at %p\n", wave,
                n, w->numModels, w->numTextures, w->numPalettes, w->numPaths, (void *)buf);
}

struct pc_wad_records *pc_wad_records(const void *file)
{
    static struct pc_wad_records none;
    struct pc_wad_records *w = pc_records_find(file, PC_RECORDS_WAD);

    if (w)
        return w;
    fprintf(stderr, "rush2049pc: no WAD was converted at %p: no models, textures, palettes or paths\n", file);
    memset(&none, 0, sizeof none);
    return &none;
}
