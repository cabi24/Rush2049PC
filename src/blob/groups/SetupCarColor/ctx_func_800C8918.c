#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct ShutdownMsg { s16 id; s8 type, used; u8 payload[20]; } ShutdownMsg;
typedef struct OSMesgQueue OSMesgQueue;

/* PC: the sound manager's messages are PC data, PC_SND_MSG_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9, AUDIO.md 10.4); PC_SND_MSG_LOOP(128) is the run's count: 128 at original. */
#define audiomgr_gCmds (*(volatile ShutdownMsg (*)[PC_SND_MSG_SLOTS])pc_snd_msgs)
#define PC_SND_MSG_LOOP(n) (pc_snd_messages())

VIEW(s32, audio_command_queue);
#define audio_command_queue audio_command_queue_view

s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
ShutdownMsg *GetCmd(void);

/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). The shutdown's stop of the song and of every effect are
 * not dropped: AudioShutdown frees the effects' blocks next, once the manager has drained the pool, so a post
 * that finds no message gives the lock back and waits for the manager to free one, and a full queue is waited
 * on (the lock is free by then). The other copy of these two (SetupCarColor/ctx_func_800C8918.c,
 * heap_release/AudioShutdown.c) is the same. */
#define PC_CMD_OR_WAIT(m) while((m)==0){audio_unlock();pc_os_spin_yield("post_type");audio_lock();(m)=GetCmd();}
#define PC_CMD_SEND(q,m) osSendMesg(q,m,1)
void post_type1(void) {
    ShutdownMsg *sp1C;
    ShutdownMsg *temp_v0;

    audio_lock();
    temp_v0 = GetCmd(); PC_CMD_OR_WAIT(temp_v0)
    sp1C = temp_v0;
    temp_v0->type = 1; audio_unlock();
    PC_CMD_SEND((OSMesgQueue *) &audio_command_queue, sp1C);
}

void post_type7(void) {
    ShutdownMsg *sp1C;
    ShutdownMsg *temp_v0;

    audio_lock();
    temp_v0 = GetCmd(); PC_CMD_OR_WAIT(temp_v0)
    sp1C = temp_v0;
    temp_v0->type = 7; audio_unlock();
    PC_CMD_SEND((OSMesgQueue *) &audio_command_queue, sp1C);
}

