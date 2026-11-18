/* AudStopAllEffects: the sound manager's request to stop every effect (MAT3 audiomgr.c). */
#include "game/game.h"
#include "game/audiomgr.h"


/* Asks the audio manager thread to stop every effect: it stops the sequences, has the sound server stop every
 * instance (AudioStopAllEffects) and frees every playing and waiting fx tag. */
void AudStopAllEffects(void) {
    tAudioMsg *msg_to_send;
    tAudioMsg *msg;

    lock_take(&audio_lock_queue);
    msg = GetCmd(); PC_CMD_OR_UNLOCK(msg)
    msg_to_send = msg;
    msg->hdr.cmd = kAudioCmdStopAllEffects;
    lock_give(&audio_lock_queue);
    PC_CMD_SEND(&audio_command_queue, msg_to_send);
}
