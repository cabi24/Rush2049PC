/* port/assets/tests/test_world.c: the world WAD converter (port/assets/world.c) on a synthetic world WAD whose
 * table of contents is big-endian (before pc_assets_wad) or already native (after it), with and without a WOBJ
 * entry; then the game's own NewWorld (wheel_torque_apply and its bsearch over the WHDR names) on the converted
 * file, and LoadTrack's native GTLD array (pc_world_chunk).
 * game: src/blob/wheel_torque_apply.c src/blob/entity_name_copy.c src/blob/pointer_offset_wrapper.c
 * game: src/blob/func_800A473C.c src/blob/func_8008AD04.c
 */
#include <string.h>

#include "assets.h"
#include "support.h"

uintptr_t pc_world_chunk(uint32_t addr, int32_t tag, int32_t *count);

/* NewWorld's globals and callees. */
int16_t D_80149D90;
void *D_80149B80, *D_80149818;
uint32_t *D_801497FC;
static void *g_placed;
static int g_placed_parent;
void differential_output(void *node, int16_t parent) { g_placed = node, g_placed_parent = parent; }
void func_800AB638(void) {}
int16_t wheel_torque_apply(uint8_t *name, int16_t parent);

enum { WHDR = 0x08, WOBJ = 0x40, NODES = 5, GDAT = WOBJ + 0x68 * NODES, GTLD = GDAT + 2 * 0x1C, TOC = GTLD + 12 };

/* The node layout as differential_output reads it. */
struct Node104 {
    char name[16];
    float transform[12];
    uint32_t flags;
    int16_t sibling, child, sfx1, sfx2;
    int32_t tidx;
    float minimum[3], maximum[3];
};

static void put_f(uint8_t *p, float f)
{
    uint32_t v;
    memcpy(&v, &f, 4);
    test_put32(p, v);
}

static void put_node(uint8_t *f, int n, int16_t next, int16_t child)
{
    uint8_t *p = f + WOBJ + 0x68 * n;
    int i;
    memset(p, 0, 0x68);
    sprintf((char *)p, "node%d", n);
    for (i = 0; i < 12; i++)
        put_f(p + 0x10 + 4 * i, (float)(n * 100 + i));
    test_put32(p + 0x40, 0xA0000000u | (uint32_t)n);
    test_put16(p + 0x44, (uint16_t)next);
    test_put16(p + 0x46, (uint16_t)child);
    test_put16(p + 0x48, 7);
    test_put16(p + 0x4A, (uint16_t)-2);
    test_put32(p + 0x4C, (uint32_t)(n - 1));
    for (i = 0; i < 6; i++)
        put_f(p + 0x50 + 4 * i, (float)(i < 3 ? -n - i : n + i));
}

/* ALPHA: nodes 0 -> 1 (siblings). BRAVO starts at node 2, whose child is its node 1 (WOBJ node 3). Node 4 is
 * reached from neither. With_wobj leaves the WOBJ entry out of the TOC. */
static void make_world(uint8_t *f, int with_wobj, int native_toc)
{
    static const uint32_t tags[4] = {0x57484452u, 0x574F424Au, 0x47444154u, 0x47544C44u};
    uint32_t offs[4] = {WHDR, WOBJ, GDAT, GTLD}, counts[4] = {44, NODES, 2, 3}, n = 0;
    int i, k;

    memset(f, 0, 0x400);
    test_put32(f + WHDR, 2);
    test_put32(f + WHDR + 4, WOBJ);
    strcpy((char *)f + WHDR + 8, "ALPHA");
    test_put32(f + WHDR + 0x18, WOBJ + 2 * 0x68);
    strcpy((char *)f + WHDR + 0x1C, "BRAVO");
    put_node(f, 0, 1, -1);
    put_node(f, 1, -1, -1);
    put_node(f, 2, -1, 1);
    put_node(f, 3, -1, -1);
    put_node(f, 4, -1, -1);
    for (i = 0; i < 2; i++) {
        uint8_t *g = f + GDAT + 0x1C * i;
        test_put16(g, (uint16_t)(i - 1));
        g[3] = (uint8_t)(i ? 0x10 : 1);
        for (k = 0; k < 4; k++)
            put_f(g + 4 + 4 * k, (float)(1000 * i + k));
        for (k = 0; k < 4; k++)
            test_put16(g + 0x14 + 2 * k, (uint16_t)(10 * i + k));
    }
    test_put32(f + GTLD, 5);
    test_put32(f + GTLD + 4, 0xFFFFFFFFu);
    test_put32(f + GTLD + 8, 0x80001234u);
    for (i = 0; i < 4; i++) {
        uint8_t *e = f + TOC + 12 * n;
        if (i == 1 && !with_wobj)
            continue;
        test_put32(e, tags[i]);
        test_put32(e + 4, offs[i]);
        test_put32(e + 8, counts[i]);
        n++;
    }
    test_put32(f, TOC);
    test_put32(f + 4, n);
    if (native_toc) {   /* what pc_assets_wad leaves */
        pc_swap32_n(f, 2);
        pc_swap32_n(f + TOC, 3 * n);
    }
}

static void check_node(const uint8_t *f, int n, int16_t next, int16_t child)
{
    struct Node104 node;
    memcpy(&node, f + WOBJ + 0x68 * n, sizeof node);
    CHECK(sizeof node == 0x68);
    CHECK(node.transform[0] == (float)(n * 100) && node.transform[11] == (float)(n * 100 + 11));
    CHECK(node.flags == (0xA0000000u | (uint32_t)n));
    CHECK(node.sibling == next && node.child == child && node.sfx1 == 7 && node.sfx2 == -2);
    CHECK(node.tidx == n - 1);
    CHECK(node.minimum[1] == (float)(-n - 1) && node.maximum[2] == (float)(n + 5));
    CHECK(strncmp(node.name, "node", 4) == 0);
}

static void check_world(uint8_t *f, int node4_converted)
{
    uint32_t v;
    int16_t h;
    float x;

    memcpy(&v, f + WHDR, 4);
    CHECK(v == 2);
    memcpy(&v, f + WHDR + 0x18, 4);
    CHECK(v == WOBJ + 2 * 0x68);
    CHECK(strcmp((char *)f + WHDR + 0x1C, "BRAVO") == 0);
    check_node(f, 0, 1, -1);
    check_node(f, 1, -1, -1);
    check_node(f, 2, -1, 1);
    check_node(f, 3, -1, -1);
    if (node4_converted)
        check_node(f, 4, -1, -1);
    else
        CHECK(pc_be32(f + WOBJ + 0x68 * 4 + 0x40) == 0xA0000004u);
    memcpy(&h, f + GDAT + 0x1C, 2);
    memcpy(&x, f + GDAT + 0x1C + 0x10, 4);
    CHECK(h == 0 && f[GDAT + 0x1C + 3] == 0x10 && x == 1003.0f);
    memcpy(&h, f + GDAT + 0x1C + 0x1A, 2);
    CHECK(h == 13);
    memcpy(&v, f + GTLD + 8, 4);
    CHECK(v == 0x80001234u);
}

int main(void)
{
    uint8_t *w = test_rdram(), *f;
    static uint8_t host[0x400];
    int native, wobj;

    f = w ? w + 0x300000 : host;
    for (native = 0; native < 2; native++)
        for (wobj = 1; wobj >= 0; wobj--) {
            make_world(f, wobj, native);
            pc_assets_world(120, f, 0x400);
            check_world(f, wobj);
        }

    /* A WHDR that claims more worlds than fit is left as it is. */
    make_world(f, 1, 1);
    test_put32(f + WHDR, 1000);
    pc_assets_world(120, f, 0x400);
    CHECK(pc_be32(f + WHDR) == 1000 && pc_be32(f + WHDR + 4) == WOBJ);

    if (!w) {
        fprintf(stderr, "test_world: no RDRAM window here, NewWorld and GTLD skipped\n");
        return TEST_DONE();
    }

    /* LoadTrack: the WHDR offsets become window addresses, then NewWorld looks a world up by name. */
    make_world(f, 1, 0);
    pc_assets_world(120, f, 0x400);
    D_801497FC = (uint32_t *)(f + WHDR);
    D_80149818 = D_801497FC + 1;
    {
        uint32_t woff;
        int i;
        for (i = 0; i < 2; i++) {
            memcpy(&woff, f + WHDR + 4 + 0x14 * i, 4);
            woff += (uint32_t)(uintptr_t)f;
            memcpy(f + WHDR + 4 + 0x14 * i, &woff, 4);
        }
    }
    CHECK(wheel_torque_apply((uint8_t *)"BRAVO", -1) == -1);
    CHECK(g_placed == f + WOBJ + 2 * 0x68 && D_80149B80 == g_placed && g_placed_parent == -1);
    wheel_torque_apply((uint8_t *)"ALPHA", 3);
    CHECK(g_placed == f + WOBJ && g_placed_parent == 3);

    /* GTLD as LoadTrack's PC view gets it: a native array of the keys, sign-extended. */
    {
        int32_t count = 3;
        uint32_t addr = (uint32_t)(uintptr_t)(f + GTLD);
        void **keys = (void **)pc_world_chunk(addr, 0x47544C44, &count);
        CHECK((uint8_t *)keys != f + GTLD);
        CHECK(keys[0] == (void *)(intptr_t)5 && keys[1] == (void *)(intptr_t)-1);
        CHECK(keys[2] == (void *)(intptr_t)(int32_t)0x80001234u);
        CHECK(pc_world_chunk(addr, 0x47444154, &count) == addr);
        CHECK(pc_world_chunk(addr, 0x57484452, NULL) == addr);
        count = 0;
        CHECK(pc_world_chunk(0, 0x47544C44, &count) == 0);
    }
    return TEST_DONE();
}
