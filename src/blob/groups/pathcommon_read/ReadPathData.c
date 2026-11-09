/* ReadPathData: the track's paths from its path file (MAT3 pathcommon.c). */
#include "game/game.h"

extern void *memcpy(void *, const void *, unsigned int);

/* The alternative paths, in host layout (ReadPathData). */
static AltPath pc_alt_paths[256];

/* Reads the resurrection path, its branches and the four drone paths from the track's path file (gPathWave; the
 * checkpoints come first, ReadCheckpointData), then finds where each path crosses each checkpoint, joins the
 * branches up, and works out the track's bounds and the checkpoint times. On a restart of the same track the paths
 * are still loaded, and only the last steps are done again.
 *
 * The file, after the checkpoints: the resurrection path's header (16 bytes), the branches' headers (16 bytes
 * each), the count of all the resurrection points (u16), the main path's points then each branch's (6 bytes each),
 * and each drone path's header (8 bytes) and points (8 bytes each). PC: the file is in host byte order
 * (port/assets/path.c), but its headers hold 4-byte pointer slots, so they are copied field by field into the
 * host-layout gResPath, gDronePaths and pc_alt_paths, and the pointers set here (docs/port/ASSETS.md 4 #22). */
void ReadPathData(void)
{
    s16 i;
    s16 p;
    char *ptr;

    if (gRestartGame) {
        CalcCheckpointTimes();
        SyncCheckpointsToPaths(-1);
        SyncAltPaths();
        CalcResPathBounds();
        CalcCheckpointTimes();
        return;
    }
    ptr = (char *) gPathWave + sizeof(gCheckpoints);
    memcpy(&gResPath, ptr, 4);                          /* numPts and the padding after it */
    memcpy(&gResPath.numAlt, ptr + 8, 4);               /* numAlt and the padding after it */
    ptr += 0x10;
    gAltPaths = pc_alt_paths;
    for (i = 0; i < gResPath.numAlt; i++) {
        memcpy(&pc_alt_paths[i], ptr, 12);              /* everything before pts */
        ptr += 16;
    }
    memcpy(&gNumResPathPts, ptr, 2);
    ptr += 2;
    gResPathPtList = (s16 (*)[3]) ptr;
    ptr += gResPath.numPts * 6;
    gResPath.pts = gResPathPtList;
    if (gResPath.numAlt > 0) {
        gResPath.alt = gAltPaths;
    } else {
        gResPath.alt = gAltPaths = 0;
    }
    for (i = 0; i < gResPath.numAlt; i++) {
        gAltPaths[i].pts = (s16 (*)[3]) ptr;
        ptr += gAltPaths[i].numPts * 6;
    }
    for (p = 0; p < 4; p++) {
        memcpy(&gDronePaths[p], ptr, 4);                /* numPts and the padding after it */
        gNumPathPts[p] = gDronePaths[p].numPts;
        ptr += 8;
        gPathPtList[p] = (DronePathPt *) ptr;
        gDronePaths[p].pts = (DronePathPt *) ptr;
        ptr += gDronePaths[p].numPts * 8;
    }
    SyncCheckpointsToPaths(-1);
    SyncAltPaths();
    CalcResPathBounds();
    CalcCheckpointTimes();
}
