#include "game/game.h"
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */
#define OSThread_s OSThread_sView   /* this file's own OSThread_s: include/game's OSThread_s is another type */
#define __OSThreadContext __OSThreadContextView   /* this file's own __OSThreadContext: include/game's __OSThreadContext is another type */

typedef struct __OSThreadContext {
    u64 at, v0, v1, a0, a1, a2, a3;
    u64 t0, t1, t2, t3, t4, t5, t6, t7;
    u64 s0, s1, s2, s3, s4, s5, s6, s7;
    u64 t8, t9, gp, sp, s8, ra;
    u64 lo, hi;
    u32 sr, pc, cause, badvaddr, rcp;
    u32 fpcsr;
    f32 fp0, fp2, fp4, fp6, fp8, fp10, fp12, fp14;
    f32 fp16, fp18, fp20, fp22, fp24, fp26, fp28, fp30;
} __OSThreadContext;
typedef struct OSThread_s {
    struct OSThread_s *next;
    s32 priority;
    struct OSThread_s **queue;
    struct OSThread_s *tlnext;
    u16 state;
    u16 flags;
    s32 id;
    s32 fp;
    struct __OSThreadprofile_s *thprof;
    __OSThreadContext context;
} OSThread;

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);

extern void *memset(void *s, s32 c, u32 n);

float fabsf(float);
float sqrtf(float);
#pragma intrinsic (fabsf)
#pragma intrinsic (sqrtf)

VIEW(M2C_UNK, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

void **CompressGhost(struct GhostHeader *header, s8 *blocks);

/* _NewGhost on the PC. A ghost (Ghost below: 0x2C bytes on the N64, 64 with its four pointers) and its handle (8
 * bytes) don't fit the N64's storage, 64 four-byte handle slots at ghost_file_handles and 64 records 0x2C apart at
 * ghost_file_records in the game arena: the PC keeps both here. Same search order; none free gives NULL
 * (docs/port/CASTS.md). */
static void *pc_ghost_slot[64];
static void *pc_ghost[64][8];
void **NewGhost(void) {
    s32 i;

    for (i = 0; i < 64; i++) {
        if (pc_ghost_slot[i] == NULL) {
            pc_ghost_slot[i] = pc_ghost[i];
            return &pc_ghost_slot[i];
        }
    }
    return NULL;
}


/* SaveServer: defined in group.c */


/* ===== func_800CC50C = GC save.c CompressGhost(header, blocks), N64 form (draft, closed) =====
 * GC transcription: memmove = game_memmove, _CalcChecksum = CalcChecksumInternal, AllocateMemoryHeapHigh =
 * AllocateMemoryHeapHigh, zip, _NewGhost = NewGhost, AllocateHandleHeap =
 * audio_task_complete, GetPtrSize, ghostRaceTimes, gRetraceRate,
 * gTrackNum; PS_GetBlocks inlined as ghost_blocks_from_size; GC `DeallocateMemory(header->start)` as
 * the inlined helper ghost_free_start (defined AFTER this function, see there).
 * Shaping: `gh` (= ghost->header) is a named local assigned twice -- once for track/backward/name, again
 * the data memcpy for playerId/finish/GetPtrSize (retail: first web in a3, second in a0; a single assignment gives
 * the first web v1). No unused locals; no compiled-out reads. */
typedef struct GhostHeader {      /* GC __anon_49776; N64 layout 0x58 (from the retail words) */
    u32 headerChecksum;           /* 0x00 */
    u8 revision;                  /* 0x04 */
    s8 flag;                      /* 0x05 */
    s8 track;                     /* 0x06 */
    u8 backward;                  /* 0x07 */
    u8 pad08[0x16 - 0x08];
    u8 name[13];                  /* 0x16 */
    u8 pad23[0x28 - 0x23];
    u32 playerId[2];              /* 0x28 */
    u8 pad30[0x38 - 0x30];
    f32 finish;                   /* 0x38 */
    u32 zippedSize;               /* 0x3C */
    u32 unzippedSize;             /* 0x40 */
    u32 zippedChecksum;           /* 0x44 */
    u32 unzippedChecksum;         /* 0x48 */
    u32 start;                    /* 0x4C: a heap block (the window), 32 bits as in the file */
    u32 offset;                   /* 0x50 */
    u32 size;                     /* 0x54 */
} GhostHeader;

typedef struct Ghost {            /* GC __anon_49a9c; N64 layout 0x2C */
    void *ll;                     /* 0x00 */
    void **fileh;                 /* 0x04 */
    s8 track;                     /* 0x08 */
    u8 backward;                  /* 0x09 */
    u8 name[13];                  /* 0x0A */
    u8 pad17;
    u32 playerId[2];              /* 0x18 */
    f32 finish;                   /* 0x20 */
    u8 **datah;                   /* 0x24 */
    GhostHeader **header;         /* 0x28 */
} Ghost;

void *memcpy(void *, const void *, u32);

/* PC: the header keeps its N64 layout, file data (docs/port/POINTERS.md 9.10); a ghost is 64 bytes. The heap's API
 * takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define START(x) ((u8 *)PC_N64P(x))
#define N64A(x) PC_N64A(x)

s32 ghost_blocks_from_size(s32 size) {   /* GC PS_GetBlocks */
    return (size + 255) >> 8;
}

void **CompressGhost(GhostHeader *header, s8 *blocks) {
    Ghost *ghost;
    void **ghosth;
    s32 maxsize;
    u8 *sorted;
    u8 *gdata;
    u32 dataSize;
    u32 num;
    GhostHeader **gh;
    u32 unzippedChecksum;

    sorted = START(header->start);
    maxsize = (f32)(ghostRaceTimes[gTrackNum] + 10.0) * gRetraceRate;
    game_memmove(START(header->start) + header->size, START(header->start) + maxsize, header->size);
    game_memmove(START(header->start) + header->size * 2, START(header->start) + maxsize * 2, header->size);
    num = header->size * 3;
    unzippedChecksum = CalcChecksumInternal(sorted, num);
    *(s16 *)((u8 *)header + 36) = 0;
    gdata = AllocateMemoryHeapHigh(NULL, num);
    dataSize = zip(sorted, num, gdata, num, 9, 10, 4);
    ghost_free_start(header);
    header->start = NULL;
    ghosth = NewGhost();
    ghost = *ghosth;
    memset(ghost, 0, GHOST_OFS(44));
    ghost->header = (GhostHeader **)AllocateHandleHeap(NULL, dataSize + 88);
    memcpy(*ghost->header, header, 88);
    (*ghost->header)->revision = 17;
    (*ghost->header)->start = NULL;
    (*ghost->header)->offset = 0;
    (*ghost->header)->size = 0;
    (*ghost->header)->zippedSize = dataSize;
    (*ghost->header)->unzippedSize = num;
    (*ghost->header)->headerChecksum = CalcChecksumInternal(&(*ghost->header)->revision,
        (u8 *)&(*ghost->header)->zippedChecksum - (u8 *)*ghost->header - 4);
    (*ghost->header)->zippedChecksum = CalcChecksumInternal(gdata, dataSize);
    (*ghost->header)->unzippedChecksum = unzippedChecksum;
    memcpy((u8 *)*ghost->header + 88, gdata, dataSize);
    osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1);
    DeallocateMemoryInternal(N64A(gdata), 0);
    osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0);
    gh = ghost->header;
    ghost->track = (*gh)->track;
    ghost->backward = (*gh)->backward;
    memcpy(ghost->name, (*gh)->name, 13);
    gh = ghost->header;
    ghost->playerId[0] = (*gh)->playerId[0];
    ghost->playerId[1] = (*gh)->playerId[1];
    ghost->finish = (*gh)->finish;
    *blocks = ghost_blocks_from_size((s32)GetPtrSize(N64A(gh)));
    return ghosth;
}

/* GC DeallocateMemory(header->start) for CompressGhost's first free, written as an inlined helper that reads the
 * pointer itself, on ONE line, defined AFTER func_800CC50C (as1 evidence): as1 breaks scheduling ties
 * source line; retail schedules `lw s2,76(s3)` between the lock's `la a0` and `move a1` with `li a2,1` in the delay
 * slot, which needs the pointer load on the lock's line and that line after the caller's `dataSize = ...` line.
 * Identity hypothesis (disclosed): the GC code calls DeallocateMemory(header->start) directly. Non-static, internal. */
void ghost_free_start(GhostHeader *header) { u8 *p = START(header->start); osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1); DeallocateMemoryInternal(N64A(p), 0); osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0); }
