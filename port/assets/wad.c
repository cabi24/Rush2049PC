/* port/assets/wad.c: the WAD container (docs/port/ASSETS.md 3.1, docs/port/NATIVE.md 4.4, steps 7 and 10): the font
 * texture waves 22..37, the model, car and track waves 54..119, and the world waves 120..138 (world.c reads their own
 * chunks after this).
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
 * The display lists (the models' in OBJS, the texture loads in TXLD) and the vertices they load are built again beside
 * the file too, in the PC's own form, with host pointers (the display lists' section below); the records point at
 * them, and the game and the renderer read only those. The texels and TLUTs (IMAG) stay in the file in the N64's
 * formats, which the renderer decodes: the whole block is one pc_mem_register_be range (port/ultra's PI DMA and
 * decompressors registered the file already, so this only makes the range cover the block).
 *
 * Release: the game's heap reports every block it frees (pc_heap_block_freed, from DeallocateMemoryInternal), and
 * pc_wave_released drops a wave's block on request; the records and the lists go with it (records.c). The game frees
 * a model wave only once the frame being drawn is done (UnloadWaveInstance waits for gRenderInProgress), so the
 * renderer never reads lists that are gone. WAD blocks never move: heap
 * compaction moves only handle blocks, and the only waves loaded as handles are the font data (38..53). Only one N64
 * thread runs at a time (docs/port/ULTRA.md), so the block list needs no lock. */
#include <stdarg.h>
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

/* ---------------------------------------------------------------- the display lists (docs/port/NATIVE.md 4.4, step 10) */

/* The file's display lists are F3DEX2 commands in the N64's form: 8 bytes, big-endian, w0 then w1, and in w1 of a
 * command that carries an address an offset: from the file's start in the models' lists (OBJS), from IMAG's in the
 * texture loads (TXLD). On the N64 SetupWaveModel added the file's or IMAG's address to those offsets in place, in 24
 * bits (prepare_model_dl, relocate_texture_loads). Here the lists are built again beside the file in the PC's own
 * form (PcGfxWords: w0, and a pointer-sized w1; docs/port/GFX.md "Command formats"), command for command: w0 as it is,
 * w1 a host pointer in a command that carries an address and the value in the rest. The records point at them (a
 * level of detail's displayList, a listed texture's texBase), so the game's frame list calls them (gSPDisplayList).
 *
 * Which bytes are commands: the lists are reached from the records (each level's list, each listed texture's load
 * list) and from each other (G_DL, G_RDPHALF_1 before G_BRANCH_Z, the LOD markers), and every walk of them, the
 * game's and the renderer's, goes on a command at a time to a G_ENDDL. So every command some walk comes to is built,
 * each from its 8 bytes, in the file's order: the command after a built one is the next built one, as in the file,
 * and a walk of the native lists comes to the commands its walk of the file came to. The bytes between the lists (the
 * vertices) are none. One G_ENDDL after the last command stops a walk that ran to the file's end.
 *
 * The addresses: the N64's relocation is played on the file's offsets (relocate_texture_loads over TXLD, then
 * prepare_model_dl's walk of each model's levels, in SetupWaveModel's order) to know what each word became: an
 * offset from the file's start or from IMAG's, made an address once. A command the renderer reads an address from
 * (VTX, MTX, MOVEMEM, G_DL, G_RDPHALF_1 before G_BRANCH_Z, SETTIMG, SETZIMG, SETCIMG; and LOAD_UCODE's) points where
 * that word did: at (origin + w1) & 0xFFFFFF in the file, which is where the renderer took the word (the RDRAM's
 * address of the file plus the offset, in the 24 bits the RCP reads). A word the N64 made an address not exactly once
 * (in a list no relocation walked, or one two walks went over) pointed into no file: the converter says so and leaves
 * it NULL (rush2049pc_assetcheck counts such a message as a failure). A LOD marker (G_SPNOOP with byte 1 set,
 * prepare_model_dl.c) gets a pointer to the place its w1 names, which the game's walk makes a G_DL branch to when the
 * race doesn't keep the marker's section.
 *
 * The vertices: a VTX's w1 points at native vertices (Vtx: s16 x, y, z, u16 flag, s16 s, t in the host's byte order,
 * the colour's four bytes as they are), copied from the file beside the lists: the runs the VTX commands load, and the
 * run mirror_vertex_texcoords walks from a level's first VTX (to the vertex whose flag has no 0x8000). Bytes two of
 * them share are one copy, and each run of the file's bytes stays in one piece, with 16 bytes of zeros after it (a
 * vertex that ends any walk past it). They are registered as model data (pc_mem_register_model_data), which the high
 * frame rate pairs a model's transforms by. The texels and TLUTs stay in the file, in the N64's formats, which the
 * renderer decodes; so do the matrices and the lights a MTX or MOVEMEM may load, which it reads big-endian there. */

/* The F3DEX2 opcodes the walks know (port/gfx/gfx_gbi.h has them all). */
#define OP_VTX 0x01
#define OP_BRANCH_Z 0x04
#define OP_MTX 0xDA
#define OP_MOVEMEM 0xDC
#define OP_LOAD_UCODE 0xDD
#define OP_DL 0xDE
#define OP_ENDDL 0xDF
#define OP_SPNOOP 0xE0
#define OP_RDPHALF_1 0xE1
#define OP_SETTIMG 0xFD
#define OP_SETZIMG 0xFE
#define OP_SETCIMG 0xFF
#define LOD_MARKER 1          /* G_SPNOOP's byte 1 in a LOD marker */
#define TEXTURE_TEXELS 0x08000000u   /* tTexture.flags: texBase is the texels (TEX_IMAGE_DIRECT); else a load list */
#define DL_BRANCH(w0) (((w0) >> 16 & 0xFF) != 0)   /* a G_DL that doesn't come back */
#define VTX_COUNT(w0) ((w0) >> 12 & 0xFF)
#define VTX_SIZE 16
#define VTX_MORE 0x8000       /* a vertex's flag: another vertex of the run follows (mirror_vertex_texcoords.c) */
#define RCP_OFFSET 0xFFFFFFu  /* the 24 bits of an address the N64's relocation kept */
#define PROBLEMS_SAID 20      /* a file's display-list problems said, at most */

/* The commands with no address: the RDP's and most of the RSP's, which the N64's relocations skip. */
static int no_address(unsigned op)
{
    return (op & 0xC0) == 0x40 || (op & 0xC0) == 0x80 || (op >= 0x09 && op < 0x40) || (op >= 0xC0 && op < 0xD6);
}

/* The commands prepare_model_dl made an address of (op not no_address; next: the next command's opcode). */
static int model_list_address(unsigned op, unsigned next)
{
    return op == OP_VTX || op == OP_LOAD_UCODE || op == OP_DL || op == OP_MTX || op == OP_MOVEMEM ||
           (op == OP_RDPHALF_1 && (next == OP_BRANCH_Z || next == OP_LOAD_UCODE)) || op >= OP_SETTIMG;
}

/* What the walks found at each 8 bytes of the file. */
typedef struct {
    uint8_t by_file, by_imag;   /* how many times the N64's relocation added the file's or IMAG's address to the word */
    uint8_t marker_walks;       /* a LOD marker: how many relocation walks came to it */
    uint8_t command;            /* a walk comes to it: it is a command */
    uint32_t walk;              /* the last relocation walk that came to it (a loop's guard) */
    int32_t index;              /* its native command */
} Slot;

typedef struct {
    int wave;
    const uint8_t *buf;
    size_t size;
    Chunk imag;
    Slot *slot;                 /* one per 8 bytes of the file */
    uint32_t walks;
    unsigned problems;
    uint32_t *todo;             /* the lists still to walk */
    size_t ntodo, todo_room;
    Span *runs;                 /* the vertices the lists use: [start, end) in the file */
    size_t nruns, runs_room;
} Lists;

#ifdef __MINGW_PRINTF_FORMAT   /* mingw-w64: the printf vsnprintf is (gnu_printf with its ANSI stdio: %zu) */
#define WAD_PRINTF __MINGW_PRINTF_FORMAT
#else
#define WAD_PRINTF printf
#endif
static void problem(Lists *L, const char *fmt, ...) __attribute__((format(WAD_PRINTF, 2, 3)));
static void problem(Lists *L, const char *fmt, ...)
{
    va_list ap;

    if (L->problems++ >= PROBLEMS_SAID) {
        if (L->problems == PROBLEMS_SAID + 1)
            fprintf(stderr, "rush2049pc: wad %d: display lists: more problems than these\n", L->wave);
        return;
    }
    fprintf(stderr, "rush2049pc: wad %d: display lists: ", L->wave);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static void bump(uint8_t *n)
{
    if (*n < 255)
        (*n)++;
}

/* The opcode of the command at off, 0 past the file (where the N64 read on into the heap). */
static unsigned op_at(const Lists *L, uint32_t off)
{
    return off + 8 <= L->size ? L->buf[off] : 0;
}

static uint32_t w1_at(const Lists *L, uint32_t off)
{
    return pc_be32(L->buf + off + 4);
}

/* Where the word in the command at off pointed once the N64 had relocated it: its origin's offset plus w1, in 24 bits,
 * into *to. -1 when the N64 made it an address not exactly once: then it pointed into no file. */
static int address_at(const Lists *L, uint32_t off, uint32_t *to)
{
    const Slot *s = &L->slot[off / 8];

    if (s->by_file + s->by_imag != 1)
        return -1;
    *to = ((s->by_file ? 0 : L->imag.off) + w1_at(L, off)) & RCP_OFFSET;
    return 0;
}

static void push_todo(Lists *L, uint32_t off)
{
    if (L->ntodo == L->todo_room) {
        L->todo_room = L->todo_room ? 2 * L->todo_room : 64;
        L->todo = realloc(L->todo, L->todo_room * sizeof *L->todo);
        if (!L->todo)
            abort();
    }
    L->todo[L->ntodo++] = off;
}

static void add_run(Lists *L, uint32_t start, uint32_t end)
{
    if (L->nruns == L->runs_room) {
        L->runs_room = L->runs_room ? 2 * L->runs_room : 64;
        L->runs = realloc(L->runs, L->runs_room * sizeof *L->runs);
        if (!L->runs)
            abort();
    }
    L->runs[L->nruns++] = (Span){start, end};
}

/* A list starts at `to`, which the command at `at` names (a G_DL, a G_RDPHALF_1 before G_BRANCH_Z, a LOD marker): it is
 * walked if it is a command's place in the file. */
static void named_list(Lists *L, uint32_t to, const char *by, uint32_t at)
{
    if (to >= L->size || (to & 7))
        problem(L, "the %s at 0x%X names 0x%X, not a command's place in the file", by, at, to);
    else
        push_todo(L, to);
}

/* A record's list (a level of detail's, a listed texture's) at `list` in the file: its offset, or -1 (said) when it is
 * no command's place in the file. */
static int64_t record_list(Lists *L, const void *list, const char *what, int i)
{
    uint32_t off = (uint32_t)((const uint8_t *)list - L->buf);

    if (off >= L->size || (off & 7)) {
        problem(L, "%s %d's list at 0x%X is not a command's place in the file", what, i, off);
        return -1;
    }
    return off;
}

/* relocate_texture_loads (SetupWaveModel's first relocation): over every command of TXLD, SETTIMG's word and that of a
 * G_RDPHALF_1 before G_BRANCH_Z got IMAG's address. */
static void relocate_texture_loads_played(Lists *L, Chunk txld)
{
    if (!txld.found || !txld.count)
        return;
    if ((txld.off & 7) || txld.off > L->size || txld.count > L->size - txld.off) {
        problem(L, "TXLD (0x%X bytes at 0x%X) is not inside the file, 8-aligned", txld.count, txld.off);
        return;
    }
    for (uint32_t off = txld.off; off < txld.off + txld.count && off + 8 <= L->size; off += 8) {
        unsigned op = L->buf[off];
        if (!no_address(op) && (op == OP_SETTIMG || (op == OP_RDPHALF_1 && op_at(L, off + 8) == OP_BRANCH_Z)))
            bump(&L->slot[off / 8].by_imag);
    }
}

/* mirror_vertex_texcoords' run from the vertices at v: to the first whose flag has no VTX_MORE. */
static void mirror_run(Lists *L, uint32_t v, const char *what, uint32_t at)
{
    uint32_t end = v;

    for (;;) {
        if (end + VTX_SIZE > L->size) {
            problem(L, "%s's first VTX (0x%X) loads a run of vertices from 0x%X that goes past the file's end", what,
                    at, v);
            break;
        }
        end += VTX_SIZE;
        if (!(pc_be16(L->buf + end - VTX_SIZE + 6) & VTX_MORE))
            break;
    }
    if (end > v)
        add_run(L, v, end);
}

/* prepare_model_dl's walk of the list at start (SetupWaveModel's, for each level of each model in turn): a command at
 * a time, past a G_DL (not into it), at a G_BRANCH_Z on at the second command of the list its G_RDPHALF_1 names, to
 * the first G_ENDDL. Each word it made an address got the file's address once more; its first VTX's vertices are the
 * run mirror_vertex_texcoords walks; each LOD marker it came to may become a branch. */
static void relocate_model_dl_played(Lists *L, uint32_t start, const char *what)
{
    uint32_t off = start, latched = UINT32_MAX, to, walk = ++L->walks;
    int first = 1;

    for (;;) {
        if (off + 8 > L->size) {
            problem(L, "%s reaches the file's end before its G_ENDDL (the N64's relocation)", what);
            return;
        }
        Slot *s = &L->slot[off / 8];
        if (s->walk == walk) {
            problem(L, "%s comes back to 0x%X: the N64's relocation would never end", what, off);
            return;
        }
        s->walk = walk;
        unsigned op = L->buf[off];
        if (no_address(op)) {
            off += 8;
            continue;
        }
        if (model_list_address(op, op_at(L, off + 8)))
            bump(&s->by_file);
        switch (op) {
        case OP_SPNOOP:
            if ((pc_be32(L->buf + off) >> 16 & 0xFF) == LOD_MARKER)
                bump(&s->marker_walks);
            break;
        case OP_VTX:
            if (first) {
                first = 0;
                if (address_at(L, off, &to) == 0)
                    mirror_run(L, to, what, off);
            }
            break;
        case OP_RDPHALF_1:
            latched = off;
            break;
        case OP_BRANCH_Z:
            if (latched == UINT32_MAX || address_at(L, latched, &to) != 0 || (to & 7) || to + 16 > L->size) {
                problem(L, "%s branches (G_BRANCH_Z at 0x%X) to no command in the file", what, off);
                return;
            }
            off = to + 8;   /* the target's second command */
            continue;
        case OP_ENDDL:
            return;
        }
        off += 8;
    }
}

/* Every command a walk comes to, from the lists on the todo list: a command at a time to G_ENDDL, past every G_DL and
 * branch too (the game's walks go on past them), and into each list a G_DL, a G_BRANCH_Z or a LOD marker names. */
static void find_commands(Lists *L)
{
    uint32_t to;

    while (L->ntodo) {
        uint32_t off = L->todo[--L->ntodo];
        for (;;) {
            if (off + 8 > L->size) {
                problem(L, "a list reaches the file's end before its G_ENDDL");
                break;
            }
            Slot *s = &L->slot[off / 8];
            if (s->command)
                break;   /* walked from here already */
            s->command = 1;
            uint32_t w0 = pc_be32(L->buf + off);
            unsigned op = w0 >> 24;
            if (op == OP_ENDDL)
                break;
            if (op == OP_DL || (op == OP_RDPHALF_1 && op_at(L, off + 8) == OP_BRANCH_Z)) {
                if (address_at(L, off, &to) == 0)
                    named_list(L, to, op == OP_DL ? "G_DL" : "G_RDPHALF_1 before G_BRANCH_Z", off);
            } else if (op == OP_SPNOOP && (w0 >> 16 & 0xFF) == LOD_MARKER && s->marker_walks)
                named_list(L, w1_at(L, off) & RCP_OFFSET, "LOD marker", off);
            off += 8;
        }
    }
}

static int run_cmp(const void *a, const void *b)
{
    const Span *x = a, *y = b;
    return x->start < y->start ? -1 : x->start > y->start ? 1 : (x->end > y->end) - (x->end < y->end);
}

/* What the converter keeps for a WAD: the records the game reads, and the native lists and vertices they point at. */
typedef struct {
    struct pc_wad_records r;    /* first: pc_wad_records hands out its address */
    PcGfxWords *commands;       /* the lists' commands in the file's order, then one G_ENDDL */
    size_t numCommands;
    uint8_t *vertices;          /* the vertices the lists load, native */
    size_t verticesSize;
} WadKept;

/* The vertices: the runs merged where they meet, each copied in one piece with its vertices swapped to the host's
 * byte order, and VTX_SIZE bytes of zeros after it. Returns the merged runs' places in *native (their offsets in
 * kept->vertices). */
static void build_vertices(Lists *L, WadKept *kept, uint32_t **native)
{
    size_t n = 0, total = 0;

    qsort(L->runs, L->nruns, sizeof *L->runs, run_cmp);
    for (size_t i = 0; i < L->nruns; i++) {   /* merge, in place */
        if (n && L->runs[i].start <= L->runs[n - 1].end) {
            if (L->runs[i].end > L->runs[n - 1].end)
                L->runs[n - 1].end = L->runs[i].end;
        } else
            L->runs[n++] = L->runs[i];
    }
    L->nruns = n;
    *native = malloc((n ? n : 1) * sizeof **native);
    if (!*native)
        abort();
    for (size_t i = 0; i < n; i++) {
        uint32_t len = L->runs[i].end - L->runs[i].start;
        (*native)[i] = (uint32_t)total;
        total += (len + VTX_SIZE - 1) / VTX_SIZE * VTX_SIZE + VTX_SIZE;
    }
    if (!total)
        return;
    kept->vertices = calloc(1, total);
    if (!kept->vertices)
        abort();
    kept->verticesSize = total;
    for (size_t i = 0; i < n; i++) {
        uint32_t start = L->runs[i].start, end = L->runs[i].end < L->size ? L->runs[i].end : (uint32_t)L->size;
        uint8_t *v = kept->vertices + (*native)[i];
        memcpy(v, L->buf + start, end - start);
        for (uint32_t k = 0; k < end - start; k += VTX_SIZE)   /* x, y, z, flag, s, t; the colour's bytes as they are */
            pc_swap16_n(v + k, (end - start - k) >= 12 ? 6 : (end - start - k) / 2);
    }
    pc_mem_register_model_data(kept->vertices, kept->verticesSize);
}

/* The native vertices for the VTX at off, which loads from `to` in the file; NULL when no run holds it. */
static void *vertices_for(Lists *L, WadKept *kept, const uint32_t *native, uint32_t off, uint32_t to)
{
    size_t lo = 0, hi = L->nruns;

    while (lo < hi) {   /* the last run that starts at or before `to` */
        size_t mid = (lo + hi) / 2;
        if (L->runs[mid].start <= to)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (!lo || to >= L->runs[lo - 1].end) {
        problem(L, "the VTX at 0x%X loads from 0x%X, where no vertices were copied", off, to);
        return NULL;
    }
    if ((to - L->runs[lo - 1].start) % VTX_SIZE)
        problem(L, "the VTX at 0x%X loads vertices from 0x%X, across others the lists load from 0x%X", off, to,
                L->runs[lo - 1].start);
    return kept->vertices + native[lo - 1] + (to - L->runs[lo - 1].start);
}

/* The native command a list at file offset `to` starts with; NULL (said) when no walk came there. */
static PcGfxWords *command_for(Lists *L, WadKept *kept, uint32_t to)
{
    if (to >= L->size || (to & 7) || !L->slot[to / 8].command)
        return NULL;
    return &kept->commands[L->slot[to / 8].index];
}

static const char *op_name(unsigned op)
{
    switch (op) {
    case OP_VTX: return "VTX";
    case OP_MTX: return "MTX";
    case OP_MOVEMEM: return "MOVEMEM";
    case OP_LOAD_UCODE: return "LOAD_UCODE";
    case OP_DL: return "G_DL";
    case OP_RDPHALF_1: return "G_RDPHALF_1";
    case OP_SPNOOP: return "LOD marker";
    case OP_SETTIMG: return "SETTIMG";
    case OP_SETZIMG: return "SETZIMG";
    default: return "SETCIMG";
    }
}

/* The native command for the file's command at off. */
static void build_command(Lists *L, WadKept *kept, const uint32_t *native, uint32_t off, PcGfxWords *cmd)
{
    uint32_t w0 = pc_be32(L->buf + off), to = 0;
    unsigned op = w0 >> 24, next = op_at(L, off + 8);
    const Slot *s = &L->slot[off / 8];
    void *p = NULL;

    cmd->w0 = w0;
    cmd->w1 = w1_at(L, off);
    if (op == OP_SPNOOP) {
        if ((w0 >> 16 & 0xFF) != LOD_MARKER || !s->marker_walks)
            return;
        if (s->marker_walks > 1)
            problem(L, "the LOD marker at 0x%X is walked by %u relocations: on the N64 the first's branch would be "
                    "relocated again", off, s->marker_walks);
        to = w1_at(L, off) & RCP_OFFSET;
    } else if (op == OP_VTX || op == OP_MTX || op == OP_MOVEMEM || op == OP_LOAD_UCODE || op == OP_DL ||
               (op == OP_RDPHALF_1 && (next == OP_BRANCH_Z || next == OP_LOAD_UCODE)) || op >= OP_SETTIMG) {
        if (address_at(L, off, &to) != 0) {
            problem(L, "the %s at 0x%X: the N64 made its word an address %u times, so it pointed into no file",
                    op_name(op), off, s->by_file + s->by_imag);
            cmd->w1 = 0;
            return;
        }
    } else
        return;
    if (op == OP_VTX)
        p = vertices_for(L, kept, native, off, to);
    else if (op == OP_DL || op == OP_SPNOOP || (op == OP_RDPHALF_1 && next == OP_BRANCH_Z))
        p = command_for(L, kept, to);
    else if (to < L->size)
        p = (void *)(L->buf + to);
    if (!p && op != OP_VTX)
        problem(L, "the %s at 0x%X names 0x%X, beyond the file", op_name(op), off, to);
    cmd->w1 = (uintptr_t)p;
}

/* The display lists and their vertices built native for the file, and the records pointed at them. */
static void build_lists(int wave, WadKept *kept, uint8_t *buf, size_t size, Chunk imag, Chunk txld)
{
    struct pc_wad_records *w = &kept->r;
    Lists L = {wave, buf, size, imag, NULL, 0, 0, NULL, 0, 0, NULL, 0, 0};
    uint32_t *native = NULL, to;
    char what[40];
    size_t n = 0;
    int64_t at;

    L.slot = calloc(size / 8 + 1, sizeof *L.slot);
    if (!L.slot)
        abort();
    /* The N64's relocation, in SetupWaveModel's order: the texture loads, then each model's levels. The commands: the
     * models' levels' lists, the listed textures' load lists, and the lists they name. */
    relocate_texture_loads_played(&L, txld);
    for (int i = 0; i < w->numModels; i++)
        for (int k = 0; k < w->models[i].lodCount; k++)
            if (w->models[i].lod[k].displayList && (at = record_list(&L, w->models[i].lod[k].displayList, "model", i)) >= 0) {
                snprintf(what, sizeof what, "model %d's level %d", i, k);
                relocate_model_dl_played(&L, (uint32_t)at, what);
                push_todo(&L, (uint32_t)at);
            }
    for (int i = 0; i < w->numTextures; i++)
        if (!(w->textures[i].flags & TEXTURE_TEXELS) && w->textures[i].texBase &&
            (at = record_list(&L, w->textures[i].texBase, "texture", i)) >= 0)
            push_todo(&L, (uint32_t)at);
    find_commands(&L);
    /* The vertices each VTX loads (the mirror runs are in already). */
    for (uint32_t off = 0; off + 8 <= size; off += 8)
        if (L.slot[off / 8].command && buf[off] == OP_VTX && address_at(&L, off, &to) == 0) {
            uint32_t count = VTX_COUNT(pc_be32(buf + off)), end = to + VTX_SIZE * (count ? count : 1);
            if (end > size)
                problem(&L, "the VTX at 0x%X loads %u vertices from 0x%X, past the file's end", off, count, to);
            if (to < size)
                add_run(&L, to, end);
        }
    build_vertices(&L, kept, &native);
    /* The commands, in the file's order, and one G_ENDDL after them. */
    for (uint32_t off = 0; off + 8 <= size; off += 8)
        if (L.slot[off / 8].command)
            L.slot[off / 8].index = (int32_t)n++;
    kept->commands = calloc(n + 1, sizeof *kept->commands);
    if (!kept->commands)
        abort();
    kept->numCommands = n;
    for (uint32_t off = 0; off + 8 <= size; off += 8)
        if (L.slot[off / 8].command)
            build_command(&L, kept, native, off, &kept->commands[L.slot[off / 8].index]);
    kept->commands[n].w0 = (uint32_t)OP_ENDDL << 24;
    /* The records point at them. */
    for (int i = 0; i < w->numModels; i++)
        for (int k = 0; k < w->models[i].lodCount; k++)
            if (w->models[i].lod[k].displayList)
                w->models[i].lod[k].displayList =
                    (union Gfx *)command_for(&L, kept, (uint32_t)((uint8_t *)w->models[i].lod[k].displayList - buf));
    for (int i = 0; i < w->numTextures; i++)
        if (!(w->textures[i].flags & TEXTURE_TEXELS) && w->textures[i].texBase)
            w->textures[i].texBase = command_for(&L, kept, (uint32_t)((uint8_t *)w->textures[i].texBase - buf));
    if (verbose())
        fprintf(stderr, "rush2049pc: wad %d: %zu display-list commands built, %zu runs of vertices (0x%zX bytes)\n",
                wave, n, L.nruns, kept->verticesSize);
    free(native);
    free(L.slot);
    free(L.todo);
    free(L.runs);
}

static void release_wad(void *records)
{
    WadKept *kept = records;
    struct pc_wad_records *w = &kept->r;

    free(w->models);
    free(w->textures);
    free(w->palettes);
    free(w->paths);
    free(kept->commands);
    if (kept->vertices)
        pc_mem_unregister_model_data(kept->vertices);
    free(kept->vertices);
    free(kept);
}

void pc_assets_wad(int wave, uint8_t *buf, size_t size)
{
    long n = check_toc(wave, buf, size);
    if (n < 0)
        return;
    Chunk imag = find_chunk(buf, n, TAG_IMAG), txld = find_chunk(buf, n, TAG_TXLD);
    WadKept *kept = calloc(1, sizeof *kept);
    struct pc_wad_records *w;

    if (!kept)
        abort();
    w = &kept->r;
    build_models(wave, w, buf, size, find_chunk(buf, n, TAG_OBHD));
    build_textures(wave, w, buf, size, find_chunk(buf, n, TAG_TXHD), imag);
    build_palettes(wave, w, buf, size, find_chunk(buf, n, TAG_PLHD), imag);
    build_lists(wave, kept, buf, size, imag, txld);   /* before the paths' points are swapped: the file as it is */
    build_paths(wave, w, buf, size, find_chunk(buf, n, TAG_PTHD));
    pc_records_keep(buf, size, PC_RECORDS_WAD, kept, release_wad);
    add_block(wave, buf, size);
    if (verbose())
        fprintf(stderr, "rush2049pc: wad %d: %ld chunks, %d models, %d textures, %d palettes, %d paths at %p\n", wave,
                n, w->numModels, w->numTextures, w->numPalettes, w->numPaths, (void *)buf);
}

struct pc_wad_records *pc_wad_records(const void *file)
{
    static struct pc_wad_records none;
    WadKept *kept = pc_records_find(file, PC_RECORDS_WAD);

    if (kept)
        return &kept->r;
    fprintf(stderr, "rush2049pc: no WAD was converted at %p: no models, textures, palettes or paths\n", file);
    memset(&none, 0, sizeof none);
    return &none;
}
