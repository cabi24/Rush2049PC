/* port/assets/tests/test_path.c: the path converter (port/assets/path.c) on a synthetic big-endian path file: the
 * file swapped in place, its headers decoded beside it (struct pc_path_records: the paths with host pointers to
 * their points); then the game's own ReadCheckpointData and ReadPathData (which copies the decoded headers into the
 * track's paths), three readers that walk the results (NextResPathPt, NextPathPt, PrevPathPt), and the records'
 * going with the block.
 * game: src/blob/ReadCheckpointData.c src/blob/groups/pathcommon_read/ReadPathData.c
 * game: src/blob/NextResPathPt.c src/blob/NextPathPt.c src/blob/PrevPathPt.c
 */
#include <string.h>

#include "assets.h"
#include "pc_game_types.h"
#include "support.h"

/* The game's paths: their one declaration (pc_game_types.h). */
int8_t gRestartGame;
void *gPathWave;
struct ResurrectPath gResPath;
struct AltPath *gAltPaths;
uint16_t gNumResPathPts;
int16_t (*gResPathPtList)[3];
struct DronePath gDronePaths[4];
int16_t gNumPathPts[4];
struct DronePathPt *gPathPtList[4];
uint8_t gCheckpoints[812] __attribute__((aligned(8)));
void *n64_memcpy(void *d, const void *s, unsigned n) { return memcpy(d, s, n); }
void CalcCheckpointTimes(void) {}
void SyncCheckpointsToPaths(int16_t mode) {}
void SyncAltPaths(void) {}
void CalcResPathBounds(void) {}
void ReadCheckpointData(void);
void ReadPathData(void);
void NextResPathPt(int32_t arg0, int32_t arg1, int32_t *arg2, int32_t *arg3);
int16_t NextPathPt(int16_t arg0, int16_t arg1);
int16_t PrevPathPt(int16_t arg0, int16_t arg1);

enum { PRIMARY = 3, ALTS = 2 };
static const uint16_t k_alt_pts[ALTS] = {1, 4}, k_drone_pts[4] = {2, 5, 3, 0};

static void put_f(uint8_t *p, float f)
{
    uint32_t v;
    memcpy(&v, &f, 4);
    test_put32(p, v);
}

/* Returns the file size; *alt_pts_at and *drone_at get the offsets of the alternative points and drone paths. */
static size_t make_path(uint8_t *f, size_t *res_at, size_t *alt_pts_at, size_t *drone_at)
{
    size_t pos;
    int i, k, p;

    memset(f, 0, 0x800);
    test_put16(f, 90);
    test_put16(f + 8, 2);   /* numPoints */
    for (i = 0; i < 10; i++) {
        uint8_t *c = f + 12 + 0x50 * i;
        for (k = 0; k < 6; k++)
            put_f(c + 4 * k, (float)(i * 10 + k));
        test_put32(c + 0x18, (uint32_t)(500 + i));
        test_put16(c + 0x1C, (uint16_t)(i == 0 ? 7 : 0));   /* flags: finish, start and loop at checkpoint 0 */
        for (k = 0; k < 20; k++)
            test_put16(c + 0x22 + 2 * k, (uint16_t)(k + 1));
        put_f(c + 0x4C, 12.5f * (float)i);
    }
    pos = 0x32C;
    test_put16(f + pos, PRIMARY);
    test_put32(f + pos + 4, 0xDEADBEEFu);   /* pointer slots: whatever the tool left there */
    f[pos + 8] = ALTS;
    test_put32(f + pos + 12, 0xDEADBEEFu);
    pos += 0x10;
    for (i = 0; i < ALTS; i++, pos += 0x10) {
        uint8_t *a = f + pos;
        a[0] = (uint8_t)(i + 1);       /* resurrect */
        a[1] = (uint8_t)-1;            /* firstPath */
        test_put16(a + 2, (uint16_t)(30 + i));
        a[4] = (uint8_t)(2 + i);       /* lastPath */
        test_put16(a + 6, (uint16_t)(77 + i));
        a[8] = 9;
        test_put16(a + 0xA, k_alt_pts[i]);
    }
    test_put16(f + pos, PRIMARY);
    pos += 2;
    *res_at = pos;
    for (i = 0; i < PRIMARY * 3; i++, pos += 2)
        test_put16(f + pos, (uint16_t)(-1000 - i));
    *alt_pts_at = pos;
    for (i = 0; i < ALTS; i++)
        for (k = 0; k < k_alt_pts[i] * 3; k++, pos += 2)
            test_put16(f + pos, (uint16_t)(2000 + 100 * i + k));
    *drone_at = pos;   /* 2 mod 4 here: only 2-byte alignment from now on */
    for (p = 0; p < 4; p++) {
        test_put16(f + pos, k_drone_pts[p]);
        test_put16(f + pos + 2, (uint16_t)(0x5A00 + p));   /* the padding after the count, kept */
        pos += 8;
        for (i = 0; i < k_drone_pts[p]; i++, pos += 8) {
            for (k = 0; k < 3; k++)
                test_put16(f + pos + 2 * k, (uint16_t)(-(300 * p + 10 * i + k)));
            f[pos + 6] = (uint8_t)(60 + i);   /* mph */
            f[pos + 7] = (uint8_t)-3;         /* hints */
        }
    }
    return pos;
}

int main(void)
{
    static uint8_t f[0x800];   /* the path file's heap block */
    const struct pc_path_records *r;
    size_t res_at, alt_at, drone_at, size, pos;
    int32_t a2 = 0, a3 = 0;
    float x;
    int i, k, p;

    size = make_path(f, &res_at, &alt_at, &drone_at);
    CHECK(drone_at % 4 == 2);
    pc_assets_path(158, f, size - 1);   /* one byte short: left as it is, no records */
    CHECK(pc_be16(f + 8) == 2 && pc_be16(f + 0x32C) == PRIMARY);
    CHECK(pc_path_records(f)->resPath.numPts == 0 && !pc_path_records(f)->alt);
    pc_assets_path(158, f, size);

    /* The records: the headers, each points a host pointer into the file. */
    r = pc_path_records(f);
    CHECK(r->resPath.numPts == PRIMARY && r->resPath.numAlt == ALTS && r->numResPathPts == PRIMARY);
    CHECK((uint8_t *)r->resPath.pts == f + res_at && r->resPath.alt == r->alt);
    CHECK((uint8_t *)r->alt[0].pts == f + alt_at && r->alt[1].numPts == k_alt_pts[1]);
    CHECK((uint8_t *)r->dronePaths[0].pts == f + drone_at + 8 && r->dronePaths[3].numPts == 0);

    /* ReadCheckpointData copies the block into gCheckpoints. */
    gPathWave = f;
    ReadCheckpointData();
    {
        int16_t h;
        int32_t cr;
        memcpy(&h, gCheckpoints + 8, 2);
        CHECK(h == 2);
        memcpy(&h, gCheckpoints + 2, 2);
        CHECK(h == 0);   /* loopPt: the first checkpoint with flag 4 */
        memcpy(&x, gCheckpoints + 12 + 0x50 + 4 * 4, 4);
        CHECK(x == 14.0f);
        memcpy(&cr, gCheckpoints + 12 + 0x50 * 3 + 0x18, 4);
        CHECK(cr == 503);
        memcpy(&x, gCheckpoints + 12 + 0x50 * 4 + 0x4C, 4);
        CHECK(x == 50.0f);
    }

    /* ReadPathData: the track's paths, the branches the game's own copy. */
    ReadPathData();
    CHECK(gResPath.numPts == PRIMARY && gResPath.numAlt == ALTS && gNumResPathPts == PRIMARY);
    CHECK((uint8_t *)gResPath.pts == f + res_at && gResPathPtList == gResPath.pts);
    CHECK(gResPath.pts[2][1] == -1000 - 7);
    CHECK(gResPath.alt == gAltPaths && gAltPaths != NULL && gAltPaths != r->alt);
    for (i = 0, pos = alt_at; i < ALTS; i++) {
        struct AltPath *a = &gResPath.alt[i];
        CHECK(a->resurrect == i + 1 && a->firstPath == -1 && a->firstIdx == 30 + i && a->numPts == k_alt_pts[i]);
        CHECK(a->lastPath == 2 + i && a->lastIdx == 77 + i && a->startSection == 9);
        CHECK((uint8_t *)a->pts == f + pos && a->pts[0][2] == 2000 + 100 * i + 2);
        pos += a->numPts * 6;
    }
    for (p = 0, pos = drone_at; p < 4; p++) {
        uint8_t *pt;
        CHECK(gDronePaths[p].numPts == k_drone_pts[p] && gNumPathPts[p] == k_drone_pts[p]);
        CHECK(gDronePaths[p].pad02 == 0x5A00 + p);
        CHECK((uint8_t *)gDronePaths[p].pts == f + pos + 8 && gPathPtList[p] == gDronePaths[p].pts);
        pt = (uint8_t *)gDronePaths[p].pts;
        for (i = 0; i < k_drone_pts[p]; i++) {
            for (k = 0; k < 3; k++)
                CHECK(gDronePaths[p].pts[i].pos[k] == -(300 * p + 10 * i + k));
            CHECK(gDronePaths[p].pts[i].mph == 60 + i && gDronePaths[p].pts[i].hints == -3);
            CHECK(pt[8 * i + 6] == 60 + i);
        }
        pos += 8 + 8 * (size_t)k_drone_pts[p];
    }

    /* Readers with their own strides over the native structs. */
    NextResPathPt(1, k_alt_pts[1] - 1, &a2, &a3);   /* the last point of alternative path 1 */
    CHECK(a2 == 3 && a3 == 78);
    NextResPathPt(1, 0, &a2, &a3);
    CHECK(a2 == 1 && a3 == 1);
    CHECK(PrevPathPt(0, 2) == k_drone_pts[2] - 1);
    CHECK(PrevPathPt(0, 1) == k_drone_pts[1] - 1);
    CHECK(NextPathPt(k_drone_pts[1] - 1, 1) == 0);   /* end of drone path 1: the loop checkpoint's entry */
    CHECK(NextPathPt(1, 1) == 2);

    /* What the game does to its copy of a branch stays there: ReadPathData copies the file's again. */
    gAltPaths[0].firstPath = 5;
    CHECK(r->alt[0].firstPath == -1);
    ReadPathData();
    CHECK(gAltPaths[0].firstPath == -1);

    /* The records go with the block. */
    pc_heap_block_freed(f, sizeof f);
    CHECK(pc_path_records(f)->resPath.numPts == 0);
    return TEST_DONE();
}
