/* musyx_alloc: the MusyX sound library's allocation service (one of the eight services the library calls through
 * its service table, docs/code/r5.md "MusyX"; N64 only, no MAT3 counterpart). */
#include "game/game.h"

/* Allocate size bytes for the library from the main heap, and mark the block not purgeable, so the game's purges
 * leave it alone. The second word (the library's alignment request) is not used. */
void *musyx_alloc(s32 size, M2C_UNK alignment) {
    void *block;

    block = AllocateMemoryHeap(0, size);
    PurgeableMemory(PC_N64A(block), 0);
    return block;
}
