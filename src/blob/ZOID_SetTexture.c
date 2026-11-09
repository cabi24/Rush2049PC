#include "game/game.h"

VIEW(s32[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* PC: the scene node's host layout (docs/port/POINTERS.md 9): 0x68 bytes, pointer slots pointer-sized. ZOID_SetTexture
 * keeps the whole texture pointer in the node's tex[] slots (all four LODs when arg2 < 0); the m2c body writes it
 * as an s32 at the N64 offsets. */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } PcNode;
void ZOID_SetTexture(s16 arg0, void *arg1, s32 arg2)
{
  PcNode *node = (PcNode *) &ObjInstanceList + arg0;
  if (arg2 < 0)
  {
    node->tex[0] = arg1;
    node->tex[1] = arg1;
    node->tex[2] = arg1;
    node->tex[3] = arg1;
    return;
  }
  node->tex[arg2] = arg1;
}

