/* AudMono: the sound manager's request to switch between mono and stereo output (MAT3 audiomgr.c).
 *
 * Posts the request to the audio manager thread, which passes it to the sound server (AudioSetMono) and from there
 * to MusyX (sound_set_mono). */
#include "game/game.h"
#include "game/audiomgr.h"


/* Asks for mono output (mono nonzero) or stereo. */
void AudMono(s32 mono) {
    tAudioMsg *msg_to_send;
    tAudioMsg *msg;

    lock_take(&audio_lock_queue);
    msg = GetCmd(); PC_CMD_OR_UNLOCK(msg)
    msg_to_send = msg;
    msg->hdr.cmd = kAudioCmdMono;
    msg->mono.mono = (s8) mono;
    lock_give(&audio_lock_queue);
    PC_CMD_SEND(&audio_command_queue, msg_to_send);
}
