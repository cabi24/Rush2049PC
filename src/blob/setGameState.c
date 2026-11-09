/* setGameState: asks for another game state (MAT3 game.c). */
#include "game/game.h"

/* Sets the game state the game loop moves to (gNewGameState): it takes it at its next turn (GameLoop). */
void setGameState(s32 state) {
    gNewGameState = state;
}
