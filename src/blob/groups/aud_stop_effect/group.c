#include "game/game.h"

/* PC: the audio manager's records have host-sized links and pointers (audiomgr_thread.c's views): an emitter
 * (AudEmitEffect's Cam) is 56 bytes, an fx tag (tFxSequence) 88, a command (tAudioMsg) 32 (docs/port/SYNTH.md). */
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

VIEW(s32, gTagMask);
#define gTagMask gTagMask_view
/* PC: the sound manager's messages are PC data, PC_SND_MSG_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9, AUDIO.md 10.4); PC_SND_MSG_LOOP(128) is the run's count: 128 at original. */
#define audiomgr_gCmds (*(Msg (*)[PC_SND_MSG_SLOTS])pc_snd_msgs)
#define PC_SND_MSG_LOOP(n) (pc_snd_messages())
VIEW(s32, audio_lock_queue);
#define audio_lock_queue audio_lock_queue_view
VIEW(s32, audio_command_queue);
#define audio_command_queue audio_command_queue_view

s32 osRecvMesg();
s32 osSendMesg();

Entity *ConvertTag(s32 h);

Msg *GetCmd(void);

Entity *ConvertTag(s32 h)
{
    if (h == -1) {
        return 0;
    }
    if (gFxTags[h & gTagMask].id != h) {
        return 0;
    }
    return &gFxTags[h & gTagMask];
}

Msg *GetCmd(void);

/* PC: a stop that finds the message pool empty, or the manager's queue full, as AudChangeEffect
 * (transform/group.c) handles a change. */
#define PC_MSG_OR_UNLOCK(m) if((m)==0){osSendMesg(&audio_lock_queue,0,0);return;}
#define PC_SEND_OR_GIVE_BACK(q,m,e) if(osSendMesg(q,m,0)!=0){(e)->msgCount--;(m)->used=0;}
void AudStopEffect(s32 h)
{
    Msg *m = 0;
    Entity *e;

    osRecvMesg(&audio_lock_queue, 0, 1);
    e = ConvertTag(h);
    if (e != 0) {
        m = GetCmd(); PC_MSG_OR_UNLOCK(m)
        m->type = 6;
        m->ent = e;
        e->msgCount++;
    }
    osSendMesg(&audio_lock_queue, 0, 0);
    if (m != 0) {
        PC_SEND_OR_GIVE_BACK(&audio_command_queue, m, e);
    }
}

