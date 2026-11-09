#include "game/game.h"

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 s22; s16 s24; s16 mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } Ent;
VIEW(Ent[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view
VIEW(s16, ObjHead);
#define ObjHead ObjHead_view

void ZOID_InsertObject(s16 a, s16 b) {
    if (b < 0) {
        if (ObjHead < 0) {
            ObjHead = a;
        } else {
            ObjInstanceList[ZOID_LastSibling(ObjHead)].s24 = a;
        }
    } else {
        if (ObjInstanceList[b].s22 == -1) {
            ObjInstanceList[b].s22 = a;
        } else {
            ObjInstanceList[ZOID_LastSibling(ObjInstanceList[b].s22)].s24 = a;
        }
    }
}
