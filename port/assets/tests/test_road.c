/* port/assets/tests/test_road.c: the road / collision converter (port/assets/road.c) on a synthetic big-endian
 * file, then the game's own InitSTree (func_800AB638: the section pointers) and listener_position_set (copies save
 * records into the surface and vertex sections, through a pointer-sized cursor on PC).
 * game: src/blob/func_800AB638.c src/blob/listener_position_set.c
 */
#include <string.h>

#include "assets.h"
#include "support.h"

/* InitSTree's section pointers and counts. */
void *D_801526DC, *D_801526F0;
uint8_t *D_80152034, *D_80124EEC, *D_801497F8, *D_8015201C, *D_801525EC, *D_80152568, *D_80152460;
uint16_t D_8015267C;
void *n64_memcpy(void *d, const void *s, unsigned n) { return memcpy(d, s, n); }
void func_800AB638(void);
void listener_position_set(int32_t arg0);

enum { P = 2, T = 3, S = 2, V = 3, SV = 2, VL = 5 };
enum { PATH = 0x10, TREE = PATH + P * 0x84, SURF = TREE + T * 0x14, SVERT = SURF + S * 0x18, SAVE = SVERT + V * 8,
       VLIST = SAVE + SV * 0x20, SLIST = VLIST + VL };

static void make_road(uint8_t *f)
{
    static const uint16_t counts[6] = {P, T, S, V, SV, VL};
    int i, k;

    memset(f, 0, 0x400);
    for (i = 0; i < 6; i++)
        test_put16(f + 2 * i, counts[i]);
    test_put32(f + 12, 7);
    for (i = 0; i < P * 33; i++) {
        float x = (float)i * 0.5f;
        uint32_t v;
        memcpy(&v, &x, 4);
        test_put32(f + PATH + 4 * i, v);
    }
    for (i = 0; i < T; i++) {
        uint8_t *t = f + TREE + 0x14 * i;
        test_put16(t, (uint16_t)(i - 1));
        t[2] = 0xEE;
        t[3] = (uint8_t)(0x10 << i);
        for (k = 0; k < 8; k++)
            test_put16(t + 4 + 2 * k, (uint16_t)(-100 * i + k));
    }
    for (i = 0; i < S * 12; i++)
        test_put16(f + SURF + 2 * i, (uint16_t)(0x1000 + i));
    for (i = 0; i < V * 4; i++)
        test_put16(f + SVERT + 2 * i, (uint16_t)(0x2000 + i));
    for (i = 0; i < SV; i++) {   /* save i: object 40 + i, into surface 1 - i and vertex 2 - i */
        uint8_t *s = f + SAVE + 0x20 * i;
        test_put16(s, (uint16_t)(40 + i));
        test_put16(s + 2, (uint16_t)(1 - i));
        for (k = 0; k < 9; k++)
            test_put16(s + 4 + 2 * k, (uint16_t)(-(10 * i + k)));
        test_put16(s + 0x16, (uint16_t)(2 - i));
        for (k = 0; k < 4; k++)
            test_put16(s + 0x18 + 2 * k, (uint16_t)(0x3000 + 10 * i + k));
    }
    for (i = 0; i < VL; i++)
        f[VLIST + i] = (uint8_t)(0xA0 + i);
    f[SLIST] = 0x55;
}

static uint16_t u16_at(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static int16_t s16_at(const uint8_t *p) { int16_t v; memcpy(&v, p, 2); return v; }

int main(void)
{
    static uint8_t buf[0x400];
    uint8_t *w = test_rdram(), *f = w ? w + 0x400000 : buf;
    float x;
    uint32_t v;
    int k;

    /* Sections that run past the block: left as they are. */
    make_road(f);
    pc_assets_road(139, f, SAVE + 0x20);
    CHECK(pc_be16(f) == P && pc_be16(f + SURF) == 0x1000);

    make_road(f);
    pc_assets_road(139, f, 0x400);
    CHECK(u16_at(f) == P && u16_at(f + 8) == SV && u16_at(f + 10) == VL);
    memcpy(&v, f + 12, 4);
    CHECK(v == 7);
    memcpy(&x, f + PATH + 4 * 65, 4);
    CHECK(x == 32.5f);
    CHECK(s16_at(f + TREE + 0x14 * 2) == 1 && f[TREE + 0x14 * 2 + 2] == 0xEE && f[TREE + 0x14 * 2 + 3] == 0x40);
    CHECK(s16_at(f + TREE + 0x14 * 2 + 4 + 2 * 7) == -200 + 7);
    CHECK(u16_at(f + SURF + 2 * 23) == 0x1000 + 23 && u16_at(f + SVERT + 2 * 11) == 0x2000 + 11);
    CHECK(f[VLIST + 4] == 0xA4 && f[SLIST] == 0x55);

    /* InitSTree points the sections at the converted file. */
    D_801526DC = f;
    func_800AB638();
    CHECK(D_801526F0 == f && D_80152034 == f + PATH && D_80124EEC == f + TREE && D_801497F8 == f + SURF);
    CHECK(D_8015201C == f + SVERT && D_801525EC == f + SAVE && D_80152568 == f + VLIST && D_80152460 == f + SLIST);
    CHECK(D_8015267C == SV);

    /* Restoring object 41's saved geometry: save 1 goes to surface 0 and vertex 1. */
    listener_position_set(41);
    for (k = 0; k < 9; k++)
        CHECK(s16_at(f + SURF + 4 + 2 * k) == -(10 + k));
    CHECK(u16_at(f + SURF) == 0x1000 && u16_at(f + SURF + 0x18 + 4) == 0x1000 + 14);   /* others untouched */
    for (k = 0; k < 4; k++)
        CHECK(u16_at(f + SVERT + 8 + 2 * k) == 0x3000 + 10 + k);
    CHECK(u16_at(f + SVERT + 16) == 0x2000 + 8);
    return TEST_DONE();
}
