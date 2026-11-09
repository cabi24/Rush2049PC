/* objcmp.c: the comparator of the model object name search (MAT3 mbox.c, objcmp()).
 *
 * MBOX_FindObject_WORK searches each model's sorted object table with game_bsearch and this. */
#include "game/game.h"

/* Compare the object records obj1 and obj2 (tModelDef: the name first) on their names' first 15
 * characters; -1 if either is missing. */
s32 objcmp(u8 *obj1, u8 *obj2) {
    if ((obj1 == NULL) || (obj2 == NULL)) {
        return -1;
    }
    return game_strncmp(obj1, obj2, 0xF);
}
