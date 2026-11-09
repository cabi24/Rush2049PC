/* The sound manager's requests that start a sound: AudPlayEffect, AudPlayEffectVP and the 3D AudEmitEffect, with
 * GetFxTag, the tag every one of them takes (MAT3 audiomgr.c). Each fills a play-effect message under the audio
 * lock and posts it to the audio manager thread; the sound gets a voice at the next priority pass. */
#include "game/game.h"
#include "game/audiomgr.h"

s32 osRecvMesg(OSMesgQueue *queue,OSMesg *message,s32 flags);s32 osSendMesg(OSMesgQueue *queue,OSMesg message,s32 flags);

#define PC_TAGS_MOST() pc_snd_most(PC_SND_MOST_TAGS, gNumFxTags - (s32)gFreeFxTags.count);
#define PC_EMITTERS_MOST(list) pc_snd_most(PC_SND_MOST_EMITTERS, (s32)(list).count);

/* Takes the first free fx tag (MAT3's _GetFxTag): gives it a handle of the next generation (gNextFxId, wrapping
 * to 1 before it reaches the bits above gTagMask), clears its times and values (volume 0, pans 0, pitch 1, start
 * -1, no data) and puts it on gInterruptedFxTags, where waiting tags are kept. The free list is not checked:
 * the callers' PC_TAG_OR_UNLOCK do it on the PC. */
tFxSequence *GetFxTag(void) {
 tFxSequence *tag=(tFxSequence*)gFreeFxTags.head;
 if(tag==0){}
 tag->id=(gNextFxId<<gTagShift)|(tag->id&gTagMask);
 if(gNextFxId>=((~gTagMask)>>(gTagShift+1)))gNextFxId=1;
 else gNextFxId++;
 tag->data=0;tag->vol=0.0f;tag->lrpan=0.0f;tag->fbpan=0.0f;
 tag->startTime=-1.0f;tag->pitch=1.0f;
 RemoveLLE(&gFreeFxTags,&tag->ll); PC_TAGS_MOST()
 InsertLLE(&gInterruptedFxTags,&tag->ll,gInterruptedFxTags.head);
 tag->list=&gInterruptedFxTags;
 return tag;
}
/* PC: the message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6; transform/group.c's
 * AudChangeEffect says when): a request that finds no message gives the lock back and returns -1 (an emitter's
 * also gives its emitter back), and a message the full queue refuses is given back. */
#define PC_OBJ_OR_UNLOCK(o) if((o)==0){osSendMesg(&audio_lock_queue,0,0);return (u32)-1;}
#define PC_OBJ_OR_UNLOCK_CAM(o,c) \
    if((o)==0){InsertLLE(&gFreeEmitters,(struct Link*)(c),gFreeEmitters.head);osSendMesg(&audio_lock_queue,0,0);return -1;}
/* So can the tags (gFreeFxTags: 80 with the Expansion Pak, 16 without, as AudioInit makes them), which
 * GetFxTag takes from the list's head unchecked: in a race of more cars than the N64's 6 their engines can take
 * the last one (docs/port/MORECARS.md 6). A start that finds none gives its message back and is not started
 * either. */
#define PC_TAG_OR_UNLOCK(o) if(gFreeFxTags.head==0){(o)->hdr.used=0;osSendMesg(&audio_lock_queue,0,0);return (u32)-1;}
#define PC_TAG_OR_UNLOCK_CAM(o,c) if(gFreeFxTags.head==0){(o)->hdr.used=0; \
    InsertLLE(&gFreeEmitters,(struct Link*)(c),gFreeEmitters.head);osSendMesg(&audio_lock_queue,0,0);return -1;}

/* Asks for effect fxid at full volume, centred: subId tells the caller's instances apart (a car's slot ...),
 * replace says what happens to a playing one with the same fxid and subId (kFxIndividual, kFxReplace,
 * kFxNoReplace), priority ranks it (WeightedPriority). Returns the fx tag handle, or -1 (PC: the pools ran out). */
u32 AudPlayEffect(u32 fxid,u32 subId,u32 replace,u8 priority) {
 tAudioMsg *msg;u32 result;
 osRecvMesg(&audio_lock_queue,0,1);
 msg=GetCmd();PC_OBJ_OR_UNLOCK(msg)PC_TAG_OR_UNLOCK(msg)msg->hdr.cmd=kAudioCmdPlayEffect;msg->playEffect.replace=replace;
 msg->playEffect.tag=GetFxTag();msg->playEffect.tag->type=kFxStandalone;
 msg->playEffect.tag->fxid=fxid;msg->playEffect.tag->subId=subId;msg->playEffect.tag->priority=priority;
 msg->playEffect.tag->state=kFxPending;msg->playEffect.tag->paused=0;
 msg->playEffect.tag->numCmds=1;
 msg->playEffect.tag->loop=gEffectLoop[fxid];
 msg->playEffect.vol=1.0f;
 msg->playEffect.lrpan=0.0f;
 msg->playEffect.fbpan=-2.0f;
 result=msg->playEffect.tag->id;
 osSendMesg(&audio_lock_queue,0,0);
 PC_CMD_SEND(&audio_command_queue,msg);
 return result;
}
/* AudPlayEffect with a volume (clamped to 0..1), a left/right and a front/back pan (each clamped to -1..1). */
u32 AudPlayEffectVP(u32 fxid,u32 subId,u32 replace,u8 priority,f32 volume,f32 lrpan,f32 fbpan) {
 tAudioMsg *msg;u32 result;
 osRecvMesg(&audio_lock_queue,0,1);
 msg=GetCmd();PC_OBJ_OR_UNLOCK(msg)PC_TAG_OR_UNLOCK(msg)msg->hdr.cmd=kAudioCmdPlayEffect;msg->playEffect.replace=replace;
 msg->playEffect.tag=GetFxTag();msg->playEffect.tag->type=kFxStandalone;
 msg->playEffect.tag->fxid=fxid;msg->playEffect.tag->subId=subId;msg->playEffect.tag->priority=priority;
 msg->playEffect.tag->state=kFxPending;msg->playEffect.tag->paused=0;
 msg->playEffect.tag->numCmds=1;
 msg->playEffect.tag->loop=gEffectLoop[fxid];
 msg->playEffect.vol=(volume<0.0f)?0.0f:((1.0f<volume)?1.0f:volume);
 msg->playEffect.lrpan=(lrpan<-1.0f)?-1.0f:((1.0f<lrpan)?1.0f:lrpan);
 msg->playEffect.fbpan=(fbpan<-1.0f)?-1.0f:((1.0f<fbpan)?1.0f:fbpan);
 result=msg->playEffect.tag->id;
 osSendMesg(&audio_lock_queue,0,0);
 PC_CMD_SEND(&audio_command_queue,msg);
 return result;
}

/* Starts effect fxid as a sound source at pos: an emitter (NewEmitter) with fadeDistance, maxVol and minVol
 * (clamped to 0..1) and pitch 1, and a play-effect message for an emitter's tag that starts silent; each retrace
 * ProcessEmitters sets its volume and pans from the listeners. dir (a pointer, never read: MAT3 has neither it nor
 * fadeCompression) and fadeCompression are not read. subId, replace
 * and priority as AudPlayEffect's. Returns the fx tag handle (the emitter keeps it), or -1 when there is no
 * emitter (none at all, or none free). */
s32 AudEmitEffect(const f32 *pos,void *dir,f32 fadeDistance,f32 fadeCompression,f32 maxVol,f32 minVol,u32 fxid,u32 subId,u32 replace,u8 priority) {
 tAudioMsg *msg;tSfxEmitter *emitter;char unused_buf[4];u32 result;
 if(gEmitters==0)return -1;
 osRecvMesg(&audio_lock_queue,0,1);
 emitter=NewEmitter();
 if(emitter==0){pc_snd_count(PC_SND_NO_EMITTER);osSendMesg(&audio_lock_queue,0,0);return -1;}
 emitter->pos[0]=pos[0];
 emitter->pos[1]=pos[1];
 emitter->pos[2]=pos[2];
 emitter->fadeDistance=fadeDistance;
 emitter->maxVol=(maxVol<0.0f)?0.0f:((1.0f<maxVol)?1.0f:maxVol);
 emitter->minVol=(minVol<0.0f)?0.0f:((1.0f<minVol)?1.0f:minVol);
 emitter->basePitch=1.0f;
 msg=GetCmd();PC_OBJ_OR_UNLOCK_CAM(msg,emitter)PC_TAG_OR_UNLOCK_CAM(msg,emitter)msg->hdr.cmd=kAudioCmdPlayEffect;msg->playEffect.replace=replace;
 msg->playEffect.tag=GetFxTag();msg->playEffect.tag->type=kFxEmitter;
 msg->playEffect.tag->fxid=fxid;msg->playEffect.tag->subId=subId;msg->playEffect.tag->priority=priority;
 msg->playEffect.tag->state=kFxPending;msg->playEffect.tag->paused=0;
 msg->playEffect.tag->numCmds=1;
 msg->playEffect.tag->loop=gEffectLoop[fxid];
 msg->playEffect.vol=0.0f;
 msg->playEffect.lrpan=0.0f;
 msg->playEffect.fbpan=-2.0f;
 msg->playEffect.tag->data=emitter;
 result=emitter->sndTag=msg->playEffect.tag->id;
 osSendMesg(&audio_lock_queue,0,0);
 PC_CMD_SEND(&audio_command_queue,msg);
 osRecvMesg(&audio_lock_queue,0,1);
 /* MAT3's _ActivateEmitter, unless the play message has already freed the emitter (free again) */
 if(emitter->free==0){InsertLLE(&gActiveEmitters,&emitter->ll,gActiveEmitters.head);emitter->active=1;PC_EMITTERS_MOST(gActiveEmitters)}
 osSendMesg(&audio_lock_queue,0,0);
 return result;
}
