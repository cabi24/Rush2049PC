/* port/assets/tests/test_wad.c: the WAD converter (wad.c) on a synthetic big-endian WAD (wad_build.h): every
 * record field comes out native, path points shared by two paths are swapped once, the big-endian payload is
 * untouched and registered, the release paths drop the registration, and files that aren't WADs are left as
 * they are. */
#include <string.h>

#include "assets.h"
#include "support.h"
#include "wad_build.h"

static uint32_t n32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint16_t n16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static float nf(const uint8_t *p) { float v; memcpy(&v, p, 4); return v; }

static uint8_t g_buf[0x1000], g_orig[0x1000];

static void check_converted(const uint8_t *b)
{
    CHECK(n32(b) == W_TOC && n32(b + 4) == W_TOC_N);
    CHECK(n32(b + W_TOC) == 0x4F424844u && n32(b + W_TOC + 4) == W_OBHD && n32(b + W_TOC + 8) == 2);
    CHECK(n32(b + W_TOC + 12 * 6) == 0x4F424A53u && n32(b + W_TOC + 12 * 6 + 8) == 0x100);

    /* OBHD */
    CHECK(memcmp(b + W_OBHD, "model0", 7) == 0);
    CHECK(nf(b + W_OBHD + 0x10) == 100.0f);
    CHECK(n16(b + W_OBHD + 0x14) == 0x1234 && n16(b + W_OBHD + 0x16) == 2);
    CHECK(n16(b + W_OBHD + 0x18) == 1 && n16(b + W_OBHD + 0x1A) == 0x0300);
    CHECK(nf(b + W_OBHD + 0x1C) == 50.0f);
    CHECK(n32(b + W_OBHD + 0x20) == W_DL0 && n32(b + W_OBHD + 0x24) == W_VTX);
    CHECK(n16(b + W_OBHD + 0x28) == 2 && nf(b + W_OBHD + 0x2C) == 500.0f && n32(b + W_OBHD + 0x30) == W_DL1);
    CHECK(nf(b + W_OBHD + 0x58 + 0x10) == 2.5f && n16(b + W_OBHD + 0x58 + 0x16) == 1);
    CHECK(n32(b + W_OBHD + 0x58 + 0x20) == W_DL_M1);

    /* TXHD */
    for (int t = 0; t < 3; t++) {
        const uint8_t *r = b + W_TXHD + 0x24 * t;
        CHECK(n16(r + 0x10) == 32 << t && n16(r + 0x12) == 16 + t);
        CHECK(r[0x14] == t + 1 && r[0x15] == t * 2);
        CHECK(n16(r + 0x16) == (t == 2 ? 1 : 0xFFFF));
        CHECK(n32(r + 0x18) == (uint32_t)(t == 1 ? W_TXLD - W_IMAG : 0x10 * t));
        CHECK(n32(r + 0x1C) == (t == 1 ? 0x00030000u : 0x08010000u));
        CHECK(n32(r + 0x20) == 0xA0B0C0D0u + (uint32_t)t);
    }
    CHECK(memcmp(b + W_TXHD + 0x24 * 2, "charlie", 8) == 0);

    /* PLHD */
    CHECK(b[W_PLHD + 0x18 + 0x10] == 16 && b[W_PLHD + 0x18 + 0x11] == 31);
    CHECK(n16(b + W_PLHD + 0x12) == 0x0102 && n32(b + W_PLHD + 0x18 + 0x14) == W_TLUT - W_IMAG + 0x20);

    /* PTHD */
    CHECK(n32(b + W_PTHD + 0x24 + 0x10) == 0x00020000u);
    CHECK(n16(b + W_PTHD + 0x24 + 0x14) == 2 && (int16_t)n16(b + W_PTHD + 0x24 + 0x16) == 0);
    CHECK((int16_t)n16(b + W_PTHD + 0x16) == -1);
    CHECK(n32(b + W_PTHD + 0x24 + 0x18) == W_PATH + 0x44 * 2 + 0x40);
    CHECK(n32(b + W_PTHD + 0x24 + 0x1C) == W_PATH + 0x44 * 2 && n32(b + W_PTHD + 0x20) == 0);

    /* PATH: every point native once, including point 2, which both paths reach */
    for (int i = 0; i < 4; i++) {
        const uint8_t *pt = b + W_PATH + 0x44 * i;
        for (int w = 0; w < 16; w++)
            CHECK(nf(pt + 4 * w) == (float)(i * 100 + w));
        CHECK(n32(pt + 0x40) == 0xF1A90000u + (uint32_t)i);
    }

    /* the big-endian payload: TXLD, OBJS (display lists and Vtx), IMAG */
    CHECK(memcmp(b + W_TXLD, g_orig + W_TXLD, W_TOC - W_TXLD) == 0);
    CHECK(pc_be32(b + W_DL0) == 0x01003006u && pc_be16(b + W_VTX + 6) == 0x8020);
}

int main(void)
{
    /* A WAD in its block. */
    size_t size = wad_build(g_orig);
    memcpy(g_buf, g_orig, size);
    test_be_reset();
    pc_wave_loaded(101, g_buf, sizeof g_buf);
    check_converted(g_buf);
    CHECK(test_be_range_count() == 1);
    CHECK(pc_mem_is_be(g_buf + W_DL0) && pc_mem_is_be(g_buf + W_TLUT) && pc_mem_is_be(g_buf + sizeof g_buf - 1));
    CHECK(!pc_mem_is_be(g_buf + sizeof g_buf));

    /* UnloadWaveInstance's release, and the heap's free */
    pc_wave_released(101, g_buf);
    CHECK(test_be_range_count() == 0);
    memcpy(g_buf, g_orig, size);
    pc_wave_loaded(30, g_buf, sizeof g_buf);   /* a font texture WAD */
    check_converted(g_buf);
    pc_heap_block_freed(g_buf, sizeof g_buf);
    CHECK(test_be_range_count() == 0);

    /* The PI DMA registered the file already (port/ultra/pi.c): one range still, and the free drops it. */
    memcpy(g_buf, g_orig, size);
    pc_mem_register_be(g_buf, size);
    pc_wave_loaded(60, g_buf, sizeof g_buf);
    CHECK(test_be_range_count() == 1);
    pc_heap_block_freed(g_buf, sizeof g_buf);
    CHECK(test_be_range_count() == 0);

    /* Two WADs in a sub-heap that is freed whole. */
    {
        static uint8_t heap[0x3000];
        memcpy(heap + 0x100, g_orig, size);
        memcpy(heap + 0x1200, g_orig, size);
        pc_wave_loaded(88, heap + 0x100, 0x1000);
        pc_wave_loaded(89, heap + 0x1200, 0x1000);
        check_converted(heap + 0x100);
        check_converted(heap + 0x1200);
        CHECK(test_be_range_count() == 2);
        pc_heap_block_freed(heap, sizeof heap);
        CHECK(test_be_range_count() == 0);
    }

    /* A block reused for a new WAD without a release in between: the stale registration goes. */
    memcpy(g_buf, g_orig, size);
    pc_wave_loaded(101, g_buf, sizeof g_buf);
    memcpy(g_buf + 0x20, g_orig, size);
    pc_wave_loaded(102, g_buf + 0x20, sizeof g_buf - 0x20);
    check_converted(g_buf + 0x20);
    CHECK(test_be_range_count() == 1);
    pc_wave_released(102, g_buf + 0x20);
    CHECK(test_be_range_count() == 0);

    /* Not WADs: nothing changes and nothing is registered. */
    {
        uint8_t b[64];
        memset(b, 0, sizeof b);   /* a blank cartridge's file */
        pc_wave_loaded(54, b, 0);
        pc_wave_loaded(54, b, 4);
        test_put32(b, 0x80001000u);   /* relocated already */
        test_put32(b + 4, 1);
        pc_wave_loaded(54, b, sizeof b);
        CHECK(pc_be32(b) == 0x80001000u);
        test_put32(b, 0x40);          /* the TOC runs past the block */
        test_put32(b + 4, 3);
        pc_wave_loaded(54, b, sizeof b);
        CHECK(pc_be32(b) == 0x40 && pc_be32(b + 4) == 3);
        CHECK(test_be_range_count() == 0);
    }

    /* A chunk that runs past the block is left as is; the others are converted. */
    memcpy(g_buf, g_orig, size);
    test_put32(g_buf + W_TOC + 8, 0x1000);   /* OBHD: 0x1000 models */
    pc_wave_loaded(101, g_buf, sizeof g_buf);
    CHECK(n32(g_buf + W_TOC + 8) == 0x1000);
    CHECK(pc_be32(g_buf + W_OBHD + 0x20) == W_DL0);              /* OBHD still big-endian */
    CHECK(n32(g_buf + W_TXHD + 0x1C) == 0x08010000u);            /* TXHD converted */
    pc_wave_released(101, g_buf);
    CHECK(test_be_range_count() == 0);
    return TEST_DONE();
}
