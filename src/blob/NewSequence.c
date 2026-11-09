/* NewSequence: take a free sequence (the sound manager's timed events; MAT3 audiomgr.c). */
#include "game/game.h"

/* A sequence (game/audiomgr.h's tAudioSequence) as this reads it (host-sized links, docs/port/SYNTH.md): its
 * handle (id), which carries a tag, and whether it is busy (free). */
typedef struct Sequence {void *pad0[2];u32 handle;u8 busy,pad13[3];} Sequence;
typedef struct SequenceList {u8 indirect,doubly,pad2[2];u32 count;Sequence *head,*tail;} SequenceList;
VIEW(SequenceList, gFreeSequences);
#define gFreeSequences gFreeSequences_view

/* Takes the first free sequence off gFreeSequences, not busy, and gives it a new handle: the next id (gNextSeqId,
 * wrapping to 1) above gTagShift, the slot's own bits (gTagMask) below. (There is always a free one.) */
struct tAudioSequence *NewSequence(void) {
 Sequence *node=gFreeSequences.head;
 RemoveLLE(&gFreeSequences,node);
 node->busy=0;
 node->handle=(gNextSeqId<<gTagShift)|(node->handle&gTagMask);
 if(gNextSeqId>=((~gTagMask)>>(gTagShift+1)))gNextSeqId=1;
 else gNextSeqId++;
 return (struct tAudioSequence *)node;
}
