/* MBOX_FindObject_WORK.c: find a model object by name (MAT3 mbox.c, MBOX_FindObject_WORK()).
 *
 * Every loaded model file (wave) has a table of object definitions sorted by name (gRomObjDefPtrs[model]); an
 * object is named by a packed handle, its model's number times 1024 plus its index in that table. */
#include "game/game.h"

extern char *strcpy(char *, const char *);

/* The handle of the object called `name` (compared on its first 15 characters, objcmp) in models firstModel to
 * lastModel (a negative bound means the first or the last model), searching only the models that are active
 * (ModelActive): (model << 10) | index, or -1 if none has it. A NULL or empty name looks for "AAANULLOBJ", the
 * models' placeholder object. `complain` (callers pass 1) asked for a warning when the object is missing; the
 * warning is compiled out. */
s32 MBOX_FindObject_WORK(char *name, s8 firstModel, s8 lastModel, s8 complain) {
    s32 i;
    tModelDef key;
    tModelDef *found;

    found = 0;
    if (name == 0 || *name == 0) {
        strcpy(key.name, "AAANULLOBJ");
    } else {
        game_strncpy(key.name, name, 16);
    }
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
        found = game_bsearch(&key, gRomObjDefPtrs[i].romObjDefs, gRomObjDefPtrs[i].count, sizeof(tModelDef), objcmp);
        if (found != 0) {
            break;
        }
    }
    if (found == 0) {
        if (complain) DEBUG_PRINT(("MBOX_FindObject: %s not found\n", key.name));
        return -1;
    }
    return (found - gRomObjDefPtrs[i].romObjDefs) | (i << 10);
}
