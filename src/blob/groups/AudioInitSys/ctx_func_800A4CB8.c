/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * AudioSfxInit @ 0x800A4CB8: audio list/pool init. Two List headers via the AudInit3D
 * list_init helper (inlined, store order +1,+0,+8,+0xC,+4), gNumInstances = count, 24-byte pool
 * gSfxInstances and 32-byte x 3*count pool audiosfx_gCmds (audio_dma_sync / audio_loop_control / memset), each
 * 32-byte record pushed with InsertLLE(&gFreeQ, rec, gFreeQ.head), gNumChannels = count,
 * gStoppedChannels[0..4] = 0 (peeled + unrolled), gStopAllEffects = 0, audiosfx_gInited = 1.  -O3 only.
 * Body: best.c (98/102) unchanged except one line.
 * Shaping quirks:
 *  - compiled-out `if (count == 0) {}` right after the first list_init: it makes `count` a coloured
 *    web in a2 that is spilled to its home before the first jal (retail `move a2,a0 ... sw a2,48(sp)`)
 *    and keeps the first list_init's stores at the top of the schedule.  Placed before the first
 *    list_init or after `gNumInstances = count` it leaves 20 / 18 rows; later placements 40-52.
 *  - gStopAllEffects is `volatile` (lui/addiu + sb 0(reg), as in every other reader).
 *  - defines the locked ClearLL (List clear) as context, exactly as src/blob/AudInit3D.c does.
 */
typedef signed char s8; typedef unsigned char u8; typedef short s16; typedef unsigned short u16;
typedef int s32; typedef unsigned int u32; typedef float f32;

typedef struct Link {
    struct Link *next, *prev;
} Link;

typedef struct List {
    u8 indirect, doubly, pad[2];
    u32 count;
    Link *head, *tail;
} List;

typedef struct { u8 bytes[24]; } Rec24;
/* PC: the record is a sound request (audiosfx/group.c's Node), whose entry pointer moves its
 * values: 48 bytes natively */
typedef struct { Link link; u8 kind, busy, pad[2]; void *entry; f32 values[4]; } Rec32;

extern List gCmdQ;
extern List gFreeQ;
extern s32 gNumInstances;
extern s32 gNumChannels;
extern Rec24 *gSfxInstances;
extern Rec32 *audiosfx_gCmds;
extern s32 gStoppedChannels[];
extern volatile s8 gStopAllEffects;
extern s8 audiosfx_gInited;

void *AllocateMemoryHeap(s32 arg0, s32 arg1);
void PurgeableMemory(void *arg0, s32 arg1);
void *memset(void *, s32, u32);
void InsertLLE(List *list, Link *object, Link *before);

void ClearLL(List *list);


