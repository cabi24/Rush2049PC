#include "game/game.h"

VIEW(s32, ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct { u32 flags, w4; u8 *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } E44;
void MBOX_GetPosition(void *arg0, int arg1)
{
  f32 *out = (f32 *) arg0;
  E44 *e = (E44 *) &ObjInstanceList;
  s32 idx;
  out[0] = *((f32 *) (e[arg1].obj + 0x24));
  out[1] = *((f32 *) (e[arg1].obj + 0x28));
  out[2] = *((f32 *) (e[arg1].obj + 0x2C));
  idx = ZOID_TrueObjParent(arg1);
  while (idx != -1)
  {
    out[0] += *((f32 *) (e[idx].obj + 0x24));
    out[1] += *((f32 *) (e[idx].obj + 0x28));
    out[2] += *((f32 *) (e[idx].obj + 0x2C));
    idx = ZOID_TrueObjParent(idx);
  }
}
