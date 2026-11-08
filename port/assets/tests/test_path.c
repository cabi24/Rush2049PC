/* port/assets/tests/test_path.c: the path converter (port/assets/path.c) on a synthetic big-endian path file, then
 * the game's own ReadCheckpointData (ReadCheckpointData) and PC ReadPathData (ReadPathData: native headers and
 * alternative paths), and three readers that index the results by hand-written strides (NextResPathPt,
 * NextPathPt, PrevPathPt).
 * game: src/blob/ReadCheckpointData.c src/blob/groups/pathcommon_read/ReadPathData.c
 * game: src/blob/NextResPathPt.c src/blob/NextPathPt.c src/blob/PrevPathPt.c
 */
#include <string.h>

#include "assets.h"
#include "support.h"

/* The native layouts the game's PC views share (ReadPathData's ResPath, AltPath, DronePath). */
struct ResPath {
    uint16_t numPrimary, unk02;
    int16_t (*pts)[3];
    uint8_t numAlt, unk09[3];
    struct AltPath *alt;
};
struct AltPath {
    uint8_t type, unk01[9];
    uint16_t numPts;
    int16_t (*pts)[3];
};
struct DronePath {
    uint16_t num, unk02;
    void *pts;
};

int8_t gRestartGame;
char *gPathWave;
struct ResPath gResPath;
struct AltPath *gAltPaths;
uint16_t gNumResPathPts;
void *gResPathPtList;
struct DronePath gDronePaths[4];
int16_t gNumPathPts[4];
void *gPathPtList[4];
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
    static uint8_t buf[0x800];
    uint8_t *w = test_rdram(), *f = w ? w + 0x500000 : buf;
    size_t res_at, alt_at, drone_at, size, pos;
    int32_t a2 = 0, a3 = 0;
    float x;
    int i, k, p;

    size = make_path(f, &res_at, &alt_at, &drone_at);
    CHECK(drone_at % 4 == 2);
    pc_assets_path(158, f, size - 1);   /* one byte short: left as it is */
    CHECK(pc_be16(f + 8) == 2 && pc_be16(f + 0x32C) == PRIMARY);
    pc_assets_path(158, f, size);

    /* ReadCheckpointData copies the block into gCheckpoints. */
    gPathWave = (char *)f;
    ReadCheckpointData();
    {
        int16_t h;
        int32_t r;
        memcpy(&h, gCheckpoints + 8, 2);
        CHECK(h == 2);
        memcpy(&h, gCheckpoints + 2, 2);
        CHECK(h == 0);   /* loopPt: the first checkpoint with flag 4 */
        memcpy(&x, gCheckpoints + 12 + 0x50 + 4 * 4, 4);
        CHECK(x == 14.0f);
        memcpy(&r, gCheckpoints + 12 + 0x50 * 3 + 0x18, 4);
        CHECK(r == 503);
        memcpy(&x, gCheckpoints + 12 + 0x50 * 4 + 0x4C, 4);
        CHECK(x == 50.0f);
    }

    /* ReadPathData. */
    ReadPathData();
    CHECK(gResPath.numPrimary == PRIMARY && gResPath.numAlt == ALTS && gNumResPathPts == PRIMARY);
    CHECK((uint8_t *)gResPath.pts == f + res_at && gResPathPtList == gResPath.pts);
    CHECK(gResPath.pts[2][1] == -1000 - 7);
    CHECK(gResPath.alt == gAltPaths && gAltPaths != NULL);
    for (i = 0, pos = alt_at; i < ALTS; i++) {
        struct AltPath *a = &gResPath.alt[i];
        CHECK(a->type == i + 1 && (int8_t)a->unk01[0] == -1 && a->numPts == k_alt_pts[i]);
        CHECK(a->unk01[3] == 2 + i && a->unk01[7] == 9);
        CHECK((uint8_t *)a->pts == f + pos && a->pts[0][2] == 2000 + 100 * i + 2);
        pos += a->numPts * 6;
    }
    for (p = 0, pos = drone_at; p < 4; p++) {
        uint8_t *pt;
        CHECK(gDronePaths[p].num == k_drone_pts[p] && gNumPathPts[p] == k_drone_pts[p]);
        CHECK((uint8_t *)gDronePaths[p].pts == f + pos + 8 && gPathPtList[p] == gDronePaths[p].pts);
        pt = (uint8_t *)gDronePaths[p].pts;
        for (i = 0; i < k_drone_pts[p]; i++)
            for (k = 0; k < 3; k++) {
                int16_t h;
                memcpy(&h, pt + 8 * i + 2 * k, 2);
                CHECK(h == -(300 * p + 10 * i + k));
                CHECK(pt[8 * i + 6] == 60 + i && (int8_t)pt[8 * i + 7] == -3);
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
    return TEST_DONE();
}
