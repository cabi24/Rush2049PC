/* port/ultra/heap.c: the game heap as the PC sees it (docs/port/MEMORY.md 4).
 *
 * The game's heap (src/blob/groups/memsys) runs from align32(0x8017A640) to osMemSize | K0BASE: game.memory sets
 * how far. Its allocator tells the PC when the heap is made (pc_heap_created, InitMemory) and after each block it
 * takes (pc_heap_allocated: AllocateMemory, allocate_memory_at, ReallocateMemory's growth), under the heap's lock, and the heap
 * is walked then: the least it has had free and the largest free block at that moment are the high-water marks the
 * exit line gives (pc_heap_report). The game's own fallbacks when it finds the heap short (LoadCars sharing a car type,
 * a car's paint) and an allocation no block can hold (which the N64 game takes as a null block) are logged the first
 * time each happens, and counted.
 *
 * RUSH2049_TEST_HEAP_FLOOR=MiB (tests): the heap's room below that address is taken at its creation, by the game's own
 * allocate-at-address (allocate_memory_at), in three blocks around the runtime images' block at 0x8038A400 (taken too, as
 * the image loads take it), each with a lock count of 1 as PurgeableMemory gives the images' block, so ClearMemory
 * keeps them; everything the game allocates afterwards lands above them: data past 16 MiB, through every path, at
 * game.memory 32 or more. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ultra.h"

#define HEAP_MAGIC 0xFEDCBA98u
#define IMAGES_LO 0x8038A400u   /* the runtime images' block (heap_release/overlay_loader.c) */
#define IMAGES_HI 0x803BB380u

/* The game's heap and block headers keep their N64 layout in the window (memsys/group.c): 32-bit links. */
typedef struct {
    uint32_t magic, next, first, last, end;
} HeapHeader;
typedef struct {
    uint32_t magic, next, prev, size, owner;
    int8_t used, tag;
    uint8_t lock;
} BlockHeader;

/* the game's allocate-at-address (heap_release/alloc_at.c) */
void *allocate_memory_at(uint32_t addr, uint32_t size);

static struct {
    uint32_t heap;                /* the default heap's header (gMainHeap), 0 before it is made */
    uint32_t end;                 /* its end: osMemSize | K0BASE */
    uint32_t free_low;            /* the least free (the game's sum of free blocks, heap_total_free) */
    uint32_t largest_low;         /* the largest free block then */
    uint32_t allocations;
    uint32_t shorts, nones;       /* the game's fallbacks; allocations nothing could hold */
    uint32_t floor;               /* RUSH2049_TEST_HEAP_FLOOR's address, 0 without */
    int walking;                  /* the floor's own allocations aren't counted */
} H;

static const void *at(uint32_t addr) { return pc_n64_ptr(addr); }

/* Free bytes (as heap_total_free sums them) and the largest free block; -1 when the list isn't a heap's. */
static int walk(uint32_t heap, uint32_t *free_bytes, uint32_t *largest, uint32_t *blocks)
{
    const HeapHeader *h = at(heap);
    uint32_t sum = 0, big = 0, n = 0, b, prev = 0;
    if (!h || h->magic != HEAP_MAGIC)
        return -1;
    for (b = h->first; b; b = ((const BlockHeader *)at(b))->next) {
        const BlockHeader *k = at(b);
        if (!k || k->magic != HEAP_MAGIC || b <= prev || b >= h->end || ++n > (1u << 22))
            return -1;
        if (k->used == 0) {
            sum += k->size;
            if (k->size > big)
                big = k->size;
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

/* One block of [addr, addr + size), taken as the game takes the images' block, its lock count 1 (what
 * lock_memory and PurgeableMemory(p, 0) do to a block with no owner). */
static void take(uint32_t addr, uint32_t size)
{
    BlockHeader *b;
    allocate_memory_at(addr, size);
    b = pc_n64_ptr(addr - 32);
    if (b && b->magic == HEAP_MAGIC && b->lock < 255)
        b->lock++;
}

static void apply_floor(const HeapHeader *h)
{
    const char *e = getenv("RUSH2049_TEST_HEAP_FLOOR");
    if (!e || !*e)
        return;
    uint32_t mb = (uint32_t)strtoul(e, NULL, 10), floor = PC_RDRAM_BASE + (mb << 20), b, first = 0;
    for (b = h->first; b && !first; b = ((const BlockHeader *)at(b))->next)   /* the free block after the handles */
        if (((const BlockHeader *)at(b))->used == 0)
            first = b + 32;
    if (mb < 5 || mb > 4095 || floor + (1u << 20) > h->end || !first || first + 64 >= IMAGES_LO - 32) {
        pc_log("heap: RUSH2049_TEST_HEAP_FLOOR=%s: the heap (to 0x%08X) has no room above %u MiB; not applied", e,
               h->end, mb);
        return;
    }
    H.walking = 1;
    take(first, IMAGES_LO - 32 - first);              /* the boot data's room, to the images' header */
    take(IMAGES_LO, IMAGES_HI - IMAGES_LO);           /* the runtime images' block */
    take(IMAGES_HI + 32, floor - 32 - (IMAGES_HI + 32));
    H.walking = 0;
    H.floor = floor;
    pc_log("heap: test: RUSH2049_TEST_HEAP_FLOOR=%u: the heap below 0x%08X is taken; the game's blocks start there",
           mb, floor);
}

void pc_heap_created(void *heap)
{
    const HeapHeader *h = heap;
    uint32_t addr = pc_n64_addr(heap);
    if (!addr || h->magic != HEAP_MAGIC)
        return;
    memset(&H, 0, sizeof H);
    H.heap = addr;
    H.end = h->end;
    H.free_low = ~0u;
    apply_floor(h);
    sample();
    if (pc_verbose)
        pc_log("heap: 0x%08X-0x%08X, %u bytes (osMemSize %u MiB)", addr, h->end, h->end - addr, pc_os_mem_size() >> 20);
}

void pc_heap_allocated(void *heap)
{
    if (!H.heap || pc_n64_addr(heap) != H.heap || H.walking)
        return;
    H.allocations++;
    sample();
}

void pc_heap_none(void *heap, uint32_t size, uint32_t free_bytes, uint32_t largest)
{
    if (H.heap && pc_n64_addr(heap) != H.heap)
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
    s->base = H.heap;
    s->end = H.end;
    s->size = H.end - H.heap;
    s->free_low = H.free_low == ~0u ? 0 : H.free_low;
    s->peak = s->size - s->free_low;
    s->largest_low = H.largest_low;
    s->allocations = H.allocations;
    s->shorts = H.shorts;
    s->nones = H.nones;
    s->floor = H.floor;
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
