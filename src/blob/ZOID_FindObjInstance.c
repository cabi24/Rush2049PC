/* ZOID_FindObjInstance.c: find a scene node by its object's name (MAT3 zoid.c, ZOID_FindObjInstance()). */
#include "game/game.h"

/* One model's table of object definitions (MAT3 anon_romObjDefs_count_8): 88-byte records, name first. */
typedef struct ModelDefTable { u8 *romObjDefs; s32 count; } ModelDefTable;

VIEW(ModelDefTable[], gRomObjDefPtrs);
#define gRomObjDefPtrs gRomObjDefPtrs_view

/* The first node whose object (index: its handle, (model << 10) | definition) is called `name`, or -1. */
s16 ZOID_FindObjInstance(void *name) {
    s32 i;
    ObjectInstance *node = ObjInstanceList;
    for (i = 0; i < gNumObjects; i++, node++) {
        if (game_strcmp(name, gRomObjDefPtrs[node->index >> 10].romObjDefs + (node->index & 0x3FF) * 88) == 0) {
            return i;
        }
    }
    return -1;
}
