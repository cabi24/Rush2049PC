#include "game/game.h"

typedef struct Table {
    char *base;
    u32 count;
} Table;

VIEW(Table[], gPalDefPtrs);
#define gPalDefPtrs gPalDefPtrs_view
VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

char *MBOX_FindPalette_WORK(char *name, u16 *out, s8 lo, s8 hi) {
    char key[28];
    s32 i;
    char *found = 0;

    game_strcpy(key, name);
    if (lo < 0) {
        lo = 0;
    }
    if (hi < 0 || hi >= gNumModels) {
        hi = gNumModels - 1;
    }
    for (i = lo; i <= hi; i++) {
        if (ModelActive(i) == 0) {
            continue;
        }
        found = game_bsearch(key, gPalDefPtrs[i].base, gPalDefPtrs[i].count, 24, texcmp);
        if (found != 0) {
            break;
        }
    }
    if (found == 0) {
        *out = 0;
        return 0;
    }
    *out = ((found - gPalDefPtrs[i].base) / 24) | (i << 10);
    return found;
}
