#include "game/game.h"

extern void *memset(void *s, s32 c, u32 n);

VIEW(s8, gTrackLoadInfo);
#define gTrackLoadInfo gTrackLoadInfo_view

VIEW(intptr_t, WorldTable);
#define WorldTable WorldTable_view

VIEW(intptr_t, WorldBase);
#define WorldBase WorldBase_view

void UnloadTrack(s32 arg0) {
    /* gTrackLoadInfo, loadtrack.c's TrackLoadInfo: each part's load state, idx its wave instance. */
    typedef struct { s8 loading, loaded, inited, ready; s32 idx; } LoadState;
    struct { s8 trackInited; LoadState track, world, collision, path, other; } *info = (void *) &gTrackLoadInfo;

    WorldTable = 0;
    WorldBase = 0;
    SkyClear();
    UnloadWaveInstance(info->track.idx);
    UnloadWaveInstance(info->world.idx);
    UnloadWaveInstance(info->collision.idx);
    memset(&gTrackLoadInfo, 0, 0x2CU);
}
