/* ClearRushControls (MAT3 controls.c): forgets every controller's input and every player's copy of it. */
#include "game/game.h"

extern tPadState gPlayers[];

/* ClearControls, then each player's pressed, held and auto-repeated buttons and stick. */
void ClearRushControls(void) {
    tPadState *player;
    s32 i;

    ClearControls();
    player = gPlayers;
    i = 0;
    if (gNumPlayers > 0) {
        do {
            player->levels = 0;
            player->edges = 0;
            player->autoEdges = 0;
            player->analog[0] = 0.0f;
            player->analog[1] = 0.0f;
            i += 1;
            player++;
        } while (i < gNumPlayers);
    }
}
