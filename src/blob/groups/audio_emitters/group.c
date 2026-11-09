#include "game/game.h"

typedef struct Link {struct Link *next,*prev;} Link;
typedef struct List {u8 indirect,doubly,pad[2];u32 count;Link *head,*tail;} List;
/* p64 holds a pointer */
typedef struct Node68 {Link link;List *list;u32 tag;s32 state;s32 p20;u8 flag,code,live,mode;u32 p28;f32 f32,f36,f40,f44,f48;u32 index,value,p60; uintptr_t p64;} Node68;
typedef struct Object24 {u8 p0,p1,kind,p3;u32 value;f32 f8,f12,f16;Node68 *node;} Object24;
typedef struct Queue24 {u32 opaque[6];} Queue24;
VIEW(List, gFreeFxTags);
#define gFreeFxTags gFreeFxTags_view
VIEW(List, gInterruptedFxTags);
#define gInterruptedFxTags gInterruptedFxTags_view
VIEW(Queue24, audio_lock_queue);
#define audio_lock_queue audio_lock_queue_view
VIEW(Queue24, audio_command_queue);
#define audio_command_queue audio_command_queue_view
s32 osRecvMesg(Queue24 *,void **,s32);s32 osSendMesg(Queue24 *,void *,s32);
Object24 *GetCmd(void);

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
VIEW(List, gFreeEmitters);
#define gFreeEmitters gFreeEmitters_view
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
VIEW(intptr_t, gEmitters);
#define gEmitters gEmitters_view
VIEW(List, gActiveEmitters);
#define gActiveEmitters gActiveEmitters_view

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

