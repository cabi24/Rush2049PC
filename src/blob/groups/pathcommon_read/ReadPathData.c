#include "game/game.h"

typedef s16 Vertex[3];
typedef struct AltPath {
    u8 type;
    u8 unk01[9];
    u16 numPts;
    Vertex *pts;
} AltPath;
typedef struct ResPath {
    u16 numPrimary;
    u16 unk02;
    Vertex *pts;
    u8 numAlt;
    u8 unk09[3];
    AltPath *alt;
} ResPath;
typedef struct DronePath {
    u16 num;
    u16 unk02;
    void *pts;
} DronePath;

VIEW(char *, gPathWave);
#define gPathWave gPathWave_view
VIEW(ResPath, gResPath);
#define gResPath gResPath_view

VIEW(DronePath[4], gDronePaths);
#define gDronePaths gDronePaths_view
VIEW(s16[4], gNumPathPts);
#define gNumPathPts gNumPathPts_view
VIEW(void *[4], gPathPtList);
#define gPathPtList gPathPtList_view
extern void *memcpy(void *, const void *, unsigned int);

/* PC: the file is in host order (port/assets/path.c), but its headers have 4-byte pointer slots. The resurrection
 * and drone path headers are copied field by field into the native gResPath and gDronePaths, and the alternative
 * paths, whose pts this sets and every reader reads with the native stride, are kept in a native array (numAlt is a
 * u8) instead of in the file (docs/port/ASSETS.md §4 #22). */
static AltPath pc_alt_paths[256];

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
    ptr = gPathWave + 0x32C;
    memcpy(&gResPath, ptr, 4);                        /* numPrimary, unk02 */
    memcpy(&gResPath.numAlt, ptr + 8, 4);             /* numAlt, unk09 */
    ptr += 0x10;
    gAltPaths = pc_alt_paths;
    for (i = 0; i < gResPath.numAlt; i++) {
        memcpy(&pc_alt_paths[i], ptr, 12);              /* everything before the pts slot */
        ptr += 16;
    }
    memcpy(&gNumResPathPts, ptr, 2);
    ptr += 2;
    gResPathPtList = (Vertex *)ptr;
    ptr += gResPath.numPrimary * 6;
    gResPath.pts = gResPathPtList;
    if (gResPath.numAlt > 0) {
        gResPath.alt = gAltPaths;
    } else {
        gResPath.alt = gAltPaths = 0;
    }
    for (i = 0; i < gResPath.numAlt; i++) {
        gAltPaths[i].pts = (Vertex *)ptr;
        ptr += gAltPaths[i].numPts * 6;
    }
    for (p = 0; p < 4; p++) {
        memcpy(&gDronePaths[p], ptr, 4);                 /* num, unk02 */
        gNumPathPts[p] = gDronePaths[p].num;
        ptr += 8;
        gPathPtList[p] = ptr;
        gDronePaths[p].pts = ptr;
        ptr += gDronePaths[p].num * 8;
    }
    SyncCheckpointsToPaths(-1);
    SyncAltPaths();
    CalcResPathBounds();
    CalcCheckpointTimes();
}
