/*
 * audio_sfx_shutdown (historical label) -- shut down the sound/heap users and
 * free two allocations. Calls AudioStopAllEffects (flag every active gSfxInstances
 * entry, then set gStopAllEffects), spins until the audio side clears
 * gStopAllEffects and empties the list gCmdQ, clears audiosfx_gInited and frees
 * the allocations held in gSfxInstances and audiosfx_gCmds.
 *
 * The free is DeallocateMemory, the locked heap free: it takes the heap lock,
 * releases the block through DeallocateMemoryInternal(address, 0) and unlocks.
 * The N64 build inlined it here (its out-of-line copy left an empty 8-byte
 * stub between DeallocateMemoryInternal and DeallocateMemory). No arcade
 * ancestor (N64 heap code).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef unsigned int u32;

typedef struct List16 { u8 indirect, doubly, pad2[2]; u32 count; void *head, *tail; } List16;
typedef struct OSMesgQueue OSMesgQueue;

extern volatile s8 gStopAllEffects;
extern s8 audiosfx_gInited;
extern volatile List16 gCmdQ;
extern uintptr_t gSfxInstances;
extern uintptr_t audiosfx_gCmds;
extern char heap_lock_queue[];
int osRecvMesg(OSMesgQueue *, void **, int);
int osSendMesg(OSMesgQueue *, void *, int);
void DeallocateMemoryInternal(u32 address, int tag);
void AudioStopAllEffects(void);

/* The locked heap free (heap_release/group.c). */
void DeallocateMemory(u32 address);

void audio_sfx_shutdown(void)
{
    AudioStopAllEffects();
    while (gStopAllEffects != 0) { pc_os_spin_yield("wheel_params_set 1"); }
    while (gCmdQ.head != 0) { pc_os_spin_yield("wheel_params_set 2"); }
    audiosfx_gInited = 0;
    DeallocateMemory(gSfxInstances);
    DeallocateMemory(audiosfx_gCmds);
}
