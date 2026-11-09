/* MBOX_GetObjectRadius.c: the bounding radius of a model object (MAT3 mbox.c, MBOX_GetObjectRadius()). */
#include "game/game.h"

/* The radius of object `handle` ((model << 10) | index, MBOX_FindObject_WORK), or 0 if the handle names no loaded
 * object. */
f32 MBOX_GetObjectRadius(s32 handle) {
    s32 model;
    s32 index;
    ModelDefTable *table;

    model = handle >> 10;
    if (model >= (s32) gNumModels[0]) {
        return 0.0f;
    }
    table = &gRomObjDefPtrs[model];
    index = handle & 0x3FF;
    if (index >= table->count) {
        return 0.0f;
    }
    return table->romObjDefs[index].radius;
}
