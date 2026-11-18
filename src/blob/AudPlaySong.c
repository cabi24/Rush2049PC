/* AudPlaySong: the sound manager's request to play a song (MAT3 audiomgr.c). */
#include "game/game.h"
#include "game/audiomgr.h"


/* Asks the audio manager thread to play song (an index of gSongNames), looped if loop is nonzero. The thread
 * starts it (AudioSongPlay) only when no song is playing: AudKillSong first to change songs. */
void AudPlaySong(s32 song, s32 loop) {
    tAudioMsg *msg_to_send;
    tAudioMsg *msg;

    lock_take(&audio_lock_queue);
    msg = GetCmd(); PC_CMD_OR_UNLOCK(msg)
    msg_to_send = msg;
    msg->hdr.cmd = kAudioCmdPlaySong;
    msg->playSong.song = song;
    msg->playSong.loop = (s8) loop;
    lock_give(&audio_lock_queue);
    PC_CMD_SEND(&audio_command_queue, msg_to_send);
}
