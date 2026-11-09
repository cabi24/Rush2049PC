/* The sound manager's changes of a playing sound: volume, pans and pitch (MAT3 audiomgr.c's _AudChangeEffect,
 * AudChangeEffectVol, AudChangeEffectPitch; the two pans' setters are the N64's). */
#include "game/game.h"
#include "game/audiomgr.h"

s32 osRecvMesg(void *,void *,s32);
s32 osSendMesg(void *,void *,s32);
/* PC: the message pool (128) and the manager's queue (128) can run out, which the N64's even retraces never
 * let happen: on a loaded host the manager thread falls behind, and each retrace message it then finds queues
 * another change per sound source. A change that finds no message, or no room in the queue, is dropped (the
 * next retrace sends the sound's values again) and its message given back, instead of a write through NULL
 * or a message lost for good (docs/port/QA.md "Integration soak"). */
#define PC_MSG_OR_RETURN(m) if((m)==0)return;
#define PC_SEND_OR_GIVE_BACK(q,m,e) if(osSendMesg(q,m,0)!=0){(e)->numCmds--;(m)->hdr.used=0;}

/* Asks the audio manager thread to change the sound of fx tag handle tag: each of volume, lrpan, fbpan and pitch
 * that is not AUDIO_NO_CHANGE and differs from the tag's last value; nothing is sent when none does. The caller
 * holds the audio lock. ProcessEmitters calls it each retrace for every emitter. */
void AudChangeEffect(s32 tag,f32 volume,f32 lrpan,f32 fbpan,f32 pitch) {
    tAudioMsg *msg=0;
    tFxSequence *fx_tag=ConvertTag(tag);
    if(fx_tag!=0){
        if((volume!=-2.0f && volume!=fx_tag->vol) || (lrpan!=-2.0f && lrpan!=fx_tag->lrpan) ||
           (fbpan!=-2.0f && fbpan!=fx_tag->fbpan) || (pitch!=-2.0f && pitch!=fx_tag->pitch)) {
            msg=GetCmd();PC_MSG_OR_RETURN(msg)
            msg->hdr.cmd=kAudioCmdChangeEffect;
            msg->changeEffect.tag=fx_tag;
            fx_tag->numCmds++;
            if(volume!=-2.0f && volume!=fx_tag->vol)msg->changeEffect.vol=volume; else msg->changeEffect.vol=-2.0f;
            if(lrpan!=-2.0f && lrpan!=fx_tag->lrpan)msg->changeEffect.lrpan=lrpan; else msg->changeEffect.lrpan=-2.0f;
            if(fbpan!=-2.0f && fbpan!=fx_tag->fbpan)msg->changeEffect.fbpan=fbpan; else msg->changeEffect.fbpan=-2.0f;
            if(pitch!=-2.0f && pitch!=fx_tag->pitch)msg->changeEffect.pitch=pitch; else msg->changeEffect.pitch=-2.0f;
        }
    }
    if(msg!=0)PC_SEND_OR_GIVE_BACK(&audio_command_queue,msg,fx_tag);
}
/* Sets the volume of the sound of fx tag handle tag (clamped to 0..1). */
void AudChangeEffectVol(s32 tag,f32 volume) {
    osRecvMesg(&audio_lock_queue,0,1);
    AudChangeEffect(tag,volume<0.0f ? 0.0f : volume>1.0f ? 1.0f : volume,
                          -2.0f,-2.0f,-2.0f);
    osSendMesg(&audio_lock_queue,0,0);
}
/* Sets the pitch of the sound of fx tag handle tag (1: its own pitch). */
void AudChangeEffectPitch(s32 tag,f32 pitch) {
    osRecvMesg(&audio_lock_queue,0,1);
    AudChangeEffect(tag,-2.0f,-2.0f,-2.0f,pitch);
    osSendMesg(&audio_lock_queue,0,0);
}

/* Sets the left/right pan of the sound of fx tag handle tag (clamped to -1..1). */
void change_effect_lr_pan(s32 tag,f32 lrpan) {
    osRecvMesg(&audio_lock_queue,0,1);
    AudChangeEffect(tag,-2.0f,lrpan<-1.0f ? -1.0f : lrpan>1.0f ? 1.0f : lrpan,-2.0f,-2.0f);
    osSendMesg(&audio_lock_queue,0,0);
}
/* Sets the front/back pan of the sound of fx tag handle tag (clamped to -1..1). */
void change_effect_fb_pan(s32 tag,f32 fbpan) {
    osRecvMesg(&audio_lock_queue,0,1);
    AudChangeEffect(tag,-2.0f,-2.0f,fbpan<-1.0f ? -1.0f : fbpan>1.0f ? 1.0f : fbpan,-2.0f);
    osSendMesg(&audio_lock_queue,0,0);
}
