/* CalcCheckpointTimes: the time each stretch between checkpoints takes, and the race's starting time (MAT3
 * pathcommon.c). */
#include "game/game.h"

f32 sqrtf(f32);

/* Times every stretch between checkpoints by driving drone path `path` (0) at its points' speeds: time[0], from
 * the starting grid's checkpoint round to the finish (the first lap), and time[1], from the finish round to the
 * finish again (the later laps). The race starts with the first stretch's time plus 10 seconds plus a thirtieth of
 * a lap's; the finish stretch's later-lap time gets that thirtieth too. */
void CalcCheckpointTimes(void) {
    s32 lap;
    s32 path;
    s32 cp;
    s32 next;
    s32 end;
    s32 i;
    s32 started;
    f32 d[3];
    f32 t;

    path = 0;
    for (lap = 0; lap < 2; lap++) {
        started = (gCheckpoints.startPt == 0 || lap != 0);
        if (lap == 0) {
            cp = gCheckpoints.startPt;
        } else {
            cp = gCheckpoints.finishPt;
        }
        for (;;) {
            next = GetNextCheckpoint(cp);
            if (next < cp) {
                end = gDronePaths[path].numPts - 1;
            } else {
                end = gCheckpoints.points[next].dronePathPt[path] - 1;
            }
            t = 0.0f;
            for (i = gCheckpoints.points[cp].dronePathPt[path]; i < end; i++) {
                d[0] = gDronePaths[path].pts[i + 1].pos[0] - gDronePaths[path].pts[i].pos[0];
                d[1] = gDronePaths[path].pts[i + 1].pos[1] - gDronePaths[path].pts[i].pos[1];
                d[2] = gDronePaths[path].pts[i + 1].pos[2] - gDronePaths[path].pts[i].pos[2];
                t += sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) / ((u32) gDronePaths[path].pts[i].mph * 1.4666667f);
            }
            /* the last step, onto the next checkpoint's point (round the loop after the path's end) */
            d[0] = gDronePaths[path].pts[NextPathPt(i, path)].pos[0] - gDronePaths[path].pts[i].pos[0];
            d[1] = gDronePaths[path].pts[NextPathPt(i, path)].pos[1] - gDronePaths[path].pts[i].pos[1];
            d[2] = gDronePaths[path].pts[NextPathPt(i, path)].pos[2] - gDronePaths[path].pts[i].pos[2];
            t += sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) / ((u32) gDronePaths[path].pts[i].mph * 1.4666667f);
            gCheckpoints.points[cp].time[lap] = t;
            cp = next;
            if (next == gCheckpoints.finishPt) {
                if (started) {
                    break;
                }
                started = 1;
            }
        }
    }
    gCheckpoints.startTime = gCheckpoints.points[0].time[0] + 10;
    cp = gCheckpoints.finishPt;
    t = 0.0f;
    do {
        t += gCheckpoints.points[cp].time[1];
        if (cp == gCheckpoints.numPoints - 1) {
            i = gCheckpoints.loopPt;
        } else {
            i = cp + 1;
        }
        cp = i;
    } while (cp != gCheckpoints.finishPt);
    gCheckpoints.startTime = gCheckpoints.startTime + t / 30.0f;
    gCheckpoints.points[gCheckpoints.finishPt].time[1] = gCheckpoints.points[gCheckpoints.finishPt].time[1] + t / 30.0f;
}
