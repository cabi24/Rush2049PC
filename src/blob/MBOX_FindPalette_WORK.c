/* MBOX_FindPalette_WORK.c: find a palette by name in the loaded model files (the N64's companion of MAT3 mbox.c's
 * MBOX_FindTexture_WORK; MAT3 has no palettes).
 *
 * Each model file has a table of 24-byte palette definitions sorted by name (gPalDefPtrs[model]); a palette is
 * named by a packed handle, its model's number times 1024 plus its index in that table. */
#include "game/game.h"
/* The palette called `name` (texcmp: the whole name) in models firstModel to lastModel (a negative bound means the
 * first or the last model), searching only the active models: returns its definition (a tPalette) and writes its
 * handle, (model << 10) | index, to *handle; NULL and handle 0 if none has it. */
tPalette *MBOX_FindPalette_WORK(char *name, u16 *handle, s8 firstModel, s8 lastModel) {
    char key[28];
    s32 i;
    char *found = 0;

    game_strcpy(key, name);
    if (firstModel < 0) {
        firstModel = 0;
    }
    if (lastModel < 0 || lastModel >= gNumModels[0]) {
        lastModel = gNumModels[0] - 1;
    }
    for (i = firstModel; i <= lastModel; i++) {
        if (ModelActive(i) == 0) {
            continue;
        }
        found = game_bsearch(key, gPalDefPtrs[i].palDefs, gPalDefPtrs[i].count, 24, texcmp);
        if (found != 0) {
            break;
        }
    }
    if (found == 0) {
        *handle = 0;
        return 0;
    }
    *handle = ((found - (char *)gPalDefPtrs[i].palDefs) / 24) | (i << 10);   /* 24-byte records, name first */
    return (tPalette *) found;
}
