#include "game/game.h"

typedef struct Route {
    /* 0x00 */ s16 total;     /* record 0: lap time */
    /* 0x02 */ s16 first;     /* record 0: section the ring wraps to */
    /* 0x04 */ s16 last;      /* record 0: last (finish) section */
    /* 0x06 */ s16 start;     /* record 0: first section of the race */
    /* 0x08 */ s16 count;     /* record 0: number of sections */
    /* 0x0A */ char padA[0x20];
    /* 0x2A */ s16 time[2];
    /* 0x2E */ s16 pad2E;
    /* 0x30 */ s16 pt_start[16];
} Route; /* 0x50 */

typedef struct PathPt {
    /* 0x00 */ s16 pos[3];
    /* 0x06 */ u8 speed;
    /* 0x07 */ u8 unk7;
} PathPt; /* 0x08 */


VIEW(Route[], gCheckpoints);
#define gCheckpoints (gCheckpoints_view[0])
typedef struct {
    /* 0x00 */ u16 num;
    /* 0x04 */ PathPt *pts;
} PathSet; /* 0x08 */

VIEW(PathSet[], gDronePaths);
#define gDronePaths gDronePaths_view

f32 sqrtf(f32);
#pragma intrinsic (sqrtf)

void CalcCheckpointTimes(void) {
    s32 p;
    s32 path;
    s32 cur;
    s32 next;
    s32 end;
    s32 i;
    s32 done;
    f32 d[3];
    f32 t;

    path = 0;
    for (p = 0; p < 2; p++) {
        done = (gCheckpoints.start == 0 || p != 0);
        if (p == 0) {
            cur = gCheckpoints.start;
        } else {
            cur = gCheckpoints.last;
        }
        for (;;) {
            next = GetNextCheckpoint(cur);
            do {} while (0);
            if (next < cur) {
                end = gDronePaths[path].num - 1;
            } else {
                end = (&gCheckpoints)[next].pt_start[path] - 1;
            }
            t = 0.0f;
            for (i = (&gCheckpoints)[cur].pt_start[path]; i < end; i++) {
                d[0] = gDronePaths[path].pts[i + 1].pos[0] - gDronePaths[path].pts[i].pos[0];
                d[1] = gDronePaths[path].pts[i + 1].pos[1] - gDronePaths[path].pts[i].pos[1];
                d[2] = gDronePaths[path].pts[i + 1].pos[2] - gDronePaths[path].pts[i].pos[2];
                t += sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) / ((u32) gDronePaths[path].pts[i].speed * 1.4666667f);
            }
            d[0] = gDronePaths[path].pts[NextPathPt(i, path)].pos[0] - gDronePaths[path].pts[i].pos[0];
            d[1] = gDronePaths[path].pts[NextPathPt(i, path)].pos[1] - gDronePaths[path].pts[i].pos[1];
            d[2] = gDronePaths[path].pts[NextPathPt(i, path)].pos[2] - gDronePaths[path].pts[i].pos[2];
            t += sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) / ((u32) gDronePaths[path].pts[i].speed * 1.4666667f);
            (&gCheckpoints)[cur].time[p] = t;
            do {} while (0);
            cur = next;
            if (next == gCheckpoints.last) {
                if (done) {
                    break;
                }
                done = 1;
            }
        }
    }
    gCheckpoints.total = gCheckpoints.time[0] + 10;
    cur = gCheckpoints.last;
    t = 0.0f;
    do {
        t += (&gCheckpoints)[cur].time[1];
        if (cur == gCheckpoints.count - 1) {
            i = gCheckpoints.first;
        } else {
            i = cur + 1;
        }
        cur = i;
    } while (cur != gCheckpoints.last);
    gCheckpoints.total = gCheckpoints.total + t / 30.0f;
    (&gCheckpoints)[gCheckpoints.last].time[1] = (&gCheckpoints)[gCheckpoints.last].time[1] + t / 30.0f;
}
