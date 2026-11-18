/* AudKillSong: the sound manager's request to stop the song (MAT3 audiomgr.c).
 *
 * Like every request of the sound manager, it fills a message under the audio lock and posts it to the audio
 * manager thread, which stops the song (AudioSongStop) when it runs it. */
#include "game/game.h"
#include "game/audiomgr.h"


/* Asks the audio manager thread to stop the playing song. */
void AudKillSong(void) {
    tAudioMsg *msg_to_send;
    tAudioMsg *msg;

    lock_take(&audio_lock_queue);
    msg = GetCmd(); PC_CMD_OR_UNLOCK(msg)
    msg_to_send = msg;
    msg->hdr.cmd = kAudioCmdKillSong;
    lock_give(&audio_lock_queue);
    PC_CMD_SEND(&audio_command_queue, msg_to_send);
}
