/*
 * Largest free block in a selected game heap, under its message-queue lock.
 * func_800E79F8: 0x800E79F8..0x800E7A98, 160 bytes.
 *
 * The two-return heap selector and wrapper/worker structure follow the
 * accepted heap_free_total group. The immediately preceding
 * callerless stub func_800E79F0 has the same placement as that group's
 * func_800BAF90 worker. Its assignment here is a source-boundary inference,
 * not a recovered original declaration or an arcade ancestry claim.
 *
 * The meaningful worker is defined after the wrapper, as in free-total.
 * IDO O3 inlines it, leaves the real 8-byte stub, and schedules the unlock
 * arguments before the scan. No filler locals or artificial reads are used.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif

#ifdef TARGET_PC
enum n64p_Block { n64p_Block_ };
enum n64p_Heap { n64p_Heap_ };
enum n64p_void { n64p_void_ };
#endif
#ifdef TARGET_PC
typedef struct Block {
    /* 0x00 */ u32 magic;
    /* 0x04 */ enum n64p_Block next;
    /* 0x08 */ enum n64p_Block prev;
    /* 0x0C */ u32 size;
    /* 0x10 */ enum n64p_void owner;
    /* 0x14 */ s8 used;
    /* 0x15 */ u8 tag;
    /* 0x16 */ u8 pad16[10];
} Block; /* 0x20 */
#else
typedef struct Block {
    /* 0x00 */ u32 magic;
    /* 0x04 */ struct Block *next;
    /* 0x08 */ struct Block *prev;
    /* 0x0C */ u32 size;
    /* 0x10 */ void *owner;
    /* 0x14 */ s8 used;
    /* 0x15 */ u8 tag;
    /* 0x16 */ u8 pad16[10];
} Block; /* 0x20 */
#endif

#ifdef TARGET_PC
typedef struct Heap {
    /* 0x00 */ u32 magic;
    /* 0x04 */ enum n64p_Heap next;
    /* 0x08 */ enum n64p_Block first;
} Heap;
#else
typedef struct Heap {
    /* 0x00 */ u32 magic;
    /* 0x04 */ struct Heap *next;
    /* 0x08 */ Block *first;
} Heap;
#endif
#ifdef TARGET_PC
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Block: (struct Block *)(uintptr_t)(x), enum n64p_Heap: (struct Heap *)(uintptr_t)(x), enum n64p_void: (void *)(uintptr_t)(x), default: (x))
#define N64W(x) ((u32)(uintptr_t)(x))
#else
#define N64P(x) x
#define N64W(x) x
#endif

typedef struct OSMesgQueue OSMesgQueue;

extern Heap *D_801527C8;
extern OSMesgQueue D_80152770;

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);

u32 func_800E79F0(Heap *heap);

static Heap *heap_or_default(Heap *heap)
{
    if (heap != 0) {
        return heap;
    }
    return D_801527C8;
}

u32 func_800E79F8(Heap *heap)
{
    u32 largest;

    osRecvMesg(&D_80152770, 0, 1);
    largest = func_800E79F0(heap);
    osSendMesg(&D_80152770, 0, 0);
    return largest;
}

u32 func_800E79F0(Heap *heap)
{
    u32 largest;
    Block *block;

    block = N64P(heap_or_default(heap)->first);
    largest = 0;
    while (block != 0) {
        if (block->used == 0) {
            if (largest < block->size) {
                largest = block->size;
            }
        }
        block = N64P(block->next);
    }
    return largest;
}
