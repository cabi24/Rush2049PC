/* port/ultra/pi.c: the cartridge: loading the ROM, the game's reads of it and the boot segment's decompressors
 * (docs/port/ULTRA.md 3 and 11).
 *
 * The ROM is read whole, byte-swapped to big-endian (.z64) order if it is a .v64 or .n64 dump, and checked
 * against the US SHA-1 (romcheck.c says what a file is when it isn't that ROM). The game reads it by ROM offset:
 * pc_cart_read copies at once and sends the completion message the game waits for, so the reader's osRecvMesg
 * returns as soon as it runs. What it copies is the cartridge's bytes as they are; the file converters (port/assets)
 * make the files native, and the WAD converter and the sound library mark what stays in the cartridge's formats
 * (texel and sample memory, pc_port.h).
 *
 * unzipdata_from_rom, unzipdata and lzss_decode were hand-written assembly over the N64's cartridge DMA. Natively a
 * stream (a wave, a ghost) is inflated with zlib, or unpacked, from the ROM image or from memory.
 *
 * The mods (docs/port/MODS.md) are a second region of the cartridge at PC_ROM_MODS_BASE (pc_rom_set_mods): their files
 * are read from there by pc_cart_read and the decompressors, as the ROM's are from the ROM; without mods it is
 * empty. */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

#include "ultra.h"

/* What an inflate may write when its destination is outside the game heap's block (inside, the block's end:
 * pc_heap_room, at any game.memory): 8 MiB, the N64's whole RDRAM. */
#define INFLATE_MAX (8u << 20)

static uint8_t *g_rom;
static size_t g_rom_size;

const uint8_t *pc_rom_bytes(void) { return g_rom; }
size_t pc_rom_size(void) { return g_rom_size; }

/* ---------------------------------------------------------------- loading */

/* The file at path as the cartridge, when it is the USA ROM, or with check_sha1 off any N64 ROM
 * (romcheck.c). No messages: info says what the file is. */
int pc_rom_open(const char *path, int check_sha1, PcRomInfo *info)
{
    struct stat st;
    memset(info, 0, sizeof *info);
    if (stat(path, &st) != 0) {
        int missing = errno == ENOENT || errno == ENOTDIR;
        snprintf(info->what, sizeof info->what, "%s", missing ? "there is no such file" : strerror(errno));
        return info->status = missing ? PC_ROM_MISSING : PC_ROM_UNREADABLE;
    }
    if (S_ISDIR(st.st_mode)) {
        snprintf(info->what, sizeof info->what, "it is a folder, not a file");
        return info->status = PC_ROM_UNREADABLE;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(info->what, sizeof info->what, "it cannot be opened: %s", strerror(errno));
        return info->status = PC_ROM_UNREADABLE;
    }
    /* A file too big for a ROM is read only far enough to say what it is. */
    size_t size = (size_t)st.st_size, want = size > PC_ROM_MAX_SIZE ? 16 : size;
    uint8_t *rom = malloc(want ? want : 1);
    size_t got = rom ? fread(rom, 1, want, f) : 0;
    int err = ferror(f) ? errno : 0;
    fclose(f);
    if (!rom || got != want) {
        snprintf(info->what, sizeof info->what, "it cannot be read: %s", rom ? strerror(err ? err : EIO) : "no memory");
        free(rom);
        return info->status = PC_ROM_UNREADABLE;
    }
    if (want < size) {
        const char *kind = pc_rom_kind(rom, want);
        info->size = size;
        if (kind) {
            snprintf(info->what, sizeof info->what, "it is %s, not a ROM: extract the ROM from it first", kind);
            info->status = PC_ROM_ARCHIVE;
        } else {
            snprintf(info->what, sizeof info->what, "it is %zu MiB, too big for an N64 ROM", size >> 20);
            info->status = PC_ROM_NOT_ROM;
        }
        free(rom);
        return info->status;
    }
    int status = pc_rom_identify(rom, size, info);
    int usable = status == PC_ROM_OK || (!check_sha1 && (status == PC_ROM_WRONG_GAME ||
                                                          status == PC_ROM_OTHER_VERSION ||
                                                          status == PC_ROM_BAD_DUMP));
    if (!usable) {
        free(rom);
        return status;
    }
    free(g_rom);
    g_rom = rom;
    g_rom_size = size;
    if (pc_verbose)
        pc_log("ROM %s: %zu bytes, .%s order, SHA-1 %s", path, size, info->order, info->sha1);
    return status;
}

int pc_rom_load(const char *path, int check_sha1)
{
    PcRomInfo info;
    int status = pc_rom_open(path, check_sha1, &info);
    if (status == PC_ROM_OK)
        return 0;
    fprintf(stderr, "rush2049pc: %s: %s\n", path, info.what);
    if (info.sha1[0])
        fprintf(stderr, "rush2049pc: its SHA-1 is %s; San Francisco Rush 2049 (USA) is %s\n", info.sha1, PC_ROM_SHA1);
    return g_rom ? 0 : -1;   /* loaded anyway with check_sha1 off */
}

/* --no-rom: a blank 12 MiB cartridge, to run the boot path without the user's ROM (bring-up only). */
int pc_rom_blank(void)
{
    g_rom_size = 12u << 20;
    g_rom = calloc(1, g_rom_size);
    if (!g_rom)
        return -1;
    g_rom[0] = 0x80, g_rom[1] = 0x37, g_rom[2] = 0x12, g_rom[3] = 0x40;
    fprintf(stderr, "rush2049pc: --no-rom: running on a blank cartridge (every ROM read is zero)\n");
    return 0;
}

/* ---------------------------------------------------------------- the mods' region (docs/port/MODS.md) */

static const uint8_t *g_mods;
static size_t g_mods_size;

void pc_rom_set_mods(const uint8_t *data, size_t size)
{
    g_mods = data;
    g_mods_size = data ? size : 0;
}

/* The mods' bytes at a ROM offset in their region, and how many follow; NULL outside it (or with no mods). */
static const uint8_t *mods_at(uint32_t off, size_t *avail)
{
    if (!g_mods || off < PC_ROM_MODS_BASE || off - PC_ROM_MODS_BASE >= g_mods_size)
        return NULL;
    *avail = g_mods_size - (off - PC_ROM_MODS_BASE);
    return g_mods + (off - PC_ROM_MODS_BASE);
}

int pc_rom_copy(uint32_t off, void *dst, uint32_t size)
{
    size_t avail;
    const uint8_t *m = mods_at(off, &avail);
    if (m) {   /* a mod's file: past its region's end is zero, as past the ROM's */
        size_t have = avail < size ? avail : size;
        memcpy(dst, m, have);
        memset((uint8_t *)dst + have, 0, size - have);
        return have == size ? 0 : -1;
    }
    if (!g_rom || off > g_rom_size || size > g_rom_size - off) {
        size_t have = g_rom && off < g_rom_size ? g_rom_size - off : 0;
        if (have > size)
            have = size;
        if (have)
            memcpy(dst, g_rom + off, have);
        memset((uint8_t *)dst + have, 0, size - have);
        return -1;
    }
    memcpy(dst, g_rom + off, size);
    return 0;
}

/* ---------------------------------------------------------------- the game's reads */

void pc_cart_read(uint32_t rom_offset, void *dst, uint32_t size, void *done)
{
    if (pc_rom_copy(rom_offset, dst, size) != 0)
        pc_log("cartridge read past the ROM: offset 0x%X, %u bytes (zero-filled)", rom_offset, size);
    if (pc_verbose > 2)
        pc_log("cartridge read: ROM 0x%06X -> %p, %u bytes (%s)", rom_offset, dst, size, pc_os_thread_name());
    if (done && pc_os_post(done, NULL) < 0)
        pc_log("cartridge read: its completion dropped, queue %p full", done);
}

/* ---------------------------------------------------------------- decompressors */

/* Inflates a raw DEFLATE stream into dst, as far as the stream goes: the game's callers give no size (on the N64 the
 * stream ended where the file did). The output's room is what is left of the game heap from dst (pc_heap_room: the
 * game inflates its files into heap blocks), or INFLATE_MAX elsewhere. */
static int32_t inflate_buffer(const uint8_t *src, size_t avail, void *dst, const char *what)
{
    z_stream z;
    uint32_t room = pc_heap_room(dst);
    memset(&z, 0, sizeof z);
    if (inflateInit2(&z, -15) != Z_OK)
        return 0;
    z.next_in = (Bytef *)src;
    z.avail_in = (uInt)avail;
    z.next_out = dst;
    z.avail_out = room ? room : INFLATE_MAX;
    int r = inflate(&z, Z_FINISH);
    int32_t n = (int32_t)z.total_out;
    inflateEnd(&z);
    if (r != Z_STREAM_END)
        pc_log("inflate %s: zlib error %d after %d bytes", what, r, (int)n);
    return n;
}

int32_t unzipdata_from_rom(uint32_t off, void *dst, int32_t heap_window)
{
    (void)heap_window;   /* where the N64's code took its 12 KB of input buffers from: none needed natively */
    size_t avail;
    const uint8_t *m = mods_at(off, &avail);
    if (!m && (!g_rom || off >= g_rom_size)) {
        pc_log("inflate from ROM 0x%X: outside the ROM", off);
        return 0;
    }
    char what[32];
    snprintf(what, sizeof what, "ROM 0x%06X", off);
    return m ? inflate_buffer(m, avail, dst, what) : inflate_buffer(g_rom + off, g_rom_size - off, dst, what);
}

int32_t unzipdata(const void *src, int32_t size, void *dst)
{
    return inflate_buffer(src, (size_t)size, dst, "from memory");
}

/* The boot LZSS: a flag byte, low bit first; 1 = a literal, 0 = a two-byte copy of (b0 & 0x0F) + 2 bytes from
 * ((b0 & 0xF0) << 4 | b1) back; offset 0 with length nibble 0 ends the stream. Returns the output size. */
int32_t lzss_decode(uint32_t off, void *dst)
{
    size_t avail;
    const uint8_t *m = mods_at(off, &avail);
    if (!m && (!g_rom || off >= g_rom_size)) {
        pc_log("lzss from ROM 0x%X: outside the ROM", off);
        return 0;
    }
    const uint8_t *in = m ? m : g_rom + off, *end = m ? m + avail : g_rom + g_rom_size;
    uint8_t *out = dst, *start = dst;
    unsigned flags = 0, bits = 0;
    for (;;) {
        if (bits == 0) {
            if (in >= end)
                break;
            flags = *in++;
            bits = 8;
        }
        int literal = flags & 1;
        flags >>= 1;
        bits--;
        if (literal) {
            if (in >= end)
                break;
            *out++ = *in++;
            continue;
        }
        if (in + 2 > end)
            break;
        unsigned b0 = in[0], b1 = in[1];
        in += 2;
        unsigned dist = ((b0 & 0xF0) << 4 | b1) & 0xFFF, len = (b0 & 0x0F) + 2;
        if (dist == 0 && (b0 & 0x0F) == 0)
            break;
        for (unsigned i = 0; i < len; i++, out++)
            *out = out[-(long)dist];
    }
    return (int32_t)(out - start);
}
