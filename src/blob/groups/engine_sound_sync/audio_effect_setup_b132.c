/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* audio_effect_setup is the sole match candidate. The rest is genuine B132
 * context, not new matching claims. A used local effect-pointer view and an
 * owner-field guard before binding owner reproduce the observed lifetimes.
 * This compile-affecting source spelling is disclosed; it adds no input,
 * unused storage, artificial volatile, synthetic caller, or runtime operation.
 */
typedef unsigned char u8;
typedef unsigned int u32;
typedef struct Link {struct Link *next, *prev;} Link;
typedef struct List {u8 indirect, doubly, pad[2];u32 count;Link *head, *tail;} List;
void func_80091FBC(List *list, Link *object, Link *before);
void func_8009211C(List *list, Link *object);

typedef int s32;
typedef signed char s8;
typedef float f32;
#define M2C_FIELD(expr,type_ptr,offset) (*(type_ptr)((s8 *)(expr)+(offset)))
extern u32 D_80146104;
extern List D_80149860,D_80149808,D_801461E8,D_801461B0,D_80143FF8,D_80144C50,D_80144020;
extern s32 D_80149868;
extern f32 D_80152748;
extern s8 D_8011023C;
extern s32 D_80110250,D_8011024C,D_80110278,D_80110280,D_8011027C;
extern void *D_80143CF0[],*D_80143AE8[];
extern s32 *D_80144C58;
extern intptr_t func_80091BA8(s32);
extern s32 entity_state_check(s32);
extern void scheduler_recv(s32);
extern s32 entity_flags_apply(s32,s32,s32,u8);
extern void engine_sound_sync(void);
extern s32 osRecvMesg(void *,void *,s32);
extern s32 osSendMesg(void *,void *,s32);
extern u8 D_80142728[],D_801427A8[];
typedef struct TreeNode { struct TreeNode *left,*right; s32 *object; } TreeNode;
typedef struct Tree { s32 (*compare)(TreeNode *,TreeNode *); s32 count; TreeNode *root; } Tree;
extern Tree D_80143F58;
extern TreeNode *D_80110248;
extern s32 func_80098710(s32 *);
extern void camera_update_b(Tree *,TreeNode **,TreeNode *);
extern void camera_update_a(Tree *,TreeNode *,s32 *,void (*)(TreeNode *,s32));
extern void func_800952E0(TreeNode *,s32);
extern void camera_transform(void);
void entity_flag_check(s32 *);
typedef struct Owner { Link link; s8 active,queued; } Owner;
typedef struct Effect { Link link; List *list; u32 handle; s32 kind,mode; s8 state,delay; u8 persistent,priority; u32 rest[9]; Owner *owner; } Effect;
void audio_effect_setup(Effect *);
void audio_pitch_adjust(s32 *);
void func_800988D8(void);
void audio_effect_setup(Effect *input) {
    Effect *effect=input;
    Owner *owner;
    Link *pending;
    effect->handle &= D_80146104;
    effect->kind=0;
    if (effect->mode==1) {
        if (effect->owner) {
            owner=effect->owner;
            pending=&owner->link;
            if (owner->queued) {
                func_8009211C(&D_801461E8,&owner->link);
                ((Owner *)pending)->queued=0;
            }
            func_80091FBC(&D_801461B0,pending,D_801461B0.head);
            ((Owner *)pending)->active=1;
            effect->owner=0;
        }
    }
    if (!effect->persistent) {
        func_8009211C(effect->list,&effect->link);
        func_80091FBC(&D_80143FF8,&effect->link,D_80143FF8.head);
        effect->list=&D_80143FF8;
    }
}
void audio_pitch_adjust(s32 *effect);
void entity_flag_check(s32 *ipa_s0);

/* func_800988D8 = _ProcessSequences@audiomgr.c (MAT3 GameCube audiomgr.c, GC 0x800A44D8; DWARF locals s, next, t).
 * Body: research packet refs/pr/408 (
 * 1/95: the state local is that packet's native reconstruction choice), closed by: DISCLOSED no-code identity
 * `(state*1)==2` - uopt orders ==/!= operands by kind (a plain variable goes second); the identity makes the state
 * side an expression so the hoisted constant 2 (s7) is the first BNE operand as in retail (wave-16 lever;
 * `state^0` and `state>>0` match too). The clock predicate is the locked func_80095198 (timeout.c), inlined. */
typedef struct Sequence { Link ll; u32 id; s8 free,active,stop; f32 startTime,timeout; u8 next; s32 ids[4]; s32 subId,replace; u8 priority; s32 sndTag; } Sequence;
typedef struct FxTag { Link ll; List *list; u32 id; s32 state,type; s8 paused,loop; u8 numCmds,priority; s32 wpriority; f32 startTime,vol,lrpan,fbpan,pitch; s32 fxid,subId; u32 sysId; void *data; } FxTag;
s32 func_80095198(f32,f32);
void func_800988D8(void) {
 Sequence *s,*next;
 FxTag *t;
 for(s=(Sequence *)D_80149860.head;s;s=next) {
  next=(Sequence *)s->ll.next;
  if(s->stop || (s->sndTag == -1 && func_80095198(s->startTime,s->timeout))) {
   scheduler_recv(s->sndTag);
   entity_flag_check((s32 *)s);
  } else {
   t=(FxTag *)func_80091BA8(s->sndTag);
   if(t) {
    s32 state=t->state;
    if((state*1)==2 && !t->numCmds && !entity_state_check(t->sysId))
     audio_effect_setup((Effect *)t);
    else continue;
   }
   while(s->ids[s->next]==-1) {
    s->next++;
    if(s->next>=4) break;
   }
   if(s->next>=4) entity_flag_check((s32 *)s);
   else {
    s->sndTag=entity_flags_apply(s->ids[s->next++],s->subId,s->replace,s->priority);
    ((FxTag *)func_80091BA8(s->sndTag))->data=s;
   }
  }
 }
}

