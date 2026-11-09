#include "game/game.h"

VIEW(s32, ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* PC: the scene node's host layout (docs/port/POINTERS.md 9): 0x68 bytes, pointer slots pointer-sized. The node's next
 * sibling (N64 +0x18, ObjInstanceList_next of node 0). */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } PcNode;
s16 MBOX_GetNext(s32 arg0) {
    return ((PcNode *) &ObjInstanceList)[arg0].next;
}
