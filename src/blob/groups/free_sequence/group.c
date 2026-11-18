/* FreeSequence (MAT3 audiomgr.c _FreeSequence): gives a sound sequence back to the sound manager's free list. */
#include "game/game.h"
#include "game/audiomgr.h"

/* Drops sequence's instance bits from its id, takes it out of the active list if it is there, and puts it at the
 * head of the free list. */
void FreeSequence(tAudioSequence *sequence) {
    sequence->id &= gTagMask;
    if (sequence->active != 0) {
        RemoveLLE(&gActiveSequences, sequence);
        sequence->active = 0;
    }
    InsertLLE(&gFreeSequences, sequence, gFreeSequences.head);
    sequence->free = 1;
}
