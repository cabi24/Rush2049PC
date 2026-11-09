/* AudMono: the sound manager's request to switch between mono and stereo output (MAT3 audiomgr.c).
 *
 * Posts the request to the audio manager thread, which passes it to the sound server (AudioSetMono) and from there
 * to MusyX (sound_set_mono). */
#include "game/game.h"
#include "game/audiomgr.h"

s32 osRecvMesg(OSMesgQueue *queue, OSMesg *message, s32 flags);
s32 osSendMesg(OSMesgQueue *queue, OSMesg message, s32 flags);

/* Asks for mono output (mono nonzero) or stereo. */
void AudMono(s32 mono) {
    tAudioMsg *msg_to_send;
    tAudioMsg *msg;

    osRecvMesg(&audio_lock_queue, NULL, 1);
    msg = GetCmd(); PC_CMD_OR_UNLOCK(msg)
    msg_to_send = msg;
    msg->hdr.cmd = kAudioCmdMono;
    msg->mono.mono = (s8) mono;
    osSendMesg(&audio_lock_queue, NULL, 0);
    PC_CMD_SEND(&audio_command_queue, msg_to_send);
}
