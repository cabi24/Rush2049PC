/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;
/* PC: an fx tag (audiomgr_thread.c's tFxSequence) is 88 bytes, its links, list and data host-sized; a command
 * (tAudioMsg) 32: a change's four floats from +4, its tag at +24; a stop's tag at +8 (docs/port/SYNTH.md). */
typedef unsigned int u32;
typedef struct Entity {
    void *pad0[3]; s32 id; s32 state; u8 pad20[6];
    u8 msgCount; u8 pad27[9]; f32 x,y,z,w; u8 pad40[16]; void *data;
} Entity;
typedef struct MsgTarget { u32 pad; Entity *target; } __attribute__((packed, aligned(4))) MsgTarget;
typedef struct Msg {
    s16 id; s8 type,used;
    union { struct { f32 x,y,z,w; } value; MsgTarget t; } payload;
    Entity *ent;
} Msg;
#define target t.target
extern Entity *gFxTags;
extern s32 gTagMask;
/* PC: the sound manager's messages are PC data, PC_SND_MSG_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9, AUDIO.md 10.4); PC_SND_MSG_LOOP(128) is the run's count: 128 at original. */
#define audiomgr_gCmds (*(Msg (*)[PC_SND_MSG_SLOTS])pc_snd_msgs)
#define PC_SND_MSG_LOOP(n) (pc_snd_messages())
extern s32 audio_lock_queue, audio_command_queue;
s32 osRecvMesg(void *,void *,s32);
s32 osSendMesg(void *,void *,s32);
Entity *ConvertTag(s32 h);
Msg *GetCmd(void);
/* PC: the message pool (128) and the manager's queue (128) can run out, which the N64's even retraces never
 * let happen: on a loaded host the manager thread falls behind, and each retrace message it then finds queues
 * another change per sound source. A change that finds no message, or no room in the queue, is dropped (the
 * next retrace sends the sound's values again) and its message given back, instead of a write through NULL
 * or a message lost for good (docs/port/QA.md "Integration soak"). */
#define PC_MSG_OR_RETURN(m) if((m)==0)return;
#define PC_SEND_OR_GIVE_BACK(q,m,e) if(osSendMesg(q,m,0)!=0){(e)->msgCount--;(m)->used=0;}
void AudChangeEffect(s32 h,f32 x,f32 y,f32 z,f32 w) {
    Msg *m=0;
    Entity *e=ConvertTag(h);
    if(e!=0){
        if((x!=-2.0f && x!=e->x) || (y!=-2.0f && y!=e->y) ||
           (z!=-2.0f && z!=e->z) || (w!=-2.0f && w!=e->w)) {
            m=GetCmd();PC_MSG_OR_RETURN(m)
            m->type=4;
            m->ent=e;
            e->msgCount++;
            if(x!=-2.0f && x!=e->x)m->payload.value.x=x; else m->payload.value.x=-2.0f;
            if(y!=-2.0f && y!=e->y)m->payload.value.y=y; else m->payload.value.y=-2.0f;
            if(z!=-2.0f && z!=e->z)m->payload.value.z=z; else m->payload.value.z=-2.0f;
            if(w!=-2.0f && w!=e->w)m->payload.value.w=w; else m->payload.value.w=-2.0f;
        }
    }
    if(m!=0)PC_SEND_OR_GIVE_BACK(&audio_command_queue,m,e);
}
void AudChangeEffectVol(s32 h,f32 opacity) {
    osRecvMesg(&audio_lock_queue,0,1);
    AudChangeEffect(h,opacity<0.0f ? 0.0f : opacity>1.0f ? 1.0f : opacity,
                          -2.0f,-2.0f,-2.0f);
    osSendMesg(&audio_lock_queue,0,0);
}
void AudChangeEffectPitch(s32 h,f32 value) {
    osRecvMesg(&audio_lock_queue,0,1);
    AudChangeEffect(h,-2.0f,-2.0f,-2.0f,value);
    osSendMesg(&audio_lock_queue,0,0);
}

void change_effect_lr_pan(s32 h,f32 value) {
    osRecvMesg(&audio_lock_queue,0,1);
    AudChangeEffect(h,-2.0f,value<-1.0f ? -1.0f : value>1.0f ? 1.0f : value,-2.0f,-2.0f);
    osSendMesg(&audio_lock_queue,0,0);
}
void change_effect_fb_pan(s32 h,f32 value) {
    osRecvMesg(&audio_lock_queue,0,1);
    AudChangeEffect(h,-2.0f,-2.0f,value<-1.0f ? -1.0f : value>1.0f ? 1.0f : value,-2.0f);
    osSendMesg(&audio_lock_queue,0,0);
}
