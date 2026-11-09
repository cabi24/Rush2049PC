/* AudResumeAllEffects: the sound manager's request to resume the paused effects (MAT3 audiomgr.c). */
#include "game/game.h"
#include "game/audiomgr.h"

s32 osRecvMesg(OSMesgQueue *queue, OSMesg *message, s32 flags);
s32 osSendMesg(OSMesgQueue *queue, OSMesg message, s32 flags);
tAudioMsg *GetCmd(void);

/* Asks the audio manager thread to give every paused effect its pitch back. Since AudPauseAllEffects stops the
 * effects rather than pausing them, there is normally nothing paused to resume. */
void AudResumeAllEffects(void) {
    tAudioMsg *msg_to_send;
    tAudioMsg *msg;

    osRecvMesg(&audio_lock_queue, NULL, 1);
    msg = GetCmd(); PC_CMD_OR_UNLOCK(msg)
    msg_to_send = msg;
    msg->hdr.cmd = kAudioCmdResumeAllEffects;
    osSendMesg(&audio_lock_queue, NULL, 0);
    PC_CMD_SEND(&audio_command_queue, msg_to_send);
}
