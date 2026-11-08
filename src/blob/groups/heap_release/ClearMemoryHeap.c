/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * ClearMemoryHeap (historical label) @ 0x800A51E0, 680 bytes: heap compaction.
 * Under the heap message-queue lock (heap_lock_queue): (1) releases every used block that has no
 * owner pointer, no tag and a zero lock count; (2) for each remaining movable block (used, owner
 * pointer set, lock count zero) looks for an earlier free block that is large enough, or the free
 * block directly in front of it, moves the payload down with memmove (game_memmove), repoints
 * the owner (*ref = new payload), splits off the remainder as a new header (when adjacent, or
 * when at least 64 bytes remain) and releases the old block (DeallocateMemoryInternal = heap release).
 * No arcade ancestor found; N64 allocator module (0xFEDCBA98 block marker).
 *
 * ConvertHeap, the "heap or the main heap" getter, was inlined here on the N64 (its empty 8-byte stub lies
 * directly in front of this function).
 * DeallocateMemoryInternal is internal in retail (arguments arrive in a1/a2), so this body only
 * reproduces with the real release module in the unit (group.c and alloc_at.c, unchanged
 * copies of src/blob/groups/heap_release).
 *
 * Shaping facts (each needed):
 * - the scan loop keeps `next` as a variable set on both paths (not `cursor = cursor->next`);
 * - no variable for the payload address: `(u8 *)block + 32` is written at both uses;
 * - `size` is assigned, then the memmove argument re-reads `block->size`;
 * - the remainder's size is stored before its ref/used/tag/lock fields;
 * - `if (block->magic != MAGIC) after = previous->next; else after = block->next;`.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
#define NULL ((void *)0)
#define BLOCK_MAGIC 0xFEDCBA98

enum n64p_Block { n64p_Block_ };
enum n64p_Heap { n64p_Heap_ };
enum n64p_u32 { n64p_u32_ };
typedef struct Block {
    /* 0x00 */ u32 magic;
    /* 0x04 */ enum n64p_Block next;
    /* 0x08 */ enum n64p_Block prev;
    /* 0x0C */ u32 size;           /* payload bytes, header excluded */
    /* 0x10 */ enum n64p_u32 ref;           /* owner's pointer to the payload, or NULL */
    /* 0x14 */ s8 used;
    /* 0x15 */ s8 tag;
    /* 0x16 */ u8 lock;
    /* 0x17 */ u8 pad17[9];
} Block;                           /* 0x20 */

typedef struct Heap {
    /* 0x00 */ u32 unk0;
    /* 0x04 */ enum n64p_Heap next;
    /* 0x08 */ enum n64p_Block first;
    /* 0x0C */ enum n64p_Block last;
} Heap;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Block: (struct Block *)PC_N64P(x), enum n64p_Heap: (struct Heap *)PC_N64P(x), enum n64p_u32: (u32 *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

typedef struct OSMesgQueue OSMesgQueue;
extern s32 heap_lock_queue;
s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
void DeallocateMemoryInternal(u32 address, s32 tag);
void *game_memmove(void *dst, const void *src, u32 n);

Heap *ConvertHeap(Heap *heap);   /* the heap, or the main heap for NULL (memsys/group.c) */

/* The heap's API takes N64 addresses: N64A(p) is a window pointer's; a block's owner is a handle slot, which the
 * compaction points at the block's new place as a slot (PC_SLOT_SET) (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
#define SLOT_SET(s, v) PC_SLOT_SET(N64P(s), v)
void ClearMemoryHeap(Heap *hint) {
    Heap *heap;
    Block *block, *cursor, *after, *tail, *previous;
    Block *next;
    s32 adjacent;
    u32 size, difference;

    osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1);
    heap = ConvertHeap(hint);
    block = N64P(heap->first);
    while (block != NULL) {
        if (block->used && !block->lock && !block->ref && !block->tag) {
            DeallocateMemoryInternal(N64A(((u8 *)block + 32)), 0);
            block = N64P(heap->first);
        }
        block = N64P(block->next);
    }
    block = N64P(heap->first);
    while (block != NULL) {
        after = N64P(block->next);
        if (block->used && block->ref && !block->lock) {
            cursor = N64P(heap->first);
            adjacent = 0;
            while (cursor != NULL) {
                if (cursor == block) {
                    cursor = NULL;
                    break;
                }
                if (cursor->used) {
                    next = N64P(cursor->next);
                } else {
                    if (cursor->size >= block->size) break;
                    next = N64P(cursor->next);
                    if (block == next) {
                        adjacent = 1;
                        break;
                    }
                }
                cursor = next;
            }
            if (cursor != NULL) {
                cursor->ref = block->ref;
                SLOT_SET(cursor->ref, N64A(((u8 *)cursor + 32)));
                cursor->used = 1;
                cursor->tag = block->tag;
                cursor->lock = block->lock;
                size = block->size;
                block->ref = 0;
                game_memmove((u8 *)cursor + 32, (u8 *)block + 32, block->size);
                if (adjacent || cursor->size - size >= 64) {
                    tail = (Block *)((u8 *)cursor + size + 32);
                    if (adjacent) tail->next = N64W(after);
                    else tail->next = cursor->next;
                    if (N64P(tail->next) != NULL) N64P(tail->next)->prev = N64W(tail);
                    else heap->last = N64W(tail);
                    tail->prev = N64W(cursor);
                    if (adjacent) difference = 0; else difference = size + 32;
                    tail->size = cursor->size - difference;
                    tail->ref = 0;
                    tail->used = adjacent;
                    tail->tag = 0;
                    tail->lock = 0;
                    tail->magic = BLOCK_MAGIC;
                    cursor->next = N64W(tail);
                    cursor->size = size;
                    if (adjacent) block = tail;
                }
                previous = N64P(block->prev);
                DeallocateMemoryInternal(N64A(((u8 *)block + 32)), 0);
                if (after->magic != BLOCK_MAGIC) {
                    if (block->magic != BLOCK_MAGIC) after = N64P(previous->next);
                    else after = N64P(block->next);
                }
            }
        }
        block = after;
    }
    osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0);
}
