/* The sound manager's 3D sound sources, after they start: stopping, testing and moving one (MAT3 audiomgr.c's
 * AudStopEmitter, AudEmitterPlaying, AudUpdateEmitter and the empty _ValidateEmitter). */
#include "game/game.h"
#include "game/audiomgr.h"

s32 osRecvMesg(void *, void *, s32);
s32 osSendMesg(void *, void *, s32);

tFxSequence *ConvertTag(s32 tag);
/* src/blob/ValidateEmitter.c has the empty ValidateEmitter that the function tables reach; this file's calls go
 * to its own empty copy, which gets its own name (docs/port/PROTOTYPES.md). */
#define ValidateEmitter pc_ValidateEmitter

/* MAT3's _ValidateEmitter: a debug build's check of an emitter, empty in the shipped game. The `if (0)` block is a
 * leftover of the N64 build's matching (it keeps the function out of line); it never runs. The parameter is
 * system.h's struct CamSlot, an emitter. */
void ValidateEmitter(struct CamSlot *emitter)
{
    if(emitter) {}
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

/* Stops the 3D sound of fx tag handle tag (AudEmitEffect's): AudStopEffect, which frees its emitter with the
 * tag. A stale handle is ignored. */
void AudStopEmitter(s32 tag)
{
    tFxSequence *fx_tag;

    osRecvMesg(&audio_lock_queue, 0, 1);
    fx_tag = ConvertTag(tag);
    if (fx_tag == 0) {
        osSendMesg(&audio_lock_queue, 0, 0);
        return;
    }
    ValidateEmitter(fx_tag->data);
    osSendMesg(&audio_lock_queue, 0, 0);
    AudStopEffect(tag);
}

/* Whether the 3D sound of fx tag handle tag still plays or is still to play (AudEffectPlaying); 0 for a stale
 * handle. */
s32 AudEmitterPlaying(s32 tag)
{
    tFxSequence *fx_tag;

    osRecvMesg(&audio_lock_queue, 0, 1);
    fx_tag = ConvertTag(tag);
    if (fx_tag == 0) {
        osSendMesg(&audio_lock_queue, 0, 0);
        return 0;
    }
    ValidateEmitter(fx_tag->data);
    osSendMesg(&audio_lock_queue, 0, 0);
    return AudEffectPlaying(tag);
}

/* Moves the emitter of fx tag handle tag to pos and sets its loudest volume (clamped to 0..1) and its base pitch;
 * the next ProcessEmitters makes the sound follow. dir_unused is the N64's velocity argument, which MAT3 doesn't
 * have and nothing reads. */
void AudUpdateEmitter(s32 tag, f32 *pos, intptr_t dir_unused, f32 maxVol, f32 pitch)
{
    tFxSequence *fx_tag;
    tSfxEmitter *emitter;
    f32 clamped;

    osRecvMesg(&audio_lock_queue, 0, 1);
    fx_tag = ConvertTag(tag);
    if (fx_tag != 0) {
        emitter = fx_tag->data;
        ValidateEmitter(emitter);
        emitter->pos[0] = pos[0];
        emitter->pos[1] = pos[1];
        emitter->pos[2] = pos[2];
        if (maxVol < 0.0f) {
            emitter->maxVol = 0.0f;
        } else {
            if (maxVol > 1.0f) clamped = 1.0f;
            else clamped = maxVol;
            emitter->maxVol = clamped;
        }
        emitter->basePitch = pitch;
    }
    osSendMesg(&audio_lock_queue, 0, 0);
}
