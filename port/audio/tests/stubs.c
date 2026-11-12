/* port/audio/tests/stubs.c: the port/ultra functions port/audio calls, for the tests (pc_port.h). */
#include <string.h>

#include "test.h"

int test_failures, test_checks;

int test_finish(const char *name)
{
    printf("%s: %d checks, %d failed\n", name, test_checks, test_failures);
    return test_failures ? 1 : 0;
}

_Alignas(16) uint8_t test_heap[TEST_HEAP_SIZE];
static size_t heap_used;

void heap_reset(void)
{
    memset(test_heap, 0, heap_used);
    heap_used = 0x1000;   /* the first 4 KiB stay unused, as the game heap's header and first chunks are */
}

void *heap_alloc(size_t size)
{
    if (!heap_used)
        heap_used = 0x1000;
    heap_used = (heap_used + 15) & ~(size_t)15;
    if (heap_used + size > TEST_HEAP_SIZE)
        return NULL;
    void *p = test_heap + heap_used;
    memset(p, 0, size);
    heap_used += size;
    return p;
}

/* The capture (musyx.c's cap_region) asks whether a region is in the game's heap, to copy its neighbours as the RSP
 * reads them: here the fake heap is. */
int pc_heap_holds(const void *p)
{
    return (const uint8_t *)p >= test_heap && (const uint8_t *)p < test_heap + TEST_HEAP_SIZE;
}

static struct {
    const uint8_t *start;
    size_t size;
} be_ranges[64];

void pc_mem_register_be(const void *start, size_t size)
{
    for (int i = 0; i < 64; i++) {
        if (!be_ranges[i].start) {
            be_ranges[i].start = start;
            be_ranges[i].size = size;
            return;
        }
    }
}

void pc_mem_unregister_be(const void *start)
{
    for (int i = 0; i < 64; i++)
        if (be_ranges[i].start == start)
            be_ranges[i].start = NULL;
}

int pc_mem_is_be(const void *p)
{
    const uint8_t *b = p;
    for (int i = 0; i < 64; i++)
        if (be_ranges[i].start && b >= be_ranges[i].start && b < be_ranges[i].start + be_ranges[i].size)
            return 1;
    return 0;
}

static const uint8_t *rom_bytes;
static size_t rom_size;

void test_set_rom(const uint8_t *rom, size_t size)
{
    rom_bytes = rom;
    rom_size = size;
}

const uint8_t *pc_rom_bytes(void)
{
    return rom_bytes;
}

size_t pc_rom_size(void)
{
    return rom_size;
}
