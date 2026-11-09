/* SaveGhostInLimbo: keep a ghost that couldn't be saved (MAT3 ghost.c). */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);

void *memcpy(void *, const void *, u32);

static void release_handle(u32 address);

/* Keeps the ghost entry (its header) and its recording payload in memory, "in limbo", when it couldn't go to a pak:
 * the gNumLimbo slots of its track and direction (all ghosts share one row without the Expansion Pak) hold the
 * fastest, slowest first; a ghost faster than some of them pushes the slowest out (its memory freed) and takes its
 * place in order, a copy of header and recording in one new heap block. */
void SaveGhostInLimbo(tGhostHeader *entry, void *payload) {
    s32 i;
    tGhostHeader **h;
    s32 j;
    s32 k;
    tGhostFile **row;

    if (expansion_pak_present == 0) {
        row = gLimboGhosts;
    } else {
        row = &gLimboGhosts[((entry->backward ? 6 : 0) + entry->track) * 3];
    }
    for (i = 0; i < gNumLimbo; i++) {
        h = row[i]->header;
        if (h != 0 && entry->track == (*h)->track && entry->backward == (*h)->backward && !(entry->finish < (*h)->finish)) {
            break;
        }
    }
    i--;
    if (i >= 0) {
        if (row[0]->header != 0) {
            release_handle(PC_N64A(row[0]->header));
        }
        if (i > 0) {
            game_memmove(row[0], row[1], i * sizeof(tGhostFile));
        }
        row[i]->track = entry->track;
        row[i]->backward = entry->backward;
        memcpy(row[i]->name, entry->name, 13);
        memcpy(row[i]->playerId, entry->playerId, 8);
        row[i]->finish = entry->finish;
        row[i]->header = AllocateHandleHeap(0, entry->zippedSize + sizeof(tGhostHeader));
        memcpy(*row[i]->header, entry, sizeof(tGhostHeader));
        memcpy((u8 *) *row[i]->header + sizeof(tGhostHeader), payload, entry->zippedSize);
        (*row[i]->header)->start = 0;
        (*row[i]->header)->size = 0;
        (*row[i]->header)->offset = (*row[i]->header)->size;
    }
}

/* Frees the heap block at the N64 address, under the heap lock. */
static void release_handle(u32 address) {
    osRecvMesg((OSMesgQueue *) &heap_lock_queue, 0, 1);
    DeallocateMemoryInternal(address, 0);
    osSendMesg((OSMesgQueue *) &heap_lock_queue, 0, 0);
}
