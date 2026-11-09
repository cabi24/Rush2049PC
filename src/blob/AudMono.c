/* AudMono: the sound manager's request to switch between mono and stereo output (MAT3 audiomgr.c).
 *
 * Posts the request to the audio manager thread, which passes it to the sound server (AudioSetMono) and from there
 * to MusyX (sound_set_mono). */
#include "game/game.h"
#include "game/audiomgr.h"

s32 osRecvMesg(OSMesgQueue *queue, OSMesg *message, s32 flags);
s32 osSendMesg(OSMesgQueue *queue, OSMesg message, s32 flags);

/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). The request is then dropped: with no message the lock is given
 * back and nothing is sent, and a message the full queue refuses is given back. */
#define PC_CMD_OR_UNLOCK(m) if((m)==0){osSendMesg(&audio_lock_queue,NULL,0);return;}
#define PC_CMD_SEND(q,m) if(osSendMesg(q,m,0)!=0)(m)->hdr.used=0

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
