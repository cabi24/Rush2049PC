/* AudStopAllEffects: the sound manager's request to stop every effect (MAT3 audiomgr.c). */
#include "game/game.h"
#include "game/audiomgr.h"

s32 osRecvMesg(OSMesgQueue *queue, OSMesg *message, s32 flags);
s32 osSendMesg(OSMesgQueue *queue, OSMesg message, s32 flags);
tAudioMsg *GetCmd(void);

/* Asks the audio manager thread to stop every effect: it stops the sequences, has the sound server stop every
 * instance (AudioStopAllEffects) and frees every playing and waiting fx tag. */
void AudStopAllEffects(void) {
    tAudioMsg *msg_to_send;
    tAudioMsg *msg;

    osRecvMesg(&audio_lock_queue, NULL, 1);
    msg = GetCmd(); PC_CMD_OR_UNLOCK(msg)
    msg_to_send = msg;
    msg->hdr.cmd = kAudioCmdStopAllEffects;
    osSendMesg(&audio_lock_queue, NULL, 0);
    PC_CMD_SEND(&audio_command_queue, msg_to_send);
}
