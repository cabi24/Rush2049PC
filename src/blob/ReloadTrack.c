/* ReloadTrack: the track set up again (MAT3 tracks.c). */
#include "game/game.h"

/* The track's load state, its first byte (trackInited). */
VIEW(s8, gTrackLoadInfo);
#define gTrackLoadInfo gTrackLoadInfo_view

/* Has the loaded track set up again (LoadTrack's set-up, without loading the cars): its files stay loaded. */
void ReloadTrack(void) {
    gTrackLoadInfo = 0;
    LoadTrack((s32) gTrackNum, 0, 1);
}
