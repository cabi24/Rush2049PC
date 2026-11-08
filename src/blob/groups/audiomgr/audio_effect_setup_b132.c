/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* FreeFxTag is the sole match candidate. The rest is genuine B132
 * context, not new matching claims. A used local effect-pointer view and an
 * owner-field guard before binding owner reproduce the observed lifetimes.
 * This compile-affecting source spelling is disclosed; it adds no input,
 * unused storage, artificial volatile, synthetic caller, or runtime operation.
 */
typedef unsigned char u8;
typedef unsigned int u32;
typedef struct Link {struct Link *next, *prev;} Link;
typedef struct List {u8 indirect, doubly, pad[2];u32 count;Link *head, *tail;} List;
void InsertLLE(List *list, Link *object, Link *before);
void RemoveLLE(List *list, Link *object);

typedef int s32;
typedef signed char s8;
typedef float f32;
#define M2C_FIELD(expr,type_ptr,offset) (*(type_ptr)((s8 *)(expr)+(offset)))
extern u32 gTagMask;
extern List gActiveSequences,gFreeSequences,gActiveEmitters,gFreeEmitters,gFreeFxTags,gInterruptedFxTags,gPlayingFxTags;
extern s32 active_sequences_head;
extern f32 gAudioSeconds;
extern s8 gInited_AudioMgr;
extern s32 gNumStopCmds,gNumStartCmds,gNumFxPlaying,gNumFxInterrupted,gNumFxPending;
extern void *gStopCmds[],*gStartCmds[];
extern s32 *D_80144C58;
extern intptr_t ConvertTag(s32);
extern s32 AudioEffectPlaying(s32);
extern void AudStopEffect(s32);
extern s32 AudPlayEffect(s32,s32,s32,u8);
extern void ProcessEmitters(void);
extern s32 osRecvMesg(void *,void *,s32);
extern s32 osSendMesg(void *,void *,s32);
extern u8 audio_lock_queue[],audio_command_queue[];
typedef struct TreeNode { struct TreeNode *left,*right; s32 *object; } TreeNode;
typedef struct Tree { s32 (*compare)(TreeNode *,TreeNode *); s32 count; TreeNode *root; } Tree;
extern Tree gSortedFxTagTree;
extern TreeNode *gSortedFxTags;
extern s32 WeightedPriority(s32 *);
extern void InsertBSTEInternal(Tree *,TreeNode **,TreeNode *);
extern void WalkBSTInternal(Tree *,TreeNode *,s32 *,void (*)(TreeNode *,s32));
extern void func_800952E0(TreeNode *,s32);
extern void AudioSfxServer(void);
void FreeSequence(s32 *);
typedef struct Owner { Link link; s8 active,queued; } Owner;
typedef struct Effect { Link link; List *list; u32 handle; s32 kind,mode; s8 state,delay; u8 persistent,priority; u32 rest[9]; Owner *owner; } Effect;
void FreeFxTag(Effect *);
void InterruptFxTag(s32 *);
void ProcessSequences(void);
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
void InterruptFxTag(s32 *effect);
void FreeSequence(s32 *ipa_s0);

/* ProcessSequences = _ProcessSequences@audiomgr.c (MAT3 GameCube audiomgr.c, GC 0x800A44D8; DWARF locals s, next, t).
 * Body: research packet refs/pr/408 (
 * 1/95: the state local is that packet's native reconstruction choice), closed by: DISCLOSED no-code identity
 * `(state*1)==2` - uopt orders ==/!= operands by kind (a plain variable goes second); the identity makes the state
 * side an expression so the hoisted constant 2 (s7) is the first BNE operand as in retail (wave-16 lever;
 * `state^0` and `state>>0` match too). The clock predicate is the locked AudioTimeout (timeout.c), inlined. */
typedef struct Sequence { Link ll; u32 id; s8 free,active,stop; f32 startTime,timeout; u8 next; s32 ids[4]; s32 subId,replace; u8 priority; s32 sndTag; } Sequence;
typedef struct FxTag { Link ll; List *list; u32 id; s32 state,type; s8 paused,loop; u8 numCmds,priority; s32 wpriority; f32 startTime,vol,lrpan,fbpan,pitch; s32 fxid,subId; u32 sysId; void *data; } FxTag;
s32 AudioTimeout(f32,f32);
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

