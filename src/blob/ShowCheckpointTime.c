#include "game/game.h"

VIEW(f32, gCheckPtTimeout);
#define gCheckPtTimeout gCheckPtTimeout_view

VIEW(f32, gLapTime);
#define gLapTime gLapTime_view

VIEW(f32, gElapsedTime);
#define gElapsedTime gElapsedTime_view

void ShowCheckpointTime(s32 arg0, f32 arg1)
{
  f32 *temp_v1;
  ;
  (&gCheckPtTimeout)[arg0] = 6.0f;
  (&gLapTime)[arg0] = arg1 - (*(&(&gElapsedTime)[arg0]));
  *(&(&gElapsedTime)[arg0]) = arg1;
  cvt_time(arg1, (arg0 * 8) + (&CheckpointStr), 0x66);
}
