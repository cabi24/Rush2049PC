/* port/ultra/pi.c: the cartridge: ROM loading, PI DMA and the boot segment's decompressors
 * (docs/port/ULTRA.md).
 *
 * The ROM is read whole, byte-swapped to big-endian (.z64) order if it is a .v64 or .n64 dump, and checked
 * against the US SHA-1. PI DMA has no manager thread: osPiStartDma copies at once, marks the destination as
 * big-endian ROM bytes (pc_mem_register_be) and sends the completion message, so the caller's osRecvMesg
 * returns as soon as it runs. The game passes ROM offsets as devAddr (libultra adds osRomBase); cartridge bus
 * addresses (0x10000000 and up) are accepted too.
 *
 * inflate_entry, inflate_entry_alt and lzss_decode were hand-written assembly over the PI. Natively: the game
 * image's inflate (game_init, ROM 0xB0CB10) does nothing, since that code and data are linked in; the two
 * runtime images' streams (ROM 0xB5C534, 0xB6FEC4) become pc_data_overlay_load; any other stream (sound
 * banks, ghosts) is inflated with zlib from the ROM image or from memory. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "ultra.h"

#define ROM_SHA1 "3f99351d7bb61656614bdb2aa1a90cfe55d1922c"
#define GAME_IMAGE_ROM 0xB0CB10u
#define GAME_IMAGE_SIZE 647072u
#define OVERLAY_A_ROM 0xB5C534u
#define OVERLAY_A_SIZE 194128u
#define OVERLAY_B_ROM 0xB6FEC4u
#define OVERLAY_B_SIZE 43888u
#define INFLATE_MAX (8u << 20)

static uint8_t *g_rom;
static size_t g_rom_size;
static void *g_pi_cmd_queue;

const uint8_t *pc_rom_bytes(void) { return g_rom; }
size_t pc_rom_size(void) { return g_rom_size; }

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

/* ---------------------------------------------------------------- loading */

int pc_rom_load(const char *path, int check_sha1)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "rush2049pc: cannot open the ROM %s (give its path with --rom)\n", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0x1000 || size > (64 << 20)) {
        fclose(f);
        fprintf(stderr, "rush2049pc: %s is not an N64 ROM (%ld bytes)\n", path, size);
        return -1;
    }
    uint8_t *rom = malloc((size_t)size);
    size_t got = fread(rom, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) {
        free(rom);
        fprintf(stderr, "rush2049pc: cannot read %s\n", path);
        return -1;
    }
    const char *order = "z64";
    if (rom[0] == 0x37 && rom[1] == 0x80) {          /* .v64: 16-bit words swapped */
        order = "v64";
        for (long i = 0; i + 1 < size; i += 2) {
            uint8_t t = rom[i];
            rom[i] = rom[i + 1];
            rom[i + 1] = t;
        }
    } else if (rom[0] == 0x40 && rom[1] == 0x12) {   /* .n64: 32-bit words little-endian */
        order = "n64";
        for (long i = 0; i + 3 < size; i += 4) {
            uint8_t a = rom[i], b = rom[i + 1];
            rom[i] = rom[i + 3];
            rom[i + 1] = rom[i + 2];
            rom[i + 2] = b;
            rom[i + 3] = a;
        }
    } else if (rom[0] != 0x80 || rom[1] != 0x37) {
        free(rom);
        fprintf(stderr, "rush2049pc: %s is not an N64 ROM (unknown header)\n", path);
        return -1;
    }
    uint8_t sum[20];
    char hex[41];
    pc_sha1(rom, (size_t)size, sum);
    for (int i = 0; i < 20; i++)
        snprintf(hex + 2 * i, 3, "%02x", sum[i]);
    if (strcmp(hex, ROM_SHA1) != 0) {
        fprintf(stderr, "rush2049pc: %s (.%s) has SHA-1 %s; San Francisco Rush 2049 (USA) is %s\n", path, order,
                hex, ROM_SHA1);
        if (check_sha1) {
            free(rom);
            fprintf(stderr, "rush2049pc: use --no-rom-check to run it anyway\n");
            return -1;
        }
    }
    g_rom = rom;
    g_rom_size = (size_t)size;
    if (pc_verbose)
        pc_log("ROM %s: %ld bytes, .%s order, SHA-1 %s", path, size, order, hex);
    return 0;
}

static uint32_t rom_offset(uint32_t dev)
{
    uint32_t phys = dev & 0x1FFFFFFFu;
    return phys >= 0x10000000u && phys < 0x1FC00000u ? phys - 0x10000000u : dev;
}

int pc_rom_copy(uint32_t off, void *dst, uint32_t size)
{
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

uint32_t pc_rom_word(uint32_t off)
{
    if (!g_rom || off > g_rom_size - 4)
        return 0;
    const uint8_t *p = g_rom + off;
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

/* ---------------------------------------------------------------- PI */

void osCreatePiManager(int32_t pri, void *cmd_queue, OSMesg *cmd_buf, int32_t cmd_count)
{
    extern void osCreateMesgQueue(void *mq, OSMesg *msg, int32_t count);
    (void)pri;
    g_pi_cmd_queue = cmd_queue;
    osCreateMesgQueue(cmd_queue, cmd_buf, cmd_count);
}

void *osPiGetCmdQueue(void) { return g_pi_cmd_queue; }
void *__osPiGetCmdQueue(void) { return g_pi_cmd_queue; }
int32_t __osPiDeviceBusy(void) { return 0; }

static void dma_read(uint32_t dev, void *dst, uint32_t size)
{
    uint32_t off = rom_offset(dev);
    if (pc_rom_copy(off, dst, size) != 0)
        pc_log("PI DMA past the ROM: offset 0x%X, %u bytes (zero-filled)", off, size);
    pc_mem_register_be(dst, size);
    if (pc_verbose > 2)
        pc_log("PI DMA ROM 0x%06X -> %p, %u bytes (%s)", off, dst, size, pc_os_thread_name());
}

int32_t osPiStartDma(void *mb, int32_t pri, int32_t direction, uint32_t dev, void *dram, uint32_t size, void *mq)
{
    (void)pri;
    if (direction == OS_READ)
        dma_read(dev, dram, size);
    else
        pc_log("PI DMA write to the cartridge ignored: 0x%X, %u bytes", dev, size);
    if (mq && pc_os_post(mq, mb) < 0)
        pc_log("PI DMA completion dropped: queue %p full", mq);
    return 0;
}

int32_t osPiReadIo(uint32_t dev, uint32_t *data)
{
    *data = pc_rom_word(rom_offset(dev));
    return 0;
}

int32_t osPiRawReadIo(uint32_t dev, uint32_t *data) { return osPiReadIo(dev, data); }

/* Libultra's extended-handle raw calls. Nothing calls them once the PI manager is replaced; the handle's
 * base address is the cartridge's. */
int32_t osEPiRawReadIo(void *handle, uint32_t dev, uint32_t *data)
{
    (void)handle;
    return osPiReadIo(dev, data);
}

int32_t osEPiRawWriteIo(void *handle, uint32_t dev, uint32_t data)
{
    (void)handle, (void)dev, (void)data;
    return 0;
}

int32_t osEPiRawStartDma(void *handle, int32_t direction, uint32_t dev, void *dram, uint32_t size)
{
    (void)handle;
    if (direction == OS_READ)
        dma_read(dev, dram, size);
    return 0;
}

/* ---------------------------------------------------------------- decompressors */

/* game_init passes this symbol's address as the game image's ROM offset (0xB0CB10 on the N64). */
uint8_t gRomCompressedData[16];

static int32_t inflate_buffer(const uint8_t *src, size_t avail, void *dst, const char *what)
{
    z_stream z;
    memset(&z, 0, sizeof z);
    if (inflateInit2(&z, -15) != Z_OK)
        return 0;
    z.next_in = (Bytef *)src;
    z.avail_in = (uInt)avail;
    z.next_out = dst;
    z.avail_out = INFLATE_MAX;
    int r = inflate(&z, Z_FINISH);
    int32_t n = (int32_t)z.total_out;
    inflateEnd(&z);
    if (r != Z_STREAM_END)
        pc_log("inflate %s: zlib error %d after %d bytes", what, r, (int)n);
    return n;
}

int32_t inflate_entry(uintptr_t src, void *dst, int32_t heap_window)
{
    (void)heap_window;   /* the N64 code took its 12 KB window from the game heap or 0x803FD120 */
    if (src == (uintptr_t)gRomCompressedData || src == GAME_IMAGE_ROM)
        return GAME_IMAGE_SIZE;   /* the game image is linked in; port/data filled its data */
    if (src == OVERLAY_A_ROM || src == OVERLAY_B_ROM) {
        int ov = src == OVERLAY_A_ROM ? PC_OVERLAY_A : PC_OVERLAY_B;
        if (pc_verbose)
            pc_log("runtime image %c load (%s)", ov == PC_OVERLAY_A ? 'A' : 'B', pc_os_thread_name());
        pc_data_overlay_load(ov);
        return ov == PC_OVERLAY_A ? OVERLAY_A_SIZE : OVERLAY_B_SIZE;
    }
    uint32_t off = rom_offset((uint32_t)src);
    if (!g_rom || off >= g_rom_size) {
        pc_log("inflate from ROM 0x%X: outside the ROM", off);
        return 0;
    }
    char what[32];
    snprintf(what, sizeof what, "ROM 0x%06X", off);
    int32_t n = inflate_buffer(g_rom + off, g_rom_size - off, dst, what);
    pc_mem_register_be(dst, (size_t)n);
    return n;
}

int32_t inflate_entry_alt(const void *src, int32_t size, void *dst)
{
    return inflate_buffer(src, (size_t)size, dst, "from memory");
}

/* The boot LZSS: a flag byte, low bit first; 1 = a literal, 0 = a two-byte copy of (b0 & 0x0F) + 2 bytes from
 * ((b0 & 0xF0) << 4 | b1) back; offset 0 with length nibble 0 ends the stream. Returns the output size. */
int32_t lzss_decode(uintptr_t src, void *dst)
{
    uint32_t off = rom_offset((uint32_t)src);
    if (!g_rom || off >= g_rom_size) {
        pc_log("lzss from ROM 0x%X: outside the ROM", off);
        return 0;
    }
    const uint8_t *in = g_rom + off, *end = g_rom + g_rom_size;
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
    int32_t n = (int32_t)(out - start);
    pc_mem_register_be(dst, (size_t)n);
    return n;
}
