#include "game/game.h"

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 key; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } Ent;
typedef struct { u8 *base; s32 x; } Tab;

VIEW(Ent[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view
VIEW(Tab[], gRomObjDefPtrs);
#define gRomObjDefPtrs gRomObjDefPtrs_view

s16 ZOID_FindObjInstance(void *a) {
    s32 i;
    Ent *e = ObjInstanceList;
    for (i = 0; i < gNumObjects; i++, e++) {
        if (game_strcmp(a, gRomObjDefPtrs[e->key >> 10].base + (e->key & 0x3FF) * 88) == 0) {
            return i;
        }
    }
    return -1;
}
