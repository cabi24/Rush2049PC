/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * car_damage_visual (historical label) @ 0x800A51E0, 680 bytes: heap compaction.
 * Under the heap message-queue lock (D_80152770): (1) releases every used block that has no
 * owner pointer, no tag and a zero lock count; (2) for each remaining movable block (used, owner
 * pointer set, lock count zero) looks for an earlier free block that is large enough, or the free
 * block directly in front of it, moves the payload down with memmove (func_800A47C0), repoints
 * the owner (*ref = new payload), splits off the remainder as a new header (when adjacent, or
 * when at least 64 bytes remain) and releases the old block (audio_reverb_update = heap release).
 * No arcade ancestor found; N64 allocator module (0xFEDCBA98 block marker).
 *
 * func_800A51D8 is the retail `jr ra; nop` stub directly in front of this function: a deleted
 * static, the "heap or default heap" getter, inlined here. It must not be on the keep list.
 * audio_reverb_update is internal in retail (arguments arrive in a1/a2), so this body only
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
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif
#define NULL ((void *)0)
#define BLOCK_MAGIC 0xFEDCBA98

#ifdef TARGET_PC
enum n64p_Block { n64p_Block_ };
enum n64p_Heap { n64p_Heap_ };
enum n64p_u32 { n64p_u32_ };
#endif
#ifdef TARGET_PC
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
#else
typedef struct Block {
    /* 0x00 */ u32 magic;
    /* 0x04 */ struct Block *next;
    /* 0x08 */ struct Block *prev;
    /* 0x0C */ u32 size;           /* payload bytes, header excluded */
    /* 0x10 */ u32 *ref;           /* owner's pointer to the payload, or NULL */
    /* 0x14 */ s8 used;
    /* 0x15 */ s8 tag;
    /* 0x16 */ u8 lock;
    /* 0x17 */ u8 pad17[9];
} Block;                           /* 0x20 */
#endif

#ifdef TARGET_PC
typedef struct Heap {
    /* 0x00 */ u32 unk0;
    /* 0x04 */ enum n64p_Heap next;
    /* 0x08 */ enum n64p_Block first;
    /* 0x0C */ enum n64p_Block last;
} Heap;
#else
typedef struct Heap {
    /* 0x00 */ u32 unk0;
    /* 0x04 */ struct Heap *next;
    /* 0x08 */ Block *first;
    /* 0x0C */ Block *last;
} Heap;
#endif
#ifdef TARGET_PC
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Block: (struct Block *)(uintptr_t)(x), enum n64p_Heap: (struct Heap *)(uintptr_t)(x), enum n64p_u32: (u32 *)(uintptr_t)(x), default: (x))
#define N64W(x) PC_N64W(x)
#else
#define N64P(x) x
#define N64W(x) x
#endif

typedef struct OSMesgQueue OSMesgQueue;
extern Heap *D_801527C8;
extern s32 D_80152770;
s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
void audio_reverb_update(u32 address, s32 tag);
void *func_800A47C0(void *dst, const void *src, u32 n);

#ifdef TARGET_PC
/* The manifest owner of func_800A51D8 is the empty deleted-procedure stub (src/blob/func_800A51D8.c), which
 * the weak copies bind to on PC. This inlined helper gets its own name there. */
#define func_800A51D8 car_damage_visual_heap
#endif
Heap *func_800A51D8(Heap *heap) {
    if (heap) return heap;
    return D_801527C8;
}

void car_damage_visual(Heap *hint) {
    Heap *heap;
    Block *block, *cursor, *after, *tail, *previous;
    Block *next;
    s32 adjacent;
    u32 size, difference;

    osRecvMesg((OSMesgQueue *)&D_80152770, NULL, 1);
    heap = func_800A51D8(hint);
    block = N64P(heap->first);
    while (block != NULL) {
        if (block->used && !block->lock && !block->ref && !block->tag) {
            audio_reverb_update((u32)((u8 *)block + 32), 0);
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
                *N64P(cursor->ref) = (u32)((u8 *)cursor + 32);
                cursor->used = 1;
                cursor->tag = block->tag;
                cursor->lock = block->lock;
                size = block->size;
                block->ref = 0;
                func_800A47C0((u8 *)cursor + 32, (u8 *)block + 32, block->size);
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
                audio_reverb_update((u32)((u8 *)block + 32), 0);
                if (after->magic != BLOCK_MAGIC) {
                    if (block->magic != BLOCK_MAGIC) after = N64P(previous->next);
                    else after = N64P(block->next);
                }
            }
        }
        block = after;
    }
    osSendMesg((OSMesgQueue *)&D_80152770, NULL, 0);
}
