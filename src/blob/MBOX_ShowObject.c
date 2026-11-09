#include "game/game.h"

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child; s16 sibling; s16 mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } Ent;
VIEW(Ent[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

#define DEBUG_PRINT(args)

void MBOX_ShowObject(s32 a, s32 mode, u32 v) {
    s32 t;
    s32 c;

    if (mode == 1) {
        t = ObjInstanceList[(s16) a].flags; ObjInstanceList[a].flags = t & ~((v << 8) | 0x80000000);
        if (a < 0) DEBUG_PRINT(("bad object %d\n", a));
        t = ObjInstanceList[a].child;
        if (t != -1) MBOX_ShowObject(t, 2, v);
    } else if (mode == 0) {
        t = ObjInstanceList[(s16) a].flags; ObjInstanceList[a].flags = t & ~((v << 8) | 0x80000000);
    } else if (mode == 0) {
        t = ObjInstanceList[(s16) a].flags; ObjInstanceList[a].flags = (t & 0x7FFFFFFF) | (v << 8);
    } else if (mode == 2) {
        do {
            t = ObjInstanceList[(s16) a].flags; ObjInstanceList[a].flags = t & ~((v << 8) | 0x80000000);
            if (a < 0) DEBUG_PRINT(("bad object %d\n", a));
            c = ObjInstanceList[a].child;
            if (c != -1) MBOX_ShowObject(c, 2, v);
            a = ObjInstanceList[a].sibling;
        } while (a != -1);
    }
}
