/* AudPlaySong: the sound manager's request to play a song (MAT3 audiomgr.c). */
#include "game/game.h"
#include "game/audiomgr.h"

s32 osRecvMesg(OSMesgQueue *queue, OSMesg *message, s32 flags);
s32 osSendMesg(OSMesgQueue *queue, OSMesg message, s32 flags);
tAudioMsg *GetCmd(void);

/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). The request is then dropped: with no message the lock is given
 * back and nothing is sent, and a message the full queue refuses is given back. */
#define PC_CMD_OR_UNLOCK(m) if((m)==0){osSendMesg(&audio_lock_queue,NULL,0);return;}
#define PC_CMD_SEND(q,m) if(osSendMesg(q,m,0)!=0)(m)->hdr.used=0

/* Asks the audio manager thread to play song (an index of gSongNames), looped if loop is nonzero. The thread
 * starts it (AudioSongPlay) only when no song is playing: AudKillSong first to change songs. */
void AudPlaySong(s32 song, s32 loop) {
    tAudioMsg *msg_to_send;
    tAudioMsg *msg;

    osRecvMesg(&audio_lock_queue, NULL, 1);
    msg = GetCmd(); PC_CMD_OR_UNLOCK(msg)
    msg_to_send = msg;
    msg->hdr.cmd = kAudioCmdPlaySong;
    msg->playSong.song = song;
    msg->playSong.loop = (s8) loop;
    osSendMesg(&audio_lock_queue, NULL, 0);
    PC_CMD_SEND(&audio_command_queue, msg_to_send);
}
