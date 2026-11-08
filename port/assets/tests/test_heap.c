/* port/assets/tests/test_heap.c: the game heap's resize in place (ReallocateMemory, heap_release; the save code
 * resizes a ghost's header block with it) through the game's own heap code, whose block and heap headers keep their
 * N64 layout in the RDRAM window (magic, next, prev, size, owner, used, tag: 32-bit links; docs/port/LAYOUT.md 5).
 * A block shrunk with its successor in use gets a free block split off after it; one shrunk next to a free block
 * moves that block's header down; one grown into a free successor takes part of it, or all of it with what is left
 * under a header's size. The heap is the game's (InitMemory, AllocateMemoryHeap) in a window the test maps
 * (exit 77, skipped, if it can't).
 * Then port/ultra/heap.c on it (docs/port/MEMORY.md 4): the heap's size and its high-water marks as the game takes
 * blocks, a fallback's count, and at game.memory 32 RUSH2049_TEST_HEAP_FLOOR, whose blocks the game's own
 * allocate-at-address takes, so that the game's next blocks start at the floor and the runtime images' block is
 * where the image loads find it.
 * game: src/blob/groups/memsys/group.c src/blob/groups/heap_release/group.c src/blob/FindChunk.c
 * game: src/blob/FindHeap.c src/blob/groups/heap_release/alloc_at.c
 */
#include <stddef.h>
#include <string.h>

#include "pc_compat.h"   /* setenv on Windows */
#include "pc_prelude.h"
#include "support.h"
#include "ultra.h"   /* port/ultra/heap.c: pc_heap_stats */

#define W32(p) ((uint32_t)(uintptr_t)(p))
#define WP(a) ((uint8_t *)(uintptr_t)(a))
#define IN_WINDOW(p) ((uintptr_t)(p) >= PC_RDRAM_BASE && (uintptr_t)(p) < PC_RDRAM_BASE + PC_RDRAM_SIZE)
#define U32(p, o) (*(uint32_t *)((uint8_t *)(p) + (o)))
#define MAGIC 0xFEDCBA98u
/* a block header: magic +0, next +4, prev +8, size +0xC, owner +0x10, used +0x14, tag +0x15; the data at +0x20 */
#define NEXT(h) WP(U32(h, 4))
#define PREV(h) WP(U32(h, 8))
#define SIZE(h) U32(h, 0xC)
#define USED(h) ((h)[0x14])

/* ---------------------------------------------------------------- the game's data */

void *gMainHeap;                               /* the heap */
int32_t heap_lock_msg;
uint8_t heap_lock_queue[32];
int8_t expansion_pak_present;
TEST_ABS(game_bss_end);                        /* the end of the game's BSS: the heap starts there */
TEST_ABS(osMemSize);                           /* osMemSize */
#define OSMEMSIZE_OFF (TEST_N64(osMemSize) - PC_RDRAM_BASE)   /* osMemSize's offset in the window */
/* what heap_release's other functions reach */
int8_t extra_overlay_loaded, start_overlay_loaded;
uint8_t overlay_base[16];
uint32_t car_palettes[16 * 6];   /* the car palette descriptors, 24 bytes each */
uintptr_t gListeners, gEmitters;

/* ---------------------------------------------------------------- what the code under test calls */

void osCreateMesgQueue(void *q, void *m, int32_t n) {}
int32_t osSendMesg(void *q, void *m, int32_t flags) { return 0; }
int32_t osRecvMesg(void *q, void **m, int32_t flags) { return 0; }
void FillFreeMem(void *p, uint32_t n) {}
void *n64_memset(void *p, int c, size_t n) { return memset(p, c, n); }

void InitMemory(uint16_t count, uint16_t a);
void *AllocateMemoryHeap(void *heap, uint32_t size);
void ReallocateMemory(uint32_t address, uint32_t desired);
void *allocate_memory_at(uint32_t addr, uint32_t size);

/* the free total, as the game sums it (heap_total_free), here from the heap's blocks */
static uint32_t free_total(void)
{
    uint32_t sum = 0;
    for (uint8_t *b = WP(U32(gMainHeap, 8)); b; b = NEXT(b))
        if (!USED(b))
            sum += SIZE(b);
    return sum;
}

int main(void)
{
    uint8_t *w, *a, *b, *c, *ha, *hb, *hc, *hm, *after;
    uint32_t rest;

    test_rdram_bytes = 32u << 20;   /* room for game.memory 32 below */
    w = test_rdram();

    if (!w) {
        fprintf(stderr, "test_heap: the RDRAM window can't be mapped here: skipped\n");
        return 77;
    }
    pc_fix_absolute_got();
    U32(w, OSMEMSIZE_OFF) = 0x400000;           /* osMemSize: 4 MB */
    InitMemory(16, 16);
    a = AllocateMemoryHeap(NULL, 256);
    b = AllocateMemoryHeap(NULL, 128);
    c = AllocateMemoryHeap(NULL, 512);
    ha = a - 32, hb = b - 32, hc = c - 32;
    CHECK(IN_WINDOW(a) && IN_WINDOW(c) && U32(ha, 0) == MAGIC && NEXT(ha) == hb && PREV(hb) == ha && SIZE(ha) == 256);

    /* shrink a: b is in use, so a free block is split off after a */
    ReallocateMemory(W32(a), 64);
    hm = ha + 32 + 64;
    CHECK(SIZE(ha) == 64 && NEXT(ha) == hm && U32(hm, 0) == MAGIC && PREV(hm) == ha && NEXT(hm) == hb);
    CHECK(PREV(hb) == hm && SIZE(hm) == 256 - 64 - 32 && USED(hm) == 0 && U32(hm, 0x10) == 0 && USED(ha) == 1);

    /* shrink c: its successor is the free rest of the heap, whose header moves down */
    after = NEXT(hc);
    CHECK(IN_WINDOW(after) && USED(after) == 0 && PREV(after) == hc);
    rest = SIZE(after);
    ReallocateMemory(W32(c), 256);
    hm = hc + 32 + 256;
    CHECK(SIZE(hc) == 256 && NEXT(hc) == hm && PREV(hm) == hc && SIZE(hm) == 512 - 256 + rest && USED(hm) == 0);

    /* grow c by 128: the free successor gives that much */
    ReallocateMemory(W32(c), 384);
    hm = hc + 32 + 384;
    CHECK(SIZE(hc) == 384 && NEXT(hc) == hm && PREV(hm) == hc && SIZE(hm) == 512 - 384 + rest);
    CHECK(U32(hm, 0) == MAGIC && USED(hm) == 0 && USED(hc) == 1);

    /* port/ultra/heap.c's numbers: the heap from its header to osMemSize | K0, the least free as the game sums it */
    PcHeapStats st;
    CHECK(pc_heap_stats(&st) == 0 && st.base == W32(gMainHeap) && st.end == PC_RDRAM_BASE + 0x400000u);
    CHECK(st.size == st.end - st.base && st.peak + st.free_low == st.size && st.allocations == 6);
    CHECK(st.free_low < free_total() && st.shorts == 0 && st.nones == 0 && st.floor == 0);   /* before the shrinks */
    AllocateMemoryHeap(NULL, 1000);
    pc_heap_stats(&st);
    CHECK(st.free_low == free_total() && st.allocations == 7);
    pc_heap_short("a test", 12345);
    pc_heap_short("a test", 12345);
    pc_heap_stats(&st);
    CHECK(st.shorts == 2);

    /* game.memory 32 with the floor at 17 MiB: the heap made again (the game makes it once, at boot) */
    U32(w, OSMEMSIZE_OFF) = 32u << 20;
    setenv("RUSH2049_TEST_HEAP_FLOOR", "17", 1);
    InitMemory(16, 16);
    unsetenv("RUSH2049_TEST_HEAP_FLOOR");
    pc_heap_stats(&st);
    const uint32_t heap_end = PC_RDRAM_BASE + (32u << 20), heap_floor = PC_RDRAM_BASE + (17u << 20);
    const uint32_t images = N64_OVERLAY_BASE, images_size = N64_OVERLAY_BLOCK_END - N64_OVERLAY_BASE;
    CHECK(st.end == heap_end && st.floor == heap_floor && st.allocations == 0);
    a = AllocateMemoryHeap(NULL, 256);
    b = AllocateMemoryHeap(NULL, 0x200000);
    CHECK(W32(a) == heap_floor && W32(b) == heap_floor + 256 + 32);   /* the game's blocks from the floor */
    /* the runtime images' block: taken and locked, where the image loads find it (load_overlay's allocate_memory_at) */
    uint8_t *img = WP(images - 32);
    CHECK(U32(img, 0) == MAGIC && USED(img) == 1 && img[0x16] == 1 && SIZE(img) == images_size);
    CHECK(allocate_memory_at(images, images_size) == WP(images) && SIZE(img) == 0x30F80u);
    /* below the images' block and above it, to the floor: two more locked blocks, nothing free */
    CHECK(USED(PREV(img)) == 1 && PREV(img)[0x16] == 1 && USED(NEXT(img)) == 1 && NEXT(img)[0x16] == 1);
    CHECK(NEXT(NEXT(img)) == WP(heap_floor - 32));
    pc_heap_stats(&st);
    CHECK(st.peak > 17u << 20 && st.free_low == free_total() && st.allocations == 3);
    return TEST_DONE();
}
