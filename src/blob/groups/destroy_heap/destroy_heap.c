/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * destroy_heap (0x800E7A98, 172 bytes): destroy a game heap. Under the heap lock (osRecvMesg /
 * osJamMesg on heap_lock_queue) it takes heap, or the default (first) heap gMainHeap when heap is NULL,
 * unlinks it from the heap list that starts at gMainHeap (p->next == heap -> p->next = heap->next),
 * and frees its memory with DeallocateMemoryInternal(address, 1) (the heap-family free: address in a1,
 * tag in a2 by IPA; it uses s0/s1 unsaved, which is why this function saves them). No arcade ancestor.
 *
 * : the "default heap" choice is the static helper heap_or_default (same body as the one in the
 * locked src/blob/AllocateMemoryHeapHigh.c); umerge inlines it. Its return value is the web that holds h (v0)
 * and the assignment `h = heap_or_default(heap)` is the separate web starting at the join that retail
 * keeps in a1 (`move a1,v0`), which every hand-written copy spelling lost to copy propagation.
 * SHAPING DEVICE (disclosed): the compiled-out `if (heap == NULL) {}` before the lock (draft had it
 * too). It adds a use of the parameter before osRecvMesg, so the param web is coloured (a3, saved in its
 * home across the call: retail's `move a3,a0; sw a3,32(sp); lw a3,32(sp)`). `if (heap) {}` right after the
 * lock also matches. Without it the param is not coloured (`sw a0,32(sp)` / `lw v1,32(sp)`: 5 words).
 * Only EQUAL in the whole-program unit / this group (DeallocateMemoryInternal must be internal).
 */
typedef signed char s8; typedef unsigned char u8; typedef short s16; typedef unsigned short u16;
typedef int s32; typedef unsigned int u32;
#define NULL ((void *)0)
typedef struct OSMesgQueue OSMesgQueue;
s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
void DeallocateMemoryInternal(u32 address, s32 tag);

extern OSMesgQueue heap_lock_queue;

enum n64p_Heap { n64p_Heap_ };
typedef struct Heap {
    u32 pad0;
    enum n64p_Heap next;  /* 4 */
} Heap;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Heap: (struct Heap *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)
extern Heap *gMainHeap;

Heap *ConvertHeap(Heap *heap);

/* The heap's API takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
void destroy_heap(Heap *heap)
{
    Heap *p;
    Heap *h;

    if (heap == NULL) {
    }
    osRecvMesg(&heap_lock_queue, NULL, 1);
    h = ConvertHeap(heap);
    for (p = gMainHeap; p != NULL; p = N64P(p->next)) {
        if (N64P(p->next) == h) {
            p->next = h->next;
            break;
        }
    }
    DeallocateMemoryInternal(N64A(h), 1);
    osSendMesg(&heap_lock_queue, NULL, 0);
}
