/* Two messages to the sound manager's thread for the sound system's shutdown (AudioShutdown): stop the song, stop
 * every effect (the audio manager's command types 1 and 7, audiomgr_thread.c). */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct ShutdownMsg { s16 id; s8 type, used; u8 payload[20]; } ShutdownMsg;
typedef struct OSMesgQueue OSMesgQueue;

/* PC: the sound manager's messages are PC data, PC_SND_MSG_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9, AUDIO.md 10.4); PC_SND_MSG_LOOP(128) is the run's count: 128 at original. */
#define audiomgr_gCmds (*(volatile ShutdownMsg (*)[PC_SND_MSG_SLOTS])pc_snd_msgs)

VIEW(s32, audio_command_queue);
#define audio_command_queue audio_command_queue_view

s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
ShutdownMsg *GetCmd(void);

/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). The shutdown's stop of the song and of every effect are
 * not dropped: AudioShutdown frees the effects' blocks next, once the manager has drained the pool, so a post
 * that finds no message gives the lock back and waits for the manager to free one, and a full queue is waited
 * on (the lock is free by then). The other copy of these two (heap_release/AudioShutdown.c) is the same. */
#define PC_CMD_OR_WAIT(m) while((m)==0){audio_unlock();pc_os_spin_yield("post_type");audio_lock();(m)=GetCmd();}

/* Posts "stop the song" to the sound manager: a free message from the pool (under the audio lock), its command
 * set, then sent to the manager's queue. */
void post_type1(void) {
    ShutdownMsg *msg;
    ShutdownMsg *cmd;

    audio_lock();
    cmd = GetCmd(); PC_CMD_OR_WAIT(cmd)
    msg = cmd;
    cmd->type = kAudioCmdKillSong; audio_unlock();
    osSendMesg((OSMesgQueue *) &audio_command_queue,msg,1);
}

/* Posts "stop every effect" to the sound manager, as post_type1. */
void post_type7(void) {
    ShutdownMsg *msg;
    ShutdownMsg *cmd;

    audio_lock();
    cmd = GetCmd(); PC_CMD_OR_WAIT(cmd)
    msg = cmd;
    cmd->type = kAudioCmdStopAllEffects; audio_unlock();
    osSendMesg((OSMesgQueue *) &audio_command_queue,msg,1);
}
