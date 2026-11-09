/* IsTrackUnlocked: whether a track is unlocked for anyone playing (MAT3 save.c). */
#include "game/game.h"

VIEW(Ent19[], gUnlockedTracks);
#define gUnlockedTracks gUnlockedTracks_view

/* 1 when track idx is unlocked for any of the gNumPlayers players (gUnlockedTracks, a row of 19 tracks each). */
s32 IsTrackUnlocked(s32 idx) {
    s32 unlocked = 0;
    s32 i;
    for (i = 0; i < gNumPlayers; i++) {
        if (gUnlockedTracks[i].b[idx]) unlocked = 1;
    }
    return unlocked;
}
