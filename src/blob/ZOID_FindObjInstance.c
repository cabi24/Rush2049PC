/* ZOID_FindObjInstance.c: find a scene node by its object's name (MAT3 zoid.c, ZOID_FindObjInstance()). */
#include "game/game.h"

/* An object definition (MAT3 tModelDef, 88 bytes): its name first. */
typedef struct tModelDef { char name[16]; u8 rest[88 - 16]; } tModelDef;


/* The first node whose object (index: its handle, (model << 10) | definition) is called `name`, or -1. */
s16 ZOID_FindObjInstance(void *name) {
    s32 i;
    ObjectInstance *node = ObjInstanceList;
    for (i = 0; i < gNumObjects; i++, node++) {
        if (game_strcmp(name, gRomObjDefPtrs[node->index >> 10].romObjDefs[node->index & 0x3FF].name) == 0) {
            return i;
        }
    }
    return -1;
}
