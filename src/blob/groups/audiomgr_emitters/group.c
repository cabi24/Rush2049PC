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

s32 osRecvMesg(void *, void *, s32);
s32 osSendMesg(void *, void *, s32);

Entity *ConvertTag(s32 h);
/* The manifest owner of ValidateEmitter is an empty deleted-procedure stub (src/blob/ValidateEmitter.c),
 * which this group's calls bind to on PC. The body written here gets its own name
 * (docs/port/PROTOTYPES.md). */
#define ValidateEmitter pc_ValidateEmitter

Entity *ConvertTag(s32 h);

void ValidateEmitter(CamSlot *c)
{
    if(c) {}
    if (0) {
        switch (gTagMask) {
        case 0:
            gTagMask = 1;
            break;
        case 1:
            gTagMask = 2;
            break;
        case 2:
            gTagMask = 3;
            break;
        case 3:
            gTagMask = 4;
            break;
        }
    }
}

void AudStopEmitter(s32 h)
{
    Entity *e;

    osRecvMesg(&audio_lock_queue, 0, 1);
    e = ConvertTag(h);
    if (e == 0) {
        osSendMesg(&audio_lock_queue, 0, 0);
        return;
    }
    ValidateEmitter(e->cam);
    osSendMesg(&audio_lock_queue, 0, 0);
    AudStopEffect(h);
}

s32 AudEmitterPlaying(s32 h)
{
    Entity *e;

    osRecvMesg(&audio_lock_queue, 0, 1);
    e = ConvertTag(h);
    if (e == 0) {
        osSendMesg(&audio_lock_queue, 0, 0);
        return 0;
    }
    ValidateEmitter(e->cam);
    osSendMesg(&audio_lock_queue, 0, 0);
    return AudEffectPlaying(h);
}

void AudUpdateEmitter(s32 h, f32 *p, intptr_t unused, f32 a, f32 b)
{
    Entity *e;
    CamSlot *c;
    f32 value;

    osRecvMesg(&audio_lock_queue, 0, 1);
    e = ConvertTag(h);
    if (e != 0) {
        c = e->cam;
        ValidateEmitter(c);
        c->x = p[0];
        c->y = p[1];
        c->z = p[2];
        if (a < 0.0f) {
            c->blend = 0.0f;
        } else {
            if (a > 1.0f) value = 1.0f;
            else value = a;
            c->blend = value;
        }
        c->extra = b;
    }
    osSendMesg(&audio_lock_queue, 0, 0);
}

