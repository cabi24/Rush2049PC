/* port/assets/tests/test_heap.c: the game heap's resize in place (ReallocateMemory, heap_release; the save code
 * resizes a ghost's header block with it) through the game's own heap code, whose chunk and heap headers hold host
 * pointers in the N64's sizes (include/game/heap.h: next, prev, master, size, used, internal, locked, magic in 32
 * bytes; docs/port/NATIVE.md step 6). A block shrunk with its successor in use gets a free block split off after it;
 * one shrunk next to a free block moves that block's header down; one grown into a free successor takes part of it, or
 * all of it with what is left under a header's size. The heap is the game's (InitMemory, AllocateMemoryHeap) over the
 * memory pc_heap_memory gives it, the N64's place in a window the test maps (exit 77, skipped, if it can't).
 * Then port/ultra/heap.c on it (docs/port/MEMORY.md 4): the heap's size and its high-water marks as the game takes
 * blocks, a fallback's count, and at game.memory 32 RUSH2049_TEST_HEAP_FLOOR, whose blocks the game's own
 * allocate-at-address takes, so that the game's next blocks start at the floor and the runtime images' block is
 * where the image loads find it: the N64's place, PC_RUNTIME_IMAGE_OFFSET past the main heap's header.
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
#define IN_WINDOW(p) ((uintptr_t)(p) >= PC_RDRAM_BASE && (uintptr_t)(p) < PC_RDRAM_BASE + PC_RDRAM_SIZE)

/* A chunk's header (include/game/heap.h's tMemoryChunk), 32 bytes before its data; a heap's header (tHeap). */
typedef struct Chunk {
    struct Chunk *next, *prev;
    void **master;
    uint32_t size;
    int8_t used;
    uint8_t internal, locked, magic;
} Chunk;
typedef struct {
    uint32_t magic;
    uint16_t more;
    void *next;
    Chunk *chunks, *last;
    uint8_t *end;
} Heap;
_Static_assert(sizeof(Chunk) == 32, "a chunk's header");
#define CHUNK_MAGIC 0x98
#define HDR(p) ((Chunk *)((uint8_t *)(p) - 32))

/* ---------------------------------------------------------------- the game's data */

void *gMainHeap;                               /* the heap */
int32_t heap_lock_msg;
uint8_t heap_lock_queue[32];
int8_t expansion_pak_present;
TEST_ABS(osMemSize);                           /* osMemSize */
#define OSMEMSIZE_OFF (TEST_N64(osMemSize) - PC_RDRAM_BASE)   /* osMemSize's offset in the window */
#define U32(p, o) (*(uint32_t *)((uint8_t *)(p) + (o)))
/* what heap_release's other functions reach */
int8_t extra_overlay_loaded, start_overlay_loaded;
uintptr_t gListeners, gEmitters;
uint8_t *runtime_image_block(void) { return NULL; }

/* ---------------------------------------------------------------- what the code under test calls */

void osCreateMesgQueue(void *q, void *m, int32_t n) {}
int32_t osSendMesg(void *q, void *m, int32_t flags) { return 0; }
int32_t osRecvMesg(void *q, void **m, int32_t flags) { return 0; }
void FillFreeMem(void *p, uint32_t n) {}
void *n64_memset(void *p, int c, size_t n) { return memset(p, c, n); }

void InitMemory(uint16_t count, uint16_t a);
void *AllocateMemoryHeap(void *heap, uint32_t size);
void ReallocateMemory(void *ptr, uint32_t desired);
void *allocate_memory_at(void *ptr, uint32_t size);

/* the free total, as the game sums it (heap_total_free), here from the heap's chunks */
static uint32_t free_total(void)
{
    uint32_t sum = 0;
    for (Chunk *b = ((Heap *)gMainHeap)->chunks; b; b = b->next)
        if (!b->used)
            sum += b->size;
    return sum;
}

int main(void)
{
    uint8_t *w, *a, *b, *c;
    Chunk *ha, *hb, *hc, *hm, *after;
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
    /* the heap where the N64's was: its header at the end of the game's BSS, its first chunk 64 bytes past it */
    CHECK(W32(gMainHeap) == N64_HEAP_BASE && ((Heap *)gMainHeap)->magic == 0xFEDCBA98u &&
          (uint8_t *)((Heap *)gMainHeap)->chunks == (uint8_t *)gMainHeap + 64 &&
          W32(((Heap *)gMainHeap)->end) == PC_RDRAM_BASE + 0x400000u && !expansion_pak_present);
    a = AllocateMemoryHeap(NULL, 256);
    b = AllocateMemoryHeap(NULL, 128);
    c = AllocateMemoryHeap(NULL, 512);
    ha = HDR(a), hb = HDR(b), hc = HDR(c);
    CHECK(IN_WINDOW(a) && IN_WINDOW(c) && ha->magic == CHUNK_MAGIC && ha->next == hb && hb->prev == ha &&
          ha->size == 256);
    /* the first chunk is the 16 handles' table: 16 slots of 8 bytes, rounded to 32 */
    CHECK(((Heap *)gMainHeap)->chunks->size == 128 && ((Heap *)gMainHeap)->chunks->internal == 1 &&
          ((Heap *)gMainHeap)->chunks->next == ha);

    /* shrink a: b is in use, so a free block is split off after a */
    ReallocateMemory(a, 64);
    hm = (Chunk *)((uint8_t *)ha + 32 + 64);
    CHECK(ha->size == 64 && ha->next == hm && hm->magic == CHUNK_MAGIC && hm->prev == ha && hm->next == hb);
    CHECK(hb->prev == hm && hm->size == 256 - 64 - 32 && hm->used == 0 && hm->master == NULL && ha->used == 1);

    /* shrink c: its successor is the free rest of the heap, whose header moves down */
    after = hc->next;
    CHECK(IN_WINDOW(after) && after->used == 0 && after->prev == hc);
    rest = after->size;
    ReallocateMemory(c, 256);
    hm = (Chunk *)((uint8_t *)hc + 32 + 256);
    CHECK(hc->size == 256 && hc->next == hm && hm->prev == hc && hm->size == 512 - 256 + rest && hm->used == 0);

    /* grow c by 128: the free successor gives that much */
    ReallocateMemory(c, 384);
    hm = (Chunk *)((uint8_t *)hc + 32 + 384);
    CHECK(hc->size == 384 && hc->next == hm && hm->prev == hc && hm->size == 512 - 384 + rest);
    CHECK(hm->magic == CHUNK_MAGIC && hm->used == 0 && hc->used == 1);

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
    uint8_t *images = (uint8_t *)gMainHeap + PC_RUNTIME_IMAGE_OFFSET;
    CHECK(st.end == heap_end && st.floor == heap_floor && st.allocations == 0 && expansion_pak_present);
    a = AllocateMemoryHeap(NULL, 256);
    b = AllocateMemoryHeap(NULL, 0x200000);
    CHECK(W32(a) == heap_floor && W32(b) == heap_floor + 256 + 32);   /* the game's blocks from the floor */
    /* the runtime images' block: the N64's place, taken and locked, where the image loads find it (load_overlay's
     * allocate_memory_at) */
    Chunk *img = HDR(images);
    CHECK(W32(images) == N64_OVERLAY_BASE && PC_RUNTIME_IMAGE_SIZE == N64_OVERLAY_BLOCK_END - N64_OVERLAY_BASE);
    CHECK(img->magic == CHUNK_MAGIC && img->used == 1 && img->locked == 1 && img->size == PC_RUNTIME_IMAGE_SIZE);
    CHECK(allocate_memory_at(images, PC_RUNTIME_IMAGE_SIZE) == images && img->size == 0x30F80u);
    /* below the images' block and above it, to the floor: two more locked blocks, nothing free */
    CHECK(img->prev->used == 1 && img->prev->locked == 1 && img->next->used == 1 && img->next->locked == 1);
    CHECK(W32(img->next->next) == heap_floor - 32);
    pc_heap_stats(&st);
    CHECK(st.peak > 17u << 20 && st.free_low == free_total() && st.allocations == 3);
    return TEST_DONE();
}
