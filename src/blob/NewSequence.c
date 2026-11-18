/* NewSequence: take a free sequence (the sound manager's timed events; MAT3 audiomgr.c). */
#include "game/game.h"
#include "game/audiomgr.h"

/* Takes the first free sequence off gFreeSequences, not free any more, and gives it a new handle: the next id
 * (gNextSeqId, wrapping to 1) above gTagShift, the slot's own bits (gTagMask) below. (There is always a free one.) */
tAudioSequence *NewSequence(void) {
 tAudioSequence *node=gFreeSequences.head;
 RemoveLLE(&gFreeSequences,node);
 node->free=0;
 node->id=(gNextSeqId<<gTagShift)|(node->id&gTagMask);
 if(gNextSeqId>=((~gTagMask)>>(gTagShift+1)))gNextSeqId=1;
 else gNextSeqId++;
 return node;
}
