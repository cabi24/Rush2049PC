/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * AudioShutdown @ 0x800C8918 (628 B): audio/resource shutdown (whole-program unit EQUAL).
 *   if the shutdown flag gInited_AudioMgr is set: release the two pending effect handles (AudFree3D, inlined),
 *   post a type-1 and a type-7 message to the slot queue audio_command_queue (24-byte slots from GetCmd, taken
 *   under the slot lock audio_lock_queue), busy-wait until no slot of audiomgr_gCmds[128] is in use, clear the flag,
 *   AudioShutdownSys(), and free three more heap blocks (DeallocateMemory, the locked heap free).
 * Structure facts (traced):
 *   - every helper is umerge-inlined: AudFree3D and DeallocateMemory (internal, a25 group), the slot
 *     lock/unlock helpers audio_lock/audio_unlock (given bodies here: the retail caller-less stubs next to
 *     GetCmd; each nested inline level adds 8 bytes per site, which is what makes the two message
 *     areas 32 bytes and the frame 208), and two file-static copies of AudKillSong/AudStopAllEffects
 *     (the kept locked functions are not inlined by umerge; SaveGhostInLimbo's static release_handle precedent).
 *   - the busy-wait is an index loop `for (i...) if (used) break; } while (i < 128)`: strength-reduced it
 *     gives retail's END copy (move a1,v1) and the rematerialised table start; pointer loops never do.
 *   - audiomgr_gCmds is volatile as in the locked allocator GetCmd.c.
 * SHAPING (disclosed): `temp_v0->type = N; audio_unlock();` on one line. Statements keep their own source
 *   line through umerge inlining and as1 breaks ready-list ties by line; retail schedules the unlock's
 *   `addiu a0` before the `sb` (the standalone AudKillSong does the opposite), which only a shared line
 *   reproduces (2 words otherwise).
 * No unused locals, no pad, no stand-ins.
 * PACKAGING: post_type1/post_type7 are non-static (not on the keep list, so uld makes them internal and
 *   umerge inlines and deletes them exactly as the static copies; codegen identical). As file statics their two
 *   deleted-procedure stubs had no symbol and landed directly after the member stub audio_lock in the group
 * object, which the image splice's extent check reads as `2 extra words`; as globals the stubs carry their own
 *   symbols, so every member's slice ends at the next symbol.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef float f32;
#define NULL ((void *)0)

/* PC: a command is 32 bytes (GetCmd.c). The wait for the audio manager to take every command is a
 * busy-wait, which lets it run (docs/port/ULTRA.md 6). */
typedef struct ShutdownMsg { s16 id; s8 type, used; u8 payload[28]; } ShutdownMsg;
typedef struct OSMesgQueue OSMesgQueue;
typedef void *OSMesg;

/* PC: the sound manager's messages are PC data, PC_SND_MSG_SLOTS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 9, AUDIO.md 10.4); PC_SND_MSG_LOOP(128) is the run's count: 128 at original. */
#define audiomgr_gCmds (*(volatile ShutdownMsg (*)[PC_SND_MSG_SLOTS])pc_snd_msgs)
#define PC_SND_MSG_LOOP(n) (pc_snd_messages())
extern s8 gInited_AudioMgr;
extern uintptr_t gFxTags;
extern uintptr_t gSortedFxTags;
extern uintptr_t gSequences;
extern u32 gListeners, gEmitters;
extern s32 audio_command_queue;
void AudioShutdownSys(void);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
ShutdownMsg *GetCmd(void);

/* The locked heap free (heap_release/group.c), which the N64 build inlined here. */
void DeallocateMemory(u32 address);
void AudFree3D(void);
void audio_lock(void);
void audio_unlock(void);
void post_type1(void);
void post_type7(void);

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
void post_type1(void);

void post_type7(void);

