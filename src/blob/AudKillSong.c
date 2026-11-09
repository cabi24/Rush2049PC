/* AudKillSong: the sound manager's request to stop the song (MAT3 audiomgr.c).
 *
 * Like every request of the sound manager, it fills a message under the audio lock and posts it to the audio
 * manager thread, which stops the song (AudioSongStop) when it runs it. */
#include "game/game.h"
#include "game/audiomgr.h"

s32 osRecvMesg(OSMesgQueue *queue, OSMesg *message, s32 flags);
s32 osSendMesg(OSMesgQueue *queue, OSMesg message, s32 flags);
tAudioMsg *GetCmd(void);

/* Asks the audio manager thread to stop the playing song. */
void AudKillSong(void) {
    tAudioMsg *msg_to_send;
    tAudioMsg *msg;

    osRecvMesg(&audio_lock_queue, NULL, 1);
    msg = GetCmd(); PC_CMD_OR_UNLOCK(msg)
    msg_to_send = msg;
    msg->hdr.cmd = kAudioCmdKillSong;
    osSendMesg(&audio_lock_queue, NULL, 0);
    PC_CMD_SEND(&audio_command_queue, msg_to_send);
}
