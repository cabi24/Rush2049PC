/* ReadPathData: the track's paths from its path file (MAT3 pathcommon.c). */
#include "game/game.h"

/* The alternative paths (gAltPaths): the game's copy of the file's, which SyncAltPaths joins up. */
static AltPath pc_alt_paths[256];

/* Reads the resurrection path, its branches and the four drone paths from the track's path file (gPathWave; the
 * checkpoints come first, ReadCheckpointData), then finds where each path crosses each checkpoint, joins the
 * branches up, and works out the track's bounds and the checkpoint times. On a restart of the same track the paths
 * are still loaded, and only the last steps are done again.
 *
 * The file, after the checkpoints: the resurrection path's header, the branches' headers, the count of all the
 * resurrection points, the main path's points then each branch's, and each drone path's header and points. MAT3
 * copies the headers out of the file and points them at their points, the branches' in the file itself; on the PC
 * the file's converter has decoded the headers beside it when it loaded (port/assets/path.c), with host pointers to
 * the points, and they are copied from there (docs/port/ASSETS.md 4 #22). */
void ReadPathData(void)
{
    const struct pc_path_records *file;
    s16 i;
    s16 p;

    if (gRestartGame) {
        CalcCheckpointTimes();
        SyncCheckpointsToPaths(-1);
        SyncAltPaths();
        CalcResPathBounds();
        CalcCheckpointTimes();
        return;
    }
    file = pc_path_records(gPathWave);
    gResPath = file->resPath;
    gAltPaths = pc_alt_paths;
    for (i = 0; i < gResPath.numAlt; i++) {
        pc_alt_paths[i] = file->alt[i];
    }
    gNumResPathPts = file->numResPathPts;
    gResPathPtList = gResPath.pts;
    if (gResPath.numAlt > 0) {
        gResPath.alt = gAltPaths;
    } else {
        gResPath.alt = gAltPaths = 0;
    }
    for (p = 0; p < 4; p++) {
        gDronePaths[p] = file->dronePaths[p];
        gNumPathPts[p] = gDronePaths[p].numPts;
        gPathPtList[p] = gDronePaths[p].pts;
    }
    SyncCheckpointsToPaths(-1);
    SyncAltPaths();
    CalcResPathBounds();
    CalcCheckpointTimes();
}
