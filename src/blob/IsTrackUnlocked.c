#include "game/game.h"

VIEW(Ent19[], gUnlockedTracks);
#define gUnlockedTracks gUnlockedTracks_view
s32 IsTrackUnlocked(s32 idx) {
    s32 r = 0;
    s32 i;
    for (i = 0; i < gNumPlayers; i++) {
        if (gUnlockedTracks[i].b[idx]) r = 1;
    }
    return r;
}
