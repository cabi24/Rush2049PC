#include "game/game.h"

typedef struct NameEntry {
    char name[36];
} NameEntry;

typedef struct NameTable {
    NameEntry *base;
    u32 count;
} NameTable;

#define MBOX_NOERR 0
#define MBOX_WARN 1
#define MBOX_FATAL 2
#define WARN_MSG(s)

VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view
VIEW(NameTable[], gTexDefPtrs);
#define gTexDefPtrs gTexDefPtrs_view

NameEntry *MBOX_FindTexture_WORK(char *name, s16 *out, s8 lo, s8 hi, s32 err) {
    s32 i;
    char buf[36];
    NameEntry *found;

    found = 0;
    game_strcpy(buf, name);
    if (lo < 0) {
        lo = 0;
    }
    if (hi < 0 || hi >= gNumModels) {
        hi = gNumModels - 1;
    }
    for (i = lo; i <= hi; i++) {
        if (!ModelActive(i)) {
            continue;
        }
        found = game_bsearch(buf, gTexDefPtrs[i].base, gTexDefPtrs[i].count, 36, texcmp);
        if (found != 0) {
            break;
        }
    }
    if (found == 0) {
        if (err == MBOX_WARN) {
            WARN_MSG(buf);
        } else if (err == MBOX_FATAL) {
            WARN_MSG(buf);
        }
        *out = 0;
        return 0;
    }
    *out = (found - gTexDefPtrs[i].base) | (i << 10);
    return found;
}
