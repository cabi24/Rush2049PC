/* FreeSequence (MAT3 audiomgr.c _FreeSequence): gives a sound sequence back to the sound manager's free list. */
#include "game/game.h"

/* A sequence's first fields (audiomgr_thread.c's anon_ll_id_3c, MAT3's) with the PC's host-sized links
 * (docs/port/SYNTH.md). */
typedef struct SequenceHead {
    void *next, *prev;  /* 0x00: its place in gActiveSequences or gFreeSequences */
    s32 id;             /* 0x10: the effect tag the sequence plays, with its instance bits above gTagMask */
    s8 free;            /* 0x14: in gFreeSequences */
    s8 active;          /* 0x15: in gActiveSequences */
} SequenceHead;

/* Drops sequence's instance bits from its id, takes it out of the active list if it is there, and puts it at the
 * head of the free list. */
void FreeSequence(SequenceHead *sequence) {
    sequence->id = (s32) (sequence->id & gTagMask);
    if (sequence->active != 0) {
        RemoveLLE(&gActiveSequences, sequence);
        sequence->active = 0;
    }
    InsertLLE(&gFreeSequences, sequence, gFreeSequences.head);
    sequence->free = 1;
}
