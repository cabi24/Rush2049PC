/* UpdateRushControls (MAT3 controls.c): the frame's input, into each player's input record. */
#include "game/game.h"

extern tPadState gPlayers[];

/* Reads the controllers (UpdateControls), then copies each player's controller's held, pressed and auto-repeated
 * buttons and stick into the player's record; a player past gNumPlayers, or without a controller, gets nothing. */
void UpdateRushControls(void) {
 int i;
 tPadState *player;
 UpdateControls();
 for(i=0;i<4;i++) {
  player=&gPlayers[i];
  if(player->controller==CONTROLLER_NONE || i>=gNumPlayers) {
   player->levels=0;
   player->edges=0;
   player->autoEdges=0;
   player->analog[0]=0.0f;
   player->analog[1]=0.0f;
  } else {
   player->levels=gContLevels[player->controller];
   player->edges=gContEdges[player->controller];
   player->autoEdges=gAutoEdges[player->controller];
   player->analog[0]=gContStick[player->controller][0];
   player->analog[1]=gContStick[player->controller][1];
  }
 }
}
