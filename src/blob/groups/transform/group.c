/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;
#ifdef TARGET_PC
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
#else
typedef struct Entity {
    u8 pad0[12]; s32 id; s32 state; u8 pad14[6];
    u8 msgCount; u8 pad1B[9]; f32 x,y,z,w; u8 pad34[16];
} Entity;
typedef struct Msg {
    s16 id; s8 type,used;
    union { struct { f32 x,y,z,w; } value; Entity *target; } payload;
    Entity *ent;
} Msg;
#endif
extern Entity *D_80110244;
extern s32 D_80146104;
extern Msg D_80142DD8[128];
extern s32 D_80142728, D_801427A8;
s32 osRecvMesg(void *,void *,s32);
s32 osSendMesg(void *,void *,s32);
Entity *func_80091BA8(s32 h) {
    if(h==-1)return 0;
    if(D_80110244[h&D_80146104].id!=h)return 0;
    return &D_80110244[h&D_80146104];
}
Msg *func_80091B00(void) {
    s32 i;
    for(i=0;i<128;i++){
        if(D_80142DD8[i].used==0){
            D_80142DD8[i].used=1;
            D_80142DD8[i].id=-1;
            return &D_80142DD8[i];
        }
    }
    return 0;
}
#ifdef TARGET_PC
/* PC: the message pool (128) and the manager's queue (128) can run out, which the N64's even retraces never
 * let happen: on a loaded host the manager thread falls behind, and each retrace message it then finds queues
 * another change per sound source. A change that finds no message, or no room in the queue, is dropped (the
 * next retrace sends the sound's values again) and its message given back, instead of a write through NULL
 * or a message lost for good (docs/port/QA.md "Integration soak"). */
#define PC_MSG_OR_RETURN(m) if((m)==0)return;
#define PC_SEND_OR_GIVE_BACK(q,m,e) if(osSendMesg(q,m,0)!=0){(e)->msgCount--;(m)->used=0;}
#else
#define PC_MSG_OR_RETURN(m)
#define PC_SEND_OR_GIVE_BACK(q,m,e) osSendMesg(q,m,0)
#endif
void entity_transform_calc(s32 h,f32 x,f32 y,f32 z,f32 w) {
    Msg *m=0;
    Entity *e=func_80091BA8(h);
    if(e!=0){
        if((x!=-2.0f && x!=e->x) || (y!=-2.0f && y!=e->y) ||
           (z!=-2.0f && z!=e->z) || (w!=-2.0f && w!=e->w)) {
            m=func_80091B00();PC_MSG_OR_RETURN(m)
            m->type=4;
            m->ent=e;
            e->msgCount++;
            if(x!=-2.0f && x!=e->x)m->payload.value.x=x; else m->payload.value.x=-2.0f;
            if(y!=-2.0f && y!=e->y)m->payload.value.y=y; else m->payload.value.y=-2.0f;
            if(z!=-2.0f && z!=e->z)m->payload.value.z=z; else m->payload.value.z=-2.0f;
            if(w!=-2.0f && w!=e->w)m->payload.value.w=w; else m->payload.value.w=-2.0f;
        }
    }
    if(m!=0)PC_SEND_OR_GIVE_BACK(&D_801427A8,m,e);
}
void client_sync(s32 h,f32 opacity) {
    osRecvMesg(&D_80142728,0,1);
    entity_transform_calc(h,opacity<0.0f ? 0.0f : opacity>1.0f ? 1.0f : opacity,
                          -2.0f,-2.0f,-2.0f);
    osSendMesg(&D_80142728,0,0);
}
void entity_hierarchy_update(s32 h,f32 value) {
    osRecvMesg(&D_80142728,0,1);
    entity_transform_calc(h,-2.0f,-2.0f,-2.0f,value);
    osSendMesg(&D_80142728,0,0);
}
void scheduler_recv(s32 h) {
    Msg *m=0;
    Entity *e;
    osRecvMesg(&D_80142728,0,1);
    e=func_80091BA8(h);
    if(e!=0) {
        m=func_80091B00();
        m->type=6;
        m->payload.target=e;
        e->msgCount++;
    }
    osSendMesg(&D_80142728,0,0);
    if(m!=0)osSendMesg(&D_801427A8,m,0);
}

void stat_lap_complete(s32 h,f32 value) {
    osRecvMesg(&D_80142728,0,1);
    entity_transform_calc(h,-2.0f,value<-1.0f ? -1.0f : value>1.0f ? 1.0f : value,-2.0f,-2.0f);
    osSendMesg(&D_80142728,0,0);
}
void game_timer_resume(s32 h,f32 value) {
    osRecvMesg(&D_80142728,0,1);
    entity_transform_calc(h,-2.0f,-2.0f,value<-1.0f ? -1.0f : value>1.0f ? 1.0f : value,-2.0f);
    osSendMesg(&D_80142728,0,0);
}
