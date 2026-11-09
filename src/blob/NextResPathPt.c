#include "game/game.h"

/* PC: gResPath and gDronePaths are native structs and the alternative paths a native array (ReadPathData), so these views take host offsets and strides */
typedef struct { u16 w0; u8 pad[22]; u8 *recs; } PGraph;
#define ALT_PATH_SIZE 24
VIEW(volatile PGraph, gResPath);
#define gResPath gResPath_view

VIEW(s32, gCheckpoints);
#define gCheckpoints gCheckpoints_view

void NextResPathPt(s32 arg0, s32 arg1, s32 *arg2, s32 *arg3)
{
  u8 *r;
  s32 temp_v0;
  unsigned int new_var;
  volatile PGraph *g;
  new_var = 1;
  g = &gResPath;
  if (arg0 >= 0)
  {
    temp_v0 = arg0 * ALT_PATH_SIZE;
    r = g->recs + temp_v0;
    if ((arg1 + new_var) == *(u16 *) (r + 0xA))
    {
      *arg3 = *(u16 *) (r + 6);
      *arg2 = *(s8 *) (g->recs + temp_v0 + 4);
      return;
    }
    *arg3 = arg1 + 1;
    *arg2 = arg0;
    return;
  }
  if (g->w0 == (arg1 + new_var))
  {
    *arg3 = *(s16 *) (((u8 (*)[0x50]) &gCheckpoints)[*(s16 *) ((u8 *) &gCheckpoints + 2)] + 0x2E);
  }
  else
  {
    *arg3 = arg1 + 1;
  }
  if (arg2 != 0)
  {
    *arg2 = -new_var;
  }
}
