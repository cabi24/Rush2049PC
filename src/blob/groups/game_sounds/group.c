#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;

#undef M2C_FIELD
#define M2C_FIELD(p,t,o) (*(t)((char *)(p)+(o)))
VIEW(s32, audio_lock_queue);
#define audio_lock_queue audio_lock_queue_view
VIEW(s32, audio_command_queue);
#define audio_command_queue audio_command_queue_view
extern void *GetCmd(void);
extern s32 osRecvMesg(OSMesgQueue *,void *,s32);
extern s32 osSendMesg(OSMesgQueue *,void *,s32);

VIEW(s8[], gOptions);
#define gOptions gOptions_view
/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). A volume change (command 10) is then dropped: with no
 * message the lock is given back and nothing is sent, and a message the full queue refuses is given back (its
 * in-use byte, +3); the volume stays as it was until the next change. */
#define PC_CMD_OR_UNLOCK(m) if((m)==0){osSendMesg((OSMesgQueue *)&audio_lock_queue,NULL,0);return;}
#define PC_CMD_SEND(q,m) if(osSendMesg(q,m,0)!=0)M2C_FIELD(m,s8 *,3)=0
void AudSetMasterVol(f32 blend,f32 amount,s32 first,s32 second) {
    f32 var_f0;
    void *temp_v0;

    osRecvMesg((OSMesgQueue *) &audio_lock_queue, NULL, 1);
    temp_v0 = GetCmd(); PC_CMD_OR_UNLOCK(temp_v0)
    M2C_FIELD(temp_v0, s8 *, 2) = 0xA;
    if (blend < 0.0f) {
        M2C_FIELD(temp_v0, f32 *, 4) = 0.0f;
    } else {
        if (blend > 1.0f) {
            var_f0 = 1.0f;
        } else {
            var_f0 = blend;
        }
        M2C_FIELD(temp_v0, f32 *, 4) = var_f0;
    }
    if (amount < 0.0f) {
        M2C_FIELD(temp_v0, f32 *, 8) = 0.0f;
    } else {
        M2C_FIELD(temp_v0, f32 *, 8) = (f32) amount;
    }
    M2C_FIELD(temp_v0, s8 *, 0xC) = (s8) first;
    M2C_FIELD(temp_v0, s8 *, 0xD) = (s8) second;
    osSendMesg((OSMesgQueue *) &audio_lock_queue, NULL, 0);
    PC_CMD_SEND((OSMesgQueue *) &audio_command_queue, temp_v0);
}

void AudSetMusicVol(f32 blend, f32 amount)
{
    AudSetMasterVol(blend, amount, 1, 0);
}

void AudSetFxVol(f32 blend, f32 amount)
{
    AudSetMasterVol(blend, amount, 0, 1);
}

void PauseGameMusic(void)
{
    AudSetMasterVol(0.0f, 0.05f, 1, 0);
}

void EnableGameSounds(s32 flag) {
    f32 blend;
    gGameSoundsEnabled=flag;
    if(flag) blend=0.0f;
    else blend=(f32)gOptions[13]/10.0f;
    AudSetMasterVol(blend,fx_volume_fade_time,0,1);
}
