/* ConvertTag and AudStopEffect: an fx tag from its handle, and the request to stop a sound (MAT3 audiomgr.c's
 * _ConvertTag and AudStopEffect). */
#include "game/game.h"
#include "game/audiomgr.h"

/* gTagMask as a signed word: the index it masks is sign-extended, as this file always read it. */
VIEW(s32, gTagMask);
#define gTagMask gTagMask_view

/* Declared without prototypes, as this file always had them (a prototype would change the calls' code). */
s32 osRecvMesg();
s32 osSendMesg();

/* The fx tag of handle tag, or NULL for -1 or a handle whose tag has been freed since (its generation, the
 * bits above gTagMask, no longer matches). */
tFxSequence *ConvertTag(s32 tag)
{
    if (tag == -1) {
        return 0;
    }
    if ((s32) gFxTags[tag & gTagMask].id != tag) {
        return 0;
    }
    return &gFxTags[tag & gTagMask];
}

/* Asks the audio manager thread to stop the sound of fx tag handle tag (a stale handle is ignored). */
void AudStopEffect(s32 tag)
{
    tAudioMsg *msg = 0;
    tFxSequence *fx_tag;

    osRecvMesg(&audio_lock_queue, 0, 1);
    fx_tag = ConvertTag(tag);
    if (fx_tag != 0) {
        msg = GetCmd(); PC_CMD_OR_UNLOCK(msg)
        msg->hdr.cmd = kAudioCmdStopEffect;
        msg->stopEffect.tag = fx_tag;
        fx_tag->numCmds++;
    }
    osSendMesg(&audio_lock_queue, 0, 0);
    if (msg != 0) {
        PC_SEND_OR_GIVE_BACK(&audio_command_queue, msg, fx_tag);
    }
}
