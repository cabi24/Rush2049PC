/* MBOX_GetObjectRadius.c: the bounding radius of a model object (MAT3 mbox.c, MBOX_GetObjectRadius()). */
#include "game/game.h"

/* An object definition (MAT3 tModelDef), 88 bytes in the N64 layout the model files keep: the name, the radius of
 * the sphere that bounds the object, its levels of detail. */
typedef struct tModelDef {
    char name[16];
    f32 radius;                 /* +0x10 */
    u8 lods[88 - 0x14];
} tModelDef;

/* The radius of object `handle` ((model << 10) | index, MBOX_FindObject_WORK), or 0 if the handle names no loaded
 * object. */
f32 MBOX_GetObjectRadius(s32 handle) {
    s32 model;
    s32 index;
    ModelDefTable *table;

    model = handle >> 10;
    if (model >= (s32) *(volatile u8 *) gNumModels) {
        return 0.0f;
    }
    table = &gRomObjDefPtrs[model];
    index = handle & 0x3FF;
    if (index >= table->count) {
        return 0.0f;
    }
    return table->romObjDefs[index].radius;
}
