/* UnloadTrack: a track's files unloaded (MAT3 tracks.c). */
#include "game/game.h"

extern void *memset(void *s, s32 c, u32 n);

VIEW(intptr_t, WorldTable);
#define WorldTable WorldTable_view

VIEW(intptr_t, WorldBase);
#define WorldBase WorldBase_view

/* Unloads the track's waves (track, world, collision; the path file's stays), clears the sky and the world's
 * tables and every part's load state. `track` is not read. */
void UnloadTrack(s32 track) {
    TrackLoadInfo *info = &gTrackLoadInfo;

    WorldTable = 0;
    WorldBase = 0;
    SkyClear();
    UnloadWaveInstance(info->track.idx);
    UnloadWaveInstance(info->world.idx);
    UnloadWaveInstance(info->collision.idx);
    memset(&gTrackLoadInfo, 0, 0x2CU);
}
