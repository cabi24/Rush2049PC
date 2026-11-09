/* ShowCheckpointTime: show a checkpoint's split time (MAT3 hud.c). */
#include "game/game.h"

/* Player reached a checkpoint at race time time: the HUD shows the time since the last one for 6 seconds
 * (gCheckPtTimeout, gLapTime), and the time itself as digits in the player's 8 bytes of CheckpointStr. */
void ShowCheckpointTime(s32 player, f32 time)
{
  f32 *unused;
  ;
  gCheckPtTimeout[player] = 6.0f;
  gLapTime[player] = time - gElapsedTime[player];
  gElapsedTime[player] = time;
  cvt_time(time, (player * 8) + (&CheckpointStr), 'f');
}
