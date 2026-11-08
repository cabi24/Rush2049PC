/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * GetCmd: allocate a free 24-byte slot from audiomgr_gCmds[128]: the first
 * slot whose in-use byte (+3) is 0 is marked used, its id (+0) set to -1 and
 * returned; NULL when the table is full.  IPA-internal in the game image (23
 * callers; the four-wide t6..t9 temp ring only appears as an internal -O3
 * procedure), so it is scored in a group with real, locked callers.
 *
 * Shaping: the slot table is volatile.  as1 then keeps the two stores of each
 * unrolled copy in source order (`sb 1,3(v1); li -1; sh 0(v1)`), the last
 * 8-word residual of earlier attempts (proved on the ugen listing first:
 * marking just the sb and sh `.set volatile` gives retail's schedule).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
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
