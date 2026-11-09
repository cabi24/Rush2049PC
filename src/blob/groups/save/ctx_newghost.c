/* Ghosts made from a race (MAT3 save.c _NewGhost and CompressGhost): a recording of the race, compressed (zip) into
 * a ghost record behind a heap handle, ready to save to a Controller Pak. */
#include "game/game.h"

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);

extern void *memset(void *s, s32 c, u32 n);

float fabsf(float);
float sqrtf(float);
#pragma intrinsic (fabsf)
#pragma intrinsic (sqrtf)

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


/* A ghost file's header (MAT3 tSaveHeader), the N64's layout (0x58 bytes, as in the file). */
typedef struct GhostHeader {
    u32 headerChecksum;           /* 0x00 */
    u8 revision;                  /* 0x04 */
    s8 flag;                      /* 0x05 */
    s8 track;                     /* 0x06 */
    u8 backward;                  /* 0x07 */
    u8 pad08[0x16 - 0x08];
    u8 name[13];                  /* 0x16 */
    u8 pad23;
    u16 tournamentKey;            /* 0x24 */
    u8 pad26[2];
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

/* A ghost record (MAT3's): the N64's offsets below; 64 bytes on the PC, its pointers host-sized (GHOST_OFS). */
typedef struct Ghost {
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

/* The Controller Pak blocks (256 bytes) size bytes take (MAT3 PS_GetBlocks). */
s32 ghost_blocks_from_size(s32 size) {
    return (size + 255) >> 8;
}

/* Makes a ghost from the race recording header describes: its three channels moved together (each header->size
 * long; the recording keeps room for the track's longest time), checksummed, compressed (zip, level 9) into a block
 * at the heap's top, the recording freed; then a ghost record (NewGhost) with a handle holding the header (revision
 * 17, its sizes and checksums) and the compressed data, the temporary block freed, and the record's track,
 * direction, name, player id and time from the header. *blocks gets the pak blocks it needs. Returns the ghost's
 * handle. */
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
    header->tournamentKey = 0;
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

/* Frees the race recording's block (header->start), under the heap lock (MAT3: DeallocateMemory(header->start)). */
void ghost_free_start(GhostHeader *header) { u8 *p = START(header->start); osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1); DeallocateMemoryInternal(N64A(p), 0); osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0); }
