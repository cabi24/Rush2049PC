/* PauseTargets: every target's sound stops for the pause (MAT3 targets.c). */
#include "game/game.h"

/* PauseTargetPaths for every target in use (gTargetList's list, whose nodes link by 32-bit window addresses on the
 * PC: docs/port/POINTERS.md 10), written out here. */
void PauseTargets(void) {
    tTarget *t;

    t = (tTarget *) gTargetList.head;
    if (t != NULL) {
        do {
            t->sndState = 2;
            t->sound = -1;
            t = (tTarget *) PC_N64P(t->next);
        } while (t != NULL);
    }
}
