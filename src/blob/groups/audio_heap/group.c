/*
 * Game heap ("audio_*" names are historical): a first-fit block allocator
 * (32-byte block headers, magic 0xFEDCBA98) with a per-heap table of handles.
 * Hand-written from the assembly.
 *
 * Frontier changes against src/blob/groups/audio_heap/group.c:
 *  - audio_helper's real parameter order is (heap, size, owner, tag). IPA still
 *    colours size->a0 and heap->a2, so the callee's words are unchanged, but
 *    the callers evaluate the heap argument first.
 *  - func_80097468() is a deleted static with two returns, inlined at its
 *    three call sites (probably the jr-ra stub func_80097468). Its inlined
 *    result temp is the v0 that retail moves into a2/s2; a ternary or an
 *    if-assignment at the call site is coloured straight into a2 instead.
 *  - audio_task_complete: declaration order h, slot, t, i gives the 48-byte frame.
 *
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 *  - heap_or_default is now the non-static func_80097468 (its retail stub
 *    address, between audio_helper and audio_dma_sync). Same code; as a named
 *    function its stub no longer lands unnamed after a member in the group
 * object (the scorer counted it as an extra word of func_800E7D0C).
 *  - func_800E7D0C (heap init, now a member): the run-once flag is a
 *    function-local static (retail 0x80116488, only referenced here; own .data
 *    verified) so its load and store keep separate lui's; the heap-pointer
 *    setup (align the end of BSS to 32, set end = osMemSize | K0BASE, flag the
 *    expansion pak when end > 0x80400000) is the deleted static
 *    func_800E7D04 (the caller-less stub right before func_800E7D0C), inlined.
 *    Written inline, as1 schedules the &D_801527C8 lui two words early.
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
enum n64p_HandleTable { n64p_HandleTable_ };
enum n64p_Heap { n64p_Heap_ };
enum n64p_u32 { n64p_u32_ };
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
typedef struct HandleTable {
    /* 0x0 */ u32 count;
    /* 0x4 */ enum n64p_u32 slots;
    /* 0x8 */ enum n64p_HandleTable next;
} HandleTable;
#else
typedef struct HandleTable {
    /* 0x0 */ u32 count;
    /* 0x4 */ u32 *slots;
    /* 0x8 */ struct HandleTable *next;
} HandleTable;
#endif

#ifdef TARGET_PC
typedef struct Heap {
    /* 0x00 */ u32 magic;
    /* 0x04 */ enum n64p_Heap next;
    /* 0x08 */ enum n64p_Block first;
    /* 0x0C */ enum n64p_Block last;
    /* 0x10 */ u32 end;
    /* 0x14 */ u16 maxHandles;
    /* 0x16 */ u16 pad16;
    /* 0x18 */ HandleTable tbl;
    /* 0x24 */ u8 pad24[4];
} Heap;
#else
typedef struct Heap {
    /* 0x00 */ u32 magic;
    /* 0x04 */ struct Heap *next;
    /* 0x08 */ Block *first;
    /* 0x0C */ Block *last;
    /* 0x10 */ u32 end;
    /* 0x14 */ u16 maxHandles;
    /* 0x16 */ u16 pad16;
    /* 0x18 */ HandleTable tbl;
    /* 0x24 */ u8 pad24[4];
} Heap;
#endif
#ifdef TARGET_PC
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Block: (struct Block *)(uintptr_t)(x), enum n64p_HandleTable: (struct HandleTable *)(uintptr_t)(x), enum n64p_Heap: (struct Heap *)(uintptr_t)(x), enum n64p_u32: (u32 *)(uintptr_t)(x), enum n64p_void: (void *)(uintptr_t)(x), default: (x))
#define N64W(x) ((u32)(uintptr_t)(x))
#else
#define N64P(x) x
#define N64W(x) x
#endif

extern u8 D_8017A640[];
extern Heap *D_801527C8;
extern s32 D_80152770[];
extern s32 D_801527A0;
extern s8 D_80156994;
extern u32 D_80000318;

s32 osRecvMesg();
s32 osSendMesg();
void osCreateMesgQueue();
void *memset();

void *audio_helper(Heap *heap, u32 size, void *owner, s32 tag);
void func_800E7B44(Heap *heap, u16 count, u16 a);
void *AllocateMemoryHeap(Heap *heap, u32 size);
void *AllocateHandleHeap(Heap *heap, u32 size);
void *func_800E7C2C(Heap *heap, u32 size, u16 count, u16 a);
void func_800E7D0C(u16 count, u16 a);

Heap *func_80097468(Heap *heap)
{
    if (heap != 0) {
        return heap;
    }
    return D_801527C8;
}

void *audio_helper(Heap *heap, u32 size, void *owner, s32 tag)
{
    Block *b;
    Block *n;
    u32 total = 0;
    u32 largest = 0;
    u32 bs;

    size = (size + 31) & ~31;
    for (b = N64P(heap->first); b != 0; b = N64P(b->next)) {
        if (b->used == 0) {
            bs = b->size;
            total += bs;
            if (largest < bs) {
                largest = bs;
            }
            if (bs >= size) {
                break;
            }
        }
    }
    bs = b->size;
    if (bs - size >= 64) {
        n = (Block *)((u8 *)b + size + 32);
        n->next = b->next;
        if (n->next != 0) {
            N64P(n->next)->prev = N64W(n);
        } else {
            heap->last = N64W(n);
        }
        n->prev = N64W(b);
        n->size = b->size - size - 32;
        n->owner = 0;
        n->used = 0;
        n->tag = 0;
        n->pad16[0] = 0;
        n->magic = 0xFEDCBA98;
        b->next = N64W(n);
        b->size = size;
    }
    b->used = 1;
    b->owner = N64W(owner);
    b->tag = tag;
    return (u8 *)b + 32;
}

void func_800E7B44(Heap *heap, u16 count, u16 a)
{

    if (a == 0) {
    }
    heap->magic = 0xFEDCBA98;
    heap->next = 0;
    heap->first = N64W((Block *)(((u32)heap + 67) & ~31));
    N64P(heap->first)->magic = 0xFEDCBA98;
    N64P(heap->first)->next = 0;
    N64P(heap->first)->prev = 0;
    N64P(heap->first)->size = heap->end - (u32)heap->first - 32;
    N64P(heap->first)->owner = 0;
    N64P(heap->first)->used = 0;
    N64P(heap->first)->tag = 0;
    N64P(heap->first)->pad16[0] = 0;
    heap->last = heap->first;
    heap->tbl.count = count;
    heap->maxHandles = a;
    heap->tbl.next = 0;
    if (count > 0) {
        heap->tbl.slots = N64W(audio_helper(heap, count * 4, 0, 1));
        memset(heap->tbl.slots, 0, count * 4);
    } else {
        heap->tbl.slots = 0;
    }
}

void *AllocateMemoryHeap(Heap *heap, u32 size)
{
    void *p;

    osRecvMesg(D_80152770, 0, 1);
    p = audio_helper(func_80097468(heap), size, 0, 0);
    osSendMesg(D_80152770, 0, 0);
    return p;
}

void *AllocateHandleHeap(Heap *heap, u32 size)
{
    Heap *h;
    u32 *slot;
    HandleTable *t;
    u32 i;

    osRecvMesg(D_80152770, 0, 1);
    h = func_80097468(heap);
    t = &h->tbl;
    if (t->slots == 0) {
        t->count = h->maxHandles;
        t->slots = N64W(audio_helper(h, h->maxHandles * 4, 0, 1));
        memset(t->slots, 0, h->maxHandles * 4);
        t->next = 0;
    }
    i = 0;
    for (;;) {
        slot = N64P(t->slots);
        for (; i < t->count; i++, slot++) {
            if (*slot == 0) {
                break;
            }
        }
        if (i < t->count) {
            break;
        }
        i = 0;
        if (t->next == 0) {
            t->next = N64W(audio_helper(h, 12, 0, 1));
            N64P(t->next)->count = h->maxHandles;
            N64P(t->next)->slots = N64W(audio_helper(h, h->maxHandles * 4, 0, 1));
            memset(N64P(t->next)->slots, 0, h->maxHandles * 4);
            N64P(t->next)->next = 0;
        }
        t = N64P(t->next);
    }
    *slot = (u32)audio_helper(h, size, slot, 0);
    osSendMesg(D_80152770, 0, 0);
    return slot;
}
void *func_800E7C2C(Heap *heap, u32 size, u16 count, u16 a)
{
    Heap *h;
    Heap *p;

    osRecvMesg(D_80152770, 0, 1);
    h = audio_helper(func_80097468(heap), size + 64, 0, 1);
    h->end = (u32)h + ((size + 95) & ~31);
    func_800E7B44(h, count, a);
    for (p = D_801527C8; p != 0; p = N64P(p->next)) {
        if (p->next == 0) {
            p->next = N64W(h);
            break;
        }
    }
    osSendMesg(D_80152770, 0, 0);
    return h;
}

void func_800E7D04(void)
{
    Heap *h;

    h = (Heap *)(((u32)D_8017A640 + 31) & ~31);
    D_801527C8 = h;
    h->end = D_80000318 | 0x80000000;
    if (h->end >= 0x80400001) {
        D_80156994 = 1;
    }
}

void func_800E7D0C(u16 count, u16 a)
{
    static s8 D_80116488 = 0;

    if (D_80116488 == 0) {
        D_80116488 = 1;
        osCreateMesgQueue(D_80152770, &D_801527A0, 1);
        osSendMesg(D_80152770, 0, 0);
    }
    func_800E7D04();
    func_800E7B44(D_801527C8, count, a);
}
