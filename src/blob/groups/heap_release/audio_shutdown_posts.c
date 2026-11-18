/* Two messages to the sound manager's thread for the sound system's shutdown (AudioShutdown): stop the song, stop
 * every effect (the audio manager's command types 1 and 7, audiomgr_thread.c). */
#include "game/game.h"
#include "game/audiomgr.h"


/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). The shutdown's stop of the song and of every effect are
 * not dropped: AudioShutdown frees the effects' blocks next, once the manager has drained the pool, so a post
 * that finds no message gives the lock back and waits for the manager to free one, and a full queue is waited
 * on (the lock is free by then). The other copy of these two (heap_release/AudioShutdown.c) is the same. */
#define PC_CMD_OR_WAIT(m) while((m)==0){audio_unlock();pc_os_spin_yield("post_type");audio_lock();(m)=GetCmd();}

/* Posts "stop the song" to the sound manager: a free message from the pool (under the audio lock), its command
 * set, then sent to the manager's queue. */
void post_stop_song(void) {
    tAudioMsg *msg;
    tAudioMsg *cmd;

    audio_lock();
    cmd = GetCmd(); PC_CMD_OR_WAIT(cmd)
    msg = cmd;
    cmd->hdr.cmd = kAudioCmdKillSong; audio_unlock();
    queue_send(&audio_command_queue, msg);
}

/* Posts "stop every effect" to the sound manager, as post_stop_song. */
void post_stop_all_effects(void) {
    tAudioMsg *msg;
    tAudioMsg *cmd;

    audio_lock();
    cmd = GetCmd(); PC_CMD_OR_WAIT(cmd)
    msg = cmd;
    cmd->hdr.cmd = kAudioCmdStopAllEffects; audio_unlock();
    queue_send(&audio_command_queue, msg);
}
