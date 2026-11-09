#include "game/game.h"

typedef struct Link {struct Link *next, *prev;} Link;
typedef struct List {u8 indirect, doubly, pad[2];u32 count;Link *head, *tail;} List;

VIEW(List, gActiveSequences);
#define gActiveSequences gActiveSequences_view
VIEW(List, gActiveEmitters);
#define gActiveEmitters gActiveEmitters_view
VIEW(List, gFreeEmitters);
#define gFreeEmitters gFreeEmitters_view
VIEW(List, gFreeFxTags);
#define gFreeFxTags gFreeFxTags_view

extern intptr_t ConvertTag(s32);

typedef struct Owner { Link link; s8 active,queued; } Owner;
typedef struct Effect { Link link; List *list; u32 handle; s32 kind,mode; s8 state,delay; u8 persistent,priority; u32 rest[9]; Owner *owner; } Effect;

void FreeFxTag(Effect *input) {
    Effect *effect=input;
    Owner *owner;
    Link *pending;
    effect->handle &= gTagMask;
    effect->kind=0;
    if (effect->mode==1) {
        if (effect->owner) {
            owner=effect->owner;
            pending=&owner->link;
            if (owner->queued) {
                RemoveLLE(&gActiveEmitters,&owner->link);
                ((Owner *)pending)->queued=0;
            }
            InsertLLE(&gFreeEmitters,pending,gFreeEmitters.head);
            ((Owner *)pending)->active=1;
            effect->owner=0;
        }
    }
    if (!effect->persistent) {
        RemoveLLE(effect->list,&effect->link);
        InsertLLE(&gFreeFxTags,&effect->link,gFreeFxTags.head);
        effect->list=&gFreeFxTags;
    }
}

/* ProcessSequences = _ProcessSequences@audiomgr.c (MAT3 GameCube audiomgr.c, GC 0x800A44D8; DWARF locals s, next, t).
 * Body: research packet refs/pr/408 (
 * 1/95: the state local is that packet's native reconstruction choice), closed by: DISCLOSED no-code identity
 * `(state*1)==2` - uopt orders ==/!= operands by kind (a plain variable goes second); the identity makes the state
 * side an expression so the hoisted constant 2 (s7) is the first BNE operand as in retail (wave-16 lever;
 * `state^0` and `state>>0` match too). The clock predicate is the locked AudioTimeout (timeout.c), inlined. */
typedef struct Sequence { Link ll; u32 id; s8 free,active,stop; f32 startTime,timeout; u8 next; s32 ids[4]; s32 subId,replace; u8 priority; s32 sndTag; } Sequence;
typedef struct FxTag { Link ll; List *list; u32 id; s32 state,type; s8 paused,loop; u8 numCmds,priority; s32 wpriority; f32 startTime,vol,lrpan,fbpan,pitch; s32 fxid,subId; u32 sysId; void *data; } FxTag;

void ProcessSequences(void) {
 Sequence *s,*next;
 FxTag *t;
 for(s=(Sequence *)gActiveSequences.head;s;s=next) {
  next=(Sequence *)s->ll.next;
  if(s->stop || (s->sndTag == -1 && AudioTimeout(s->startTime,s->timeout))) {
   AudStopEffect(s->sndTag);
   FreeSequence((s32 *)s);
  } else {
   t=(FxTag *)ConvertTag(s->sndTag);
   if(t) {
    s32 state=t->state;
    if((state*1)==2 && !t->numCmds && !AudioEffectPlaying(t->sysId))
     FreeFxTag((Effect *)t);
    else continue;
   }
   while(s->ids[s->next]==-1) {
    s->next++;
    if(s->next>=4) break;
   }
   if(s->next>=4) FreeSequence((s32 *)s);
   else {
    s->sndTag=AudPlayEffect(s->ids[s->next++],s->subId,s->replace,s->priority);
    ((FxTag *)ConvertTag(s->sndTag))->data=s;
   }
  }
 }
}

