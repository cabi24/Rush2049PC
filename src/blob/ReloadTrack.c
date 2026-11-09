#include "game/game.h"

VIEW(s8, gTrackLoadInfo);
#define gTrackLoadInfo gTrackLoadInfo_view

void ReloadTrack(void) {
    gTrackLoadInfo = 0;
    LoadTrack((s32) gTrackNum, 0, 1);
}
