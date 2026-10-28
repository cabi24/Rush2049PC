/* port/assets/tests/assetcheck.c: every file the game loads, from your ROM through port/assets' converters, then
 * checked as the game's readers read it (docs/port/ASSETS.md §8).
 *
 *     rush2049pc_assetcheck [baserom.us.z64] [-v]
 *
 * Each of the 183 waves is taken from the file table as _ReadWave takes it (ROM offset D_8011B5BC, size
 * gWaveSizes, storage D_80123564: raw, LZSS or DEFLATE), put in a 32-byte-rounded block in the RDRAM window and
 * passed to pc_wave_loaded. Then:
 *   - no converter may report a file it refused or cut ("runs past", "outside", "doesn't parse", ...);
 *   - WADs: each record chunk's count fits before the next chunk; OBHD radii and LOD radii are finite, display
 *     lists 8-aligned in the file; texture and palette bases inside IMAG; PTHD points inside the file, their
 *     normals unit length and distToNext the distance to the next point; trigPtr 0 or a PTHD record;
 *   - worlds: WHDR names and offsets; every node WHDR reaches is inside WOBJ, finite, with ordered bounds; the
 *     quadtree's children inside GDAT and its leaves' keys inside GTLD;
 *   - roads: the sections fit; path floats finite; tree parents inside the tree; each surface's basis rows of
 *     about one length (a swapped s16 is off by orders of magnitude);
 *   - paths: the walk fits the file; checkpoint normals unit length;
 *   - text: every string offset inside the file, right after a NUL;
 *   - font data: letters and kern table inside the block.
 * It prints counts and failures only, nothing of the files' contents. Without a ROM it says so and exits 77
 * (ctest: skipped). The game image's place in the ROM is the build's (port/data's image table). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "assets.h"
#include "support.h"

#ifndef PC_DATA_ROM
#define PC_DATA_ROM "baserom.us.z64"
#endif

void pc_wave_loaded(int wave, void *buf, size_t size);

#define WAVES 183
#define GAME_ROM 0xB0CB10u    /* the game image: raw DEFLATE, inflated to GAME_BASE (ASSETS.md §2.1) */
#define GAME_BASE 0x80086A50u
#define GAME_SIZE 647072u
#define BOOT_ROM 0x283D0u     /* the boot segment's data, raw */
#define BOOT_BASE 0x800277D0u
#define BLOCK_MAX 0x5F0000u

static const uint8_t *g_rom;
static size_t g_rom_size;
static uint8_t *g_game;
static int g_verbose;
static unsigned g_fail, g_checked;
static int g_wave;

#define T4(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))

static void fail(const char *what, unsigned long a, unsigned long b)
{
    if (g_fail++ < 40)
        printf("FAIL: wave %d: %s (0x%lX, 0x%lX)\n", g_wave, what, a, b);
}
#define EXPECT(c, what, a, b) do { g_checked++; if (!(c)) fail(what, (unsigned long)(a), (unsigned long)(b)); } while (0)

static uint16_t n16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static int16_t s16(const uint8_t *p) { int16_t v; memcpy(&v, p, 2); return v; }
static uint32_t n32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static float f32(const uint8_t *p) { float v; memcpy(&v, p, 4); return v; }
static int finite_f(const uint8_t *p, int n)
{
    for (int i = 0; i < n; i++)
        if (!isfinite(f32(p + 4 * i)) || fabsf(f32(p + 4 * i)) > 1e7f)
            return 0;
    return 1;
}
static double len3f(const uint8_t *p)
{
    double l = 0;
    for (int i = 0; i < 3; i++)
        l += (double)f32(p + 4 * i) * f32(p + 4 * i);
    return sqrt(l);
}

/* ---------------------------------------------------------------- reading the ROM as _ReadWave does */

static size_t inflate_raw(const uint8_t *in, size_t avail, uint8_t *out, size_t cap)
{
    z_stream z;
    memset(&z, 0, sizeof z);
    if (inflateInit2(&z, -15) != Z_OK)
        return 0;
    z.next_in = (Bytef *)in;
    z.avail_in = (uInt)avail;
    z.next_out = out;
    z.avail_out = (uInt)cap;
    int r = inflate(&z, Z_FINISH);
    size_t n = z.total_out;
    inflateEnd(&z);
    return r == Z_STREAM_END ? n : 0;
}

/* The boot LZSS (port/ultra/pi.c's lzss_decode, bounded by cap). */
static size_t lzss(const uint8_t *in, const uint8_t *end, uint8_t *out, size_t cap)
{
    size_t n = 0;
    unsigned flags = 0, bits = 0;
    for (;;) {
        if (!bits) {
            if (in >= end)
                break;
            flags = *in++;
            bits = 8;
        }
        int literal = flags & 1;
        flags >>= 1;
        bits--;
        if (literal) {
            if (in >= end || n >= cap)
                break;
            out[n++] = *in++;
            continue;
        }
        if (in + 2 > end)
            break;
        unsigned b0 = in[0], b1 = in[1], dist = ((b0 & 0xF0) << 4 | b1) & 0xFFF, len = (b0 & 0xF) + 2;
        in += 2;
        if (dist == 0 && (b0 & 0xF) == 0)
            break;
        if (dist > n || n + len > cap)
            return 0;
        for (unsigned i = 0; i < len; i++, n++)
            out[n] = out[n - dist];
    }
    return n;
}

static uint32_t game32(uint32_t addr) { return pc_be32(g_game + (addr - GAME_BASE)); }
static uint32_t boot32(uint32_t addr) { return pc_be32(g_rom + BOOT_ROM + (addr - BOOT_BASE)); }

/* ---------------------------------------------------------------- WADs */

typedef struct { uint32_t tag, off, count; } Entry;
static Entry g_toc[256];
static int g_ntoc;
static const uint8_t *W;
static size_t WS;

static int read_toc(void)
{
    uint32_t toc = n32(W), n = n32(W + 4);
    g_ntoc = 0;
    if (toc < 8 || toc > WS || n > 256 || (size_t)n * 12 > WS - toc)
        return 0;
    for (uint32_t i = 0; i < n; i++)
        g_toc[i] = (Entry){n32(W + toc + 12 * i), n32(W + toc + 12 * i + 4), n32(W + toc + 12 * i + 8)};
    g_ntoc = (int)n;
    return 1;
}

static const Entry *chunk(uint32_t tag)
{
    for (int i = 0; i < g_ntoc; i++)
        if (g_toc[i].tag == tag)
            return &g_toc[i];
    return NULL;
}

/* Bytes from a chunk's start to the next chunk (or the TOC, or the end). */
static uint32_t room(uint32_t off)
{
    uint32_t end = (uint32_t)WS, toc = n32(W);
    for (int i = 0; i < g_ntoc; i++)
        if (g_toc[i].off > off && g_toc[i].off < end)
            end = g_toc[i].off;
    if (toc > off && toc < end)
        end = toc;
    return end - off;
}

static void check_records(void)
{
    static const struct { uint32_t tag, size; } k[] = {
        {T4('O', 'B', 'H', 'D'), 0x58}, {T4('T', 'X', 'H', 'D'), 0x24}, {T4('P', 'L', 'H', 'D'), 0x18},
        {T4('P', 'T', 'H', 'D'), 0x24}, {T4('W', 'O', 'B', 'J'), 0x68}, {T4('G', 'D', 'A', 'T'), 0x1C},
        {T4('G', 'T', 'L', 'D'), 4}};
    for (int i = 0; i < g_ntoc; i++)
        for (size_t j = 0; j < sizeof k / sizeof k[0]; j++)
            if (g_toc[i].tag == k[j].tag)
                EXPECT((uint64_t)g_toc[i].count * k[j].size <= room(g_toc[i].off), "record count past the chunk",
                       g_toc[i].tag, g_toc[i].count);
}

static void check_wad(void)
{
    const Entry *ob, *tx, *pl, *pt, *im;
    uint32_t imag;

    EXPECT(read_toc(), "no table of contents after conversion", n32(W), n32(W + 4));
    if (!g_ntoc)
        return;
    ob = chunk(T4('O', 'B', 'H', 'D')), tx = chunk(T4('T', 'X', 'H', 'D'));
    pl = chunk(T4('P', 'L', 'H', 'D')), pt = chunk(T4('P', 'T', 'H', 'D')), im = chunk(T4('I', 'M', 'A', 'G'));
    imag = im ? room(im->off) : 0;
    check_records();
    for (uint32_t i = 0; ob && i < ob->count && (uint64_t)(i + 1) * 0x58 <= room(ob->off); i++) {
        const uint8_t *r = W + ob->off + 0x58 * i;
        int16_t lods = s16(r + 0x16);
        EXPECT(finite_f(r + 0x10, 1) && f32(r + 0x10) >= 0, "OBHD radius", i, n32(r + 0x10));
        EXPECT(lods >= 0 && lods <= 4, "OBHD lodCount", i, (uint16_t)lods);
        for (int l = 0; l < lods && l < 4; l++) {
            const uint8_t *d = r + 0x18 + 0x10 * l;
            EXPECT(finite_f(d + 4, 1), "OBHD lodRad", i, l);
            EXPECT(n32(d + 8) < WS && !(n32(d + 8) & 7), "OBHD display list", i, n32(d + 8));
            EXPECT(n32(d + 12) < WS, "OBHD vertices", i, n32(d + 12));
            EXPECT(!tx || n16(d) < tx->count || n16(d) == 0 || n16(d) == 0xFFFF, "OBHD texidx", i, n16(d));
        }
    }
    for (uint32_t i = 0; tx && i < tx->count && (uint64_t)(i + 1) * 0x24 <= room(tx->off); i++) {
        const uint8_t *r = W + tx->off + 0x24 * i;
        EXPECT(n16(r + 0x10) && n16(r + 0x10) <= 1024 && n16(r + 0x12) && n16(r + 0x12) <= 1024, "TXHD size", i,
               n32(r + 0x10));
        EXPECT(r[0x14] <= 3, "TXHD texel size", i, r[0x14]);
        EXPECT(!(n32(r + 0x1C) & 0x08000000) || !im || n32(r + 0x18) < imag, "TXHD texBase", i, n32(r + 0x18));
    }
    for (uint32_t i = 0; pl && i < pl->count && (uint64_t)(i + 1) * 0x18 <= room(pl->off); i++) {
        const uint8_t *r = W + pl->off + 0x18 * i;
        EXPECT(r[0x10] <= r[0x11], "PLHD range", i, n16(r + 0x10));
        EXPECT(!im || n32(r + 0x14) < imag, "PLHD palBase", i, n32(r + 0x14));
    }
    for (uint32_t i = 0; pt && i < pt->count && (uint64_t)(i + 1) * 0x24 <= room(pt->off); i++) {
        const uint8_t *r = W + pt->off + 0x24 * i;
        int16_t n = s16(r + 0x14);
        uint32_t pts = n32(r + 0x1C), trig = n32(r + 0x18);
        EXPECT(!trig || (trig >= pt->off && (trig - pt->off) % 0x24 == 0 && (trig - pt->off) / 0x24 < pt->count),
               "PTHD trigPtr is not a PTHD record", i, trig);
        EXPECT(n > 0 && !(pts & 3) && pts + 0x44ull * n <= WS, "PTHD points", i, pts);
        if (n <= 0 || pts + 0x44ull * n > WS)
            continue;
        for (int k = 0; k < n; k++) {
            const uint8_t *q = W + pts + 0x44 * k;
            EXPECT(finite_f(q, 16), "PATH point floats", i, k);
            EXPECT(len3f(q + 12) == 0 || fabs(len3f(q + 12) - 1) < 1e-3, "PATH point normal", i, k);
            if (k + 1 < n && f32(q + 52) > 0) {
                double d = 0;
                for (int j = 0; j < 3; j++)
                    d += ((double)f32(q + 0x44 + 4 * j) - f32(q + 4 * j)) * ((double)f32(q + 0x44 + 4 * j) - f32(q + 4 * j));
                EXPECT(fabs(sqrt(d) - f32(q + 52)) <= 1e-3 * f32(q + 52) + 1e-3, "PATH distToNext", i, k);
            }
        }
    }
}

/* ---------------------------------------------------------------- worlds */

static void check_world(void)
{
    const Entry *wh = chunk(T4('W', 'H', 'D', 'R')), *wo = chunk(T4('W', 'O', 'B', 'J'));
    const Entry *gd = chunk(T4('G', 'D', 'A', 'T')), *gt = chunk(T4('G', 'T', 'L', 'D'));
    EXPECT(wh, "no WHDR", 0, 0);
    if (!wh)
        return;
    uint32_t worlds = n32(W + wh->off);
    EXPECT(4 + 0x14ull * worlds <= room(wh->off), "WHDR count", worlds, room(wh->off));
    if (4 + 0x14ull * worlds > room(wh->off))
        return;
    uint8_t *seen = calloc(WS / 4 + 1, 1);
    size_t *stack = malloc(sizeof *stack * (WS / 0x68 + 1));
    if (!seen || !stack)
        abort();
    for (uint32_t w = 0; w < worlds; w++) {
        const uint8_t *e = W + wh->off + 4 + 0x14 * w;
        uint32_t base = n32(e);
        EXPECT(memchr(e + 4, 0, 16) != NULL, "WHDR name not terminated", w, 0);
        EXPECT(base + 0x68ull <= WS && !(base & 3), "WHDR offset", w, base);
        if (base + 0x68ull > WS || (base & 3) || seen[base / 4])
            continue;
        size_t sp = 0;
        seen[base / 4] = 1;
        stack[sp++] = base;
        while (sp) {
            size_t at = stack[--sp];
            const uint8_t *nd = W + at;
            EXPECT(!wo || (at >= wo->off && (at - wo->off) % 0x68 == 0 && (at - wo->off) / 0x68 < wo->count),
                   "a node WHDR reaches is not a WOBJ record", w, at);
            EXPECT(finite_f(nd + 0x10, 12) && finite_f(nd + 0x50, 6), "node floats", w, at);
            EXPECT(f32(nd + 0x50) <= f32(nd + 0x5C) && f32(nd + 0x54) <= f32(nd + 0x60) && f32(nd + 0x58) <= f32(nd + 0x64),
                   "node bounds", w, at);
            for (int k = 0; k < 2; k++) {
                int16_t link = s16(nd + 0x44 + 2 * k);
                size_t to = base + 0x68 * (size_t)(uint16_t)link;
                if (link < 0)
                    continue;
                EXPECT(to + 0x68 <= WS, "node link outside the file", at, (uint16_t)link);
                if (to + 0x68 <= WS && !seen[to / 4]) {
                    seen[to / 4] = 1;
                    stack[sp++] = to;
                }
            }
        }
    }
    free(stack);
    free(seen);
    if (!gd)
        return;
    for (uint32_t i = 0; i < gd->count && (uint64_t)(i + 1) * 0x1C <= room(gd->off); i++) {
        const uint8_t *g = W + gd->off + 0x1C * i;
        EXPECT(finite_f(g + 4, 4) && f32(g + 4) <= f32(g + 8) && f32(g + 12) <= f32(g + 16), "GDAT bounds", i, 0);
        if (g[3] & 1) {
            for (int k = 0; k < 4; k++)
                EXPECT(n16(g + 0x14 + 2 * k) < gd->count, "GDAT child", i, n16(g + 0x14 + 2 * k));
        } else
            EXPECT(!gt || (uint32_t)n16(g + 0x14) + n16(g + 0x16) <= gt->count, "GDAT leaf keys past GTLD", i,
                   n32(g + 0x14));
    }
}

/* ---------------------------------------------------------------- roads, paths, text, fonts */

static void check_road(size_t size)
{
    static const size_t rec[5] = {0x84, 0x14, 0x18, 8, 0x20};
    const uint8_t *b = W;
    size_t pos = 0x10, at[5];
    for (int s = 0; s < 5; s++) {
        at[s] = pos;
        pos += n16(b + 2 * s) * rec[s];
    }
    pos += n16(b + 10) + (size_t)n32(b + 12);
    EXPECT(pos <= size, "road sections past the file", pos, size);
    if (pos > size)
        return;
    for (unsigned i = 0; i < n16(b); i++)
        EXPECT(finite_f(b + at[0] + 0x84 * i, 33), "road path floats", i, 0);
    for (unsigned i = 0; i < n16(b + 2); i++) {
        int16_t parent = s16(b + at[1] + 0x14 * i);
        EXPECT(parent >= -1 && parent < (int)n16(b + 2), "road tree parent", i, (uint16_t)parent);
    }
    for (unsigned i = 0; i < n16(b + 4); i++) {
        const uint8_t *s = b + at[2] + 0x18 * i + 4;
        double l[3];
        for (int r = 0; r < 3; r++) {
            l[r] = 0;
            for (int k = 0; k < 3; k++)
                l[r] += (double)s16(s + 6 * r + 2 * k) * s16(s + 6 * r + 2 * k);
            l[r] = sqrt(l[r]);
        }
        EXPECT(l[0] > 0 && fabs(l[1] - l[0]) < 0.1 * l[0] && fabs(l[2] - l[0]) < 0.1 * l[0], "road surface basis", i,
               (unsigned long)l[0]);
    }
}

static void check_path(size_t size)
{
    const uint8_t *b = W;
    int n = s16(b + 8);
    EXPECT(n >= 0 && n <= 10, "checkpoint count", (uint16_t)n, 0);
    for (int i = 0; i < n && i < 10; i++) {
        const uint8_t *c = b + 12 + 0x50 * i;
        EXPECT(finite_f(c, 6) && fabs(len3f(c + 12) - 1) < 1e-3, "checkpoint position or normal", i, 0);
    }
    size_t pos = 0x32C, alts = b[pos + 8], primary = n16(b + pos), alt_at = pos + 0x10;
    pos = alt_at + 0x10 * alts + 2 + 6 * primary;
    for (size_t a = 0; a < alts && pos <= size; a++)
        pos += 6 * (size_t)n16(b + alt_at + 0x10 * a + 0xA);
    for (int p = 0; p < 4 && pos + 8 <= size; p++)
        pos += 8 + 8 * (size_t)n16(b + pos);
    EXPECT(pos <= size, "path walk past the file", pos, size);
}

static void check_text(size_t size)
{
    static const int offsets[6] = {1, 0, 1, 0, 0, 1};
    size_t pos = 0, at[6];
    uint32_t n[6];
    for (int s = 0; s < 6; s++) {
        n[s] = n32(W + pos);
        at[s] = pos + 4;
        pos += 4 + (offsets[s] ? 4 * (size_t)n[s] : 2 * (size_t)((n[s] + 1) & ~1u));
        EXPECT(pos <= size, "text section past the file", s, n[s]);
        if (pos > size)
            return;
    }
    for (int s = 0; s < 6; s++)
        for (uint32_t i = 0; offsets[s] && i < n[s]; i++) {
            uint32_t o = n32(W + at[s] + 4 * i);
            EXPECT(o >= pos && o < size && (o == pos || W[o - 1] == 0) && memchr(W + o, 0, size - o),
                   "text string offset", s, i);
        }
}

static void check_font(size_t block)
{
    size_t chars = W[12], need = 16 + chars * 12 + (W[10] ? chars * chars : 0);
    EXPECT(need <= block, "font letters past the block", need, block);
}

/* ---------------------------------------------------------------- the waves */

/* Runs one wave's converter with its stderr in a file, and counts the lines that report a refused or cut file. */
static void convert(int wave, uint8_t *buf, size_t block)
{
    static const char *const bad[] = {"runs past", "outside", "no WHDR", "doesn't parse", "not converted",
                                      "left as is", "need 0x"};
    FILE *log = tmpfile();
    char line[512];
    int saved;
    fflush(stderr);
    saved = log ? dup(2) : -1;
    if (saved >= 0)
        dup2(fileno(log), 2);
    pc_wave_loaded(wave, buf, block);
    fflush(stderr);
    if (saved < 0)
        return;
    dup2(saved, 2);
    close(saved);
    rewind(log);
    while (fgets(line, sizeof line, log)) {
        int is_bad = 0;
        for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++)
            is_bad |= strstr(line, bad[i]) != NULL;
        g_checked++;
        if (is_bad)
            fail("the converter refused or cut the file", 0, 0), fputs(line, stdout);
        else if (g_verbose)
            fputs(line, stdout);
    }
    fclose(log);
}

int main(int argc, char **argv)
{
    const char *path = PC_DATA_ROM;
    (void)test_failures;   /* support.h's counter; this program keeps its own */
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v"))
            g_verbose = 1;
        else
            path = argv[i];
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("assetcheck: no ROM at %s: skipped\n", path);
        return 77;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *rom = malloc(len > 0 ? (size_t)len : 1);
    if (!rom || len < 0x1000000 / 2 || fread(rom, 1, (size_t)len, f) != (size_t)len) {
        printf("assetcheck: %s is not a readable ROM\n", path);
        return 1;
    }
    fclose(f);
    if (rom[0] != 0x80 || rom[1] != 0x37) {
        printf("assetcheck: %s is not a big-endian (.z64) ROM\n", path);
        return 1;
    }
    g_rom = rom, g_rom_size = (size_t)len;
    uint8_t *window = test_rdram();
    if (!window) {
        printf("assetcheck: the RDRAM window at 0x%X can't be mapped here: skipped\n", PC_RDRAM_BASE);
        return 77;
    }
    g_game = malloc(GAME_SIZE);
    if (!g_game || inflate_raw(rom + GAME_ROM, g_rom_size - GAME_ROM, g_game, GAME_SIZE) != GAME_SIZE) {
        printf("assetcheck: the game image doesn't inflate from ROM 0x%X\n", GAME_ROM);
        return 1;
    }
    uint8_t *buf = window + 0x200000;
    unsigned files[8] = {0};
    for (int w = 0; w < WAVES; w++) {
        uint32_t off = game32(0x8011B5BC + 4 * w) & 0x0FFFFFFF, size = boot32(0x8002E580 + 4 * w);
        uint32_t kind = game32(0x80123564 + 4 * w);
        size_t block = (size + 31) & ~(size_t)31, got = 0;
        g_wave = w;
        EXPECT(kind <= 2 && off < g_rom_size && size <= BLOCK_MAX, "file table entry", off, size);
        if (kind > 2 || off >= g_rom_size || size > BLOCK_MAX)
            continue;
        memset(buf, 0, block);
        if (kind == 0)
            got = size <= g_rom_size - off ? (memcpy(buf, rom + off, size), size) : 0;
        else if (kind == 1)
            got = lzss(rom + off, rom + g_rom_size, buf, BLOCK_MAX);
        else
            got = inflate_raw(rom + off, g_rom_size - off, buf, BLOCK_MAX);
        EXPECT(got == size, "decompressed size is not gWaveSizes", got, size);
        convert(w, buf, block);
        W = buf, WS = block;
        if (w <= PC_WAVE_TEXT_LAST)
            check_text(size), files[0]++;
        else if (w <= PC_WAVE_MUSYX_LAST)
            files[1]++;
        else if (w <= PC_WAVE_FONTWAD_LAST || (w >= PC_WAVE_WAD_FIRST && w <= PC_WAVE_WAD_LAST))
            check_wad(), files[2]++;
        else if (w <= PC_WAVE_FONT_LAST)
            check_font(block), files[3]++;
        else if (w <= PC_WAVE_WORLD_LAST) {
            check_wad();
            check_world(), files[4]++;
        } else if (w <= PC_WAVE_ROAD_LAST)
            check_road(size), files[5]++;
        else
            check_path(size), files[6]++;
    }
    printf("assetcheck: %u text, %u MusyX, %u WAD, %u font, %u world, %u road, %u path files; %u checks, %u failed\n",
           files[0], files[1], files[2], files[3], files[4], files[5], files[6], g_checked, g_fail);
    return g_fail != 0;
}
