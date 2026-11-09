/* FindTempNameIdx: a slot for a name typed on the high score screen (MAT3 gamerecords.c). */
#include "game/game.h"

VIEW(intptr_t[12][3][5], gTotalNames);
#define gTotalNames gTotalNames_view

/* The index of name in the 20 names typed for the high score tables without a profile (gTempNames), which the
 * tables (gTotalNames) refer to by that index: the slot already holding it, else an empty slot, else one no table
 * entry refers to, else the oldest (gTempNameAges counts the calls since each was used), whose entries are then
 * cleared. The slot's age becomes 0. */
s16 FindTempNameIdx(u8 *name)
{
    s16 i, j, k;
    s16 references[20];
    s16 oldest, best;
    intptr_t slot;
    u32 victim;

    for (i = 0; i < 20; i++) {
        gTempNameAges[i]++;
    }
    for (i = 0; i < 20; i++) {
        if (game_strcmp(gTempNames[i], name) == 0) {
            gTempNameAges[i] = 0;
            return i;
        }
    }
    for (i = 0; i < 20; i++) {
        if (gTempNames[i][0] == 0) {
            game_strcpy(gTempNames[i], name);
            gTempNameAges[i] = 0;
            return i;
        }
    }
    for (i = 0; i < 20; i++) {
        references[i] = 0;
    }
    for (j = 0; j < 12; j++) {
        for (k = 0; k < 3; k++) {
            for (i = 0; i < 5; i++) {
                slot = gTotalNames[j][k][i];
                if (slot >= 0 && slot < 20) {
                    references[slot]++;
                }
            }
        }
    }
    for (i = 0; i < 20; i++) {
        if (references[i] == 0) {
            game_strcpy(gTempNames[i], name);
            gTempNameAges[i] = 0;
            return i;
        }
    }
    oldest = 0;
    for (i = 0; i < 20; i++) {
        if (oldest < gTempNameAges[i]) {
            oldest = gTempNameAges[i];
            best = i;
        }
    }
    victim = best;
    for (j = 0; j < 12; j++) {
        for (k = 0; k < 3; k++) {
            for (i = 0; i < 5; i++) {
                if (gTotalNames[j][k][i] == victim) {
                    gTotalNames[j][k][i] = -1;
                }
            }
        }
    }
    game_strcpy(gTempNames[best], name);
    gTempNameAges[best] = 0;
    return best;
}

