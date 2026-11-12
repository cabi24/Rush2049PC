/* port/assets/tests/test_mods.c: the mods folder (port/mods, docs/port/MODS.md) and the checks a mod's file passes
 * (port/assets/check.c), on files and folders the test makes. ctest assets_mods; `test_assets_mods names` prints the
 * waves' names for tools/mods/rushmods.py's to be compared with (ctest mods_names), and `verdicts FILE...` the check's
 * verdict on each file named <wave>_<anything> (ctest mods_verdicts, against rushmods.py's on the same files).
 *
 *   - the names: by number and back, the unused waves;
 *   - the decompressors: LZSS and DEFLATE round trips, bad streams and the cap;
 *   - the checks: a good file of every kind passes; every truncation and 1000 garbage files of each kind never read
 *     outside the file (under ASan in the sanitizer builds) and the garbage is refused; 3000 small mutations of each,
 *     and the converter runs on every one that passes, in the RDRAM window;
 *   - a mods folder: mod.ini's fields, a mod without one, another game's, files by number and name, a stored LZSS
 *     file, an unknown name, the order (priorities, game.mods_order, [mods] off), two mods for one wave (the first
 *     wins), a refused file falling back to the next mod's, text overrides over the cartridge's bank (the first mod's
 *     string wins), a zip mod, and the file table pointed at the mods' region;
 *   - the memory: no growth, growth at original (16 or more), a car file counted per car, expansion_pak = off (the
 *     larger file refused), more than 256 MiB (the last mod's larger file refused). */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include "assets.h"
#include "config.h"
#include "mods.h"
#include "pc_compat.h"
#include "support.h"
#include "ultra.h"
#include "wad_build.h"
#include "wad_lists_check.h"

void pc_wave_loaded(int wave, void *buf, size_t size);

/* pi.c's: what the mods' region would serve */
static const uint8_t *g_region;
static size_t g_region_size;
void pc_rom_set_mods(const uint8_t *data, size_t size) { g_region = data, g_region_size = size; }

static uint32_t rng = 12345;
static uint32_t rnd(void) { rng = rng * 1103515245u + 12345u; return rng >> 8; }

/* ---------------------------------------------------------------- files of each kind */

typedef struct {
    uint8_t *b;
    size_t n;
} File;

static File text_bank(int strings)
{
    static const uint32_t counts[6] = {0, 4, 0, 2, 2, 0};
    File f = {calloc(1, 4096), 0};
    size_t pos = 0, at[6];
    uint32_t n[6];
    for (int s = 0; s < 6; s++) {
        n[s] = (s == 0 || s == 2 || s == 5) ? (uint32_t)strings : counts[s];
        test_put32(f.b + pos, n[s]);
        at[s] = pos + 4;
        pos += 4 + ((s == 0 || s == 2 || s == 5) ? 4 * n[s] : 2 * ((n[s] + 1) & ~1u));
    }
    for (int s = 0; s < 6; s++)
        for (uint32_t i = 0; (s == 0 || s == 2 || s == 5) && i < n[s]; i++) {
            test_put32(f.b + at[s] + 4 * i, (uint32_t)pos);
            pos += (size_t)sprintf((char *)f.b + pos, "S%d.%u", s, i) + 1;
        }
    f.n = pos;
    return f;
}

static File font_file(void)
{
    File f = {calloc(1, 16 + 95 * 12), 16 + 95 * 12};
    f.b[11] = 1, f.b[12] = 95;   /* one texture page, 95 letters, no kerning */
    for (int i = 0; i < 95; i++)
        f.b[16 + 12 * i + 4] = (uint8_t)(32 + i);
    return f;
}

static File road_file(void)
{
    File f = {calloc(1, 256), 0};
    uint8_t *b = f.b;
    size_t p = 0;
    const uint16_t head[6] = {0, 1, 1, 4, 0, 5};
    for (int i = 0; i < 6; i++, p += 2)
        test_put16(b + p, head[i]);
    test_put32(b + p, 4), p += 4;
    test_put16(b + p, 0xFFFF), p += 4;                 /* the tree node: no parent, every quarter a leaf */
    for (int i = 0; i < 4; i++, p += 2)
        test_put16(b + p, (uint16_t)(i < 2 ? -1000 : 3000));
    for (int i = 0; i < 4; i++, p += 2)
        test_put16(b + p, 1);                          /* each quarter: the surface list at offset 1 */
    test_put16(b + p, 0), test_put16(b + p + 2, 4);    /* the surface */
    test_put16(b + p + 4, 0x4000), test_put16(b + p + 12, 0x4000), test_put16(b + p + 20, 0x4000);
    p += 0x18;
    p += 4 * 8;                                        /* four vertices */
    memcpy(b + p, "\x00\x00\xE3\x00\x00", 5), p += 5;  /* the vertex list */
    memcpy(b + p, "\x00\x01\x00\x00", 4), p += 4;      /* the surface list: the pad, one surface from 0 */
    f.n = p;
    return f;
}

static File path_file(void)
{
    File f = {calloc(1, 0x1000), 0};
    uint8_t *b = f.b;
    size_t p = 0x32C;
    test_put16(b + 8, 4);                  /* four checkpoints */
    test_put16(b + p, 8);                  /* the resurrection path: 8 points, no alternative paths */
    p += 0x10;
    test_put16(b + p, 8), p += 2 + 6 * 8;
    for (int d = 0; d < 4; d++) {          /* four drone paths of 4 points */
        test_put16(b + p, 4);
        p += 8 + 8 * 4;
    }
    f.n = p;
    return f;
}

static File project_file(void)
{
    File f = {calloc(1, 256), 0};
    uint8_t *b = f.b;
    test_put32(b, 8 + 4 * 5 + 2 * 5);      /* a group of type 2: five id lists, each just its end */
    test_put16(b + 4, 1), test_put16(b + 6, 2);
    for (int k = 0; k < 5; k++) {
        test_put32(b + 8 + 4 * k, 20 + 2 * (uint32_t)k);
        test_put16(b + 8 + 20 + 2 * k, 0xFFFF);
    }
    test_put32(b + 38, 0xFFFFFFFF);
    f.n = 42;
    return f;
}

static File wad_file(void)
{
    File f = {calloc(1, W_SIZE), 0};
    f.n = wad_build(f.b);
    for (int p = 0; p < 3; p++)   /* a track's paths link to path records (wad_build.h's point elsewhere) */
        test_put32(f.b + W_PTHD + 0x24 * p + 0x18, p == 2 ? 0 : (uint32_t)(W_PTHD + 0x24 * (p + 1)));
    return f;
}

/* A world WAD: WHDR naming one world, one node (WOBJ), one quadtree leaf (GDAT), no targets (GTLD). */
static File world_file(void)
{
    File f = {calloc(1, 512), 0};
    uint8_t *b = f.b;
    const uint32_t whdr = 8, wobj = 0x20, gdat = wobj + 0x68, gtld = gdat + 0x1C, toc = gtld;
    test_put32(b, toc), test_put32(b + 4, 4);
    test_put32(b + whdr, 1), test_put32(b + whdr + 4, wobj), memcpy(b + whdr + 8, "WORLD", 5);
    memcpy(b + wobj, "TRACK", 5);
    test_put16(b + wobj + 0x44, 0xFFFF), test_put16(b + wobj + 0x46, 0xFFFF);
    const uint32_t e[4][3] = {{0x57484452u, whdr, 0x18}, {0x574F424Au, wobj, 1}, {0x47444154u, gdat, 1},
                              {0x47544C44u, gtld, 0}};
    for (int i = 0; i < 4; i++)
        for (int k = 0; k < 3; k++)
            test_put32(b + toc + 12 * i + 4 * k, e[i][k]);
    f.n = toc + 48;
    return f;
}

typedef struct {
    const char *what;
    int wave;
    File (*make)(void);
} Kind;

static File text_file(void) { return text_bank(3); }
static const Kind k_kinds[] = {
    {"text", 0, text_file}, {"project", 6, project_file}, {"WAD", 90, wad_file}, {"track", 101, wad_file},
    {"font", 38, font_file},
    {"world", 120, world_file}, {"road", 139, road_file}, {"path", 158, path_file}};

/* ---------------------------------------------------------------- the names and the decompressors */

static void test_names(void)
{
    const char *folder;
    CHECK(!strcmp(pc_mods_wave_name(88, &folder), "car01") && !strcmp(folder, "cars"));
    CHECK(!strcmp(pc_mods_wave_name(0, &folder), "text_en") && !strcmp(folder, "text"));
    CHECK(!strcmp(pc_mods_wave_name(139, NULL), "track1_road"));
    CHECK(!strcmp(pc_mods_wave_name(182, NULL), "track6_pathback"));
    CHECK(!strcmp(pc_mods_wave_name(119, NULL), "obstacle"));
    CHECK(pc_mods_wave_name(70, NULL) == NULL && pc_mods_wave_name(183, NULL) == NULL);
    CHECK(pc_mods_wave_by_name("car01") == 88 && pc_mods_wave_by_name("CAR13") == 100);
    CHECK(pc_mods_wave_by_name("wave_088") == 88 && pc_mods_wave_by_name("wave90") == 90);
    CHECK(pc_mods_wave_by_name("wave_183") == -1 && pc_mods_wave_by_name("wave_x") == -1);
    CHECK(pc_mods_wave_by_name("nothing") == -1 && pc_mods_wave_by_name("stunt4_path") == 175);
    for (int w = 0; w < PC_MODS_WAVES; w++)   /* every name leads back to its wave */
        CHECK(!pc_mods_wave_name(w, NULL) || pc_mods_wave_by_name(pc_mods_wave_name(w, NULL)) == w);
}

/* A greedy LZSS stream (the boot format): copies of 3-17 bytes from up to 4095 back, else literals. */
static size_t lzss_encode(const uint8_t *in, size_t n, uint8_t *out)
{
    size_t i = 0, o = 0;
    while (i < n) {
        size_t flag_at = o++;
        uint8_t flags = 0;
        for (int bit = 0; bit < 8 && i < n; bit++) {
            size_t best = 0, dist = 0;
            for (size_t d = 1; d <= i && d < 4096; d++) {
                size_t l = 0;
                while (l < 17 && i + l < n && in[i + l - d] == in[i + l])
                    l++;
                if (l > best)
                    best = l, dist = d;
            }
            if (best >= 3) {
                out[o++] = (uint8_t)((dist >> 4 & 0xF0) | (best - 2));
                out[o++] = (uint8_t)dist;
                i += best;
            } else {
                flags |= (uint8_t)(1 << bit);
                out[o++] = in[i++];
            }
        }
        out[flag_at] = flags;
    }
    out[o++] = 0, out[o++] = 0, out[o++] = 0;   /* a pair 0 0: the end */
    return o;
}

static void test_decompress(void)
{
    uint8_t in[3000], enc[4000];
    size_t n;
    for (size_t i = 0; i < sizeof in; i++)
        in[i] = (uint8_t)(i % 97 < 40 ? 'a' + i % 7 : rnd());
    size_t e = lzss_encode(in, sizeof in, enc);
    uint8_t *out = pc_mods_lzss(enc, e, 1 << 20, &n);
    CHECK(out && n == sizeof in && !memcmp(out, in, n));
    free(out);
    CHECK(pc_mods_lzss(enc, e, sizeof in - 1, &n) == NULL);   /* past the cap */
    const uint8_t back[] = {0x00, 0x01, 0x05};                /* a copy from before the start */
    CHECK(pc_mods_lzss(back, sizeof back, 100, &n) == NULL);
    uLongf zn = compressBound(sizeof in);
    uint8_t z[8192];
    z_stream s;
    memset(&s, 0, sizeof s);
    CHECK(deflateInit2(&s, 9, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) == Z_OK);
    s.next_in = in, s.avail_in = sizeof in, s.next_out = z, s.avail_out = (uInt)sizeof z;
    CHECK(deflate(&s, Z_FINISH) == Z_STREAM_END);
    zn = s.total_out;
    deflateEnd(&s);
    out = pc_mods_inflate(z, zn, 1 << 20, &n);
    CHECK(out && n == sizeof in && !memcmp(out, in, n));
    free(out);
    CHECK(pc_mods_inflate(z, zn / 2, 1 << 20, &n) == NULL);   /* cut short */
    CHECK(pc_mods_inflate(z, zn, 100, &n) == NULL);           /* past the cap */
}

/* ---------------------------------------------------------------- the checks */

/* The converter's messages while it runs (as assetcheck.c reads them): how many say it refused or cut the file. */
static int convert_complaints(int wave, uint8_t *b, size_t n)
{
    static const char *const bad[] = {"runs past", "outside", "no WHDR", "doesn't parse", "not converted",
                                      "left as is", "need 0x"};
    FILE *log = tmpfile();
    char line[512];
    int saved, complaints = 0;
    fflush(stderr);
    saved = log ? dup(2) : -1;
    if (saved >= 0)
        dup2(fileno(log), 2);
    pc_wave_loaded(wave, b, n);
    fflush(stderr);
    if (saved < 0)
        return 0;
    dup2(saved, 2);
    close(saved);
    rewind(log);
    while (fgets(line, sizeof line, log))
        for (size_t i = 0; i < sizeof bad / sizeof *bad; i++)
            if (strstr(line, bad[i])) {
                if (!complaints++)
                    fprintf(stderr, "wave %d: %s", wave, line);
                break;
            }
    fclose(log);
    return complaints;
}

static void test_checks(uint8_t *window)
{
    char why[256];
    for (size_t k = 0; k < sizeof k_kinds / sizeof *k_kinds; k++) {
        const Kind *kd = &k_kinds[k];
        File f = kd->make();
        int ok = pc_assets_check(kd->wave, f.b, f.n, NULL, 0, why, sizeof why);
        if (ok)
            fprintf(stderr, "%s: %s\n", kd->what, why);
        CHECK(ok == 0);
        CHECK(pc_assets_check(kd->wave, f.b, f.n, f.b, f.n, why, sizeof why) == 0);   /* against itself */
        /* every truncation, each in a block of exactly its size (ASan sees a read past it) */
        int refused = 0;
        for (size_t n = 0; n < f.n; n++) {
            uint8_t *cut = malloc(n ? n : 1);
            memcpy(cut, f.b, n);
            refused += pc_assets_check(kd->wave, cut, n, f.b, f.n, why, sizeof why) != 0;
            free(cut);
        }
        CHECK(refused >= (int)f.n / 2);
        CHECK(pc_assets_check(kd->wave, f.b, 0, NULL, 0, why, sizeof why) != 0 && strstr(why, "empty"));
        /* garbage of its size */
        int garbage_refused = 0;
        for (int t = 0; t < 1000; t++) {
            uint8_t *g = malloc(f.n);
            for (size_t i = 0; i < f.n; i++)
                g[i] = (uint8_t)rnd();
            garbage_refused += pc_assets_check(kd->wave, g, f.n, f.b, f.n, why, sizeof why) != 0;
            free(g);
        }
        if (garbage_refused < 1000)
            fprintf(stderr, "%s: %d of 1000 garbage files passed\n", kd->what, 1000 - garbage_refused);
        CHECK(garbage_refused >= 990);
        /* small mutations: those that pass go through the converter, in a window block of their size */
        int passed = 0, complaints = 0, lists_unlike = 0;
        for (int t = 0; t < 3000; t++) {
            uint8_t *blk = window + 0x100000;
            memcpy(blk, f.b, f.n);
            for (int m = 1 + (int)(rnd() % 4); m > 0; m--)
                blk[rnd() % f.n] = (uint8_t)rnd();
            if (pc_assets_check(kd->wave, blk, f.n, f.b, f.n, why, sizeof why) == 0) {
                uint8_t *raw = malloc(f.n);   /* the file before its converter (wad_lists_check) */
                memcpy(raw, blk, f.n);
                passed++;
                complaints += convert_complaints(kd->wave, blk, f.n) != 0;   /* a file that passes converts whole */
                if ((kd->wave >= PC_WAVE_WAD_FIRST && kd->wave <= PC_WAVE_WORLD_LAST) &&   /* its lists the N64's */
                    wad_lists_check(blk, raw, f.n, pc_wad_records(blk), kd->what) != 0)
                    lists_unlike++;
                pc_heap_block_freed(blk, f.n);
                free(raw);
            }
        }
        CHECK(complaints == 0 && lists_unlike == 0);
        fprintf(stderr, "assets_mods: %s: %zu-byte file, %d of %zu truncations and %d of 1000 garbage files refused, "
                "%d of 3000 mutations converted, %d of them not whole, %d with display lists unlike the N64's\n",
                kd->what, f.n, refused, f.n, garbage_refused, passed, complaints, lists_unlike);
        free(f.b);
    }
    /* the cartridge's file's rules: names the game looks up, the text's counts */
    File a = text_bank(3), b = text_bank(2);
    CHECK(pc_assets_check(0, b.b, b.n, a.b, a.n, why, sizeof why) != 0 && strstr(why, "by number"));
    CHECK(pc_assets_check(0, a.b, a.n, b.b, b.n, why, sizeof why) == 0);
    {   /* a string keeps the cartridge's printf conversions: "%d" where it has none is refused, "%%" passes */
        File f = text_bank(3);
        char *s1 = (char *)f.b + pc_be32(f.b + 4 + 4 * 1);
        CHECK(!strcmp(s1, "S0.1"));
        memcpy(s1, "%d.1", 4);
        CHECK(pc_assets_check(0, f.b, f.n, a.b, a.n, why, sizeof why) != 0 && strstr(why, "conversions 'd'"));
        CHECK(pc_assets_check(0, f.b, f.n, NULL, 0, why, sizeof why) == 0);   /* without the cartridge's: no rule */
        memcpy(s1, "%%.1", 4);
        CHECK(pc_assets_check(0, f.b, f.n, a.b, a.n, why, sizeof why) == 0);
        free(f.b);
    }
    File w = wad_file(), w2 = wad_file();
    w2.b[0x100] ^= 1;   /* no such place: the model chunk's first name, compared below by its effect */
    CHECK(pc_assets_check(90, w.b, w.n, w.b, w.n, why, sizeof why) == 0);
    File world = world_file();   /* wrong kind: a world WAD in a car's place */
    CHECK(pc_assets_check(90, world.b, world.n, w.b, w.n, why, sizeof why) != 0);
    CHECK(pc_assets_check(120, w.b, w.n, world.b, world.n, why, sizeof why) != 0 && strstr(why, "WHDR"));
    CHECK(pc_assets_check(139, w.b, w.n, NULL, 0, why, sizeof why) != 0);   /* a WAD as a road */
    free(a.b), free(b.b), free(w.b), free(w2.b), free(world.b);
}

/* `verdicts FILE...`: what the check says of each file, its wave the number its name starts with, against the file
 * CART/<wave> when CART is given first as -c CART (ctest mods_verdicts compares rushmods.py's). */
static int verdicts(int argc, char **argv)
{
    const char *cart = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-c") && i + 1 < argc) {
            cart = argv[++i];
            continue;
        }
        const char *base = strrchr(argv[i], '/'), *bs = strrchr(argv[i], '\\');   /* Windows: either */
        if (bs && (!base || bs > base))
            base = bs;
        base = base ? base + 1 : argv[i];
        int wave = atoi(base);
        FILE *f = fopen(argv[i], "rb");
        uint8_t *b = malloc(1 << 24), *c = NULL;
        size_t n = f ? fread(b, 1, 1 << 24, f) : 0, cn = 0;
        char why[256], cp[1024];
        if (f)
            fclose(f);
        if (cart) {
            snprintf(cp, sizeof cp, "%s/%d", cart, wave);
            if ((f = fopen(cp, "rb")) != NULL) {
                c = malloc(1 << 24);
                cn = fread(c, 1, 1 << 24, f);
                fclose(f);
            }
        }
        int r = pc_assets_check(wave, b, n, c, cn, why, sizeof why);
        printf("%s: %s\n", base, r ? "refused" : "ok");
        free(b), free(c);
    }
    return 0;
}

/* ---------------------------------------------------------------- a cartridge and a mods folder */

/* A synthetic cartridge: a boot and a game image at KSEG0 addresses of the test's own, the file table's four symbols at
 * places of its own in them (handed to pc_mods_cart_read as main.c hands it the game's, found by name), the files
 * after. SAMPLE_BASE is the ROM offset the sample directory's offsets count from (AudioInitSys.c's
 * SAMPLE_BANK_ROM_OFFSET), and SAMPLE_BASE_K1 the cartridge address the game adds them to. */
#define BOOT_BASE (N64_KSEG0 + 0x2E000u)
#define GAME_BASE (N64_KSEG0 + 0x11B000u)
#define IMG_SIZE 0x9000u
#define ROM_BOOT 0x1000u
#define ROM_GAME 0x10000u
#define ROM_FILES 0x20000u
#define AT_SIZES 0x580u          /* gWaveSizes, in the boot image */
#define AT_TEXT_INITIAL 0x860u   /* gLargestTextWave, in the boot image */
#define AT_OFFSETS 0x5BCu        /* wave_rom_offsets, in the game image */
#define AT_KINDS 0x8564u         /* wave_storage_kinds, in the game image */
#define SAMPLE_BASE 0x39B40u
#define SAMPLE_BASE_K1 (N64_CART_KSEG1 | SAMPLE_BASE)

static const PcModsTableAt k_table_at = {BOOT_BASE + AT_SIZES, BOOT_BASE + AT_TEXT_INITIAL, GAME_BASE + AT_OFFSETS,
                                         GAME_BASE + AT_KINDS};

typedef struct {
    uint8_t *rom;
    size_t size, at;
    PcModsImage img[2];
} TestCart;

static void cart_file(TestCart *t, int wave, const uint8_t *b, size_t n, int kind)
{
    uint8_t *boot = t->rom + ROM_BOOT, *game = t->rom + ROM_GAME, enc[65536];
    size_t stored = n;
    if (kind == 1)
        stored = lzss_encode(b, n, enc), b = enc;
    if (kind == 2) {
        z_stream s;
        memset(&s, 0, sizeof s);
        deflateInit2(&s, 9, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
        s.next_in = (Bytef *)b, s.avail_in = (uInt)n, s.next_out = enc, s.avail_out = sizeof enc;
        deflate(&s, Z_FINISH);
        stored = s.total_out;
        deflateEnd(&s);
        b = enc;
    }
    memcpy(t->rom + t->at, b, stored);
    test_put32(boot + AT_SIZES + 4 * wave, (uint32_t)n);           /* gWaveSizes */
    test_put32(game + AT_OFFSETS + 4 * wave, (uint32_t)t->at);     /* the ROM offset */
    test_put32(game + AT_KINDS + 4 * wave, (uint32_t)kind);
    t->at = (t->at + stored + 15) & ~(size_t)15;
}

static void cart_make(TestCart *t)
{
    t->size = 4 << 20;
    t->rom = calloc(1, t->size);
    t->at = ROM_FILES;
    t->img[0] = (PcModsImage){"boot", 0, ROM_BOOT, IMG_SIZE, IMG_SIZE, BOOT_BASE};
    t->img[1] = (PcModsImage){"game", 0, ROM_GAME, IMG_SIZE, IMG_SIZE, GAME_BASE};
    t->rom[ROM_BOOT + AT_TEXT_INITIAL] = 0;   /* the first text bank: English */
    for (int w = 0; w < PC_MODS_WAVES; w++) {   /* every wave a small file of zeros by default */
        static uint8_t z[64];
        cart_file(t, w, z, sizeof z, 0);
    }
}

static void put_file(const char *path, const void *b, size_t n)
{
    char dir[1024];
    snprintf(dir, sizeof dir, "%s", path);
    for (char *c = dir + 1; *c; c++)
        if (*c == '/') {
            *c = 0;
            mkdir(dir, 0755);
            *c = '/';
        }
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f) {
        fwrite(b, 1, n, f);
        fclose(f);
    }
}

static void put_text(const char *path, const char *text) { put_file(path, text, strlen(text)); }

static void rm_tree(const char *path)
{
    DIR *d = opendir(path);
    struct dirent *e;
    if (!d) {
        unlink(path);
        return;
    }
    while ((e = readdir(d)) != NULL) {
        char p[1024];
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        snprintf(p, sizeof p, "%s/%s", path, e->d_name);
        rm_tree(p);
    }
    closedir(d);
    rmdir(path);
}

typedef struct {
    const char *off[6];
} Flags;

static int flag_of(void *user, const char *folder)
{
    const Flags *f = user;
    for (int i = 0; i < 6 && f->off[i]; i++)
        if (!strcasecmp(f->off[i], folder))
            return 0;
    return -1;
}

static const PcMod *find_mod(const PcMods *m, const char *folder)
{
    for (int i = 0; i < m->n; i++)
        if (!strcmp(m->mod[i].folder, folder))
            return &m->mod[i];
    return NULL;
}

/* A stored zip of one folder's files, written by the test (local headers, central directory, end record). */
static void zip_write(const char *path, const char *const *names, const uint8_t *const *data, const size_t *sizes,
                      int n)
{
    uint8_t *z = calloc(1, 1 << 20);
    size_t at = 0, cd, lho[16];
    for (int i = 0; i < n; i++) {
        uint32_t crc = (uint32_t)crc32(0, data[i], (uInt)sizes[i]);
        size_t nl = strlen(names[i]);
        lho[i] = at;
        uint8_t *h = z + at;
        h[0] = 'P', h[1] = 'K', h[2] = 3, h[3] = 4, h[4] = 20;
        memcpy(h + 14, &crc, 4);
        h[18] = (uint8_t)sizes[i], h[19] = (uint8_t)(sizes[i] >> 8);
        h[22] = (uint8_t)sizes[i], h[23] = (uint8_t)(sizes[i] >> 8);
        h[26] = (uint8_t)nl;
        memcpy(h + 30, names[i], nl);
        memcpy(h + 30 + nl, data[i], sizes[i]);
        at += 30 + nl + sizes[i];
    }
    cd = at;
    for (int i = 0; i < n; i++) {
        uint32_t crc = (uint32_t)crc32(0, data[i], (uInt)sizes[i]);
        size_t nl = strlen(names[i]);
        uint8_t *h = z + at;
        h[0] = 'P', h[1] = 'K', h[2] = 1, h[3] = 2, h[4] = 20, h[6] = 20;
        memcpy(h + 16, &crc, 4);
        h[20] = (uint8_t)sizes[i], h[21] = (uint8_t)(sizes[i] >> 8);
        h[24] = (uint8_t)sizes[i], h[25] = (uint8_t)(sizes[i] >> 8);
        h[28] = (uint8_t)nl;
        h[42] = (uint8_t)lho[i], h[43] = (uint8_t)(lho[i] >> 8), h[44] = (uint8_t)(lho[i] >> 16);
        memcpy(h + 46, names[i], nl);
        at += 46 + nl;
    }
    uint8_t *e = z + at;
    e[0] = 'P', e[1] = 'K', e[2] = 5, e[3] = 6;
    e[8] = e[10] = (uint8_t)n;
    size_t cds = at - cd;
    e[12] = (uint8_t)cds, e[13] = (uint8_t)(cds >> 8);
    e[16] = (uint8_t)cd, e[17] = (uint8_t)(cd >> 8), e[18] = (uint8_t)(cd >> 16);
    put_file(path, z, at + 22);
    free(z);
}

/* A car WAD whose one model's list calls a list 64 times, which calls another 64 times, seven deep: 64^7 walks of the
 * last for the mirror pass (days). Refused in a moment (check.c's DL_STEPS). */
static void test_dl_budget(void)
{
    enum { K = 64, DEPTH = 8, LIST = (K + 1) * 8, DL0 = 0x80 };
    size_t n = DL0 + DEPTH * LIST;
    uint8_t *b = calloc(1, n);
    char why[PC_MODS_WHY] = "";
    test_put32(b, 8), test_put32(b + 4, 1);   /* the TOC: one OBHD chunk of one model */
    memcpy(b + 8, "OBHD", 4), test_put32(b + 12, 0x20), test_put32(b + 16, 1);
    memcpy(b + 0x20, "m", 1);
    b[0x20 + 0x17] = 1;                         /* one level of detail */
    test_put32(b + 0x20 + 0x18 + 8, DL0);
    for (int d = 0; d < DEPTH; d++) {
        uint8_t *l = b + DL0 + d * LIST;
        for (int k = 0; d + 1 < DEPTH && k < K; k++)
            test_put32(l + 8 * k, 0xDE000000u), test_put32(l + 8 * k + 4, DL0 + (d + 1) * LIST);
        test_put32(l + (d + 1 < DEPTH ? 8 * K : 0), 0xDF000000u);
    }
    CHECK(pc_assets_check(88, b, n, NULL, 0, why, sizeof why) != 0 && strstr(why, "steps"));
    test_put32(b + DL0 + 4, DL0 + (DEPTH - 1) * LIST);   /* one call of the last list, no fan: it passes */
    test_put32(b + DL0 + 8, 0xDF000000u);
    CHECK(pc_assets_check(88, b, n, NULL, 0, why, sizeof why) == 0);
    free(b);
}

static void put_le(uint8_t *p, uint32_t v, int bytes)
{
    for (int i = 0; i < bytes; i++)
        p[i] = (uint8_t)(v >> 8 * i);
}

/* A zip bomb: a few hundred KB whose 8 entries all name one deflated stream of 200 MiB of zeros, 1.6 GB unpacked:
 * refused before any of it is unpacked (zip.c's UNPACKED_MAX, 1 GiB). */
static void test_zip_bomb(void)
{
    enum { ENTRIES = 8, NAME = 6 };
    const uint32_t usize = 200u << 20;
    const size_t cap = 4u << 20;
    const char *tmp = getenv("TMPDIR") ? getenv("TMPDIR") : getenv("TEMP") ? getenv("TEMP") : "/tmp";
    uint8_t *zero = calloc(1, 1 << 20), *z = calloc(1, cap);
    char path[512], why[PC_MODS_WHY] = "";
    PcZipEntry *e = NULL;
    int n = -1;
    z_stream s;
    memset(&s, 0, sizeof s);
    CHECK(zero && z && deflateInit2(&s, 1, Z_DEFLATED, -15, 8, Z_RLE) == Z_OK);
    if (!zero || !z)
        return;
    size_t at = 30 + NAME;
    s.next_out = z + at, s.avail_out = (uInt)(cap - at - 4096);
    int r = Z_OK;
    for (uint32_t done = 0; done < usize; done += 1 << 20) {
        s.next_in = zero, s.avail_in = 1 << 20;
        r = deflate(&s, done + (1u << 20) >= usize ? Z_FINISH : Z_NO_FLUSH);
    }
    CHECK(r == Z_STREAM_END);
    uint32_t csize = (uint32_t)s.total_out;
    deflateEnd(&s);
    memcpy(z, "PK\3\4", 4), put_le(z + 4, 20, 2), put_le(z + 8, 8, 2);
    put_le(z + 18, csize, 4), put_le(z + 22, usize, 4), put_le(z + 26, NAME, 2);
    memcpy(z + 30, "b0.bin", NAME);
    at += csize;
    size_t cd = at;
    for (int i = 0; i < ENTRIES; i++, at += 46 + NAME) {
        uint8_t *h = z + at;
        memcpy(h, "PK\1\2", 4), put_le(h + 4, 20, 2), put_le(h + 6, 20, 2), put_le(h + 10, 8, 2);
        put_le(h + 20, csize, 4), put_le(h + 24, usize, 4), put_le(h + 28, NAME, 2), put_le(h + 42, 0, 4);
        snprintf((char *)h + 46, NAME + 1, "b%d.bin", i);
    }
    memcpy(z + at, "PK\5\6", 4), put_le(z + at + 8, ENTRIES, 2), put_le(z + at + 10, ENTRIES, 2);
    put_le(z + at + 12, (uint32_t)(at - cd), 4), put_le(z + at + 16, (uint32_t)cd, 4);
    snprintf(path, sizeof path, "%s/rush2049pc-zipbomb-%d.zip", tmp, (int)getpid());
    put_file(path, z, at + 22);
    CHECK(pc_mods_zip_read(path, &e, &n, why, sizeof why) == -1 && strstr(why, "unpack") && e == NULL);
    if (e)
        pc_mods_zip_free(e, n);
    remove(path);
    free(zero), free(z);
}

/* A zip's texture pack unpacked into the cache: a name Windows would take for a device (con.png: the write goes to the
 * console) or a stream (a:b.png), or change (a folder "x."), is not unpacked, on any system; a good one is. */
static void test_zip_names(void)
{
    char dir[512], cache[600], p[1200];
    const char *tmp = getenv("TMPDIR") ? getenv("TMPDIR") : getenv("TEMP") ? getenv("TEMP") : "/tmp";
    snprintf(dir, sizeof dir, "%s/rush2049pc-zipnames-%d", tmp, (int)getpid());
    snprintf(cache, sizeof cache, "%s/cache", dir);
    rm_tree(dir);
    CHECK(mkdir(dir, 0700) == 0);
    static const char *const names[6] = {"z/mod.ini", "z/textures/good.png", "z/textures/con.png",
                                         "z/textures/a:b.png", "z/textures/x./c.png", "z/textures/COM1.tex.png"};
    const char *ini = "[mod]\nname = Names\ngame = us\n";
    const uint8_t png[4] = {0x89, 'P', 'N', 'G'};
    const uint8_t *data[6] = {(const uint8_t *)ini, png, png, png, png, png};
    size_t sizes[6] = {strlen(ini), 4, 4, 4, 4, 4};
    snprintf(p, sizeof p, "%s/names.zip", dir);
    zip_write(p, names, data, sizes, 6);
    Flags fl = {{NULL}};
    PcModsConfig cfg = {1, dir, "", flag_of, &fl, 0, 1, 0, cache};
    PcMods m;
    CHECK(pc_mods_scan(&m, &cfg) == 0 && find_mod(&m, "names") && find_mod(&m, "names")->textures[0]);
    struct stat st;
    snprintf(p, sizeof p, "%s/names/textures/good.png", cache);
    CHECK(stat(p, &st) == 0 && st.st_size == 4);
    static const char *const bad[4] = {"con.png", "a:b.png", "x.", "COM1.tex.png"};
    for (int i = 0; i < 4; i++) {
        snprintf(p, sizeof p, "%s/names/textures/%s", cache, bad[i]);
        CHECK(stat(p, &st) != 0);
    }
    pc_mods_free(&m);
    rm_tree(dir);
}

static void test_folder(void)
{
    char dir[512], p[1024], line[512];
    const char *tmp = getenv("TMPDIR") ? getenv("TMPDIR") : getenv("TEMP") ? getenv("TEMP") : "/tmp";
    TestCart t;
    PcModsCart cart;
    PcMods m;
    snprintf(dir, sizeof dir, "%s/rush2049pc-mods-%d", tmp, (int)getpid());
    rm_tree(dir);
    if (mkdir(dir, 0700) != 0) {
        CHECK(0);
        return;
    }
    cart_make(&t);
    File bank = text_bank(3), wad = wad_file(), road = road_file(), font = font_file();
    cart_file(&t, 0, bank.b, bank.n, 1);   /* the English bank, LZSS as the cartridge stores them */
    cart_file(&t, 38, font.b, font.n, 2);  /* a font, DEFLATE */
    cart_file(&t, 90, wad.b, wad.n, 0);
    cart_file(&t, 91, wad.b, wad.n, 0);
    cart_file(&t, 139, road.b, road.n, 0);
    CHECK(pc_mods_cart_read(&cart, t.rom, t.size, t.img, 2, &k_table_at) == 0);
    CHECK(cart.size[0] == bank.n && cart.kind[0] == 1 && cart.size[90] == wad.n);
    size_t n = 0;
    uint8_t *got = pc_mods_cart_file(&cart, 0, &n);
    CHECK(got && n == bank.n && !memcmp(got, bank.b, n));
    free(got);

    /* red: priority 5, car03 by name (a recoloured copy) and wave 91 as stored LZSS, a text override */
    File red = wad_file();
    red.b[red.n - 1] ^= 0x55;
    snprintf(p, sizeof p, "%s/red/mod.ini", dir);
    put_text(p, "[mod]\nname = Red Cars\nauthor = Someone\nversion = 1.2\ndescription = Red\ngame = us\npriority = 5\n"
                "colour = red\n");
    snprintf(p, sizeof p, "%s/red/cars/car03.bin", dir);
    put_file(p, red.b, red.n);
    {
        uint8_t enc[65536];
        size_t e = lzss_encode(red.b, red.n, enc);
        snprintf(p, sizeof p, "%s/red/cars/wave_091.lzs", dir);
        put_file(p, enc, e);
    }
    snprintf(p, sizeof p, "%s/red/text/en.txt", dir);
    put_text(p, "[single]\n1 = \"RED \\x41\"\n[taba]\n0 = from red\n[bogus]\n1 = x\n");
    snprintf(p, sizeof p, "%s/red/README.txt", dir);
    put_text(p, "notes\n");
    snprintf(p, sizeof p, "%s/red/cars/notacar.bin", dir);
    put_text(p, "x");
    /* blue: priority 0, the same car (loses), the road truncated (refused), a text override of the same string */
    File blue = wad_file();
    blue.b[blue.n - 1] ^= 0x33;
    snprintf(p, sizeof p, "%s/blue/mod.ini", dir);
    put_text(p, "[mod]\nname = Blue\ngame = us\n");
    snprintf(p, sizeof p, "%s/blue/wave_090.bin", dir);
    put_file(p, blue.b, blue.n);
    snprintf(p, sizeof p, "%s/blue/tracks/track1_road.bin", dir);
    put_file(p, road.b, road.n / 2);
    snprintf(p, sizeof p, "%s/blue/text/en.txt", dir);
    put_text(p, "[single]\n1 = BLUE\n2 = blue two\n");
    /* green: a good road under the bad one's mod: the bad one is first only if ordered so */
    File road2 = road_file();
    road2.b[0x10 + 4] ^= 0;   /* the same road */
    snprintf(p, sizeof p, "%s/green/mod.ini", dir);
    put_text(p, "[mod]\nname = Green\ngame = us\npriority = -1\n");
    snprintf(p, sizeof p, "%s/green/tracks/track1_road.bin", dir);
    put_file(p, road2.b, road2.n);
    /* problems: no mod.ini; another game; a name the ini can't hold */
    snprintf(p, sizeof p, "%s/noini/wave_090.bin", dir);
    put_file(p, wad.b, wad.n);
    snprintf(p, sizeof p, "%s/euro/mod.ini", dir);
    put_text(p, "[mod]\ngame = eu\n");
    snprintf(p, sizeof p, "%s/bad,name/mod.ini", dir);
    put_text(p, "[mod]\n");
    snprintf(p, sizeof p, "%s/README.txt", dir);
    put_text(p, "the folder's own readme\n");
    /* off: listed off in [mods] */
    snprintf(p, sizeof p, "%s/off/mod.ini", dir);
    put_text(p, "[mod]\nname = Off\ngame = us\npriority = 100\n");
    snprintf(p, sizeof p, "%s/off/wave_090.bin", dir);
    put_file(p, wad.b, wad.n);
    /* a zip: a font file in a folder of its own */
    {
        File font = font_file();
        const char *names[2] = {"zipped/mod.ini", "zipped/fonts/fontdat00.bin"};
        const char *ini = "[mod]\nname = Zipped\ngame = us\n";
        const uint8_t *data[2] = {(const uint8_t *)ini, font.b};
        size_t sizes[2] = {strlen(ini), font.n};
        snprintf(p, sizeof p, "%s/zipped.zip", dir);
        zip_write(p, names, data, sizes, 2);
        free(font.b);
    }

    Flags fl = {{"off", NULL}};
    PcModsConfig cfg = {1, dir, "", flag_of, &fl, 0, 1, 0, ""};
    CHECK(pc_mods_scan(&m, &cfg) == 0);
    CHECK(m.n == 7);   /* blue, euro, green, noini, off, red, zipped; not bad,name nor the README */
    const PcMod *r = find_mod(&m, "red"), *b = find_mod(&m, "blue"), *g = find_mod(&m, "green");
    CHECK(r && !strcmp(r->name, "Red Cars") && !strcmp(r->author, "Someone") && !strcmp(r->version, "1.2") &&
          r->priority == 5 && r->on && !r->problem[0]);
    CHECK(r && r->nfiles == 2 && r->nstrings == 2);
    CHECK(find_mod(&m, "noini") && strstr(find_mod(&m, "noini")->problem, "mod.ini"));
    CHECK(find_mod(&m, "euro") && strstr(find_mod(&m, "euro")->problem, "'eu'"));
    CHECK(find_mod(&m, "off") && !find_mod(&m, "off")->on);
    CHECK(find_mod(&m, "zipped") && find_mod(&m, "zipped")->zip && find_mod(&m, "zipped")->nfiles == 1);
    CHECK(m.order[0] == (int)(find_mod(&m, "off") - m.mod) && m.order[1] == (int)(r - m.mod));   /* by priority */
    CHECK(b && g && b->place < g->place);
    CHECK(pc_mods_prepare(&m, &cfg, &cart, SAMPLE_BASE) == 0);
    CHECK(m.wave_mod[90] == (int)(r - m.mod));   /* red's car03 (wave 90) over blue's */
    CHECK(m.wave_mod[91] == (int)(r - m.mod) && m.wave_size[91] == red.n && !memcmp(m.wave_data[91], red.b, red.n));
    CHECK(m.wave_mod[139] == (int)(g - m.mod));  /* blue's road refused: green's is next */
    CHECK(b->refused == 1 && b->used == 0);
    CHECK(m.wave_mod[38] == (int)(find_mod(&m, "zipped") - m.mod));
    CHECK(m.memory == 16 && m.growth > 0);   /* the text bank is larger: the heap whole, 16 MiB */
    /* the English bank: red's string 1 and taba 0, blue's string 2 */
    CHECK(m.made[0] != NULL);
    if (m.made[0]) {
        uint32_t s1 = pc_be32(m.made[0] + 4 + 4 * 1), s2 = pc_be32(m.made[0] + 4 + 4 * 2);
        uint32_t taba = 16 + 12 + 4;   /* single's count and 3 offsets, tab's count and 4 u16, taba's count */
        CHECK(!strcmp((char *)m.made[0] + s1, "RED A"));
        CHECK(!strcmp((char *)m.made[0] + s2, "blue two"));
        CHECK(!strcmp((char *)m.made[0] + pc_be32(m.made[0] + taba), "from red"));
        CHECK(pc_assets_check(0, m.made[0], m.wave_size[0], bank.b, bank.n, line, sizeof line) == 0);
        CHECK(m.wave_size[0] > bank.n);
    }
    pc_mods_summary(&m, line, sizeof line);
    CHECK(strstr(line, "4 on:") && strstr(line, "Red Cars 1.2 (2 files, text)"));
    /* the file table */
    uint32_t off[PC_MODS_WAVES], size[PC_MODS_WAVES];
    int32_t kind[PC_MODS_WAVES];
    memcpy(off, cart.off, sizeof off), memcpy(size, cart.size, sizeof size), memcpy(kind, cart.kind, sizeof kind);
    int changed = pc_mods_install(&m, off, size, kind);
    CHECK(changed == 5);   /* 0, 38, 90, 91, 139 */
    CHECK(g_region && off[90] >= PC_ROM_MODS_BASE && kind[90] == 0 && size[90] == red.n);
    CHECK(g_region && !memcmp(g_region + (off[90] - PC_ROM_MODS_BASE), red.b, red.n));
    CHECK(off[92] == cart.off[92] && kind[0] == 0 && size[0] == m.wave_size[0]);
    pc_mods_free(&m);
    CHECK(g_region == NULL);

    /* game.mods_order puts blue first: its car and its (refused) road; green's road still fills in */
    cfg.order = "blue, red";
    CHECK(pc_mods_scan(&m, &cfg) == 0 && pc_mods_prepare(&m, &cfg, &cart, SAMPLE_BASE) == 0);
    CHECK(m.wave_mod[90] == (int)(find_mod(&m, "blue") - m.mod) && m.wave_mod[139] == (int)(find_mod(&m, "green") - m.mod));
    pc_mods_free(&m);
    /* game.mods = off: listed, nothing used */
    cfg.enabled = 0;
    CHECK(pc_mods_scan(&m, &cfg) == 0 && pc_mods_prepare(&m, &cfg, &cart, SAMPLE_BASE) == 0);
    CHECK(m.n == 7 && m.wave_mod[90] == -1 && find_mod(&m, "red")->nfiles == 2 && !find_mod(&m, "red")->files[0].data);
    pc_mods_install(&m, off, size, kind);
    CHECK(g_region == NULL);
    pc_mods_summary(&m, line, sizeof line);
    CHECK(strstr(line, "off"));
    pc_mods_free(&m);
    cfg.enabled = 1, cfg.order = "";

    /* the memory: red's car 3 MB larger */
    File big = {calloc(1, wad.n + (3u << 20)), wad.n + (3u << 20)};
    memcpy(big.b, red.b, red.n);
    snprintf(p, sizeof p, "%s/red/cars/car03.bin", dir);
    put_file(p, big.b, big.n);
    CHECK(pc_mods_scan(&m, &cfg) == 0 && pc_mods_prepare(&m, &cfg, &cart, SAMPLE_BASE) == 0);
    CHECK(m.wave_mod[90] == (int)(find_mod(&m, "red") - m.mod));
    CHECK(m.growth >= 6ull * (3u << 20) && m.memory == 32);   /* 8 MiB + 6 cars x 3 MB: 26 MiB, so 32 */
    pc_mods_free(&m);
    cfg.cars = 12;   /* 12 cars: 8 + 36 MiB */
    CHECK(pc_mods_scan(&m, &cfg) == 0 && pc_mods_prepare(&m, &cfg, &cart, SAMPLE_BASE) == 0 && m.memory == 64);
    pc_mods_free(&m);
    cfg.cars = 0, cfg.memory = 64;   /* room already */
    CHECK(pc_mods_scan(&m, &cfg) == 0 && pc_mods_prepare(&m, &cfg, &cart, SAMPLE_BASE) == 0 && m.memory == 64);
    pc_mods_free(&m);
    cfg.memory = 0, cfg.expansion_pak = 0;   /* no Expansion Pak: the larger car refused, the cartridge's used */
    CHECK(pc_mods_scan(&m, &cfg) == 0 && pc_mods_prepare(&m, &cfg, &cart, SAMPLE_BASE) == 0 && m.memory == 0);
    CHECK(m.wave_mod[90] == -1 && find_mod(&m, "red")->refused >= 1);
    pc_mods_free(&m);
    cfg.expansion_pak = 1;
    free(big.b);
    big = (File){calloc(1, wad.n + (60u << 20)), wad.n + (60u << 20)};   /* 6 x 60 MB: past 256 MiB */
    memcpy(big.b, red.b, red.n);
    put_file(p, big.b, big.n);
    CHECK(pc_mods_scan(&m, &cfg) == 0 && pc_mods_prepare(&m, &cfg, &cart, SAMPLE_BASE) == 0);
    CHECK(m.wave_mod[90] == -1 && m.memory == 16 && m.growth < 4096);   /* the text bank's growth is left */
    pc_mods_free(&m);
    free(big.b);
    /* moved samples: a mod's sample data (wave 9) goes in the region, and the sample directory (wave 8, the
     * cartridge's here) is made again with every record's offset counting from the samples' new place */
    {
        uint8_t sdir[3 * 0x1C] = {0}, samples[300];
        test_put16(sdir, 1), test_put32(sdir + 4, 0x10);
        test_put16(sdir + 0x1C, 2), test_put32(sdir + 0x1C + 4, 0x40);
        test_put16(sdir + 0x38, 0xFFFF);
        cart_file(&t, 8, sdir, sizeof sdir, 0);
        CHECK(pc_mods_cart_read(&cart, t.rom, t.size, t.img, 2, &k_table_at) == 0);
        memset(samples, 0x5A, sizeof samples);
        snprintf(p, sizeof p, "%s/snd/mod.ini", dir);
        put_text(p, "[mod]\nname = Sounds\ngame = us\n");
        snprintf(p, sizeof p, "%s/snd/sound/samples.bin", dir);
        put_file(p, samples, sizeof samples);
        Flags snd = {{"red", "blue", "green", "zipped", "off"}};
        cfg.user = &snd;
        CHECK(pc_mods_scan(&m, &cfg) == 0 && pc_mods_prepare(&m, &cfg, &cart, SAMPLE_BASE) == 0);
        CHECK(m.wave_data[9] && m.made[8] && m.growth == 0 && m.memory == 0);   /* samples aren't heap */
        memcpy(off, cart.off, sizeof off), memcpy(size, cart.size, sizeof size), memcpy(kind, cart.kind, sizeof kind);
        CHECK(pc_mods_install(&m, off, size, kind) == 2 && off[9] == cart.off[9] && off[8] >= PC_ROM_MODS_BASE);
        if (g_region && off[8] >= PC_ROM_MODS_BASE) {
            const uint8_t *d = g_region + (off[8] - PC_ROM_MODS_BASE);
            uint32_t at = 0xFFFFFFFF;
            for (size_t i = 0; i + sizeof samples <= g_region_size; i++)
                if (!memcmp(g_region + i, samples, sizeof samples)) {
                    at = (uint32_t)i;
                    break;
                }
            CHECK(at != 0xFFFFFFFF);
            /* the game's sum: the base's cartridge address + offset = the samples' new cartridge address */
            CHECK(SAMPLE_BASE_K1 + pc_be32(d + 4) == (N64_CART_KSEG1 | (PC_ROM_MODS_BASE + at)) + 0x10);
            CHECK(SAMPLE_BASE_K1 + pc_be32(d + 0x1C + 4) == (N64_CART_KSEG1 | (PC_ROM_MODS_BASE + at)) + 0x40);
            CHECK(pc_be16(d + 0x38) == 0xFFFF);
        }
        pc_mods_free(&m);
        snprintf(p, sizeof p, "%s/snd", dir);
        rm_tree(p);
    }
    /* only green, whose road is the cartridge's size: no growth, original stays */
    Flags only = {{"red", "blue", "off", "zipped"}};
    cfg.user = &only;
    CHECK(pc_mods_scan(&m, &cfg) == 0 && pc_mods_prepare(&m, &cfg, &cart, SAMPLE_BASE) == 0);
    CHECK(m.wave_mod[139] == (int)(find_mod(&m, "green") - m.mod) && m.growth == 0 && m.memory == 0);
    pc_mods_free(&m);

    rm_tree(dir);
    free(bank.b), free(wad.b), free(road.b), free(font.b), free(red.b), free(blue.b), free(road2.b), free(t.rom);
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "names")) {
        for (int w = 0; w < PC_MODS_WAVES; w++) {
            const char *folder, *name = pc_mods_wave_name(w, &folder);
            if (name)
                printf("%d %s/%s\n", w, folder, name);
        }
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "verdicts"))
        return verdicts(argc, argv);
    if (argc > 2 && !strcmp(argv[1], "samples")) {   /* a good file of each kind, as <wave>: rushmods.py's corpus */
        char p[1024];
        for (size_t k = 0; k < sizeof k_kinds / sizeof *k_kinds; k++) {
            File f = k_kinds[k].make();
            snprintf(p, sizeof p, "%s/%d", argv[2], k_kinds[k].wave);
            put_file(p, f.b, f.n);
            free(f.b);
        }
        return 0;
    }
    uint8_t *window = test_rdram();
    if (!window) {
        printf("assets_mods: the RDRAM window can't be mapped here: skipped\n");
        return 77;
    }
    test_names();
    test_decompress();
    test_checks(window);
    test_folder();
    test_zip_bomb();
    test_dl_budget();
    test_zip_names();
    return TEST_DONE();
}
