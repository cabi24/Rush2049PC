/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
/*
 * Historical label GetPathDistance is misleading: this returns the
 * horizontal path length between two path-point indices of the track graph
 * gResPath (u16 count, 6-byte s16[3] points), walking successors with
 * NextResPathPt. When `to` is behind `from` the walk runs to the end of the
 * path and continues from the loop point gCheckpoints[gCheckpoints[0].unk2].unk2E;
 * it returns -1 when `to` lies before that loop point. Arcade relative: the
 * path_dist accumulation in reference/repos/rushtherock/game/scp.c.
 *
 * The y delta is computed and then overwritten with 0 (retail stores both).
 * No compile-shaping quirks: only the declaration order of the locals
 * (which fixes the stack offsets) was chosen to fit the frame.
 */
typedef float f32;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef signed char s8;

typedef struct {
    s16 pos[3];
} PathPoint;

typedef struct {
    u16 count;
    PathPoint *points;
} PathGraph;

typedef struct {
    s16 unk0;
    s16 unk2;
    char pad4[0x2A];
    s16 unk2E;
    char pad30[0x20];
} Track; /* 0x50 */

extern PathGraph gResPath;
extern Track gCheckpoints[];

extern f32 sqrtf(f32);
#pragma intrinsic(sqrtf)
void NextResPathPt(s32 arg0, s32 arg1, s32 *arg2, s32 *arg3);

f32 GetPathDistance(s32 from, s32 to) {
    s32 i;
    s32 next;
    s32 end;
    f32 dist;
    f32 d[3];

    if (to < from && to < gCheckpoints[gCheckpoints[0].unk2].unk2E) {
        return -1.0f;
    }
    if (to < from) {
        end = gResPath.count;
    } else {
        end = to;
    }
    dist = 0.0f;
    for (i = from; i < end; i++) {
        NextResPathPt(-1, i, 0, &next);
        d[0] = gResPath.points[next].pos[0] - gResPath.points[i].pos[0];
        d[1] = gResPath.points[next].pos[1] - gResPath.points[i].pos[1];
        d[2] = gResPath.points[next].pos[2] - gResPath.points[i].pos[2];
        d[1] = 0.0f;
        dist += sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    }
    if (end != to) {
        for (i = gCheckpoints[gCheckpoints[0].unk2].unk2E; i < to; i++) {
            NextResPathPt(-1, i, 0, &next);
            d[0] = gResPath.points[next].pos[0] - gResPath.points[i].pos[0];
            d[1] = gResPath.points[next].pos[1] - gResPath.points[i].pos[1];
            d[2] = gResPath.points[next].pos[2] - gResPath.points[i].pos[2];
            d[1] = 0.0f;
            dist += sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        }
    }
    return dist;
}
