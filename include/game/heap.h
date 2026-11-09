/* include/game/heap.h: the heap's headers, as the heap's own code reads them (MAT3 memsys.c).
 *
 * The game allocates from heaps. The main heap (gMainHeap) is the memory the PC gives the game for it
 * (pc_heap_memory: on the N64 everything from the end of the game's BSS to the end of RDRAM); a heap made inside
 * another (create_heap) is a chunk of it. A heap is a tHeap header, then chunks, each a 32-byte tMemoryChunk header
 * followed by its data, linked both ways in address order. A chunk that is freed is merged with its free neighbours.
 * A heap also keeps tables of master pointers ("handles", MAT3's masters): AllocateHandleHeap returns the address of
 * a slot that holds the data's address, so the heap may move the data (compaction) and update the slot.
 *
 * The headers hold host pointers, in MAT3's types (docs/port/NATIVE.md step 6). On the N64 each link was a 32-bit
 * address; the PC kept that layout in the RDRAM window until wave 3, with every link and every argument of the heap's
 * interface translated. What the native headers keep is the N64's sizes, which is what the allocator's arithmetic
 * reads, so what fits in the heap, and so what the game loads when memory is short, is the original game's:
 *   - a chunk's header is 32 bytes (HEAP_CHUNK_HEADER): its three links are 8 bytes each now, so the magic that was a
 *     word at the front is a byte at the end (HEAP_CHUNK_MAGIC);
 *   - a heap's header fits in the 64 bytes before its first chunk (HEAP_HEADER_ROOM): MAT3's out-of-memory callback,
 *     which the N64 game never set, is left out to make room;
 *   - a master table's header (tMasterBlock) is a chunk of its own, 24 bytes rounded to a chunk's 32 as the N64's 12
 *     were; the master slots are pointer-sized (HEAP_MASTER_SLOT), as they have been since docs/port/POINTERS.md 9.10.
 * The heap's addresses compare and subtract as the N64's did (the free's merge with its neighbours, FindChunk's and
 * FindHeap's ranges, the compaction's moves): a heap is one block of memory, its chunks in address order.
 *
 * Only the heap's own files include this header (after game/game.h); others see struct Heap only through pointers. */
#ifndef GAME_HEAP_H
#define GAME_HEAP_H

#include "game/types.h"

/* A heap's header starts with this word (MAT3's), which port/ultra/heap.c checks before it walks a heap. */
#define HEAP_MAGIC_WORD 0xFEDCBA98
/* A chunk's header ends with this byte: the low byte of MAT3's word (a chunk's header has no room for the word beside
 * three pointers). A freed header is filled over (FillFreeMem's 0x7FFF0BAD), which is how the compaction tells a
 * chunk it has merged away (ClearMemoryHeap). */
#define HEAP_CHUNK_MAGIC 0x98
/* A chunk's header is 32 bytes; its data follows, a multiple of 32 bytes long. */
#define HEAP_CHUNK_HEADER 32
/* A free chunk is split only if what is left could hold a header and 32 bytes of data. */
#define HEAP_MIN_SPLIT 64
/* The room a heap keeps for its header before its first chunk: the first chunk is at (heap + 67) & ~31, 64 bytes
 * past a heap that starts on a 32-byte boundary. */
#define HEAP_HEADER_ROOM 64
/* A master slot's size: a pointer (4 bytes on the N64). */
#define HEAP_MASTER_SLOT ((u32)sizeof(void *))

/* A chunk's header (MAT3 tMemoryChunk), 32 bytes. */
typedef struct tMemoryChunk {
    struct tMemoryChunk *next;  /* 0x00: the next chunk up, or NULL for the last */
    struct tMemoryChunk *prev;  /* 0x08: the chunk below, or NULL for the first */
    void **master;              /* 0x10: the master slot that holds the data's address (a handle's chunk), or NULL */
    u32 size;                   /* 0x18: the data's size in bytes, header not counted */
    s8 used;                    /* 0x1C: allocated */
    u8 internal;                /* 0x1D: the heap's own (a master table, a heap inside this one): 1 */
    u8 locked;                  /* 0x1E: lock count (lock_memory, PurgeableMemory), 0-255: compaction leaves it */
    u8 magic;                   /* 0x1F: HEAP_CHUNK_MAGIC */
} tMemoryChunk;

/* A table of master slots (MAT3 tMasterBlock); a heap's tables are chained. */
typedef struct tMasterBlock {
    u32 numMasters;                 /* 0x00: slots in this table */
    void **masters;                 /* 0x08: the slots, each the address of a handle's data or NULL when free */
    struct tMasterBlock *nextBlock; /* 0x10: the next table, or NULL */
} tMasterBlock;

/* A heap's header (MAT3 tHeap), 64 bytes: MAT3's fields but its callback, numAdditionalMasters moved up beside the
 * magic. The game's prototypes name it struct Heap. */
typedef struct Heap {
    u32 magic;                      /* 0x00: HEAP_MAGIC_WORD */
    u16 numAdditionalMasters;       /* 0x04: slots in each master table added when the others are full */
    struct Heap *next;              /* 0x08: the next heap after gMainHeap, in the order they were made */
    tMemoryChunk *chunks;           /* 0x10: the first chunk */
    tMemoryChunk *lastChunk;        /* 0x18: the last chunk */
    u8 *endRam;                     /* 0x20: just past the heap */
    tMasterBlock masters;           /* 0x28: the first master table */
} tHeap;

/* The heap's functions are game/memory.h's, with these headers as struct Heap and struct tMemoryChunk. */

#endif /* GAME_HEAP_H */
