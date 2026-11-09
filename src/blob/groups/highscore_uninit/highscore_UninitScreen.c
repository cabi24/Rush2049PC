/* highscore_UninitScreen: the high-score screen's tear-down (MAT3 highscorescr.c uninitScreen). */
#include "game/game.h"

VIEW(s8 *[2], scorePanes);
#define scorePanes scorePanes_view
VIEW(s8 *[3], ghostPanes);
#define ghostPanes ghostPanes_view

/* Remove every pane the screen may have up (the circuit code's, the players' name boxes, the tables' and the
 * ghost menu's) and go on to the game-over menu. */
void highscore_UninitScreen(void)
{
    s32 i;

    if (codePane != 0) {
        RemovePane(codePane);
        codePane = 0;
    }
    for (i = 0; i < gNumPlayers; i++) {
        if (namePanes[i] != 0) {
            RemovePane(namePanes[i]);
            namePanes[i] = 0;
        }
    }
    for (i = 0; i < 2; i++) {
        if (scorePanes[i] != 0) {
            RemovePane(scorePanes[i]);
            scorePanes[i] = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        if (ghostPanes[i] != 0) {
            RemovePane(ghostPanes[i]);
            ghostPanes[i] = 0;
        }
    }
    highscore_inited = 0;
    gNewGameState = GS_GAME_OVER;
}
