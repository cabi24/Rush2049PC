/*
 * Entity handle lookup and the small message/camera-slot helpers built on it.
 * Hand-written from the assembly (round 5).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

/* PC: the audio manager's records have host-sized links and pointers (audiomgr_thread.c's views): an emitter
 * (camera_target_track's Cam) is 56 bytes, an fx tag (tFxSequence) 88, a command (tAudioMsg) 32 (docs/port/SYNTH.md). */
typedef struct CamSlot {
    void *link[2];
    u8 pad10[4];
    /* 0x14 */ f32 x, y, z;
    u8 pad20[4];
    /* 0x24 */ f32 blend;
    u8 pad28[4];
    /* 0x2C */ f32 extra;
} CamSlot;

typedef struct Entity {
    void *pad0[3];
    /* 0x18 */ s32 id;
    /* 0x1C */ s32 state;
    u8 pad20[6];
    /* 0x26 */ u8 msgCount;
    u8 pad27[0x48 - 0x27];
    /* 0x48 */ s32 link;
    /* 0x50 */ CamSlot *cam;
} Entity; /* 0x58 */

typedef struct Msg {
    s16 id;
    s8 type;
    s8 used;
    /* 0x08 */ Entity *ent;
    u8 pad10[16];
} Msg; /* 0x20 */

extern Entity *D_80110244;
extern s32 D_80146104;
/* PC: the sound manager's messages are PC data, PC_SND_MSG_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9, AUDIO.md 10.4); PC_SND_MSG_LOOP(128) is the run's count: 128 at original. */
#define D_80142DD8 (*(Msg (*)[PC_SND_MSG_SLOTS])pc_snd_msgs)
#define PC_SND_MSG_LOOP(n) (pc_snd_messages())
extern s32 D_80142728;
extern s32 D_801427A8;

s32 osRecvMesg();
s32 osSendMesg();
s32 entity_state_check();

Entity *func_80091BA8(s32 h);
void func_800BF01C(CamSlot *c);
Msg *func_80091B00(void);

Entity *func_80091BA8(s32 h)
{
    if (h == -1) {
        return 0;
    }
    if (D_80110244[h & D_80146104].id != h) {
        return 0;
    }
    return &D_80110244[h & D_80146104];
}

void func_800BF01C(CamSlot *c);

Msg *func_80091B00(void);

void results_screen_update(s32 h);
s32 results_time_display(s32 h);
void scheduler_recv(s32 h);

void results_screen_update(s32 h);

s32 results_time_display(s32 h);

/* PC: a stop that finds the message pool empty, or the manager's queue full, as entity_transform_calc
 * (transform/group.c) handles a change. */
#define PC_MSG_OR_UNLOCK(m) if((m)==0){osSendMesg(&D_80142728,0,0);return;}
#define PC_SEND_OR_GIVE_BACK(q,m,e) if(osSendMesg(q,m,0)!=0){(e)->msgCount--;(m)->used=0;}
void scheduler_recv(s32 h)
{
    Msg *m = 0;
    Entity *e;

    osRecvMesg(&D_80142728, 0, 1);
    e = func_80091BA8(h);
    if (e != 0) {
        m = func_80091B00(); PC_MSG_OR_UNLOCK(m)
        m->type = 6;
        m->ent = e;
        e->msgCount++;
    }
    osSendMesg(&D_80142728, 0, 0);
    if (m != 0) {
        PC_SEND_OR_GIVE_BACK(&D_801427A8, m, e);
    }
}

