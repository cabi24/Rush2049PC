#include "game/game.h"

/* PC: a command is 32 bytes (GetCmd.c). The wait for the audio manager to take every command is a
 * busy-wait, which lets it run (docs/port/ULTRA.md 6). */
typedef struct ShutdownMsg { s16 id; s8 type, used; u8 payload[28]; } ShutdownMsg;

/* PC: the sound manager's messages are PC data, PC_SND_MSG_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9, AUDIO.md 10.4); PC_SND_MSG_LOOP(128) is the run's count: 128 at original. */
#define audiomgr_gCmds (*(volatile ShutdownMsg (*)[PC_SND_MSG_SLOTS])pc_snd_msgs)
#define PC_SND_MSG_LOOP(n) (pc_snd_messages())

VIEW(uintptr_t, gFxTags);
#define gFxTags gFxTags_view
VIEW(uintptr_t, gSortedFxTags);
#define gSortedFxTags gSortedFxTags_view
VIEW(uintptr_t, gSequences);
#define gSequences gSequences_view

s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
ShutdownMsg *GetCmd(void);

void AudioShutdown(void) {
    s32 i;

    if (gInited_AudioMgr) {
        AudFree3D();
        post_type1();
        post_type7();
        do { pc_os_spin_yield("AudioShutdown");
            for (i = 0; i < PC_SND_MSG_LOOP(128); i++) {
                if (audiomgr_gCmds[i].used) {
                    break;
                }
            }
        } while (i < PC_SND_MSG_LOOP(128));
        gInited_AudioMgr = 0;
        AudioShutdownSys();
        DeallocateMemory(gFxTags);
        DeallocateMemory(gSortedFxTags);
        DeallocateMemory(gSequences);
    }
}

/* PC: the audio manager's message pool and queue can run out (docs/port/QA.md "Integration soak", fix 6;
 * transform/group.c's AudChangeEffect says when). The shutdown's stop of the song and of every effect are
 * not dropped: AudioShutdown frees the effects' blocks next, once the manager has drained the pool, so a post
 * that finds no message gives the lock back and waits for the manager to free one, and a full queue is waited
 * on (the lock is free by then). The other copy of these two (SetupCarColor/ctx_func_800C8918.c,
 * heap_release/AudioShutdown.c) is the same. */
#define PC_CMD_OR_WAIT(m) while((m)==0){audio_unlock();pc_os_spin_yield("post_type");audio_lock();(m)=GetCmd();}
#define PC_CMD_SEND(q,m) osSendMesg(q,m,1)

