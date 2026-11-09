#include "game/game.h"

/* PC: a command (tAudioMsg, audiomgr_thread.c) is 32 bytes, its fx tag pointer host-sized (docs/port/SYNTH.md). */
typedef struct Slot18 { s16 f0; s8 f2; s8 f3; u8 pad[0x1C]; } Slot18;
/* PC: the sound manager's messages are PC data, PC_SND_MSG_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9, AUDIO.md 10.4); PC_SND_MSG_LOOP(128) is the run's count: 128 at original. */
#define audiomgr_gCmds (*(volatile Slot18 (*)[PC_SND_MSG_SLOTS])pc_snd_msgs)
#define PC_SND_MSG_LOOP(n) (pc_snd_messages())
/* PC test hook (docs/port/QA.md "Integration soak", fix 6): with pc_audio_pool_fail_every N > 0, every Nth call
 * finds the pool empty, as a starved host's would, to run its callers' guards; port/main.c sets it from
 * RUSH2049_TEST_AUDIO_POOL_FAIL. 0, the default, never. */
int pc_audio_pool_fail_every;
static unsigned int pc_audio_pool_calls;
#define PC_POOL_TEST_FAIL() \
    if(pc_audio_pool_fail_every>0&&++pc_audio_pool_calls%(unsigned int)pc_audio_pool_fail_every==0)return 0;
Slot18 *GetCmd(void)
{
    s32 i; PC_POOL_TEST_FAIL()
    for (i = 0; i < PC_SND_MSG_LOOP(128); i++) {
        if (audiomgr_gCmds[i].f3 == 0) {
            audiomgr_gCmds[i].f3 = 1;
            audiomgr_gCmds[i].f0 = -1;
            pc_snd_msg_taken(i); return (Slot18 *) &audiomgr_gCmds[i];
        }
    }
    pc_snd_count(PC_SND_NO_MESSAGE); return 0;
}
