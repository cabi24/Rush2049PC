/* port/ultra/heap.c: the game heap as the PC sees it (docs/port/MEMORY.md 4).
 *
 * The game's heap (src/blob/groups/memsys) manages the memory pc_heap_memory gives it: on the N64 everything from the
 * end of the game image's BSS to the end of RDRAM, osMemSize, which game.memory sets. Its allocator tells the PC when
 * the heap is made (pc_heap_created, InitMemory) and after each block it takes (pc_heap_allocated: AllocateMemory,
 * allocate_memory_at, ReallocateMemory's growth), under the heap's lock, and the heap is walked then: the least it has
 * had free and the largest free block at that moment are the high-water marks the exit line gives (pc_heap_report).
 * The game's own fallbacks when it finds the heap short (LoadCars sharing a car type, a car's paint) and an allocation
 * no block can hold (which the N64 game takes as a null block) are logged the first time each happens, and counted.
 *
 * RUSH2049_TEST_HEAP_FLOOR=MiB (tests): the heap's room below that place in RDRAM is taken at its creation, by the
 * game's own allocate-at-address (allocate_memory_at), in three blocks around the runtime images' block (taken too, as
 * the image loads take it), each with a lock count of 1 as PurgeableMemory gives the images' block, so ClearMemory
 * keeps them; everything the game allocates afterwards lands above them: data past 16 MiB, through every path, at
 * game.memory 32 or more.
 *
 * The heap's memory is the N64's place for it in the RDRAM window until docs/port/NATIVE.md step 12 makes it a block
 * of PC memory: the exit line and pc_heap_stats give its window addresses, as they always have. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_game_types.h"   /* PC_RUNTIME_IMAGE_OFFSET, PC_RUNTIME_IMAGE_SIZE */
#include "ultra.h"

/* The game's heap headers (include/game/heap.h, MAT3 memsys.c's tHeap and tMemoryChunk): host pointers in the N64's
 * sizes. The fields read here, at heap.h's places. */
#define HEAP_MAGIC 0xFEDCBA98u   /* a heap header's first word */
#define CHUNK_MAGIC 0x98u        /* a chunk header's last byte */
#define CHUNK_HEADER 32u
typedef struct HeapChunk {
    struct HeapChunk *next, *prev;
    void **master;
    uint32_t size;
    int8_t used;
    uint8_t internal, locked, magic;
} HeapChunk;
typedef struct {
    uint32_t magic;
    uint16_t num_additional_masters;
    void *next;
    HeapChunk *chunks, *last_chunk;
    uint8_t *end;
} HeapHeader;
_Static_assert(sizeof(HeapChunk) == CHUNK_HEADER && offsetof(HeapChunk, size) == 0x18 &&
                   offsetof(HeapChunk, used) == 0x1C && offsetof(HeapChunk, locked) == 0x1E &&
                   offsetof(HeapChunk, magic) == 0x1F,
               "a chunk's header as include/game/heap.h has it");
_Static_assert(offsetof(HeapHeader, chunks) == 0x10 && offsetof(HeapHeader, end) == 0x20,
               "a heap's header as include/game/heap.h has it");

/* the game's allocate-at-address (heap_release/alloc_at.c) */
void *allocate_memory_at(void *ptr, uint32_t size);

static struct {
    const HeapHeader *heap;       /* the default heap's header (gMainHeap), NULL before it is made */
    uint32_t free_low;            /* the least free (the game's sum of free blocks, heap_total_free) */
    uint32_t largest_low;         /* the largest free block then */
    uint32_t allocations;
    uint32_t shorts, nones;       /* the game's fallbacks; allocations nothing could hold */
    const uint8_t *floor;         /* RUSH2049_TEST_HEAP_FLOOR's place, NULL without */
    int walking;                  /* the floor's own allocations aren't counted */
} H;

/* A place in the heap's memory as the N64 addressed it: its KSEG0 address in the RDRAM window. */
static uint32_t window_address(const void *p)
{
    return p ? PC_RDRAM_BASE + (uint32_t)((const uint8_t *)p - pc_rdram()) : 0;
}

/* The memory the game's main heap manages on a console with mem_size bytes of RDRAM (locate_main_heap): its first
 * byte, and its size in *size. The N64 game's heap ran from the end of its image's BSS (N64_HEAP_BASE) to the end of
 * RDRAM; until NATIVE.md step 12 it is that part of the window, so every block the heap hands out is where the N64's
 * was. */
void *pc_heap_memory(uint32_t mem_size, uint32_t *size)
{
    const uint32_t below = N64_HEAP_BASE - PC_RDRAM_BASE;   /* the game image's code, data and BSS, low memory */
    *size = mem_size > below ? mem_size - below : 0;
    return pc_rdram() + below;
}

/* Free bytes (as heap_total_free sums them) and the largest free block; -1 when the list isn't a heap's: a chunk
 * outside the heap, out of order or without its magic, or more of them than the heap could hold. */
static int walk(const HeapHeader *h, uint32_t *free_bytes, uint32_t *largest, uint32_t *blocks)
{
    uint32_t sum = 0, big = 0, n = 0;
    const HeapChunk *b, *prev = NULL;
    if (!h || h->magic != HEAP_MAGIC)
        return -1;
    for (b = h->chunks; b; b = b->next) {
        const uint8_t *at = (const uint8_t *)b;
        if (at <= (const uint8_t *)prev || at < (const uint8_t *)h || at + CHUNK_HEADER > h->end ||
            b->magic != CHUNK_MAGIC || ++n > (1u << 22))
            return -1;
        if (b->used == 0) {
            sum += b->size;
            if (b->size > big)
                big = b->size;
        }
        prev = b;
    }
    *free_bytes = sum, *largest = big, *blocks = n;
    return 0;
}

static void sample(void)
{
    uint32_t free_bytes, largest, blocks;
    if (walk(H.heap, &free_bytes, &largest, &blocks) != 0)
        return;
    if (free_bytes < H.free_low) {
        H.free_low = free_bytes;
        H.largest_low = largest;
    }
}

/* One block of [p, p + size), taken as the game takes the images' block, its lock count 1 (what lock_memory and
 * PurgeableMemory(p, 0) do to a block with no owner). */
static void take(uint8_t *p, uint32_t size)
{
    HeapChunk *b = (HeapChunk *)(p - CHUNK_HEADER);
    allocate_memory_at(p, size);
    if (b->magic == CHUNK_MAGIC && b->locked < 255)
        b->locked++;
}

static void apply_floor(const HeapHeader *h)
{
    const char *e = getenv("RUSH2049_TEST_HEAP_FLOOR");
    if (!e || !*e)
        return;
    uint32_t mb = (uint32_t)strtoul(e, NULL, 10);
    uint8_t *floor = pc_rdram() + ((size_t)mb << 20), *first = NULL;
    uint8_t *images = (uint8_t *)h + PC_RUNTIME_IMAGE_OFFSET, *images_end = images + PC_RUNTIME_IMAGE_SIZE;
    for (const HeapChunk *b = h->chunks; b && !first; b = b->next)   /* the free block after the handles */
        if (b->used == 0)
            first = (uint8_t *)b + CHUNK_HEADER;
    if (mb < 5 || mb > 4095 || floor + (1u << 20) > h->end || !first || first + 64 >= images - CHUNK_HEADER) {
        pc_log("heap: RUSH2049_TEST_HEAP_FLOOR=%s: the heap (to 0x%08X) has no room above %u MiB; not applied", e,
               window_address(h->end), mb);
        return;
    }
    H.walking = 1;
    take(first, (uint32_t)(images - CHUNK_HEADER - first));             /* the boot data's room, to the images' header */
    take(images, PC_RUNTIME_IMAGE_SIZE);                                /* the runtime images' block */
    take(images_end + CHUNK_HEADER, (uint32_t)(floor - CHUNK_HEADER - (images_end + CHUNK_HEADER)));
    H.walking = 0;
    H.floor = floor;
    pc_log("heap: test: RUSH2049_TEST_HEAP_FLOOR=%u: the heap below 0x%08X is taken; the game's blocks start there",
           mb, window_address(floor));
}

void pc_heap_created(void *heap)
{
    const HeapHeader *h = heap;
    if (!h || h->magic != HEAP_MAGIC)
        return;
    memset(&H, 0, sizeof H);
    H.heap = h;
    H.free_low = ~0u;
    apply_floor(h);
    sample();
    if (pc_verbose)
        pc_log("heap: 0x%08X-0x%08X, %u bytes (osMemSize %u MiB)", window_address(h), window_address(h->end),
               (uint32_t)(h->end - (const uint8_t *)h), pc_os_mem_size() >> 20);
}

void pc_heap_allocated(void *heap)
{
    if (!H.heap || heap != H.heap || H.walking)
        return;
    H.allocations++;
    sample();
}

void pc_heap_none(void *heap, uint32_t size, uint32_t free_bytes, uint32_t largest)
{
    if (H.heap && heap != H.heap)
        return;
    if (!H.nones++)
        pc_log("heap: no free block of %u bytes (%u free, the largest %u): the game takes a null block, as on the "
               "N64; game.memory gives it more", size, free_bytes, largest);
}

/* Called from the game thread outside the heap's functions, so the heap is whole: no thread switch happens inside
 * them (they make no libultra call between taking the heap's lock and giving it back). */
void pc_heap_short(const char *what, uint32_t need)
{
    static const char *seen[8];
    uint32_t have = 0, largest = 0, blocks = 0;
    size_t i;
    H.shorts++;
    if (H.heap)
        walk(H.heap, &have, &largest, &blocks);
    for (i = 0; i < 8 && seen[i] && strcmp(seen[i], what); i++)
        ;
    if (i < 8 && !seen[i]) {
        seen[i] = what;
        pc_log("heap: %s: %u bytes wanted, %u free: the game falls back (the first time; the exit line counts "
               "them); game.memory gives it more", what, need, have);
    }
}

int pc_heap_stats(PcHeapStats *s)
{
    memset(s, 0, sizeof *s);
    s->memory = pc_rdram_size();
    s->mem_size = pc_os_mem_size();
    if (!H.heap)
        return -1;
    s->base = window_address(H.heap);
    s->end = window_address(H.heap->end);
    s->size = (uint32_t)(H.heap->end - (const uint8_t *)H.heap);
    s->free_low = H.free_low == ~0u ? 0 : H.free_low;
    s->peak = s->size - s->free_low;
    s->largest_low = H.largest_low;
    s->allocations = H.allocations;
    s->shorts = H.shorts;
    s->nones = H.nones;
    s->floor = window_address(H.floor);
    return 0;
}

void pc_heap_report(void)
{
    PcHeapStats s;
    if (pc_heap_stats(&s) != 0) {
        fprintf(stderr, "rush2049pc: heap: not made (the game didn't start); window %u MiB\n", s.memory >> 20);
        return;
    }
    char floor[48] = "";
    if (s.floor)
        snprintf(floor, sizeof floor, " (test floor at 0x%08X)", s.floor);
    fprintf(stderr, "rush2049pc: heap: peak %u of %u bytes, free-low %u (largest block %u), at 0x%08X-0x%08X, "
                    "osMemSize %u MiB, window %u MiB; %u allocations, short %u, none %u%s\n",
            s.peak, s.size, s.free_low, s.largest_low, s.base, s.end, s.mem_size >> 20, s.memory >> 20,
            s.allocations, s.shorts, s.nones, floor);
}
