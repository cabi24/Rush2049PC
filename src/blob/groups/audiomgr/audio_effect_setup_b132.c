/* FreeFxTag and ProcessSequences: giving back an fx tag, and the timed sequences' pass of each retrace (MAT3
 * audiomgr.c's _FreeFxTag, with _FreeEmitter inlined, and _ProcessSequences). */
#include "game/game.h"
#include "game/audiomgr.h"

extern intptr_t ConvertTag(s32);

/* Frees tag: invalidates its handle (keeps the index bits only), and for an emitter's tag gives the emitter back
 * (off the active list, onto the free list). The tag itself goes back to gFreeFxTags only when no message names it
 * any more (numCmds 0); otherwise the last message's handler frees it. */
void FreeFxTag(tFxSequence *input) {
    tFxSequence *tag=input;
    tSfxEmitter *owner;
    tSfxEmitter *emitter;
    tag->id &= gTagMask;
    tag->state=kFxInvalid;
    if (tag->type==kFxEmitter) {
        if (tag->data) {
            owner=tag->data;
            emitter=owner;
            if (owner->active) {
                RemoveLLE(&gActiveEmitters,&owner->ll);
                emitter->active=0;
            }
            InsertLLE(&gFreeEmitters,emitter,gFreeEmitters.head);
            emitter->free=1;
            tag->data=0;
        }
    }
    if (!tag->numCmds) {
        RemoveLLE(tag->list,&tag->ll);
        InsertLLE(&gFreeFxTags,&tag->ll,gFreeFxTags.head);
        tag->list=&gFreeFxTags;
    }
}

/* Each retrace (PeriodicMaintenance): for every active sequence, frees it when it was stopped, or when its first
 * effect never started within its timeout; otherwise, once its current effect has ended (or none has started),
 * starts the next of its effects (skipping -1s) with the sequence's sub id, replace and priority, or frees the
 * sequence when none is left. */
void ProcessSequences(void) {
 tAudioSequence *seq,*next;
 tFxSequence *tag;
 for(seq=(tAudioSequence *)gActiveSequences.head;seq;seq=next) {
  next=(tAudioSequence *)seq->ll.next;
  if(seq->stop || (seq->sndTag == -1 && AudioTimeout(seq->startTime,seq->timeout))) {
   AudStopEffect(seq->sndTag);
   FreeSequence((s32 *)seq);
  } else {
   tag=(tFxSequence *)ConvertTag(seq->sndTag);
   if(tag) {
    s32 state=tag->state;
    if(state==kFxPlaying && !tag->numCmds && !AudioEffectPlaying(tag->sysId))
     FreeFxTag(tag);
    else continue;
   }
   while(seq->ids[seq->next]==-1) {
    seq->next++;
    if(seq->next>=4) break;
   }
   if(seq->next>=4) FreeSequence((s32 *)seq);
   else {
    seq->sndTag=AudPlayEffect(seq->ids[seq->next++],seq->subId,seq->replace,seq->priority);
    ((tFxSequence *)ConvertTag(seq->sndTag))->data=seq;
   }
  }
 }
}
