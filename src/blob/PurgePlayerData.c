/* PurgePlayerData: drop a profile's file data from memory (MAT3 save.c). */
#include "game/game.h"

/* Frees the profile's file data (PS_PurgeFile), which ReadPlayerData reads again when needed; nothing when it is not
 * read or the profile has no file. */
void PurgePlayerData(tProfile **profile) {
    tProfile *p;
    tPakHandle *fileh;

    p = *profile;
    if (p->datah != NULL) {
        fileh = p->fileh;
        if (fileh != NULL) {
            PS_PurgeFile(fileh);
            p->datah = 0;
        }
    }
}
