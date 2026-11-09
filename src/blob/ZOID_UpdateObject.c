#include "game/game.h"

VIEW(s32, ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* PC: the scene node's host layout (docs/port/POINTERS.md 9): 0x68 bytes, pointer slots pointer-sized. The node's
 * object (a View: rot[3][3], pos[3], no pointers) keeps its N64 offsets. */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } PcNode;
void ZOID_UpdateObject(s16 arg0, void *arg1, void *arg2)
{
  u8 *obj = (u8 *) ((PcNode *) &ObjInstanceList)[arg0].obj;
  if (arg1 != ((void *) 0))
  {
    *((f32 *) (obj + 0x24)) = ((f32 *) arg1)[0];
    *((f32 *) (obj + 0x28)) = ((f32 *) arg1)[1];
    *((f32 *) (obj + 0x2C)) = ((f32 *) arg1)[2];
  }
  if (arg2 != ((void *) 0))
  {
    CopyUV(arg2, obj);
  }
}
