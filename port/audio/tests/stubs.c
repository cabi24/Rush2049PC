/* port/audio/tests/stubs.c: the port/ultra functions port/audio calls, for the tests (pc_port.h). */
#include <string.h>

#include "test.h"

int test_failures, test_checks;

int test_finish(const char *name)
{
    printf("%s: %d checks, %d failed\n", name, test_checks, test_failures);
    return test_failures ? 1 : 0;
}

_Alignas(16) uint8_t test_rdram[TEST_RDRAM_SIZE];
static size_t rdram_used;

void rdram_reset(void)
{
    memset(test_rdram, 0, rdram_used);
    rdram_used = 0x1000;   /* address 0x80000000 itself stays unused */
}

void *rdram_alloc(size_t size)
{
    if (!rdram_used)
        rdram_used = 0x1000;
    rdram_used = (rdram_used + 15) & ~(size_t)15;
    if (rdram_used + size > TEST_RDRAM_SIZE)
        return NULL;
    void *p = test_rdram + rdram_used;
    memset(p, 0, size);
    rdram_used += size;
    return p;
}

uint32_t rdram_addr(const void *p)
{
    return pc_n64_addr(p);
}

void *pc_n64_ptr(uint32_t addr)
{
    uint32_t off = addr & 0x1FFFFFFFu;
    return off < TEST_RDRAM_SIZE ? test_rdram + off : NULL;
}

uint32_t pc_n64_addr(const void *host)
{
    const uint8_t *p = host;
    if (p < test_rdram || p >= test_rdram + TEST_RDRAM_SIZE)
        return 0;
    return 0x80000000u + (uint32_t)(p - test_rdram);
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
