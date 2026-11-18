/* AudPauseAllEffects: the sound manager's request to pause the effects, made by the pause screen (MAT3
 * audiomgr.c). */
#include "game/game.h"
#include "game/audiomgr.h"


/* Meant to pause every effect (the pause command sets their pitch to 0, AudResumeAllEffects gives it back).
 * Original game: it posts the stop-all command (7), as MAT3's does, not the pause command (8), which nothing
 * sends: the effects playing when the game is paused are stopped, not held. */
void AudPauseAllEffects(void) {
    tAudioMsg *msg_to_send;
    tAudioMsg *msg;

    lock_take(&audio_lock_queue);
    msg = GetCmd(); PC_CMD_OR_UNLOCK(msg)
    msg_to_send = msg;
    msg->hdr.cmd = kAudioCmdStopAllEffects;
    lock_give(&audio_lock_queue);
    PC_CMD_SEND(&audio_command_queue, msg_to_send);
}
