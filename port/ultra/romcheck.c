/* port/ultra/romcheck.c: what a ROM file is (docs/port/RELEASE.md, "The ROM"). Its byte order (put right in
 * place), its SHA-1, its header, and whether it is the cartridge the port runs, San Francisco Rush 2049 (USA); if
 * not, a sentence for the player saying what it is instead. Pure: no files and no messages, so the tests run it on
 * images they make. The USA cartridge's header fields are the decompilation's asm/us/header.s. */
#include <stdio.h>
#include <string.h>

#include "ultra.h"

#define USA_CODE "NRUE"   /* cartridge 'N', ID "RU", country 'E' */
#define USA_VERSION 0

/* ---------------------------------------------------------------- SHA-1 (FIPS 180-1) */

static uint32_t rol(uint32_t x, int n) { return x << n | x >> (32 - n); }

static void sha1_block(uint32_t h[5], const uint8_t *p)
{
    uint32_t w[80], a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 80; i++)
        w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20)
            f = (b & c) | (~b & d), k = 0x5A827999;
        else if (i < 40)
            f = b ^ c ^ d, k = 0x6ED9EBA1;
        else if (i < 60)
            f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
        else
            f = b ^ c ^ d, k = 0xCA62C1D6;
        uint32_t t = rol(a, 5) + f + e + k + w[i];
        e = d, d = c, c = rol(b, 30), b = a, a = t;
    }
    h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
}

void pc_sha1(const void *data, size_t size, uint8_t out[20])
{
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    const uint8_t *p = data;
    size_t n = size;
    for (; n >= 64; n -= 64, p += 64)
        sha1_block(h, p);
    uint8_t tail[128] = {0};
    memcpy(tail, p, n);
    tail[n] = 0x80;
    size_t len = n < 56 ? 64 : 128;
    uint64_t bits = (uint64_t)size * 8;
    for (int i = 0; i < 8; i++)
        tail[len - 1 - i] = (uint8_t)(bits >> (8 * i));
    sha1_block(h, tail);
    if (len == 128)
        sha1_block(h, tail + 64);
    for (int i = 0; i < 20; i++)
        out[i] = (uint8_t)(h[i / 4] >> (24 - 8 * (i % 4)));
}

/* ---------------------------------------------------------------- what the file is */

const char *pc_rom_kind(const uint8_t *head, size_t n)
{
    static const struct { const char *magic; size_t len; const char *name; } kinds[] = {
        {"PK\x03\x04", 4, "a ZIP archive"}, {"PK\x05\x06", 4, "an empty ZIP archive"},
        {"7z\xBC\xAF\x27\x1C", 6, "a 7-Zip archive"}, {"Rar!\x1A\x07", 6, "a RAR archive"},
        {"\x1F\x8B", 2, "a gzip file"}, {"BZh", 3, "a bzip2 file"}, {"\xFD" "7zXZ", 5, "an xz file"},
    };
    for (size_t i = 0; i < sizeof kinds / sizeof *kinds; i++)
        if (n >= kinds[i].len && !memcmp(head, kinds[i].magic, kinds[i].len))
            return kinds[i].name;
    return NULL;
}

static const char *region_name(char c)
{
    switch (c) {
    case 'E': return "the USA";
    case 'J': return "Japan";
    case 'P': case 'X': case 'Y': return "Europe";
    case 'D': return "Germany";
    case 'F': return "France";
    case 'I': return "Italy";
    case 'S': return "Spain";
    case 'U': return "Australia";
    default: return NULL;
    }
}

/* The header's text: printable ASCII, others as '?', trailing spaces and NULs dropped. */
static void header_text(const uint8_t *p, size_t n, char *out)
{
    size_t len = 0;
    for (size_t i = 0; i < n; i++) {
        out[i] = p[i] >= 0x20 && p[i] < 0x7F ? (char)p[i] : p[i] ? '?' : ' ';
        if (out[i] != ' ')
            len = i + 1;
    }
    out[len] = 0;
}

int pc_rom_identify(uint8_t *rom, size_t size, PcRomInfo *info)
{
    memset(info, 0, sizeof *info);
    info->size = size;
    const char *kind = pc_rom_kind(rom, size);
    if (kind) {
        snprintf(info->what, sizeof info->what, "it is %s, not a ROM: extract the ROM from it first", kind);
        return info->status = PC_ROM_ARCHIVE;
    }
    if (size == 0) {
        snprintf(info->what, sizeof info->what, "it is empty");
        return info->status = PC_ROM_NOT_ROM;
    }
    if (size < 0x1000) {
        snprintf(info->what, sizeof info->what, "it is %zu bytes, too small for an N64 ROM", size);
        return info->status = PC_ROM_NOT_ROM;
    }
    if (size > PC_ROM_MAX_SIZE) {
        snprintf(info->what, sizeof info->what, "it is %zu MiB, too big for an N64 ROM", size >> 20);
        return info->status = PC_ROM_NOT_ROM;
    }
    if (rom[0] == 0x37 && rom[1] == 0x80) {          /* .v64: 16-bit words swapped */
        snprintf(info->order, sizeof info->order, "v64");
        for (size_t i = 0; i + 1 < size; i += 2) {
            uint8_t t = rom[i];
            rom[i] = rom[i + 1];
            rom[i + 1] = t;
        }
    } else if (rom[0] == 0x40 && rom[1] == 0x12) {   /* .n64: 32-bit words little-endian */
        snprintf(info->order, sizeof info->order, "n64");
        for (size_t i = 0; i + 3 < size; i += 4) {
            uint8_t a = rom[i], b = rom[i + 1];
            rom[i] = rom[i + 3];
            rom[i + 1] = rom[i + 2];
            rom[i + 2] = b;
            rom[i + 3] = a;
        }
    } else if (rom[0] == 0x80 && rom[1] == 0x37) {
        snprintf(info->order, sizeof info->order, "z64");
    } else {
        snprintf(info->what, sizeof info->what,
                 "it is not an N64 ROM: it starts with %02X %02X %02X %02X, not an N64 cartridge's header", rom[0],
                 rom[1], rom[2], rom[3]);
        return info->status = PC_ROM_NOT_ROM;
    }

    uint8_t sum[20];
    pc_sha1(rom, size, sum);
    for (int i = 0; i < 20; i++)
        snprintf(info->sha1 + 2 * i, 3, "%02x", sum[i]);
    header_text(rom + 0x20, 20, info->title);
    header_text(rom + 0x3B, 4, info->code);
    info->version = rom[0x3F];
    if (!strcmp(info->sha1, PC_ROM_SHA1)) {
        snprintf(info->what, sizeof info->what, "San Francisco Rush 2049 (USA)");
        return info->status = PC_ROM_OK;
    }

    /* Rush 2049 by its cartridge ID, "RU", and its name */
    int rush = info->code[0] == 'N' && info->code[1] == 'R' && info->code[2] == 'U' && strstr(info->title, "2049");
    if (!rush) {
        snprintf(info->what, sizeof info->what, "it is another N64 game: \"%s\" (game code %s)",
                 info->title[0] ? info->title : "no name", info->code[0] ? info->code : "none");
        return info->status = PC_ROM_WRONG_GAME;
    }
    if (info->code[3] != USA_CODE[3]) {
        const char *region = region_name(info->code[3]);
        if (region)
            snprintf(info->what, sizeof info->what, "it is San Francisco Rush 2049 for %s (game code %s), not the USA "
                     "cartridge", region, info->code);
        else
            snprintf(info->what, sizeof info->what, "it is San Francisco Rush 2049 with region code '%c' (game code "
                     "%s), not the USA cartridge", info->code[3], info->code);
        return info->status = PC_ROM_OTHER_VERSION;
    }
    if (info->version != USA_VERSION) {
        snprintf(info->what, sizeof info->what, "it is revision %d of San Francisco Rush 2049 (USA); the port runs "
                 "the first, revision 0", info->version);
        return info->status = PC_ROM_OTHER_VERSION;
    }
    if (size != PC_ROM_USA_SIZE)
        snprintf(info->what, sizeof info->what, "its header says San Francisco Rush 2049 (USA), but it is %zu bytes, "
                 "not %u (12 MiB): a cut, padded or bad dump", size, PC_ROM_USA_SIZE);
    else
        snprintf(info->what, sizeof info->what, "its header says San Francisco Rush 2049 (USA), but its contents "
                 "differ: a bad dump or a modified ROM");
    return info->status = PC_ROM_BAD_DUMP;
}
