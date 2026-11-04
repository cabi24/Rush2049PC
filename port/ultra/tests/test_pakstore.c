/* port/ultra/tests/test_pakstore.c: a controller pak's notes as files (port/ultra/pakfs.c, pakstore.c,
 * docs/port/SAVES.md), in a folder under the directory given (argv[1]): an image's notes and an image of notes (round
 * trips, scattered pages, a full pak, broken chains), the save file (round trip, every damage refused), the ghost
 * check (every field, an inflate that runs past its size), the saves folder (the .mpk imported once and left alone,
 * mpk -> files -> mpk with the same notes at the same slots, a note added, changed in place, deleted, made again,
 * renamed, written while not whole yet), the choice of what goes on the pak (more ghosts than fit, pins, the reserve),
 * imports (good, already there, refused), exports, a damaged file restored from its backup, a missing folder, an
 * unwritable one, and a fuzz of the readers (a ghost file mutated 20,000 ways, random images). Exit status: the
 * number of checks that failed. */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include "pakfs.h"
#include "ultra.h"
#include "pc_compat.h"

static int g_failed, g_checks;
#define CHECK(cond, ...)                                                                                          \
    do {                                                                                                          \
        g_checks++;                                                                                               \
        if (!(cond)) {                                                                                            \
            g_failed++;                                                                                           \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                           \
            printf(__VA_ARGS__);                                                                                  \
            printf("\n");                                                                                         \
        }                                                                                                         \
    } while (0)

static char g_root[512];
static uint32_t g_seed = 12345;
static uint32_t rnd(void)
{
    g_seed = g_seed * 1103515245u + 12345u;
    return g_seed >> 8;
}

static void path_in(char *out, size_t n, const char *dir, const char *name)
{
    snprintf(out, n, "%s/%s", dir, name);
}

static int file_exists(const char *dir, const char *name)
{
    char p[1200];
    struct stat st;
    path_in(p, sizeof p, dir, name);
    return stat(p, &st) == 0;
}

static int count_files(const char *dir, const char *ext)
{
    DIR *d = opendir(dir);
    int n = 0;
    if (!d)
        return -1;
    for (struct dirent *de; (de = readdir(d));) {
        size_t a = strlen(de->d_name), b = strlen(ext);
        n += de->d_name[0] != '.' && a > b && !strcmp(de->d_name + a - b, ext);
    }
    closedir(d);
    return n;
}

/* A folder and everything in it removed (no shell: the Windows build runs this too). */
static void remove_tree(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        for (struct dirent *de; d && (de = readdir(d));) {
            char sub[1200];
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
                continue;
            path_in(sub, sizeof sub, path, de->d_name);
            remove_tree(sub);
        }
        if (d)
            closedir(d);
        rmdir(path);
    } else {
        remove(path);
    }
}

static void make_dirs(const char *path)
{
    char p[1200];
    snprintf(p, sizeof p, "%s", path);
    for (char *c = p + 1; *c; c++)
        if (*c == '/') {
            *c = 0;
            mkdir(p, 0755);
            *c = '/';
        }
    mkdir(p, 0755);
}

static void fresh_dir(char *out, size_t n, const char *name)
{
    snprintf(out, n, "%s/%s", g_root, name);
    remove_tree(out);
}

static int write_bytes(const char *path, const void *data, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;
    int ok = fwrite(data, 1, n, f) == n;
    return fclose(f) == 0 && ok ? 0 : -1;
}

static uint8_t *read_bytes(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    uint8_t *b = malloc(1 << 20);
    *n = fread(b, 1, 1 << 20, f);
    fclose(f);
    return b;
}

/* ---------------------------------------------------------------- notes of the test's own */

static void u32h(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static void u16h(uint8_t *p, uint16_t v) { memcpy(p, &v, 2); }

/* A name in the pak's character set: digits 16-25, letters 26-51 (the test's own, as test_save.c's) */
static void pak_name(uint8_t *out, const char *ascii, int n)
{
    memset(out, 0, (size_t)n);
    for (int i = 0; i < n && ascii[i]; i++) {
        char c = ascii[i];
        out[i] = c >= '0' && c <= '9' ? (uint8_t)(16 + c - '0') : c >= 'A' && c <= 'Z' ? (uint8_t)(26 + c - 'A') : 15;
    }
}

static void note_dir(PcNote *n, const char *name, const char *ext, uint32_t game, uint16_t company)
{
    memset(n->dir, 0, 32);
    u32h(n->dir, game);
    u16h(n->dir + 4, company);
    n->dir[8] = 0xA6;   /* the status byte: osPfsAllocateFile leaves it as its stack had it, osPfsReadWriteFile's
                         * first write sets bit 1 (DIR_STATUS_OCCUPIED); kept as it is */
    pak_name(n->dir + 12, ext, 4);
    pak_name(n->dir + 16, name, 16);
}

static void make_profile(PcNote *n, const char *player, int slot)
{
    char name[17];
    snprintf(name, sizeof name, "TESTPREF%s", player);
    note_dir(n, name, "P", PC_RUSH_GAME_CODE, PC_RUSH_COMPANY);
    n->slot = slot;
    n->pages = 9;
    n->data = calloc(9, PC_PAK_PAGE);
    u32h(n->data + 4, 0x2049), u32h(n->data + 8, (uint32_t)(slot << 3 | 1));
    n->data[12] = 16;
    snprintf((char *)n->data + 13, 13, "%s", player);
    u32h(n->data, pc_save_hash(n->data + 4, 24));
    for (int i = 32; i < 2064; i++)
        n->data[i] = (uint8_t)(i * 7 + slot);
}

/* A ghost the way CompressGhost makes one: the 88-byte header, then the samples zipped (raw deflate). */
static void make_ghost(PcNote *n, const char *player, int track, int backward, float finish, int frames, int slot)
{
    char name[17];
    snprintf(name, sizeof name, "TESTPREF%s%d%c", player, track + 1, backward ? 'B' : 'F');
    note_dir(n, name, "G", PC_RUSH_GAME_CODE, PC_RUSH_COMPANY);
    size_t usize = (size_t)frames * 3;
    uint8_t *raw = malloc(usize);
    for (size_t i = 0; i < usize; i++)
        raw[i] = (uint8_t)(i % 3 == 0 ? i / 3 % 7 : i % 3 == 1 ? (i * 13) & 0xFF : 64);
    uLong bound = compressBound((uLong)usize) + 64;
    uint8_t *z = malloc(bound);
    z_stream s;
    memset(&s, 0, sizeof s);
    deflateInit2(&s, 9, Z_DEFLATED, -10, 4, Z_DEFAULT_STRATEGY);
    s.next_in = raw, s.avail_in = (uInt)usize, s.next_out = z, s.avail_out = (uInt)bound;
    deflate(&s, Z_FINISH);
    uint32_t zsize = (uint32_t)s.total_out;
    deflateEnd(&s);
    n->pages = (int)((PC_GHOST_HEADER + zsize + 255) / 256);
    n->slot = slot;
    n->data = calloc((size_t)n->pages, PC_PAK_PAGE);
    uint8_t *h = n->data;
    h[4] = PC_GHOST_REVISION, h[5] = 0, h[6] = (uint8_t)track, h[7] = (uint8_t)backward, h[8] = 3, h[9] = 1;
    for (int i = 0; i < 12; i++)
        h[10 + i] = (uint8_t)i;
    snprintf((char *)h + 0x16, 13, "%s", player);
    u32h(h + 0x28, 0x2049), u32h(h + 0x2C, 0x11);
    float frame = 1.0f / 60;
    memcpy(h + 0x34, &frame, 4);
    memcpy(h + 0x38, &finish, 4);
    u32h(h + 0x3C, zsize), u32h(h + 0x40, (uint32_t)usize);
    memcpy(h + PC_GHOST_HEADER, z, zsize);
    u32h(h + 0x44, pc_save_hash(h + PC_GHOST_HEADER, zsize));
    u32h(h + 0x48, pc_save_hash(raw, (uint32_t)usize));
    u32h(h, pc_save_hash(h + 4, 0x40));
    free(raw);
    free(z);
}

static void make_other(PcNote *n, uint32_t game, int pages, int slot)
{
    note_dir(n, "OTHERGAME", "A", game, 0x3031);
    n->slot = slot;
    n->pages = pages;
    n->data = malloc((size_t)pages * PC_PAK_PAGE);
    for (int i = 0; i < pages * PC_PAK_PAGE; i++)
        n->data[i] = (uint8_t)rnd();
}

/* ---------------------------------------------------------------- images */

static int notes_match(const PcNote *a, int na, const PcNote *b, int nb)
{
    if (na != nb)
        return 0;
    for (int i = 0; i < na; i++) {
        int found = 0;
        for (int j = 0; j < nb && !found; j++)
            found = a[i].slot == b[j].slot && pc_note_equal(&a[i], &b[j]);
        if (!found)
            return 0;
    }
    return 1;
}

static void test_images(void)
{
    static uint8_t img[PC_PAK_SIZE], img2[PC_PAK_SIZE];
    PcNote n[4], back[PC_PAK_NOTES], again[PC_PAK_NOTES];
    const PcNote *p[PC_PAK_NOTES];
    int slots[PC_PAK_NOTES], bad;
    char why[200];
    make_profile(&n[0], "ALICE", 3);
    make_ghost(&n[1], "ALICE", 2, 1, 61.5f, 900, 7);
    make_other(&n[2], 0x12345678, 5, -1);
    make_profile(&n[3], "BOB", 3);   /* asks for a slot taken: the first free one */
    for (int i = 0; i < 4; i++)
        p[i] = &n[i];
    CHECK(pc_pak_build(img, NULL, p, 4, slots, why, sizeof why) == 0, "build: %s", why);
    CHECK(pc_pak_image_check(img, PC_PAK_SIZE, why, sizeof why) == PC_PAK_IMAGE_OK, "a good image: %s", why);
    CHECK(slots[3] == 0 && slots[7] == 1 && slots[0] == 2 && slots[1] == 3, "slots: the own ones, then the first free");
    int k = pc_pak_notes(img, back, &bad, why, sizeof why);
    CHECK(k == 4 && bad == 0, "four notes back, none damaged (%d, %d)", k, bad);
    n[2].slot = 0, n[3].slot = 1;
    CHECK(notes_match(n, 4, back, k), "the notes back as they went in, at their slots");
    CHECK(img[0x300 + 32 * 3 + 8] == 0xA6, "the directory entry's status byte kept");
    /* scattered pages, as a pak that had notes deleted holds them: parse, rebuild, the same notes */
    memcpy(img2, img, sizeof img2);
    {
        /* move ALICE's profile (slot 3, pages 5-13) to pages 120-127 and 100 */
        uint8_t *inode = img2 + 0x100, *e = img2 + 0x300 + 32 * 3;
        int first = e[7], to[9] = {120, 121, 100, 122, 123, 124, 125, 126, 127};
        for (int i = 0; i < 9; i++)
            memcpy(img2 + to[i] * 256, img + (first + i) * 256, 256);
        for (int i = 0; i < 9; i++) {
            uint16_t unused = 3;
            memcpy(inode + 2 * (first + i), &unused, 2);
        }
        for (int i = 0; i < 9; i++) {
            if (i == 8) {
                uint16_t eof = 1;
                memcpy(inode + 2 * to[i], &eof, 2);
            } else {
                inode[2 * to[i]] = 0, inode[2 * to[i] + 1] = (uint8_t)to[i + 1];
            }
        }
        e[7] = (uint8_t)to[0];
        unsigned sum = 0;
        for (int i = 10; i < 256; i++)
            sum += inode[i];
        inode[1] = (uint8_t)sum;
        memcpy(img2 + 0x200, inode, 256);
    }
    k = pc_pak_notes(img2, again, &bad, why, sizeof why);
    CHECK(k == 4 && notes_match(back, 4, again, k), "scattered pages: the same notes");
    for (int i = 0; i < k; i++)
        p[i] = &again[i];
    CHECK(pc_pak_build(img2, img, p, k, slots, why, sizeof why) == 0 && !memcmp(img, img2, PC_PAK_SIZE),
          "rebuilt from them: the image laid out from page 5 again, byte for byte");
    /* the backup inode table is taken when the first is damaged */
    img2[0x100 + 40] ^= 0xFF;
    PcNote b2[PC_PAK_NOTES];
    k = pc_pak_notes(img2, b2, &bad, why, sizeof why);
    CHECK(k == 4, "the inode table's backup used (%d)", k);
    for (int i = 0; i < k; i++)
        pc_note_free(&b2[i]);
    img2[0x200 + 40] ^= 0xFF;
    CHECK(pc_pak_notes(img2, b2, &bad, why, sizeof why) == -1, "both inode tables damaged: no notes (%s)", why);
    /* broken chains: a loop, a page out of range, no end */
    for (int t = 0; t < 3; t++) {
        memcpy(img2, img, sizeof img2);
        uint8_t *inode = img2 + 0x100, *e = img2 + 0x300 + 32 * 7;
        int first = e[7];
        if (t == 0)
            inode[2 * first] = 0, inode[2 * first + 1] = (uint8_t)first;   /* a loop */
        else if (t == 1)
            e[7] = 2;   /* starts in the directory's pages: ends at once, as no chain */
        else
            inode[2 * first] = 1, inode[2 * first + 1] = (uint8_t)(first + 1);   /* bank 1 */
        unsigned sum = 0;
        for (int i = 10; i < 256; i++)
            sum += inode[i];
        inode[1] = (uint8_t)sum;
        k = pc_pak_notes(img2, b2, &bad, why, sizeof why);
        CHECK(k == 3 && bad == 1, "broken chain %d: left out and counted (%d, %d)", t, k, bad);
        for (int i = 0; i < k; i++)
            pc_note_free(&b2[i]);
    }
    for (int i = 0; i < 4; i++)
        pc_note_free(&back[i]), pc_note_free(&again[i]), pc_note_free(&n[i]);
    /* a full pak: 16 notes, 123 pages; one page or one note more doesn't fit */
    PcNote full[17];
    for (int i = 0; i < 17; i++) {
        make_other(&full[i], 0x100 + (uint32_t)i, i < 15 ? 7 : i == 15 ? 18 : 1, i);
        if (i < 16)
            p[i] = &full[i];
    }
    CHECK(pc_pak_build(img, NULL, p, 16, slots, why, sizeof why) == 0, "16 notes in 123 pages: %s", why);
    k = pc_pak_notes(img, back, &bad, why, sizeof why);
    CHECK(k == 16, "a full pak: 16 notes back");
    for (int i = 0; i < k; i++)
        pc_note_free(&back[i]);
    full[16].pages = 1;
    p[15] = &full[16];   /* 15 x 7 + 1 pages, then a 19-page note in its place: 124 */
    full[16].data = realloc(full[16].data, 19 * 256);
    full[16].pages = 19;
    CHECK(pc_pak_build(img, NULL, p, 16, slots, why, sizeof why) == -1 && strstr(why, "124 pages"), "124 pages: %s", why);
    full[16].pages = 1;
    const PcNote *q[17];
    for (int i = 0; i < 17; i++)
        q[i] = &full[i];
    CHECK(pc_pak_build(img, NULL, q, 17, slots, why, sizeof why) == -1 && strstr(why, "17 notes"), "17 notes: %s", why);
    for (int i = 0; i < 17; i++)
        pc_note_free(&full[i]);
}

/* ---------------------------------------------------------------- save files and the ghost check */

static void test_files(void)
{
    PcNote g, back;
    PcSaveFileInfo fi;
    uint8_t *b;
    size_t size;
    char why[240], name[200];
    make_ghost(&g, "ALICE", 3, 0, 75.25f, 1200, 4);
    CHECK(pc_ghost_check(g.data, (size_t)g.pages * 256, 1, why, sizeof why) == 0, "the test's ghost is good: %s", why);
    CHECK(pc_save_file_make(&g, PC_ORIGIN_GAME, "v-test", &b, &size) == 0, "a ghost's file");
    CHECK(size == 128 + (size_t)g.pages * 256 && !memcmp(b, "R49S", 4) && b[8] == PC_NOTE_GHOST && b[10] == 4,
          "its header");
    CHECK(b[0x34] == 3 && b[0x35] == 0 && b[0x36] == 3 && b[0x38] == (75250 & 0xFF) && !strcmp((char *)b + 0x3C, "ALICE") &&
          !strcmp((char *)b + 0x4C, "v-test"), "the header's track, way, car, time, name, writer");
    CHECK(pc_save_file_read(b, size, &back, &fi, why, sizeof why) == 0 && pc_note_equal(&g, &back) && back.slot == 4 &&
          fi.kind == PC_NOTE_GHOST && fi.origin == PC_ORIGIN_GAME, "read back: the same note (%s)", why);
    pc_note_free(&back);
    pc_save_file_name(&g, name, sizeof name);
    CHECK(!strcmp(name, "track 4 forward - ALICE - 1m15.25s.r49ghost"), "its name: %s", name);
    /* damage, one kind at a time */
    struct { const char *what; size_t at; int set; size_t cut; } cases[] = {
        {"a data byte (the CRC)", 200, 0x55, 0}, {"the magic", 0, 'X', 0}, {"the version", 4, 2, 0},
        {"the size field", 12, 0x13, 0}, {"the byte order", 11, 'B', 0}, {"the kind", 8, 1, 0},
        {"truncated", 0, -1, 100}, {"short of its pages", 0, -1, size - 256}, {"header only", 0, -1, 128},
    };
    for (size_t c = 0; c < sizeof cases / sizeof *cases; c++) {
        uint8_t *d = malloc(size + 512);
        memcpy(d, b, size);
        size_t n = cases[c].cut ? cases[c].cut : size;
        if (cases[c].set >= 0) {
            d[cases[c].at] ^= (uint8_t)cases[c].set;
            if (cases[c].at != 200 && cases[c].at != 0) {   /* the CRC made right, so the field itself is checked */
                uLong crc = crc32(0, d, 0x10);
                static const uint8_t z4[4];
                crc = crc32(crc, z4, 4);
                crc = crc32(crc, d + 0x14, (uInt)(n - 0x14));
                d[0x10] = (uint8_t)crc, d[0x11] = (uint8_t)(crc >> 8), d[0x12] = (uint8_t)(crc >> 16),
                d[0x13] = (uint8_t)(crc >> 24);
            }
        }
        int r = pc_save_file_read(d, n, &back, &fi, why, sizeof why);
        CHECK(r == -1 && why[0], "%s refused: %s", cases[c].what, why);
        if (r == 0)
            pc_note_free(&back);
        free(d);
    }
    /* oversized: a page more than the header says */
    uint8_t *big = calloc(1, size + 256);
    memcpy(big, b, size);
    CHECK(pc_save_file_read(big, size + 256, &back, &fi, why, sizeof why) == -1, "oversized refused: %s", why);
    free(big);
    free(b);
    /* the ghost check, a field at a time; the header's checksum made right each time unless that is the case */
    const size_t gsize = (size_t)g.pages * 256;
    struct { const char *what; int off, width; uint32_t value; int fix; } gc[] = {
        {"revision 16", 4, 1, 16, 1},         {"the header's checksum", 0, 4, 0x12345678, 0},
        {"track 6", 6, 1, 6, 1},              {"track -1", 6, 1, 0xFF, 1},
        {"direction 2", 7, 1, 2, 1},          {"car 13", 8, 1, 13, 1},
        {"zipped size 0", 0x3C, 4, 0, 1},     {"zipped size past the note", 0x3C, 4, 0x10000, 1},
        {"unzipped not whole frames", 0x40, 4, 3600 - 1, 1}, {"a billion frames", 0x40, 4, 0xFFFFFFF0u, 1},
        {"unzipped larger than the stream", 0x40, 4, 3600 + 3, 1},
        {"unzipped smaller than the stream (it would run past the buffer)", 0x40, 4, 3600 - 3, 1},
        {"the samples' checksum", 0x44, 4, 1, 0},  {"the unzipped checksum", 0x48, 4, 1, 1},
        {"a frame time of 0", 0x34, 4, 0, 1}, {"a NaN race time", 0x38, 4, 0x7FC00000u, 1},
    };
    for (size_t c = 0; c < sizeof gc / sizeof *gc; c++) {
        uint8_t *d = malloc(gsize);
        memcpy(d, g.data, gsize);
        if (gc[c].width == 1)
            d[gc[c].off] = (uint8_t)gc[c].value;
        else if (gc[c].off == 0x44 || gc[c].off == 0x48)
            u32h(d + gc[c].off, pc_save_hash(d + 0, 4) ^ gc[c].value ^ 0xABCD);
        else
            u32h(d + gc[c].off, gc[c].value);
        if (gc[c].fix)
            u32h(d, pc_save_hash(d + 4, 0x40));
        CHECK(pc_ghost_check(d, gsize, 1, why, sizeof why) == -1, "ghost: %s refused: %s", gc[c].what, why);
        free(d);
    }
    CHECK(pc_ghost_check(g.data, 50, 1, why, sizeof why) == -1, "ghost: 50 bytes refused: %s", why);
    pc_note_free(&g);
    /* names: Windows' characters and reserved names */
    char s1[64] = "A:B*C?\"<>|/\\. ", s2[64] = "CON", s3[64] = "  com3.txt", s4[64] = "...";
    pc_save_safe_name(s1), pc_save_safe_name(s2), pc_save_safe_name(s3), pc_save_safe_name(s4);
    CHECK(!strcmp(s1, "A_B_C_______") && !strcmp(s2, "CON_") && !strcmp(s3, "com3_.txt") && !strcmp(s4, "unnamed"),
          "safe names: '%s' '%s' '%s' '%s'", s1, s2, s3, s4);
    PcNote pr;
    make_profile(&pr, "AUX", 0);
    pc_save_file_name(&pr, name, sizeof name);
    CHECK(!strcmp(name, "AUX_.r49save"), "a profile named AUX: %s", name);
    pc_note_free(&pr);
}

/* ---------------------------------------------------------------- the saves folder */

static uint8_t g_img[PC_PAK_SIZE];

/* The game's image: these notes at their slots, on the pak's own first page (the store's image). */
static void game_image(uint8_t *img, PcNote *const *notes, int n)
{
    const PcNote *p[PC_PAK_NOTES];
    char why[100];
    uint8_t page0[256];
    memcpy(page0, img, 256);
    for (int i = 0; i < n; i++)
        p[i] = notes[i];
    if (pc_pak_build(img, page0, p, n, NULL, why, sizeof why) != 0)
        printf("game_image: %s\n", why);
}

static int image_notes(const uint8_t *img, PcNote *out)
{
    int bad;
    char why[100];
    return pc_pak_notes(img, out, &bad, why, sizeof why);
}

static void free_notes(PcNote *n, int k)
{
    for (int i = 0; i < k; i++)
        pc_note_free(&n[i]);
}

static void test_migration(void)
{
    char dir[1100], mpk[1100], why[200], msg[300];
    static uint8_t mpk_img[PC_PAK_SIZE];
    PcNote n[5];
    const PcNote *p[5];
    fresh_dir(dir, sizeof dir, "mig");
    make_dirs(dir);
    snprintf(mpk, sizeof mpk, "%s/old.mpk", dir);
    make_profile(&n[0], "ALICE", 2);
    make_ghost(&n[1], "ALICE", 0, 0, 70.0f, 700, 5);
    make_ghost(&n[2], "ALICE", 0, 1, 80.0f, 800, 9);
    make_other(&n[3], 0x4E414B45, 3, 12);
    make_profile(&n[4], "BOB", 0);
    for (int i = 0; i < 5; i++)
        p[i] = &n[i];
    pc_pak_format(mpk_img);
    pc_pak_own_id(mpk_img, 2);
    uint8_t page0[256];
    memcpy(page0, mpk_img, 256);
    CHECK(pc_pak_build(mpk_img, page0, p, 5, NULL, why, sizeof why) == 0, "the .mpk: %s", why);
    write_bytes(mpk, mpk_img, PC_PAK_SIZE);
    char saves[1100];
    snprintf(saves, sizeof saves, "%s/saves/p1", dir);
    CHECK(pc_store_open(0, saves, mpk, "v-test", msg, sizeof msg) == 0, "open with an .mpk: %s", msg);
    CHECK(strstr(msg, "imported 5 notes"), "the summary says the import: %s", msg);
    CHECK(count_files(saves, ".r49save") == 2 && count_files(saves, ".r49ghost") == 2 && count_files(saves, ".r49note") == 1,
          "a file per note");
    CHECK(file_exists(saves, "ALICE.r49save") && file_exists(saves, "BOB.r49save") &&
          file_exists(saves, "track 1 backward - ALICE - 1m20.00s.r49ghost") && file_exists(saves, "imported.txt") &&
          file_exists(saves, "pak-id.bin"), "the readable names, the marker and the ID");
    size_t sz;
    uint8_t *now = read_bytes(mpk, &sz);
    CHECK(now && sz == PC_PAK_SIZE && !memcmp(now, mpk_img, PC_PAK_SIZE), "the .mpk left as it was");
    free(now);
    /* files -> the pak the game sees: the same notes at the same slots, the same first page */
    CHECK(pc_store_build(0, g_img, why, sizeof why) == 0, "build: %s", why);
    PcNote a[PC_PAK_NOTES], b[PC_PAK_NOTES];
    int ka = image_notes(mpk_img, a), kb = image_notes(g_img, b);
    CHECK(ka == 5 && notes_match(a, ka, b, kb), "mpk -> files -> pak: the same notes at the same slots (%d, %d)", ka, kb);
    CHECK(!memcmp(g_img, mpk_img, 256), "the pak's first page (label, ID) kept");
    CHECK(!memcmp(g_img, mpk_img, PC_PAK_SIZE), "and here, a pak the game laid out from page 5, the image byte for byte");
    free_notes(b, kb);
    /* files -> mpk */
    char out[1100];
    snprintf(out, sizeof out, "%s/back.mpk", dir);
    CHECK(pc_store_export_mpk(0, out, msg, sizeof msg) == 0, "export: %s", msg);
    now = read_bytes(out, &sz);
    kb = now ? image_notes(now, b) : -1;
    CHECK(kb == 5 && notes_match(a, ka, b, kb), "files -> mpk: the same notes");
    free_notes(b, kb > 0 ? kb : 0);
    free(now);
    CHECK(pc_store_export_mpk(0, out, msg, sizeof msg) == 0 && file_exists(dir, "back.mpk.bak"),
          "a second export keeps the first as .bak");
    pc_store_close(0);
    /* the next start: nothing imported again, even with the folder emptied */
    CHECK(pc_store_open(0, saves, mpk, "v-test", msg, sizeof msg) == 0 && !strstr(msg, "imported 5"), "no second import: %s",
          msg);
    pc_store_close(0);
    {   /* the notes' files moved out */
        char x[1200];
        snprintf(x, sizeof x, "%s/x", dir);
        make_dirs(x);
        DIR *d = opendir(saves);
        for (struct dirent *de; d && (de = readdir(d));) {
            char from[1300], to[1300];
            if (!strstr(de->d_name, ".r49"))
                continue;
            path_in(from, sizeof from, saves, de->d_name);
            path_in(to, sizeof to, x, de->d_name);
            rename(from, to);
        }
        if (d)
            closedir(d);
    }
    CHECK(pc_store_open(0, saves, mpk, "v-test", msg, sizeof msg) == 0 && count_files(saves, ".r49save") == 0,
          "an emptied folder isn't filled from the .mpk again (imported.txt): %s", msg);
    pc_store_close(0);
    free_notes(a, ka);
    for (int i = 0; i < 5; i++)
        pc_note_free(&n[i]);
    /* an .mpk the game can't use is not imported */
    fresh_dir(dir, sizeof dir, "mig2");
    make_dirs(dir);
    snprintf(mpk, sizeof mpk, "%s/bad.mpk", dir);
    write_bytes(mpk, "short", 5);
    snprintf(saves, sizeof saves, "%s/s", dir);
    CHECK(pc_store_open(0, saves, mpk, "v", msg, sizeof msg) == 0 && count_files(saves, ".r49save") == 0 &&
          !file_exists(saves, "imported.txt"), "a damaged .mpk: nothing imported, no marker");
    pc_store_close(0);
}

static PcNote g_alice;

static void test_writes(void)
{
    char dir[1100], why[200], msg[300], trash[1200];
    PcNote g1, g2, g3, in[PC_PAK_NOTES];
    fresh_dir(dir, sizeof dir, "w");   /* a missing folder: made */
    CHECK(pc_store_open(1, dir, NULL, "v", msg, sizeof msg) == 0 && file_exists(dir, "trash"), "a new folder: %s", msg);
    CHECK(pc_store_build(1, g_img, why, sizeof why) == 0, "an empty pak");
    CHECK(image_notes(g_img, in) == 0, "no notes on it");
    CHECK(pc_pak_id_valid(g_img + 0x20, 0) && file_exists(dir, "pak-id.bin"), "its own ID, kept in pak-id.bin");
    uint8_t id1[32];
    memcpy(id1, g_img + 0x20, 32);
    /* the game makes a profile: a file */
    make_profile(&g_alice, "ALICE", 0);
    PcNote *on[PC_PAK_NOTES] = {&g_alice};
    game_image(g_img, on, 1);
    CHECK(pc_store_write(dir, g_img, PC_PAK_SIZE) == 0 && file_exists(dir, "ALICE.r49save"), "a profile added: a file");
    /* the same image again: nothing */
    CHECK(pc_store_write(dir, g_img, PC_PAK_SIZE) == 0 && count_files(dir, ".r49save") == 1, "the same image: nothing new");
    /* changed in place (a setting): the file written again */
    g_alice.data[500] ^= 0xFF;
    game_image(g_img, on, 1);
    CHECK(pc_store_write(dir, g_img, PC_PAK_SIZE) == 0, "a profile changed");
    pc_store_close(1);
    CHECK(pc_store_open(1, dir, NULL, "v", msg, sizeof msg) == 0, "reopened");
    PcStoreEntry list[32];
    int k = pc_store_list(1, list, 32);
    CHECK(k == 1 && list[0].kind == PC_NOTE_PROFILE && !strcmp(list[0].name, "ALICE"), "the profile read back");
    CHECK(pc_store_build(1, g_img, why, sizeof why) == 0 && !memcmp(g_img + 0x20, id1, 32), "the same pak ID again");
    PcNote back[PC_PAK_NOTES];
    int kb = image_notes(g_img, back);
    CHECK(kb == 1 && pc_note_equal(&back[0], &g_alice) && back[0].slot == 0, "the changed profile on the pak, slot 0");
    free_notes(back, kb);
    /* a ghost being written (header there, samples not yet): waits; whole: a file */
    make_ghost(&g1, "ALICE", 1, 0, 65.0f, 600, 1);
    PcNote partial;
    pc_note_copy(&partial, &g1);
    memset(partial.data + PC_GHOST_HEADER, 0, (size_t)partial.pages * 256 - PC_GHOST_HEADER);
    PcNote *on2[] = {&g_alice, &partial};
    game_image(g_img, on2, 2);
    CHECK(pc_store_write(dir, g_img, PC_PAK_SIZE) == 0 && count_files(dir, ".r49ghost") == 0, "a ghost not whole: waits");
    on2[1] = &g1;
    game_image(g_img, on2, 2);
    CHECK(pc_store_write(dir, g_img, PC_PAK_SIZE) == 0 && file_exists(dir, "track 2 forward - ALICE - 1m05.00s.r49ghost"),
          "whole: a file");
    /* a new best (SaveGhost: the old note deleted, a new one by the same name): the old to the trash */
    make_ghost(&g2, "ALICE", 1, 0, 60.0f, 600, 1);
    on2[1] = &g2;
    game_image(g_img, on2, 2);
    CHECK(pc_store_write(dir, g_img, PC_PAK_SIZE) == 0 && file_exists(dir, "track 2 forward - ALICE - 1m00.00s.r49ghost"),
          "a new best: its own file");
    snprintf(trash, sizeof trash, "%s/trash", dir);
    CHECK(file_exists(trash, "track 2 forward - ALICE - 1m05.00s.r49ghost") &&
          !file_exists(dir, "track 2 forward - ALICE - 1m05.00s.r49ghost"), "the old best in the trash (the game deleted it)");
    /* the same ghost with its status byte changed (the Pfs C sets DIR_STATUS_OCCUPIED after its first write): the
     * file written again, nothing in the trash */
    g2.dir[8] ^= 0x40;
    game_image(g_img, on2, 2);
    CHECK(pc_store_write(dir, g_img, PC_PAK_SIZE) == 0 && count_files(trash, ".r49ghost") == 1 &&
          file_exists(dir, "track 2 forward - ALICE - 1m00.00s.r49ghost"), "a status byte changed: in place");
    /* a Rush note never written (no DIR_STATUS_OCCUPIED): waits */
    {
        PcNote fresh;
        make_ghost(&fresh, "BOB", 4, 1, 50.0f, 300, 5);
        fresh.dir[8] &= (uint8_t)~0x02;
        PcNote *on4[] = {&g_alice, &g2, &fresh};
        game_image(g_img, on4, 3);
        CHECK(pc_store_write(dir, g_img, PC_PAK_SIZE) == 0 && count_files(dir, ".r49ghost") == 1,
              "a note allocated, never written: waits");
        pc_note_free(&fresh);
        game_image(g_img, on2, 2);
        pc_store_write(dir, g_img, PC_PAK_SIZE);
    }
    /* renamed: the same bytes under another name: a file of the new name, the old to the trash */
    pc_note_copy(&g3, &g2);
    pak_name(g3.dir + 16, "TESTPREFALICE2X", 16);
    on2[1] = &g3;
    game_image(g_img, on2, 2);
    CHECK(pc_store_write(dir, g_img, PC_PAK_SIZE) == 0 && count_files(dir, ".r49ghost") == 1 &&
          count_files(trash, ".r49ghost") == 2, "renamed: one file in the folder, the old one in the trash");
    /* deleted by the game: to the trash */
    game_image(g_img, on2, 1);
    CHECK(pc_store_write(dir, g_img, PC_PAK_SIZE) == 0 && count_files(dir, ".r49ghost") == 0 &&
          count_files(trash, ".r49ghost") == 3, "deleted: in the trash, nothing removed");
    /* an unwritable folder: the write fails, is said, and works once it can (as the writer tries it again) */
#ifndef _WIN32
    if (geteuid() != 0) {
        PcNote bob;
        make_profile(&bob, "BOB", 2);
        PcNote *on3[] = {&g_alice, &bob};
        game_image(g_img, on3, 2);
        chmod(dir, 0555);
        errno = 0;
        int r = pc_store_write(dir, g_img, PC_PAK_SIZE);
        CHECK(r == -1 && errno == EACCES, "an unwritable folder: the write fails (%d, %s)", r, strerror(errno));
        CHECK(!file_exists(dir, "BOB.r49save") && count_files(dir, ".tmp") == 0, "no file, no temp file left");
        chmod(dir, 0755);
        CHECK(pc_store_write(dir, g_img, PC_PAK_SIZE) == 0 && file_exists(dir, "BOB.r49save"), "tried again: written");
        pc_note_free(&bob);
    } else {
        printf("skip: an unwritable folder (running as root)\n");
    }
#endif
    pc_store_close(1);
    pc_note_free(&g1), pc_note_free(&g2), pc_note_free(&g3), pc_note_free(&partial);
    /* a folder that can't be made */
    char bad[1200];
    snprintf(bad, sizeof bad, "%s/w/ALICE.r49save/sub", g_root);
    CHECK(pc_store_open(2, bad, NULL, "v", msg, sizeof msg) == -1 && strstr(msg, "can't be made"), "no folder: %s", msg);
}

static void put_file(const char *dir, const char *name, const PcNote *n, int origin)
{
    char p[1200];
    uint8_t *b;
    size_t size;
    pc_save_file_make(n, origin, "v", &b, &size);
    path_in(p, sizeof p, dir, name);
    write_bytes(p, b, size);
    free(b);
}

static void test_choice(void)
{
    char dir[1100], why[200], msg[300];
    fresh_dir(dir, sizeof dir, "choice");
    make_dirs(dir);
    /* two profiles (18 pages) and 30 ghosts: three players, five tracks, both ways */
    PcNote pr[2], gh[30];
    make_profile(&pr[0], "ALICE", 0), make_profile(&pr[1], "BOB", 1);
    put_file(dir, "ALICE.r49save", &pr[0], PC_ORIGIN_GAME), put_file(dir, "BOB.r49save", &pr[1], PC_ORIGIN_GAME);
    static const char *const who[] = {"ALICE", "BOB", "CAROL"};
    for (int i = 0; i < 30; i++) {
        char name[200];
        make_ghost(&gh[i], who[i % 3], i / 6 % 5, i / 3 % 2, 60.0f + (float)i, 2000 + 300 * i, -1);
        snprintf(name, sizeof name, "g%02d.r49ghost", i);
        put_file(dir, name, &gh[i], PC_ORIGIN_GAME);
    }
    CHECK(pc_store_open(0, dir, NULL, "v", msg, sizeof msg) == 0 && strstr(msg, "2 profiles, 30 ghosts"), "%s", msg);
    CHECK(pc_store_build(0, g_img, why, sizeof why) == 0, "build: %s", why);
    PcNote in[PC_PAK_NOTES];
    int k = image_notes(g_img, in), pages = 0, profiles = 0;
    for (int i = 0; i < k; i++) {
        PcNoteInfo info;
        pc_note_info(&in[i], &info);
        pages += in[i].pages;
        profiles += info.kind == PC_NOTE_PROFILE;
    }
    CHECK(profiles == 2 && k <= 15 && pages <= 123 - 24 + 24, "the profiles, and ghosts while room is left (%d notes, %d "
          "pages)", k, pages);
    CHECK(k < 16 || pages <= 99, "the reserve: a note or 24 pages left for the game's next save");
    PcStoreEntry list[64];
    int n = pc_store_list(0, list, 64), on = 0;
    for (int i = 0; i < n; i++)
        on += list[i].on_pak;
    CHECK(n == 32 && on == k, "the list: all 32, %d on the pak", on);
    CHECK(list[0].kind == PC_NOTE_PROFILE && list[2].kind == PC_NOTE_GHOST && list[2].track == 0, "profiles first, then by "
          "track");
    /* the fastest of each player per track and way first: g00 (track 0 forward, ALICE, 60 s) is on it */
    int g00 = -1;
    for (int i = 0; i < n; i++)
        if (!strcmp(list[i].file, "g00.r49ghost"))
            g00 = i;
    CHECK(g00 >= 0 && list[g00].on_pak, "track 1 forward's fastest is on the pak");
    free_notes(in, k);
    /* a pin: a ghost not on the pak goes on, as a swap the game takes at its title */
    const char *off = NULL;
    for (int i = n - 1; i >= 0 && !off; i--)
        if (list[i].kind == PC_NOTE_GHOST && !list[i].on_pak)
            off = list[i].file;
    char pinned[PC_STORE_NAME];
    snprintf(pinned, sizeof pinned, "%s", off ? off : "");
    CHECK(off && pc_store_pin(0, pinned, 1, msg, sizeof msg) == 0 && pc_store_swap_wanted(0), "pinned: a swap wanted (%s)",
          msg);
    CHECK(pc_store_build(0, g_img, why, sizeof why) == 0 && !pc_store_swap_wanted(0), "built again");
    n = pc_store_list(0, list, 64);
    int now_on = 0;
    for (int i = 0; i < n; i++)
        if (!strcmp(list[i].file, pinned))
            now_on = list[i].on_pak && list[i].pinned;
    CHECK(now_on, "the pinned ghost on the pak");
    CHECK(pc_store_pin(0, pinned, 0, msg, sizeof msg) == 0 && !pc_store_swap_wanted(0),
          "unpinned: it stays, as on the pak last time (%s)", msg);
    /* delete from F1: one on the pak waits for the swap; one off it goes at once */
    const char *gone_now = NULL;
    for (int i = 0; i < n && !gone_now; i++)
        if (list[i].kind == PC_NOTE_GHOST && !list[i].on_pak)
            gone_now = list[i].file;
    char tdir[1200], name1[PC_STORE_NAME];
    snprintf(name1, sizeof name1, "%s", gone_now);
    snprintf(tdir, sizeof tdir, "%s/trash", dir);
    CHECK(pc_store_delete(0, name1, msg, sizeof msg) == 0 && file_exists(tdir, name1) && !file_exists(dir, name1),
          "deleted off the pak: in the trash (%s)", msg);
    CHECK(pc_store_delete(0, pinned, msg, sizeof msg) == 0 && file_exists(dir, pinned) && pc_store_swap_wanted(0),
          "deleted while on the pak: kept until the swap (%s)", msg);
    CHECK(pc_store_build(0, g_img, why, sizeof why) == 0 && file_exists(tdir, pinned) && !file_exists(dir, pinned),
          "at the swap: in the trash");
    /* exports */
    CHECK(pc_store_export_ghost(0, "g00.r49ghost", msg, sizeof msg) == 0, "export: %s", msg);
    char edir[1200];
    snprintf(edir, sizeof edir, "%s/export", dir);
    CHECK(count_files(edir, ".r49ghost") == 1, "one file in export/");
    CHECK(pc_store_export_ghost(0, "ALICE.r49save", msg, sizeof msg) == -1, "a profile isn't exported: %s", msg);
    pc_store_close(0);
    /* pins.txt and pak.txt with Windows' line ends (an editor's): read the same */
    char pp[1200];
    path_in(pp, sizeof pp, dir, "pins.txt");
    static const char pins_crlf[] = "g05.r49ghost\r\ng06.r49ghost\r\n", pak_crlf[] = "0\tALICE.r49save\r\n1\tBOB.r49save\r\n";
    write_bytes(pp, pins_crlf, strlen(pins_crlf));
    path_in(pp, sizeof pp, dir, "pak.txt");
    write_bytes(pp, pak_crlf, strlen(pak_crlf));
    CHECK(pc_store_open(0, dir, NULL, "v", msg, sizeof msg) == 0, "reopened");
    n = pc_store_list(0, list, 64);
    int pins = 0;
    for (int i = 0; i < n; i++)
        pins += list[i].pinned && (!strcmp(list[i].file, "g05.r49ghost") || !strcmp(list[i].file, "g06.r49ghost"));
    CHECK(pins == 2, "CRLF lists: both pins read (%d)", pins);
    CHECK(pc_store_build(0, g_img, why, sizeof why) == 0, "built");
    in[0].data = NULL;
    k = image_notes(g_img, in);
    CHECK(k > 2 && in[0].slot == 0 && in[1].slot == 1, "CRLF pak.txt: the profiles at their slots");
    free_notes(in, k);
    pc_store_close(0);
    for (int i = 0; i < 30; i++)
        pc_note_free(&gh[i]);
    pc_note_free(&pr[0]), pc_note_free(&pr[1]);
}

static void test_imports_and_damage(void)
{
    char dir[1100], why[200], msg[300], imp[1200], p[1300];
    fresh_dir(dir, sizeof dir, "imp");
    snprintf(imp, sizeof imp, "%s/import", dir);
    make_dirs(imp);
    PcNote mine, friend_, pr;
    make_profile(&pr, "ALICE", 0);
    put_file(dir, "ALICE.r49save", &pr, PC_ORIGIN_GAME);
    make_ghost(&mine, "ALICE", 2, 0, 90.0f, 900, 3);
    put_file(dir, "mine.r49ghost", &mine, PC_ORIGIN_GAME);
    make_ghost(&friend_, "ALICE", 2, 0, 80.0f, 800, 5);   /* a friend named the same: the same note name */
    put_file(imp, "friend.r49ghost", &friend_, PC_ORIGIN_EXPORT);
    put_file(imp, "again.r49ghost", &mine, PC_ORIGIN_EXPORT);   /* already here */
    put_file(imp, "profile.r49ghost", &pr, PC_ORIGIN_EXPORT);   /* not a ghost */
    path_in(p, sizeof p, imp, "garbage.r49ghost");
    write_bytes(p, "R49S garbage garbage garbage", 28);
    uint8_t prefix[8] = {1, 2, 3, 4, 5, 6, 7, 8}, gext[4] = {9, 0, 0, 0};
    pc_store_set_game_names(prefix, gext);
    CHECK(pc_store_open(3, dir, NULL, "v", msg, sizeof msg) == 0 && strstr(msg, "refused"), "%s", msg);
    char done[1300], refused[1300];
    snprintf(done, sizeof done, "%s/done", imp);
    snprintf(refused, sizeof refused, "%s/refused", imp);
    CHECK(count_files(done, ".r49ghost") == 2 && count_files(refused, ".r49ghost") == 2 && count_files(imp, ".r49ghost") == 0,
          "two taken (one new, one already here), two refused, import/ empty");
    CHECK(pc_store_build(3, g_img, why, sizeof why) == 0, "build: %s", why);
    PcNote in[PC_PAK_NOTES];
    int k = image_notes(g_img, in), renamed = 0, own = 0;
    for (int i = 0; i < k; i++) {
        PcNoteInfo info;
        pc_note_info(&in[i], &info);
        if (info.kind != PC_NOTE_GHOST)
            continue;
        if (!memcmp(in[i].dir + 16, prefix, 8) && in[i].dir[12] == 9) {
            int len = 0;
            while (len < 16 && in[i].dir[16 + len])
                len++;
            renamed = in[i].dir[16 + len - 2] >= 0x10 && in[i].dir[16 + len - 2] <= 0x19 && in[i].dir[16 + len - 1] == 0x10;
        } else if (pc_note_equal(&in[i], &mine)) {
            own = 1;
        }
    }
    CHECK(k == 3 && renamed && own, "the friend's ghost on the pak beside the player's own of the same name: the PC's name "
          "and the game's prefix and extension (%d notes)", k);
    PcStoreEntry list[8];
    int n = pc_store_list(3, list, 8), pinned = 0, imported = 0;
    for (int i = 0; i < n; i++)
        pinned += list[i].pinned, imported += list[i].imported;
    CHECK(pinned == 1 && imported == 1, "the import pinned");
    /* the game deletes the imported ghost: to the trash */
    PcNote keep[PC_PAK_NOTES];
    int kk = 0;
    for (int i = 0; i < k; i++) {
        if (!memcmp(in[i].dir + 16, prefix, 8))
            continue;
        keep[kk++] = in[i];
    }
    PcNote *ptr[PC_PAK_NOTES];
    for (int i = 0; i < kk; i++)
        ptr[i] = &keep[i];
    game_image(g_img, ptr, kk);
    char tdir[1200];
    snprintf(tdir, sizeof tdir, "%s/trash", dir);
    CHECK(pc_store_write(dir, g_img, PC_PAK_SIZE) == 0 && count_files(tdir, ".r49ghost") == 1, "the game deleted the import: "
          "in the trash");
    free_notes(in, k);
    /* from F1: a file elsewhere */
    path_in(p, sizeof p, g_root, "elsewhere.r49ghost");
    put_file(g_root, "elsewhere.r49ghost", &friend_, PC_ORIGIN_EXPORT);
    CHECK(pc_store_import_file(3, p, msg, sizeof msg) == 0 && strstr(msg, "Imported"), "F1's import of a file: %s", msg);
    CHECK(pc_store_import(3, msg, sizeof msg) == -1 && strstr(msg, "No ghost files"), "an empty import/: %s", msg);
    pc_store_close(3);
    pc_store_set_game_names(NULL, NULL);
    /* a damaged file: set aside under its date, the backup put back */
    char name[200];
    snprintf(name, sizeof name, "ALICE.r49save");
    CHECK(pc_store_open(3, dir, NULL, "v", msg, sizeof msg) == 0, "reopened (backups made)");
    pc_store_close(3);
    char bdir[1200];
    snprintf(bdir, sizeof bdir, "%s/backup", dir);
    CHECK(file_exists(bdir, "ALICE.r49save.bak"), "a backup of each file");
    path_in(p, sizeof p, dir, name);
    write_bytes(p, "R49S\1\0\x80\0 damaged", 16);
    CHECK(pc_store_open(3, dir, NULL, "v", msg, sizeof msg) == 0 && strstr(msg, "1 profile"), "a damaged file restored: %s",
          msg);
    CHECK(count_files(dir, ".r49save") == 1, "the file back");
    DIR *d = opendir(dir);
    int corrupt = 0;
    for (struct dirent *de; d && (de = readdir(d));)
        corrupt += strstr(de->d_name, "ALICE.r49save.corrupt-") != NULL;
    if (d)
        closedir(d);
    CHECK(corrupt == 1, "the damaged one kept as .corrupt-DATE");
    pc_store_close(3);
    pc_note_free(&mine), pc_note_free(&friend_), pc_note_free(&pr);
}

/* A ghost put in the folder itself, not in import/: the import's check (pc_ghost_check) still runs, so a hostile one
 * (car 200, which GhostStart indexes [5][13] by; samples that unzip past their header's size) is set aside as damaged
 * and never reaches the pak; a good one beside it does. And no folder at all (""): refused, not walked past. */
static void test_ghost_in_folder(void)
{
    char dir[1100], why[200], msg[300];
    fresh_dir(dir, sizeof dir, "byhand");
    make_dirs(dir);
    PcNote good, car, zip;
    make_ghost(&good, "CAROL", 1, 0, 70.0f, 700, -1);
    make_ghost(&car, "DAVE", 2, 0, 80.0f, 800, -1);
    car.data[8] = 200;
    u32h(car.data, pc_save_hash(car.data + 4, 0x40));
    make_ghost(&zip, "EVE", 3, 1, 90.0f, 900, -1);
    u32h(zip.data + 0x40, 900 * 3 - 3);   /* the samples make 3 bytes more than the header says */
    u32h(zip.data, pc_save_hash(zip.data + 4, 0x40));
    put_file(dir, "good.r49ghost", &good, PC_ORIGIN_GAME);
    put_file(dir, "car.r49ghost", &car, PC_ORIGIN_GAME);
    put_file(dir, "zip.r49ghost", &zip, PC_ORIGIN_IMPORT);
    CHECK(pc_store_open(2, dir, NULL, "v", msg, sizeof msg) == 0, "opened: %s", msg);
    CHECK(pc_store_build(2, g_img, why, sizeof why) == 0, "build: %s", why);
    PcNote in[PC_PAK_NOTES];
    int k = image_notes(g_img, in), ghosts = 0, has_good = 0;
    for (int i = 0; i < k; i++) {
        PcNoteInfo info;
        pc_note_info(&in[i], &info);
        ghosts += info.kind == PC_NOTE_GHOST;
        has_good |= pc_note_equal(&in[i], &good);
    }
    CHECK(ghosts == 1 && has_good, "only the good ghost on the pak (%d ghosts)", ghosts);
    DIR *d = opendir(dir);
    int corrupt = 0;
    for (struct dirent *de; d && (de = readdir(d));)
        corrupt += strstr(de->d_name, ".r49ghost.corrupt-") != NULL;
    if (d)
        closedir(d);
    CHECK(corrupt == 2, "the two set aside as .corrupt-DATE (%d)", corrupt);
    free_notes(in, k);
    pc_store_close(2);
    CHECK(pc_store_open(2, "", NULL, "v", msg, sizeof msg) == -1, "no folder: %s", msg);
    pc_store_close(2);
    pc_note_free(&good), pc_note_free(&car), pc_note_free(&zip);
}

/* Every reader under hostile input: a ghost file mutated (bytes, truncations, extensions; the CRC made right half the
 * time, so the note's own checks are reached), and random images. Never a crash (the ASan build), and a refusal
 * always says why. */
static void test_fuzz(void)
{
    PcNote g, note;
    PcSaveFileInfo fi;
    uint8_t *b;
    size_t size;
    char why[240];
    make_ghost(&g, "FUZZ", 1, 1, 70.0f, 1500, 2);
    pc_save_file_make(&g, PC_ORIGIN_EXPORT, "v", &b, &size);
    int accepted = 0, refused_silently = 0;
    for (int i = 0; i < 20000; i++) {
        size_t n = size;
        uint8_t *d = malloc(size + 4096);
        memcpy(d, b, size);
        int kind = (int)(rnd() % 6);
        if (kind == 0)
            n = rnd() % size;
        else if (kind == 1)
            n = size + rnd() % 4096, memset(d + size, (int)rnd(), n - size);
        int flips = 1 + (int)(rnd() % 8);
        for (int f = 0; f < flips && n; f++) {
            size_t at = kind == 2 ? 128 + rnd() % PC_GHOST_HEADER : rnd() % n;
            if (at < n)
                d[at] = kind == 3 ? 0xFF : (uint8_t)rnd();
        }
        if (kind == 4 && n > 0x48 + 128)   /* hostile counts in the ghost header */
            u32h(d + 128 + 0x40, rnd()), u32h(d + 128 + 0x3C, rnd() % 40000);
        if (rnd() & 1 && n >= 0x14) {
            uLong crc = crc32(0, d, 0x10);
            static const uint8_t z4[4];
            crc = crc32(crc, z4, 4);
            crc = crc32(crc, d + 0x14, (uInt)(n - 0x14));
            d[0x10] = (uint8_t)crc, d[0x11] = (uint8_t)(crc >> 8), d[0x12] = (uint8_t)(crc >> 16), d[0x13] = (uint8_t)(crc >> 24);
        }
        why[0] = 0;
        if (pc_save_file_read(d, n, &note, &fi, why, sizeof why) == 0) {
            if (pc_ghost_check(note.data, (size_t)note.pages * 256, 1, why, sizeof why) == 0)
                accepted++;
            else
                refused_silently += !why[0];
            pc_note_free(&note);
        } else {
            refused_silently += !why[0];
        }
        free(d);
    }
    CHECK(refused_silently == 0, "every refusal says why (%d silent)", refused_silently);
    printf("fuzz: 20000 mutated ghost files, %d still good ghosts (a byte past the samples, or the time)\n", accepted);
    static uint8_t img[PC_PAK_SIZE];
    for (int i = 0; i < 2000; i++) {
        for (size_t j = 0; j < sizeof img; j++)
            img[j] = (uint8_t)rnd();
        if (i & 1) {   /* a good inode checksum, so the chains are followed */
            unsigned sum = 0;
            for (int j = 10; j < 256; j++)
                sum += img[0x100 + j];
            img[0x101] = (uint8_t)sum;
        }
        PcNote out[PC_PAK_NOTES];
        int bad, k = pc_pak_notes(img, out, &bad, why, sizeof why);
        for (int j = 0; j < k; j++) {
            PcNoteInfo info;
            pc_note_info(&out[j], &info);
            pc_note_free(&out[j]);
        }
    }
    printf("fuzz: 2000 random images read\n");
    free(b);
    pc_note_free(&g);
}

int main(int argc, char **argv)
{
    snprintf(g_root, sizeof g_root, "%s", argc > 1 ? argv[1] : "test-scratch-pakstore");
    remove_tree(g_root);
    make_dirs(g_root);
    test_images();
    test_files();
    test_migration();
    test_writes();
    test_choice();
    test_imports_and_damage();
    test_ghost_in_folder();
    test_fuzz();
    printf("%d of %d checks passed\n", g_checks - g_failed, g_checks);
    return g_failed;
}
