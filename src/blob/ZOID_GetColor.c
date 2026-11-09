#include "game/game.h"

VIEW(s32, ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* PC: the scene node's host layout (docs/port/POINTERS.md 9): 0x68 bytes, pointer slots pointer-sized. The address
 * of the node's primitive colour (N64 +0x3C). */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } PcNode;
void *ZOID_GetColor(s16 arg0) {
    return &((PcNode *) &ObjInstanceList)[arg0].prim;
}
