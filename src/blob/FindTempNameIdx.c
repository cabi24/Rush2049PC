#include "game/game.h"

VIEW(u8[20][13], gTempNames);
#define gTempNames gTempNames_view
VIEW(u16[20], gTempNameAges);
#define gTempNameAges gTempNameAges_view
VIEW(intptr_t[12][3][5], gTotalNames);
#define gTotalNames gTotalNames_view

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

