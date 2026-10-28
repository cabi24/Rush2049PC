/* port/ultra/tests/test_romcheck.c: what romcheck.c says a file is (docs/port/RELEASE.md, "The ROM"), on images
 * made here: no ROM is read. Each byte order of one image, the USA header with other contents or another size, other
 * regions and revisions, another game, archives, empty and short files, a file with no N64 header. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ultra.h"

static int failures;

#define CHECK(cond, ...)                                                                                          \
    do {                                                                                                          \
        if (!(cond)) {                                                                                            \
            failures++;                                                                                           \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                           \
            printf(__VA_ARGS__);                                                                                  \
            printf("\n");                                                                                         \
        }                                                                                                         \
    } while (0)

/* A small .z64 image: the N64 header's first word, a name, a game code and a revision, then a pattern. */
static uint8_t *image(size_t size, const char *title, const char *code, int version)
{
    uint8_t *p = malloc(size);
    for (size_t i = 0; i < size; i++)
        p[i] = (uint8_t)(i * 7 + (i >> 8));
    if (size >= 0x40) {
        memcpy(p, "\x80\x37\x12\x40", 4);
        memset(p + 0x20, ' ', 20);
        memcpy(p + 0x20, title, strlen(title));
        memset(p + 0x3B, 0, 4);
        memcpy(p + 0x3B, code, strnlen(code, 4));   /* "" is one byte: 4 read past it (AddressSanitizer) */
        p[0x3F] = (uint8_t)version;
    }
    return p;
}

static void swap16(uint8_t *p, size_t n)
{
    for (size_t i = 0; i + 1 < n; i += 2) {
        uint8_t t = p[i];
        p[i] = p[i + 1], p[i + 1] = t;
    }
}

static void swap32(uint8_t *p, size_t n)
{
    for (size_t i = 0; i + 3 < n; i += 4) {
        uint8_t a = p[i], b = p[i + 1];
        p[i] = p[i + 3], p[i + 1] = p[i + 2], p[i + 2] = b, p[i + 3] = a;
    }
}

static int identify(uint8_t *p, size_t n, PcRomInfo *info)
{
    int s = pc_rom_identify(p, n, info);
    printf("  %-16s %s\n", (const char *[]){"ok", "missing", "unreadable", "not a rom", "archive", "wrong game",
                                            "other version", "bad dump"}[s], info->what);
    return s;
}

int main(void)
{
    PcRomInfo info;
    char sha1[41];

    /* SHA-1 against FIPS 180-1's examples */
    uint8_t d[20];
    pc_sha1("abc", 3, d);
    for (int i = 0; i < 20; i++)
        snprintf(sha1 + 2 * i, 3, "%02x", d[i]);
    CHECK(!strcmp(sha1, "a9993e364706816aba3e25717850c26c9cd0d89d"), "sha1(abc) = %s", sha1);
    const char *two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    pc_sha1(two, strlen(two), d);
    for (int i = 0; i < 20; i++)
        snprintf(sha1 + 2 * i, 3, "%02x", d[i]);
    CHECK(!strcmp(sha1, "84983e441c3bd26ebaae4aa1f95129e5e54670f1"), "sha1(two blocks) = %s", sha1);

    /* The USA header with other contents: a bad dump, in every byte order, with the same SHA-1 */
    const size_t n = 0x4000;
    uint8_t *z = image(n, "Rush 2049", "NRUE", 0);
    CHECK(identify(z, n, &info) == PC_ROM_BAD_DUMP, "USA header, other contents");
    CHECK(!strcmp(info.order, "z64") && !strcmp(info.title, "Rush 2049") && !strcmp(info.code, "NRUE"),
          "order %s title '%s' code '%s'", info.order, info.title, info.code);
    CHECK(strstr(info.what, "not 12582912") != NULL, "a short dump says its size: %s", info.what);
    char z_sha1[41];
    snprintf(z_sha1, sizeof z_sha1, "%s", info.sha1);
    uint8_t *v = image(n, "Rush 2049", "NRUE", 0);
    swap16(v, n);
    CHECK(identify(v, n, &info) == PC_ROM_BAD_DUMP && !strcmp(info.order, "v64"), ".v64: order %s", info.order);
    CHECK(!strcmp(info.sha1, z_sha1) && !memcmp(v, z, n), ".v64 swapped back to the .z64 image");
    uint8_t *w = image(n, "Rush 2049", "NRUE", 0);
    swap32(w, n);
    CHECK(identify(w, n, &info) == PC_ROM_BAD_DUMP && !strcmp(info.order, "n64"), ".n64: order %s", info.order);
    CHECK(!strcmp(info.sha1, z_sha1) && !memcmp(w, z, n), ".n64 swapped back to the .z64 image");
    free(z), free(v), free(w);

    /* The USA header at the cartridge's size: other contents */
    z = image(PC_ROM_USA_SIZE, "Rush 2049", "NRUE", 0);
    CHECK(identify(z, PC_ROM_USA_SIZE, &info) == PC_ROM_BAD_DUMP && strstr(info.what, "modified"), "%s", info.what);
    free(z);

    /* Other regions and revisions; another game; a name without a code */
    z = image(n, "Rush 2049", "NRUP", 0);
    CHECK(identify(z, n, &info) == PC_ROM_OTHER_VERSION && strstr(info.what, "Europe"), "%s", info.what);
    free(z);
    z = image(n, "Rush 2049", "NRUJ", 0);
    CHECK(identify(z, n, &info) == PC_ROM_OTHER_VERSION && strstr(info.what, "Japan"), "%s", info.what);
    free(z);
    z = image(n, "Rush 2049", "NRUQ", 0);
    CHECK(identify(z, n, &info) == PC_ROM_OTHER_VERSION && strstr(info.what, "'Q'"), "%s", info.what);
    free(z);
    z = image(n, "Rush 2049", "NRUE", 1);
    CHECK(identify(z, n, &info) == PC_ROM_OTHER_VERSION && strstr(info.what, "revision 1"), "%s", info.what);
    free(z);
    z = image(n, "SOME OTHER RACER", "NXRE", 0);
    CHECK(identify(z, n, &info) == PC_ROM_WRONG_GAME && strstr(info.what, "\"SOME OTHER RACER\"") &&
          strstr(info.what, "NXRE"), "%s", info.what);
    CHECK(info.sha1[0] != 0, "another game still has its SHA-1");
    free(z);
    z = image(n, "RUSH2049 SYNTHETIC", "\0\0\0\0", 0);   /* tools/port/synth_rom.py's cartridge */
    CHECK(identify(z, n, &info) == PC_ROM_WRONG_GAME && strstr(info.what, "game code none"), "%s", info.what);
    free(z);
    z = image(n, "\x83\x8B\x83\x62\x83\x56\x83\x85", "NXXJ", 0);   /* a name that isn't ASCII */
    CHECK(identify(z, n, &info) == PC_ROM_WRONG_GAME && !strcmp(info.title, "???b?V??"), "title '%s'", info.title);
    free(z);

    /* Not ROMs */
    z = image(n, "", "", 0);
    memcpy(z, "PK\x03\x04", 4);
    CHECK(identify(z, n, &info) == PC_ROM_ARCHIVE && strstr(info.what, "ZIP") && strstr(info.what, "extract"),
          "%s", info.what);
    memcpy(z, "7z\xBC\xAF\x27\x1C", 6);
    CHECK(identify(z, n, &info) == PC_ROM_ARCHIVE && strstr(info.what, "7-Zip"), "%s", info.what);
    memcpy(z, "\x1F\x8B\x08\x00", 4);
    CHECK(identify(z, n, &info) == PC_ROM_ARCHIVE && strstr(info.what, "gzip"), "%s", info.what);
    memcpy(z, "\x7F" "ELF", 4);
    CHECK(identify(z, n, &info) == PC_ROM_NOT_ROM && strstr(info.what, "7F 45 4C 46"), "%s", info.what);
    CHECK(info.sha1[0] == 0 && info.order[0] == 0, "no SHA-1 or order for a file that isn't a ROM");
    CHECK(identify(z, 0, &info) == PC_ROM_NOT_ROM && strstr(info.what, "empty"), "%s", info.what);
    memcpy(z, "\x80\x37\x12\x40", 4);
    CHECK(identify(z, 0x800, &info) == PC_ROM_NOT_ROM && strstr(info.what, "2048 bytes"), "%s", info.what);
    free(z);

    printf("%s\n", failures ? "romcheck: FAILED" : "romcheck: all passed");
    return failures != 0;
}
