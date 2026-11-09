/* FindPlayerNum (MAT3 controls.c): gives a player a controller when that controller is first used. */
#include "game/game.h"

extern tPadState gPlayers[];

VIEW(s32[4], gContLevels);
#define gContLevels gContLevels_view
VIEW(s32[4], gContEdges);
#define gContEdges gContEdges_view
VIEW(s32[4], gAutoEdges);
#define gAutoEdges gAutoEdges_view
VIEW(f32[4][2], gContStick);
#define gContStick gContStick_view

/* Gives player num controller num, with this frame's input copied in, and makes the controller active: always when
 * force is set, else only when the player has no controller yet and controller num's first button (bit 0) was just
 * pressed. */
void FindPlayerNum(u32 num, s32 force)
{
  tPadState *player;
  s32 *edges;
  f32 *stick;
  if (force != 0) {
    player = &gPlayers[num];
    edges = &gContEdges[num];
  } else {
    player = &gPlayers[num];
    if (player->controller != CONTROLLER_NONE) return;
    edges = &gContEdges[num];
    if (!(*edges & 1)) return;
  }
  player->controller = num;
  player->levels = gContLevels[num];
  player->edges = *edges;
  player->autoEdges = gAutoEdges[num];
  stick = gContStick[num];
  player->analog[0] = stick[0];
  player->analog[1] = stick[1];
  SetControllerActive(num & 0xFF, 1);
}
