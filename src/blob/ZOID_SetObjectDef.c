#include "game/game.h"

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct IndexedRecord44 {
    unsigned int flags, w4;
    void *obj;
    float scale, w;
    u16 value;
    s16 child, next, mode;
    void *light, *tex[4], *rect, *clip, *pal;
    unsigned int prim, env;
} IndexedRecord44;

VIEW(IndexedRecord44[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

void ZOID_SetObjectDef(s16 index, u16 value) {
    ObjInstanceList[index].value = value;
}
