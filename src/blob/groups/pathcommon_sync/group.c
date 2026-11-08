/* flags: -g0 -O3 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
/*
 * Group route_cross (installed),
 * routes/unsigned_above.c as scored.
 * SyncCheckpointsToPaths: SyncCheckpointsToPaths @ 0x800BA7C4, 732 native bytes.
 * FindAltPathIntersect: FindAltPathIntersect @ 0x800BA46C, 432 native bytes;
 * provisional internal 108/108, reverified in this group.
 * Helper names: the PR's private gc_lane_FindPathDistances is named
 * FindPathDistances after its GC counterpart; gc_lane_GetNextCheckpoint is
 * route_next_checkpoint_u, because the locked GetNextCheckpoint is N64's
 * GetNextCheckpoint with a different (index+1 == count) body. Both are static
 * and inlined (no symbols).
 *
 * GC DWARF supplies unsigned GetNextCheckpoint(unsigned int) and a static
 * FindPathDistances(void) boundary. These are hypotheses about the N64 source.
 * No GC endian swaps or GC record layouts are transplanted. The exact N64
 * Route/Tracker views and FindAltPathIntersect body are preserved.
 *
 * Shaping disclosure: the baseline's unused s32 pad[2] is removed; actual
 * source helpers now represent the proposed inline nesting.
 * (2026-10-14): TWO UNUSED s32 LOCALS (unused1, unused2) are the exact
 * frame residual: retail frame 80; without them 72 (a0 home at sp+72, 6 words
 * differ, everything else identical); 2 slots = 8 B. One unused local gives
 * 3 words differ. Disclosed per the owner's unused-locals rule; no pad array. pos[3] and its assignments are retained
 * from as an unproved N64 source feature (absent from GC's synchronizer).
 * The partial Tracker.idx[17] view is inherited; physical checkpoint records
 * carry 20 slots, as loops already use. Retyping it would be another
 * experiment and is deliberately not mixed into this boundary test.
 *
 * The locked GetNextCheckpoint definition/prototype is never changed or included;
 * route_next_checkpoint_u is not a stub identity (no retail stub is claimed).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef float f32;

typedef struct {
    s16 x;
    s16 y;
    s16 z;
} RoutePoint;

typedef struct {
    u8 type;
    char pad1[9];
    u16 num;
    RoutePoint *pts;
} Route;

/* PC: gResPath and gDronePaths are native structs and the alternative paths a native array (ReadPathData), so these views take host offsets and strides */
typedef struct {
    char pad0[16];
    u8 count;
    char pad9[3];
    Route *routes;
} RouteSet;

typedef struct {
    s16 unk0;
    s16 first;
    s16 last;
    s16 unk6;
    s16 count;
    s16 unkA;
    f32 x;
    f32 y;
    f32 z;
    f32 dx;
    char pad1C[4];
    f32 dz;
    s32 range;
    char pad28[6];
    s16 idx[17];
} Tracker;

typedef struct {
    char pad0[0x58];
    f32 len;
} TrackerLen;

extern RouteSet gResPath;
extern Tracker gCheckpoints[];

s32 FindAltPathIntersect(s16 who, s16 route) {
    s16 i;
    s16 prev;
    s16 side;
    s16 first;
    f32 d[2];
    f32 dir[2];
    Route *r;

    first = 1;
    if (route >= gResPath.count) {
        return -1;
    }
    r = (Route *)((u8 *)gResPath.routes + route * sizeof(Route));
    if (r->type == 2) {
        if (who == 0) {
            return 0;
        }
        return -1;
    }
    dir[0] = gCheckpoints[who].dx;
    dir[1] = gCheckpoints[who].dz;
    for (i = 0; i < r->num; i++) {
        d[0] = r->pts[i].x - gCheckpoints[who].x;
        d[1] = r->pts[i].z - gCheckpoints[who].z;
        if (d[0] * d[0] + d[1] * d[1] <= gCheckpoints[who].range) {
            if (d[0] * dir[0] + d[1] * dir[1] < 0.0f) {
                side = -1;
            } else {
                side = 1;
            }
            if (first) {
                prev = side;
                first = 0;
            } else if (prev != side) {
                return i;
            }
        }
    }
    return -1;
}

extern f32 gFirstLapDistance;
extern f32 gLapDistance;
s16 FindResPathIntersect(s16 who);
s16 FindDronePathIntersect(s16 who, s16 path);
f32 GetPathDistance(s32 from, s32 to);

#define LEN(n) (((TrackerLen *)&gCheckpoints[n])->len)

static void FindPathDistances(void);

void SyncCheckpointsToPaths(s16 mode) {
    s32 unused1;
    s32 unused2;
    s16 pos[3];
    s32 i;
    s32 j;

    for (i = 0; i < gCheckpoints[0].count; i++) {
        pos[0] = gCheckpoints[i].x;
        pos[1] = gCheckpoints[i].y;
        pos[2] = gCheckpoints[i].z;
        if (mode < 0) {
            for (j = 0; j < 20; j++) {
                if (j == 0) {
                    gCheckpoints[i].idx[j] = FindResPathIntersect(i);
                } else if (j >= 5) {
                    gCheckpoints[i].idx[j] = FindAltPathIntersect(i, j - 5);
                } else {
                    gCheckpoints[i].idx[j] = FindDronePathIntersect(i, j - 1);
                }
            }
        } else if (mode == 0) {
            gCheckpoints[i].idx[mode] = FindResPathIntersect(i);
            for (j = 0; j < gResPath.count; j++) {
                gCheckpoints[i].idx[j + 5] = FindAltPathIntersect(i, j);
            }
        } else {
            gCheckpoints[i].idx[mode] = FindDronePathIntersect(i, mode - 1);
        }
    }
    FindPathDistances();
}

static unsigned int route_next_checkpoint_u(unsigned int cp) {
    if (cp == gCheckpoints[0].count - 1) {
        return gCheckpoints[0].first;
    }
    return cp + 1U;
}

static void FindPathDistances(void) {
    s32 k;
    s32 next;
    s32 done;

    gFirstLapDistance = 0.0f;
    gLapDistance = gFirstLapDistance;
    done = 0;
    for (k = 0; k < gCheckpoints[0].count; k++) {
        next = route_next_checkpoint_u(k);
        LEN(k) = GetPathDistance(gCheckpoints[k].idx[0], gCheckpoints[next].idx[0]);
        if (k > 0 && k == gCheckpoints[0].last) {
            done = 1;
        }
        if (!done) {
            gFirstLapDistance += LEN(k);
        }
        if (k >= gCheckpoints[0].first) {
            gLapDistance += LEN(k);
        }
    }
}
