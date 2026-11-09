/* port/assets/tests/test_world.c: the world WAD converter (port/assets/world.c) on a synthetic world WAD whose
 * table of contents is big-endian (before pc_assets_wad) or already native (after it), with and without a WOBJ
 * entry, and with WOBJ and GDAT count words that don't fit; then the world's records the converter builds beside
 * the file (struct pc_world_records: the names with host pointers to their roots, GDAT, GTLD's side table of
 * struct pc_target_group), the game's own NewWorld (its bsearch over the names) on them as LoadTrack sets them, and
 * the records' going with the block.
 * game: src/blob/NewWorld.c src/blob/game_bsearch.c src/blob/world_name_compare.c
 * game: src/blob/game_strcmp.c
 */
#include <string.h>

#include "assets.h"
#include "pc_game_types.h"
#include "support.h"

/* NewWorld's globals and callees. */
int16_t Worlddlidx;
void *WorldBase;
struct NameEntry *WorldTable;
int32_t *gWorldData;
static void *g_placed;
static int g_placed_parent;
void NewWorldObject(void *node, int16_t parent) { g_placed = node, g_placed_parent = parent; }
void InitSTree(void) {}
int16_t NewWorld(uint8_t *name, int16_t parent);

enum { WHDR = 0x08, WOBJ = 0x40, NODES = 5, GDAT = WOBJ + 0x68 * NODES, GTLD = GDAT + 2 * 0x1C, TOC = GTLD + 12 };

/* The node layout as NewWorldObject reads it. */
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

#define NODE_FLAGS (1u << 31 | 1u << 29)   /* a world node's flag bits 31 and 29, with its index below */

static void put_node(uint8_t *f, int n, int16_t next, int16_t child)
{
    uint8_t *p = f + WOBJ + 0x68 * n;
    int i;
    memset(p, 0, 0x68);
    sprintf((char *)p, "node%d", n);
    for (i = 0; i < 12; i++)
        put_f(p + 0x10 + 4 * i, (float)(n * 100 + i));
    test_put32(p + 0x40, NODE_FLAGS | (uint32_t)n);
    test_put16(p + 0x44, (uint16_t)next);
    test_put16(p + 0x46, (uint16_t)child);
    test_put16(p + 0x48, 7);
    test_put16(p + 0x4A, (uint16_t)-2);
    test_put32(p + 0x4C, (uint32_t)(n - 1));
    for (i = 0; i < 6; i++)
        put_f(p + 0x50 + 4 * i, (float)(i < 3 ? -n - i : n + i));
}

/* ALPHA: nodes 0 -> 1 (siblings). BRAVO starts at node 2, whose child is its node 1 (WOBJ node 3). Node 4 is
 * reached from neither. With_wobj leaves the WOBJ entry out of the TOC. GDAT node 0 has four children, all
 * node 1, a leaf. wobj_count and gdat_count are the TOC's count words (record counts in a good file). */
static void make_world_counts(uint8_t *f, int with_wobj, int native_toc, uint32_t wobj_count, uint32_t gdat_count)
{
    static const uint32_t tags[4] = {0x57484452u, 0x574F424Au, 0x47444154u, 0x47544C44u};
    uint32_t offs[4] = {WHDR, WOBJ, GDAT, GTLD}, counts[4] = {44, wobj_count, gdat_count, 3}, n = 0;
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
            test_put16(g + 0x14 + 2 * k, (uint16_t)(i ? 10 * i + k : 1));
    }
    test_put32(f + GTLD, 5);
    test_put32(f + GTLD + 4, 0xFFFFFFFFu);
    test_put32(f + GTLD + 8, 0xFEDC1234u);   /* a key with its top bit set */
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

static void make_world(uint8_t *f, int with_wobj, int native_toc)
{
    make_world_counts(f, with_wobj, native_toc, NODES, 2);
}

static void check_node(const uint8_t *f, int n, int16_t next, int16_t child)
{
    struct Node104 node;
    memcpy(&node, f + WOBJ + 0x68 * n, sizeof node);
    CHECK(sizeof node == 0x68);
    CHECK(node.transform[0] == (float)(n * 100) && node.transform[11] == (float)(n * 100 + 11));
    CHECK(node.flags == (NODE_FLAGS | (uint32_t)n));
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
        CHECK(pc_be32(f + WOBJ + 0x68 * 4 + 0x40) == (NODE_FLAGS | 4u));
    memcpy(&h, f + GDAT, 2);
    memcpy(&x, f + GDAT + 4, 4);
    CHECK(h == -1 && f[GDAT + 3] == 1 && x == 0.0f);
    memcpy(&h, f + GDAT + 0x14, 2);
    CHECK(h == 1);
    memcpy(&h, f + GDAT + 0x1C, 2);
    memcpy(&x, f + GDAT + 0x1C + 0x10, 4);
    CHECK(h == 0 && f[GDAT + 0x1C + 3] == 0x10 && x == 1003.0f);
    memcpy(&h, f + GDAT + 0x1C + 0x1A, 2);
    CHECK(h == 13);
    memcpy(&v, f + GTLD + 8, 4);
    CHECK(v == 0xFEDC1234u);
}

/* The records the converter built for the world WAD at f: the names with their roots in the file, GDAT where it is
 * in the file with its count word, GTLD's keys (s32) with no targets yet. The file keeps its offsets. */
static void check_records(uint8_t *f)
{
    struct pc_world_records *r = pc_world_records(f);
    uint32_t woff;

    CHECK(r->numWorlds == 2 && r->worlds);
    CHECK(strcmp(r->worlds[0].name, "ALPHA") == 0 && (uint8_t *)r->worlds[0].node == f + WOBJ);
    CHECK(strcmp(r->worlds[1].name, "BRAVO") == 0 && (uint8_t *)r->worlds[1].node == f + WOBJ + 2 * 0x68);
    memcpy(&woff, f + WHDR + 0x18, 4);
    CHECK(woff == WOBJ + 2 * 0x68);
    CHECK((uint8_t *)r->targetGroups == f + GDAT);
    CHECK(r->numTargetGroupList == 3 && (uint8_t *)r->targetGroupList != f + GTLD);
    CHECK(r->targetGroupList[0].key == 5 && r->targetGroupList[1].key == -1);
    CHECK(r->targetGroupList[2].key == (int32_t)0xFEDC1234u);
    CHECK(!r->targetGroupList[0].target && !r->targetGroupList[1].target && !r->targetGroupList[2].target);
}

int main(void)
{
    static uint8_t f[0x400];   /* the world WAD's heap block */
    struct pc_world_records *r;
    int native, wobj;

    for (native = 0; native < 2; native++)
        for (wobj = 1; wobj >= 0; wobj--) {
            make_world(f, wobj, native);
            pc_assets_world(120, f, sizeof f);
            check_world(f, wobj);
            check_records(f);
            CHECK(pc_world_records(f)->numTargetGroups == 2);
        }

    /* The WOBJ and GDAT count words are record counts, which the game itself never reads: one that doesn't fit
     * the file (0, or a byte length) is not reread as bytes; the nodes NewWorld reaches and the quadtree
     * CheckTargetCollision walks are converted instead, and GTLD, after them, is converted once. GDAT's count word
     * is what numgTargetGroups gets, as it is. */
    {
        static const uint32_t bad[3][2] = {{0, 0}, {NODES * 0x68, 2 * 0x1C}, {0x10000, 0xFFFFFFFFu}};
        int b;
        for (b = 0; b < 3; b++) {
            make_world_counts(f, 1, b & 1, bad[b][0], bad[b][1]);
            pc_assets_world(120, f, sizeof f);
            check_world(f, 0);
            check_records(f);
            CHECK(pc_world_records(f)->numTargetGroups == (int32_t)bad[b][1]);
        }
    }

    /* A WHDR that claims more worlds than fit is left as it is: no names. */
    make_world(f, 1, 1);
    test_put32(f + WHDR, 1000);
    pc_assets_world(120, f, sizeof f);
    CHECK(pc_be32(f + WHDR) == 1000 && pc_be32(f + WHDR + 4) == WOBJ);
    CHECK(pc_world_records(f)->numWorlds == 0 && !pc_world_records(f)->worlds);

    /* LoadTrack points the globals at the records, then NewWorld looks a world up by name. */
    make_world(f, 1, 0);
    pc_assets_world(120, f, sizeof f);
    r = pc_world_records(f);
    gWorldData = &r->numWorlds;
    WorldTable = r->worlds;
    CHECK(NewWorld((uint8_t *)"BRAVO", -1) == -1);
    CHECK(g_placed == f + WOBJ + 2 * 0x68 && WorldBase == g_placed && g_placed_parent == -1);
    NewWorld((uint8_t *)"ALPHA", 3);
    CHECK(g_placed == f + WOBJ && g_placed_parent == 3);

    /* A target SetTargetDefs finds stays in the list; the next load of the world (into the block) has none. */
    r->targetGroupList[1].target = (struct tTarget *)r;
    CHECK(pc_world_records(f)->targetGroupList[1].target == (struct tTarget *)r);
    make_world(f, 1, 1);
    pc_assets_world(121, f, sizeof f);
    check_records(f);

    /* A world without GTLD and GDAT: none, and counts of 0, as MBGetFromWad gave them. */
    make_world(f, 1, 0);
    test_put32(f + 4, 2);   /* the TOC's first two entries: WHDR and WOBJ */
    pc_assets_world(120, f, sizeof f);
    r = pc_world_records(f);
    CHECK(r->numWorlds == 2 && !r->targetGroups && !r->numTargetGroups);
    CHECK(!r->targetGroupList && !r->numTargetGroupList);

    /* The records go with the block. */
    pc_heap_block_freed(f, sizeof f);
    CHECK(pc_world_records(f)->numWorlds == 0 && !pc_world_records(f)->worlds);
    return TEST_DONE();
}
