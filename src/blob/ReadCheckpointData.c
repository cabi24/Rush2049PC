/* ReadCheckpointData: the track's checkpoints from its path file (MAT3 pathcommon.c). */
#include "game/game.h"

extern void *memcpy(void *, const void *, unsigned int);

/* Copies the checkpoints from the start of the track's path file (gPathWave, already in host byte order:
 * port/assets/path.c) into gCheckpoints, finds the finish line, the starting grid's checkpoint and the lap's first
 * checkpoint by their flags (checkpoint 0 for any the file doesn't flag), clears the paths' crossing points
 * (SyncCheckpointsToPaths finds them) and gives every stretch 45 seconds (CalcCheckpointTimes works them out) and the
 * race 90. */
void ReadCheckpointData(void)
{
    s16 i, j;
    memcpy(&gCheckpoints, gPathWave, sizeof(gCheckpoints));
    gCheckpoints.startPt = -1;
    gCheckpoints.loopPt = gCheckpoints.startPt;
    gCheckpoints.finishPt = gCheckpoints.loopPt;
    gCheckpoints.startTime = 90;
    for (i = 0; i < gCheckpoints.numPoints; i++) {
        /* MAT3 byte-swaps the gate's position and normal here; the N64's swap is nothing, its loop is left */
        for (j = 0; j < 3; j++) {}
        if (gCheckpoints.finishPt < 0 && (gCheckpoints.points[i].flags & CP_FLAG_FINISH))
            gCheckpoints.finishPt = i;
        if (gCheckpoints.loopPt < 0 && (gCheckpoints.points[i].flags & CP_FLAG_LOOP))
            gCheckpoints.loopPt = i;
        if (gCheckpoints.startPt < 0 && (gCheckpoints.points[i].flags & CP_FLAG_START))
            gCheckpoints.startPt = i;
        for (j = 0; j < 20; j++) gCheckpoints.points[i].pathPts[j] = 0;
        gCheckpoints.points[i].time[1] = 45;
        gCheckpoints.points[i].time[0] = gCheckpoints.points[i].time[1];
    }
    if (gCheckpoints.finishPt < 0) gCheckpoints.finishPt = 0;
    if (gCheckpoints.loopPt < 0) gCheckpoints.loopPt = 0;
    if (gCheckpoints.startPt < 0) gCheckpoints.startPt = 0;
}
