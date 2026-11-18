/* Ghosts made from a race (MAT3 save.c _NewGhost and CompressGhost): a recording of the race, compressed (zip) into
 * a ghost record behind a heap handle, ready to save to a Controller Pak. */
#include "game/game.h"


extern void *memset(void *s, s32 c, u32 n);

float fabsf(float);
float sqrtf(float);
#pragma intrinsic (fabsf)
#pragma intrinsic (sqrtf)

/* _NewGhost: a free ghost record and its handle, from a pool of 64 (pc_ghost, their handles pc_ghost_slot; a handle
 * is free while it is NULL), searched from the first; NULL when none is free (docs/port/CASTS.md). */
static tGhostFile *pc_ghost_slot[64];
static tGhostFile pc_ghost[64];
tGhostFile **NewGhost(void) {
    s32 i;

    for (i = 0; i < 64; i++) {
        if (pc_ghost_slot[i] == NULL) {
            pc_ghost_slot[i] = &pc_ghost[i];
            return &pc_ghost_slot[i];
        }
    }
    return NULL;
}

void *memcpy(void *, const void *, u32);

/* The race's recordings: player i's ghost header, gRealNewGhostHeaders[i], records into a heap block GhostStart
 * allocates, ghost_recordings[i], which CompressGhost makes a ghost of and frees. MAT3 kept the block's address in the
 * header (start); the header is a ghost file's layout, whose 4 bytes can't hold a host pointer (tGhostHeader). */
static u8 *ghost_recordings[4];
_Static_assert(sizeof(tGhostHeader) == 0x58 && __builtin_offsetof(tGhostHeader, hasSamples) == 0x4C,
               "a ghost header is the file's 88 bytes");

/* The slot of the block that the recording header (one of gRealNewGhostHeaders) records into. */
u8 **ghost_recording(tGhostHeader *header) {
    return &ghost_recordings[header - gRealNewGhostHeaders];
}

/* The Controller Pak blocks (256 bytes) size bytes take (MAT3 PS_GetBlocks). */
s32 ghost_blocks_from_size(s32 size) {
    return (size + 255) >> 8;
}

/* Makes a ghost from the race recording header describes (ghost_recording): its three channels moved together (each
 * header->size long; the recording keeps room for the track's longest time), checksummed, compressed (zip, level 9)
 * into a block at the heap's top, the recording freed; then a ghost record (NewGhost) with a handle holding the
 * header (revision 17, its sizes and checksums) and the compressed data, the temporary block freed, and the record's
 * track, direction, name, player id and time from the header. *blocks gets the pak blocks it needs. Returns the
 * ghost's handle. */
tGhostFile **CompressGhost(tGhostHeader *header, u8 *blocks) {
    tGhostFile *ghost;
    tGhostFile **ghosth;
    s32 maxsize;
    u8 *sorted;
    u8 *gdata;
    u32 dataSize;
    u32 num;
    tGhostHeader **gh;
    u32 unzippedChecksum;

    sorted = *ghost_recording(header);
    maxsize = (f32)(ghostRaceTimes[gTrackNum] + 10.0) * gRetraceRate;
    game_memmove(sorted + header->size, sorted + maxsize, header->size);
    game_memmove(sorted + header->size * 2, sorted + maxsize * 2, header->size);
    num = header->size * 3;
    unzippedChecksum = CalcChecksumInternal(sorted, num);
    header->tournamentKey = 0;
    gdata = AllocateMemoryHeapHigh(NULL, num);
    dataSize = zip(sorted, num, gdata, num, 9, 10, 4);
    ghost_free_start(header);
    header->hasSamples = 0;
    ghosth = NewGhost();
    ghost = *ghosth;
    memset(ghost, 0, sizeof(tGhostFile));
    ghost->header = (tGhostHeader **)AllocateHandleHeap(NULL, dataSize + 88);
    memcpy(*ghost->header, header, 88);
    (*ghost->header)->revision = 17;
    (*ghost->header)->hasSamples = 0;
    (*ghost->header)->offset = 0;
    (*ghost->header)->size = 0;
    (*ghost->header)->zippedSize = dataSize;
    (*ghost->header)->unzippedSize = num;
    (*ghost->header)->headerChecksum = CalcChecksumInternal(&(*ghost->header)->revision,
        (u8 *)&(*ghost->header)->zippedChecksum - (u8 *)*ghost->header - 4);
    (*ghost->header)->zippedChecksum = CalcChecksumInternal(gdata, dataSize);
    (*ghost->header)->unzippedChecksum = unzippedChecksum;
    memcpy((u8 *)*ghost->header + 88, gdata, dataSize);
    lock_take(&heap_lock_queue);
    DeallocateMemoryInternal(gdata, 0);
    lock_give(&heap_lock_queue);
    gh = ghost->header;
    ghost->track = (*gh)->track;
    ghost->backward = (*gh)->backward;
    memcpy(ghost->name, (*gh)->name, 13);
    gh = ghost->header;
    ghost->playerId[0] = (*gh)->playerId[0];
    ghost->playerId[1] = (*gh)->playerId[1];
    ghost->finish = (*gh)->finish;
    *blocks = ghost_blocks_from_size((s32)GetPtrSize(gh));
    return ghosth;
}

/* Frees the block the recording header records into, under the heap lock, and forgets it (MAT3:
 * DeallocateMemory(header->start)). */
void ghost_free_start(tGhostHeader *header) {
    u8 **recording = ghost_recording(header);
    u8 *p = *recording;

    lock_take(&heap_lock_queue);
    DeallocateMemoryInternal(p, 0);
    lock_give(&heap_lock_queue);
    *recording = NULL;
}
