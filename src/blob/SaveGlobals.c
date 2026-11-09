/* SaveGlobals: the game's options are set (MAT3 save.c). */
#include "game/game.h"

/* Notes that the game's options are set, so the next profile chosen takes them rather than giving its own
 * (SaveSelectPlayer). */
void SaveGlobals(void) {
    gGlobalsSet = 1;
}
