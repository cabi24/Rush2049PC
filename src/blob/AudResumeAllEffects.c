/* AudResumeAllEffects: the sound manager's request to resume the paused effects (MAT3 audiomgr.c). */
#include "game/game.h"
#include "game/audiomgr.h"


/* Asks the audio manager thread to give every paused effect its pitch back. Since AudPauseAllEffects stops the
 * effects rather than pausing them, there is normally nothing paused to resume. */
void AudResumeAllEffects(void) {
    tAudioMsg *msg_to_send;
    tAudioMsg *msg;

    lock_take(&audio_lock_queue);
    msg = GetCmd(); PC_CMD_OR_UNLOCK(msg)
    msg_to_send = msg;
    msg->hdr.cmd = kAudioCmdResumeAllEffects;
    lock_give(&audio_lock_queue);
    PC_CMD_SEND(&audio_command_queue, msg_to_send);
}
