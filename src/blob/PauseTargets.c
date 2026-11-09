/* PauseTargets: every target's sound stops for the pause (MAT3 targets.c). */
#include "game/game.h"

/* PauseTargetPaths for every target in use (gTargetList's list), written out here: the sound is stopped and its
 * emitter gone, so it starts over when the game goes on. */
void PauseTargets(void) {
    tTarget *t;

    t = gTargetList.head;
    if (t != NULL) {
        do {
            t->sndState = 2;
            t->sound = -1;
            t = t->next;
        } while (t != NULL);
    }
}
