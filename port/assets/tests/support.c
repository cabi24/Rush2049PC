/* port/assets/tests/support.c: what the converters call from port/ultra, for test programs that link them alone.
 * A small big-endian range registry with the same rules as port/ultra/memory.c (overlapping ranges merge;
 * unregister drops the range that contains the address). */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "support.h"

static struct { uintptr_t start, end; } g_be[256];
static size_t g_nbe;

void pc_mem_register_be(const void *start, size_t size)
{
    uintptr_t s = (uintptr_t)start, e = s + size;
    size_t i, j, k;
    if (!size)
        return;
    for (i = 0; i < g_nbe && g_be[i].end <= s; i++)
        ;
    for (j = i; j < g_nbe && g_be[j].start < e; j++) {
        if (g_be[j].start < s)
            s = g_be[j].start;
        if (g_be[j].end > e)
            e = g_be[j].end;
    }
    if (j == i) {
        if (g_nbe == sizeof g_be / sizeof g_be[0])
            abort();
        memmove(&g_be[i + 1], &g_be[i], (g_nbe - i) * sizeof g_be[0]);
        g_nbe++;
    } else {
        k = j - i - 1;   /* ranges merged into g_be[i] */
        memmove(&g_be[i + 1], &g_be[j], (g_nbe - j) * sizeof g_be[0]);
        g_nbe -= k;
    }
    g_be[i].start = s;
    g_be[i].end = e;
}

void pc_mem_unregister_be(const void *start)
{
    uintptr_t a = (uintptr_t)start;
    for (size_t i = 0; i < g_nbe; i++)
        if (g_be[i].start <= a && a < g_be[i].end) {
            memmove(&g_be[i], &g_be[i + 1], (g_nbe - i - 1) * sizeof g_be[0]);
            g_nbe--;
            return;
        }
}

int pc_mem_is_be(const void *p)
{
    uintptr_t a = (uintptr_t)p;
    for (size_t i = 0; i < g_nbe; i++)
        if (g_be[i].start <= a && a < g_be[i].end)
            return 1;
    return 0;
}

size_t test_be_range_count(void) { return g_nbe; }

void test_be_reset(void) { g_nbe = 0; }

/* The cars a race holds (pc_game_types.h), for tests that build game C reading PC_RACE_CARS: the N64's 6 unless the
 * test sets test_race_cars. */
int test_race_cars = 6;
int pc_race_cars(void) { return test_race_cars; }

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

uint8_t *test_rdram(void)
{
    static uint8_t *w;
    static int tried;
    void *want = (void *)(uintptr_t)PC_RDRAM_BASE, *p;

    if (tried)
        return w;
    tried = 1;
    p = mmap(want, PC_RDRAM_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == want)
        w = (uint8_t *)p;
    else if (p != MAP_FAILED)
        munmap(p, PC_RDRAM_SIZE);
    return w;
}
