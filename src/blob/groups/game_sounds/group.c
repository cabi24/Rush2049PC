/* The master volume requests of the sound manager (MAT3 audiomgr.c's _AudSetMasterVol, AudSetMusicVol,
 * AudSetFxVol) and the race's music and sound switches (MAT3 audio.c's PauseGameMusic, EnableGameSounds). */
#include "game/game.h"
#include "game/audiomgr.h"

s32 osRecvMesg(OSMesgQueue *queue, OSMesg *message, s32 flags);
s32 osSendMesg(OSMesgQueue *queue, OSMesg message, s32 flags);
tAudioMsg *GetCmd(void);

VIEW(s8[], gOptions);
#define gOptions gOptions_view

/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). A volume change is then dropped: with no message the lock is
 * given back and nothing is sent, and a message the full queue refuses is given back; the volume stays as it was
 * until the next change. */
#define PC_CMD_OR_UNLOCK(m) if((m)==0){osSendMesg(&audio_lock_queue,NULL,0);return;}
#define PC_CMD_SEND(q,m) if(osSendMesg(q,m,0)!=0)(m)->hdr.used=0

/* Asks the audio manager thread to fade the music (music nonzero) and/or the effects (sfx nonzero) to volume
 * (clamped to 0..1) over time seconds (not below 0). */
void AudSetMasterVol(f32 volume,f32 time,s32 music,s32 sfx) {
    f32 clamped;
    tAudioMsg *msg;

    osRecvMesg(&audio_lock_queue, NULL, 1);
    msg = GetCmd(); PC_CMD_OR_UNLOCK(msg)
    msg->hdr.cmd = kAudioCmdSetMasterVol;
    if (volume < 0.0f) {
        msg->setMasterVol.vol = 0.0f;
    } else {
        if (volume > 1.0f) {
            clamped = 1.0f;
        } else {
            clamped = volume;
        }
        msg->setMasterVol.vol = clamped;
    }
    if (time < 0.0f) {
        msg->setMasterVol.time = 0.0f;
    } else {
        msg->setMasterVol.time = (f32) time;
    }
    msg->setMasterVol.music = (s8) music;
    msg->setMasterVol.sfx = (s8) sfx;
    osSendMesg(&audio_lock_queue, NULL, 0);
    PC_CMD_SEND(&audio_command_queue, msg);
}

/* Fades the music to volume over time seconds. */
void AudSetMusicVol(f32 volume, f32 time)
{
    AudSetMasterVol(volume, time, 1, 0);
}

/* Fades the effects to volume over time seconds. */
void AudSetFxVol(f32 volume, f32 time)
{
    AudSetMasterVol(volume, time, 0, 1);
}

/* Silences the music quickly (a twentieth of a second), for the pause screen; ResumeGameMusic brings it back.
 * (Empty in MAT3.) */
void PauseGameMusic(void)
{
    AudSetMasterVol(0.0f, 0.05f, 1, 0);
}

/* Turns the race's sounds on or off (gGameSoundsEnabled, which PlayGameSound, EmitGameSound and the car sounds
 * test), at the race's start (PrePlay: off in the demo). Turning them on also silences the effects, which
 * StartListeners then fades up to the options' volume; turning them off sets the options' volume at once, for the
 * menu sounds over the demo. */
void EnableGameSounds(s32 enabled) {
    f32 volume;
    gGameSoundsEnabled=enabled;
    if(enabled) volume=0.0f;
    else volume=(f32)gOptions[kOptEffectsVolume]/10.0f;
    AudSetMasterVol(volume,fx_volume_fade_time,0,1);
}
