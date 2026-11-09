#include "game/game.h"

VIEW(u16, gDronePaths);
#define gDronePaths gDronePaths_view

VIEW(s32, gCheckpoints);
#define gCheckpoints gCheckpoints_view

/* PC: gDronePaths is native (ReadPathData), 16 bytes a path. */
#define DRONE_PATH_SIZE 16
s16 PrevPathPt(s16 arg0, s16 arg1)
{
  if ((arg0 == 0) && ((*((s16 *) (((s8 *) (&gCheckpoints)) + 2))) > 0))
  {
    return arg0;
  }
  if ((arg0 == 0) || (arg0 == ((s16 (*)[0x28]) (((s8 *) (&gCheckpoints)) + 0x30))[(*((s16 *) (((s8 *) (&gCheckpoints)) + 2)))][arg1]))
  {
    return (s16) ((*((u16 *) (((u8 *) (&gDronePaths)) + (arg1 * DRONE_PATH_SIZE)))) - 1);
  }
  return (s16) (arg0 - 1);
}

