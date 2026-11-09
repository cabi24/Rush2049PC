#include "game/game.h"

VIEW(s32, gCheckpoints);
#define gCheckpoints gCheckpoints_view

void SyncMaxpathToLastCheckpoint(s16 arg0)
{
  /* gCheckpoints, resurrect.c's tCheckpoints: pathPts[0] the main path's point at a checkpoint, pathPts[1 + n] drone
   * path n's. */
  typedef struct { f32 pos[3], normal[3]; s32 radius; s16 flags, time[2], pathPts[20]; f32 distToNext; } Checkpoint;
  struct { s16 startTime, loopPt, finishPt, startPt, numPoints; Checkpoint points[10]; } *checkpoints = (void *) &gCheckpoints;
  s8 *car;

  car = (s8 *) (PC_CAR(arg0));
  /* the car's path point (+0x33A): the last checkpoint's on the car's drone path (+0x33C) */
  *((s16 *) (car + CAR_OFS(0x33A))) = checkpoints->points[PC_MODEL_LAST_CP(arg0)].pathPts[*((s16 *) (car + CAR_OFS(0x33C))) + 1];
}

