/* port/assets/tests/test_dispatch.c: the byte-order helpers, and that pc_wave_loaded reaches every wave without
 * touching the formats nobody converts yet. */
#include <string.h>

#include "assets.h"
#include "support.h"

int main(void)
{
    uint8_t b[16];

    test_put32(b, 0x11223344u);
    test_put16(b + 4, 0xA1B2u);
    CHECK(pc_be32(b) == 0x11223344u);
    CHECK(pc_be16(b + 4) == 0xA1B2u);
    pc_swap32_at(b);
    pc_swap16_at(b + 4);
    {
        uint32_t w;
        uint16_t h;
        memcpy(&w, b, 4);
        memcpy(&h, b + 4, 2);
        CHECK(w == 0x11223344u);
        CHECK(h == 0xA1B2u);
    }

    test_put32(b, 1), test_put32(b + 4, 2), test_put32(b + 8, 0x3F800000u);
    pc_swap32_n(b, 3);
    {
        uint32_t w[3];
        float f;
        memcpy(w, b, 12);
        memcpy(&f, b + 8, 4);
        CHECK(w[0] == 1 && w[1] == 2);
        CHECK(f == 1.0f);
    }

    /* Every wave number dispatches, and the release path tolerates a block with nothing registered. */
    for (int wave = 0; wave < 200; wave++) {
        uint8_t buf[64] = {0};
        pc_wave_loaded(wave, buf, sizeof buf);
        pc_wave_released(wave, buf);
    }
    CHECK(test_be_range_count() == 0);
    return TEST_DONE();
}
