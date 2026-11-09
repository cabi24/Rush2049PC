/* include/game/heap.h: the heap's headers, as the heap's own code reads them (MAT3 memsys.c).
 *
 * The game allocates from heaps. The main heap (gMainHeap) starts above the game's BSS and runs to the end of
 * memory; a heap made inside another (create_heap) is a chunk of it. A heap is a tHeap header, then chunks, each a
 * 32-byte tMemoryChunk header followed by its data, linked both ways in address order. A chunk that is free is
 * merged with its free neighbours when it is freed. A heap also keeps tables of master pointers ("handles", MAT3's
 * masters): AllocateHandleHeap returns the address of a slot that holds the data's address, so the heap may move
 * the data (compaction) and update the slot.
 *
 * On the PC the headers keep their N64 layout in the RDRAM window (docs/port/POINTERS.md 1, MEMORY.md 4): each link
 * is the 32-bit window address of what it points to. The links are typed as enums so that N64P(link) turns one into
 * a pointer of the right type; N64W(p) turns a pointer back into a link. The heap's interface (DeallocateMemory,
 * PurgeableMemory, FindHeap ...) also takes 32-bit window addresses: N64A(p) is a pointer's, N64PTR(T, a) the
 * pointer at one (port/include/pc_prelude.h, docs/port/NOFIXED.md 2.3). Wave 3 of the PC-only work makes all of
 * this native memory.
 *
 * Only the heap's own files include this header (after game/game.h); others see struct Heap only through pointers.
 * The handle slots are pointer-sized on the PC (HEAP_MASTER_SLOT): each holds a chunk's data address, zero-extended,
 * so the code that reads a slot as a u32 still finds the address in its first four bytes (POINTERS.md 9.10). */
#ifndef GAME_HEAP_H
#define GAME_HEAP_H

#include "game/types.h"

/* The 32-bit window addresses the headers hold, one enum per kind of target (see N64P below). */
enum ChunkLink { ChunkLink_ };              /* a tMemoryChunk */
enum HeapLink { HeapLink_ };                /* a tHeap */
enum MasterBlockLink { MasterBlockLink_ };  /* a tMasterBlock */
enum MastersLink { MastersLink_ };          /* an array of master slots */
enum DataLink { DataLink_ };                /* anything else: the master slot that owns a chunk */

/* Every header starts with this; port/ultra/heap.c checks it when it walks a heap. */
#define HEAP_MAGIC_WORD 0xFEDCBA98
/* A chunk's header is 32 bytes; its data follows, a multiple of 32 bytes long. */
#define HEAP_CHUNK_HEADER 32
/* A free chunk is split only if what is left could hold a header and 32 bytes of data. */
#define HEAP_MIN_SPLIT 64
/* A master slot's size on the PC (pointer-sized; 4 bytes on the N64). */
#define HEAP_MASTER_SLOT 8

/* A chunk's header (MAT3 tMemoryChunk), 32 bytes. */
typedef struct tMemoryChunk {
    u32 magic;              /* 0x00: HEAP_MAGIC_WORD */
    enum ChunkLink next;    /* 0x04: the next chunk up, or 0 for the last */
    enum ChunkLink prev;    /* 0x08: the chunk below, or 0 for the first */
    u32 size;               /* 0x0C: the data's size in bytes, header not counted */
    enum DataLink master;   /* 0x10: the master slot that holds the data's address (a handle's chunk), or 0 */
    s8 used;                /* 0x14: allocated */
    u8 internal;            /* 0x15: the heap's own (a master table, a heap inside this one): 1 */
    u8 locked;              /* 0x16: lock count (lock_memory, PurgeableMemory), 0-255: compaction leaves it */
    u8 pad17[9];
} tMemoryChunk;

/* A table of master slots (MAT3 tMasterBlock); a heap's tables are chained. */
typedef struct tMasterBlock {
    u32 numMasters;                 /* 0x0: slots in this table */
    enum MastersLink masters;       /* 0x4: the slots (HEAP_MASTER_SLOT bytes each) */
    enum MasterBlockLink nextBlock; /* 0x8: the next table, or 0 */
} tMasterBlock;

/* A heap's header (MAT3 tHeap), 0x28 bytes. The game's prototypes name it struct Heap. */
typedef struct Heap {
    u32 magic;                      /* 0x00: HEAP_MAGIC_WORD */
    enum HeapLink next;             /* 0x04: the next heap after gMainHeap, in the order they were made */
    enum ChunkLink chunks;          /* 0x08: the first chunk */
    enum ChunkLink lastChunk;       /* 0x0C: the last chunk */
    u32 endRam;                     /* 0x10: the window address just past the heap */
    u16 numAdditionalMasters;       /* 0x14: slots in each master table added when the others are full */
    u16 pad16;
    tMasterBlock masters;           /* 0x18: the first master table */
    u32 emergencyFunc;              /* 0x24: MAT3's out-of-memory callback; unused on the N64 */
} tHeap;

/* A link's pointer, typed by the link's kind; anything that is not a link passes through. */
#define N64P(x) _Generic((x), enum ChunkLink: (tMemoryChunk *)PC_N64P(x), enum HeapLink: (tHeap *)PC_N64P(x), \
    enum MasterBlockLink: (tMasterBlock *)PC_N64P(x), enum MastersLink: (uintptr_t *)PC_N64P(x), \
    enum DataLink: (void *)PC_N64P(x), default: (x))
/* A pointer as a link. */
#define N64W(x) PC_N64W(x)
/* A pointer as the heap interface's 32-bit address, and the pointer at such an address. */
#define N64A(x) PC_N64A(x)
#define N64PTR(T, a) ((T)PC_N64P(a))

/* The heap's functions are game/memory.h's, with these headers as struct Heap and struct tMemoryChunk. */

#endif /* GAME_HEAP_H */
