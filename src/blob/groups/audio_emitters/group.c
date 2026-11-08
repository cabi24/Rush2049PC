/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Group: GetFxTag (internal) with its two matched callers.
 *  - GetFxTag: take the head node of the free list gFreeFxTags (no empty check: the
 *    `if (node == 0) {}` is a compiled-out diagnostic, see below), stamp word 12 with a
 *    generation counter (gNextFxId << gTagShift, keeping the bits in gTagMask; the
 *    counter wraps to 1 at its field limit), clear/initialise the node, move it from the
 *    free list to the head of the active list gInterruptedFxTags (RemoveLLE = list remove,
 *    InsertLLE = list insert-before) and return it.  Internal: it clobbers s0/s1 that
 *    its callers do not keep live (IPA); it only reproduces with its callers in the unit.
 *  - AudPlayEffect / AudPlayEffectVP (historical labels): under the audio_lock_queue
 *    lock, allocate a 24-byte command (GetCmd), kind 2, attach a fresh node, fill
 *    index/value/mode/state, look the index's code up in gEffectLoop, set three floats
 *    (constants 1, 0, -2, or the clamped x in [0,1], y/z in [-1,1]), read the node tag,
 *    release the lock and post the command on audio_command_queue.  Returns the tag.
 * Shaping quirks (all compile-affecting, proved with the unit, the traced as1 and uopt):
 *  - `if (node == 0) {}` after the free-list read ends a block, so the three global
 *    address loads of the tag statement get a later line than the free-list address and
 *    as1 schedules `lui a2` second as in retail (any condition works);
 *  - Node68.state is s32 while the bytes flag/live are u8: with a u32 state the two
 *    stores of 1 share one constant web that takes v1; with s32 they are separate and
 *    retail rematerialises `li 1` at each store (forcing that web to split reproduced
 *    retail exactly, which pointed at the type);
 *  - the three float stores are on separate lines (as1 tie-break by line);
 *  - the clamps are one `?:` expression per field (a single store after the select).
 * Third caller AudEmitEffect (0x800AED64, claimed here; from 's source):
 *  a positioned variant of AudPlayEffectVP. If gEmitters is 0 return -1; else take the
 *  audio_lock_queue lock, pop an emitter from free list gFreeEmitters (NewEmitter; none -> unlock,
 *  return -1), store pos[3], volume, x and y clamped to [0,1] and 1.0f, allocate the command and
 *  node as above (p20 = 1, floats 0/0/-2), link node->p64 to the emitter, copy the node tag to
 *  the emitter, post the command, re-lock, insert the emitter into the active list gActiveEmitters
 *  unless it is already active (+8), mark it queued (+9), unlock and return the tag.
 *  It is kept (ABI arguments; floats arrive in integer registers and are homed).
 *  Quirks:
 *  - Node68.p20 is s32 (the locked members only store 0 to it; their words are unchanged);
 *  - Cam.queued (+9) is `signed char`: its store of 1 then joins the s32 constant web of
 *    p20/state, which becomes live out of the block that ends at the first osJamMesg, so the
 *    allocator gives it t1 (a0-a2 and t0 interfere) and rematerialises `li 1` for the queued
 *    store, as retail does. With `u8 queued` the web stays in one block and takes a0
 * (the 12-row residual). The u8 `live` keeps its own local `li 1`;
 *  - `char buf[4]` is an unused local that supplies 4 frame bytes (result home at sp+32).
 */
typedef unsigned char u8;typedef unsigned int u32;typedef int s32;typedef float f32;
typedef struct Link {struct Link *next,*prev;} Link;
typedef struct List {u8 indirect,doubly,pad[2];u32 count;Link *head,*tail;} List;
/* p64 holds a pointer */
typedef struct Node68 {Link link;List *list;u32 tag;s32 state;s32 p20;u8 flag,code,live,mode;u32 p28;f32 f32,f36,f40,f44,f48;u32 index,value,p60; uintptr_t p64;} Node68;
typedef struct Object24 {u8 p0,p1,kind,p3;u32 value;f32 f8,f12,f16;Node68 *node;} Object24;
typedef struct Queue24 {u32 opaque[6];} Queue24;
extern List gFreeFxTags,gInterruptedFxTags;extern u32 gNextFxId,gTagShift,gTagMask;
extern Queue24 audio_lock_queue,audio_command_queue;extern u8 gEffectLoop[];
s32 osRecvMesg(Queue24 *,void **,s32);s32 osSendMesg(Queue24 *,void *,s32);
Object24 *GetCmd(void);
void RemoveLLE(List *,Link *);void InsertLLE(List *,Link *,Link *);
/* PC: the pools' high-water marks (--log-voices, docs/port/AUDIO.md 10): the fx tags in use after one is taken (of
 * gNumFxTags), the emitters active after one is put in their list. */
extern s32 gNumFxTags;
#define PC_TAGS_MOST() pc_snd_most(PC_SND_MOST_TAGS, gNumFxTags - (s32)gFreeFxTags.count);
#define PC_EMITTERS_MOST(list) pc_snd_most(PC_SND_MOST_EMITTERS, (s32)(list).count);
Node68 *GetFxTag(void) {
 Node68 *node=(Node68*)gFreeFxTags.head;
 if(node==0){}
 node->tag=(gNextFxId<<gTagShift)|(node->tag&gTagMask);
 if(gNextFxId>=((~gTagMask)>>(gTagShift+1)))gNextFxId=1;
 else gNextFxId++;
 node->p64=0;node->f36=0.0f;node->f40=0.0f;node->f44=0.0f;
 node->f32=-1.0f;node->f48=1.0f;
 RemoveLLE(&gFreeFxTags,(Link*)node); PC_TAGS_MOST()
 InsertLLE(&gInterruptedFxTags,(Link*)node,gInterruptedFxTags.head);
 node->list=&gInterruptedFxTags;
 return node;
}
/* PC: the message pool and the manager's queue can run out (transform/group.c's AudChangeEffect says
 * when): a sound start that finds no message is not started (handle -1, which every handle lookup takes as
 * none; AudEmitEffect gives its emitter back), and one the full queue refuses gives its message back,
 * its tag left to the next priority pass. */
extern List gFreeEmitters;
#define PC_OBJ_OR_UNLOCK(o) if((o)==0){osSendMesg(&audio_lock_queue,0,0);return (u32)-1;}
#define PC_OBJ_OR_UNLOCK_CAM(o,c) \
    if((o)==0){InsertLLE(&gFreeEmitters,(Link*)(c),gFreeEmitters.head);osSendMesg(&audio_lock_queue,0,0);return -1;}
#define PC_OBJ_SEND(q,o) if(osSendMesg(q,o,0)!=0)(o)->p3=0
/* So can the tags (gFreeFxTags: 80 with the Expansion Pak, 16 without, as AudioInit makes them), which
 * GetFxTag takes from the list's head unchecked: in a race of more cars than the N64's 6 their engines can take
 * the last one (docs/port/MORECARS.md 6). A start that finds none gives its message back (the in-use byte, +3) and
 * is not started either. */
#define PC_TAG_OR_UNLOCK(o) if(gFreeFxTags.head==0){(o)->p3=0;osSendMesg(&audio_lock_queue,0,0);return (u32)-1;}
#define PC_TAG_OR_UNLOCK_CAM(o,c) if(gFreeFxTags.head==0){(o)->p3=0; \
    InsertLLE(&gFreeEmitters,(Link*)(c),gFreeEmitters.head);osSendMesg(&audio_lock_queue,0,0);return -1;}
u32 AudPlayEffect(u32 index,u32 other,u32 value,u8 mode) {
 Object24 *object;u32 result;
 osRecvMesg(&audio_lock_queue,0,1);
 object=GetCmd();PC_OBJ_OR_UNLOCK(object)PC_TAG_OR_UNLOCK(object)object->kind=2;object->value=value;
 object->node=GetFxTag();object->node->p20=0;
 object->node->index=index;object->node->value=other;object->node->mode=mode;
 object->node->state=1;object->node->flag=0;
 object->node->live=1;
 object->node->code=gEffectLoop[index];
 object->f8=1.0f;
 object->f12=0.0f;
 object->f16=-2.0f;
 result=object->node->tag;
 osSendMesg(&audio_lock_queue,0,0);
 PC_OBJ_SEND(&audio_command_queue,object);
 return result;
}
u32 AudPlayEffectVP(u32 index,u32 other,u32 value,u8 mode,f32 x,f32 y,f32 z) {
 Object24 *object;u32 result;
 osRecvMesg(&audio_lock_queue,0,1);
 object=GetCmd();PC_OBJ_OR_UNLOCK(object)PC_TAG_OR_UNLOCK(object)object->kind=2;object->value=value;
 object->node=GetFxTag();object->node->p20=0;
 object->node->index=index;object->node->value=other;object->node->mode=mode;
 object->node->state=1;object->node->flag=0;
 object->node->live=1;
 object->node->code=gEffectLoop[index];
 object->f8=(x<0.0f)?0.0f:((1.0f<x)?1.0f:x);
 object->f12=(y<-1.0f)?-1.0f:((1.0f<y)?1.0f:y);
 object->f16=(z<-1.0f)?-1.0f:((1.0f<z)?1.0f:z);
 result=object->node->tag;
 osSendMesg(&audio_lock_queue,0,0);
 PC_OBJ_SEND(&audio_command_queue,object);
 return result;
}

typedef struct Cam {Link link;signed char active;signed char queued;u8 pad[2];f32 pos[3];f32 volume,x,y,f36;u32 tag;} Cam;
extern intptr_t gEmitters;
extern List gActiveEmitters;
Cam *NewEmitter(void);
/* a1 is never read, and every caller passes a pointer there. On the N64 (o32) everything after the leading
 * pointer travels in GPRs, so its type was moot; on the PC an f32 would take s0 and shift every float after it
 * (docs/port/PROTOTYPES.md). */
#define CTT_A1 void *
s32 AudEmitEffect(f32 *pos,CTT_A1 a1,f32 volume,f32 a3,f32 x,f32 y,u32 index,u32 other,u32 value,u8 mode) {
 Object24 *object;Cam *cam;char buf[4];u32 result;
 if(gEmitters==0)return -1;
 osRecvMesg(&audio_lock_queue,0,1);
 cam=NewEmitter();
 if(cam==0){pc_snd_count(PC_SND_NO_EMITTER);osSendMesg(&audio_lock_queue,0,0);return -1;}
 cam->pos[0]=pos[0];
 cam->pos[1]=pos[1];
 cam->pos[2]=pos[2];
 cam->volume=volume;
 cam->x=(x<0.0f)?0.0f:((1.0f<x)?1.0f:x);
 cam->y=(y<0.0f)?0.0f:((1.0f<y)?1.0f:y);
 cam->f36=1.0f;
 object=GetCmd();PC_OBJ_OR_UNLOCK_CAM(object,cam)PC_TAG_OR_UNLOCK_CAM(object,cam)object->kind=2;object->value=value;
 object->node=GetFxTag();object->node->p20=1;
 object->node->index=index;object->node->value=other;object->node->mode=mode;
 object->node->state=1;object->node->flag=0;
 object->node->live=1;
 object->node->code=gEffectLoop[index];
 object->f8=0.0f;
 object->f12=0.0f;
 object->f16=-2.0f;
 object->node->p64=(uintptr_t)cam;
 result=cam->tag=object->node->tag;
 osSendMesg(&audio_lock_queue,0,0);
 PC_OBJ_SEND(&audio_command_queue,object);
 osRecvMesg(&audio_lock_queue,0,1);
 if(cam->active==0){InsertLLE(&gActiveEmitters,(Link*)cam,gActiveEmitters.head);cam->queued=1;PC_EMITTERS_MOST(gActiveEmitters)}
 osSendMesg(&audio_lock_queue,0,0);
 return result;
}
void RemoveLLE(List *list, Link *object);

void InsertLLE(List *list, Link *object, Link *before);
