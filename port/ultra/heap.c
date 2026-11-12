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
 * RUSH2049_TEST_HEAP_FLOOR=MiB (tests): the heap's room below that place in the console's RDRAM (counted as the N64
 * counted it, the heap's header PC_HEAP_OFFSET bytes in) is taken at its creation, by the game's own
 * allocate-at-address (allocate_memory_at), in three blocks around the runtime images' block (taken too, as the image
 * loads take it), each with a lock count of 1 as PurgeableMemory gives the images' block, so ClearMemory keeps them;
 * everything the game allocates afterwards lands above them: data past 16 MiB, through every path, at game.memory 32
 * or more.
 *
 * The heap's memory is a block of PC memory (port/ultra/memory.c maps it, docs/port/NATIVE.md step 12), wherever the OS
 * puts it: the exit line and pc_heap_stats give sizes and offsets, never where it lies in the process, which differs
 * from one run to the next. */
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

/* The game heap's block: the memory the main heap manages, osMemSize less what lay below the heap on the N64
 * (PC_HEAP_OFFSET), its first byte returned and its size in *size; NULL before there is one. memory.c maps it
 * (pc_mem_map_heap, at startup); a test program gives memory of its own (port/assets/tests/support.c). */
uint8_t *pc_heap_block(uint32_t *size)
{
    return pc_mem_heap(size);
}

/* The memory the game's main heap manages on a console with mem_size bytes of RDRAM (locate_main_heap, which passes
 * osMemSize): the heap's block, its first byte returned and in *size the N64's room for the heap, mem_size less what
 * lay below the heap (no more than the block holds). */
void *pc_heap_memory(uint32_t mem_size, uint32_t *size)
{
    uint32_t block_size;
    uint8_t *block = pc_heap_block(&block_size);
    *size = mem_size > PC_HEAP_OFFSET ? mem_size - PC_HEAP_OFFSET : 0;
    if (*size > block_size)
        *size = block_size;
    return block;
}

/* Where p is as the N64 counted RDRAM: its offset in the heap's block plus PC_HEAP_OFFSET, the bytes below the heap.
 * For the logs and RUSH2049_TEST_HEAP_FLOOR. */
static uint32_t rdram_offset(const void *p)
{
    uint32_t size;
    return (uint32_t)((const uint8_t *)p - pc_heap_block(&size)) + PC_HEAP_OFFSET;
}

/* The bytes from p to the heap's end: an inflated file's room (port/ultra/pi.c); 0 when p isn't in the heap. */
uint32_t pc_heap_room(const void *p)
{
    uint32_t size;
    const uint8_t *block = pc_heap_block(&size);
    return block && (const uint8_t *)p >= block && (const uint8_t *)p < block + size ?
        (uint32_t)(block + size - (const uint8_t *)p) : 0;
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
    uint32_t mb = (uint32_t)strtoul(e, NULL, 10), size;
    uint8_t *floor = pc_heap_block(&size) + ((size_t)mb << 20) - PC_HEAP_OFFSET, *first = NULL;
    uint8_t *images = (uint8_t *)h + PC_RUNTIME_IMAGE_OFFSET, *images_end = images + PC_RUNTIME_IMAGE_SIZE;
    for (const HeapChunk *b = h->chunks; b && !first; b = b->next)   /* the free block after the handles */
        if (b->used == 0)
            first = (uint8_t *)b + CHUNK_HEADER;
    if (mb < 5 || mb > 4095 || floor + (1u << 20) > h->end || !first || first + 64 >= images - CHUNK_HEADER) {
        pc_log("heap: RUSH2049_TEST_HEAP_FLOOR=%s: the heap (to %u MiB) has no room above %u MiB; not applied", e,
               rdram_offset(h->end) >> 20, mb);
        return;
    }
    H.walking = 1;
    take(first, (uint32_t)(images - CHUNK_HEADER - first));   /* the boot data's room, to the images' header */
    take(images, PC_RUNTIME_IMAGE_SIZE);                                /* the runtime images' block */
    take(images_end + CHUNK_HEADER, (uint32_t)(floor - CHUNK_HEADER - (images_end + CHUNK_HEADER)));
    H.walking = 0;
    H.floor = floor;
    pc_log("heap: test: RUSH2049_TEST_HEAP_FLOOR=%u: the heap below %u MiB is taken; the game's blocks start there",
           mb, mb);
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
        pc_log("heap: %u bytes from %#x bytes into RDRAM (osMemSize %u MiB)", (uint32_t)(h->end - (const uint8_t *)h),
               rdram_offset(h), pc_os_mem_size() >> 20);
}

/* Whether p lies in the main heap's memory, from its header to its end (the audio heap and the others are blocks of
 * it): the audio task capture copies a region's neighbours there, as the RSP's DMA reads whole 8-byte units. */
int pc_heap_holds(const void *p)
{
    return H.heap && (const uint8_t *)p >= (const uint8_t *)H.heap && (const uint8_t *)p < H.heap->end;
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
    s->mem_size = pc_os_mem_size();
    if (!H.heap)
        return -1;
    s->base = rdram_offset(H.heap);
    s->size = (uint32_t)(H.heap->end - (const uint8_t *)H.heap);
    s->free_low = H.free_low == ~0u ? 0 : H.free_low;
    s->peak = s->size - s->free_low;
    s->largest_low = H.largest_low;
    s->allocations = H.allocations;
    s->shorts = H.shorts;
    s->nones = H.nones;
    s->floor = H.floor ? rdram_offset(H.floor) : 0;
    return 0;
}

void pc_heap_report(void)
{
    PcHeapStats s;
    if (pc_heap_stats(&s) != 0) {
        fprintf(stderr, "rush2049pc: heap: not made (the game didn't start); osMemSize %u MiB\n", s.mem_size >> 20);
        return;
    }
    char floor[48] = "";
    if (s.floor)
        snprintf(floor, sizeof floor, " (test floor at %u MiB)", s.floor >> 20);
    fprintf(stderr, "rush2049pc: heap: peak %u of %u bytes, free-low %u (largest block %u), osMemSize %u MiB, "
                    "the heap from %u bytes in; %u allocations, short %u, none %u%s\n",
            s.peak, s.size, s.free_low, s.largest_low, s.mem_size >> 20, s.base, s.allocations, s.shorts, s.nones,
            floor);
}
