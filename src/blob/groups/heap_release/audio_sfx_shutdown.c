/*
 * audio_sfx_shutdown (historical label) -- shut down the sound/heap users and
 * free two allocations. Calls AudioStopAllEffects (flag every active gSfxInstances
 * entry, then set gStopAllEffects), spins until the audio side clears
 * gStopAllEffects and empties the list gCmdQ, clears audiosfx_gInited and frees
 * the allocations held in gSfxInstances and audiosfx_gCmds.
 *
 * Structure recovered from retail: the free is the deleted internal helper
 * DeallocateMemory_stub (stub between DeallocateMemoryInternal and DeallocateMemory),
 * which takes the heap lock (deleted internal heap_lock), releases the
 * block through the internal DeallocateMemoryInternal(address, 0) (IPA: a1/a2,
 * s0/s1 left to the caller, hence the unused s0/s1 saves here) and unlocks
 * (deleted internal heap_unlock). umerge inlines all three into every
 * caller; each inlined DeallocateMemory_stub instance accounts for 24 bytes of
 * frame (homes 56 and 32, frame 64). Defining the three stubs for real
 * reproduces their retail jr-ra stubs; landing needs prefer_definition
 * entries for them. No arcade ancestor (N64 heap code).
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

void DeallocateMemory_stub(u32 address);

/* This file's DeallocateMemory_stub is a weak copy; the link keeps the empty stub of that name, and the heap free
 * itself as DeallocateMemory (heap_release/group.c, docs/port/PROTOTYPES.md). */
#define DeallocateMemory_stub DeallocateMemory
void DeallocateMemory(u32 address);

void audio_sfx_shutdown(void)
{
    AudioStopAllEffects();
    while (gStopAllEffects != 0) { pc_os_spin_yield("wheel_params_set 1"); }
    while (gCmdQ.head != 0) { pc_os_spin_yield("wheel_params_set 2"); }
    audiosfx_gInited = 0;
    DeallocateMemory_stub(gSfxInstances);
    DeallocateMemory_stub(audiosfx_gCmds);
}
