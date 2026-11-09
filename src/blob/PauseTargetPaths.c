/* PauseTargetPaths: a target's sound stops for the pause (MAT3 targetpaths.c). */
#include "game/game.h"

/* Marks target `t`'s sound stopped and its emitter gone (the pause has stopped every sound), so it starts over
 * when the game goes on. */
void PauseTargetPaths(tTarget *t) {
    t->sndState = 2;
    t->sound = -1;
}
