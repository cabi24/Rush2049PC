/* port/assets/tests/support.c: what the converters call from port/ultra, for test programs that link them alone.
 * A small registry of texel memory with the same rules as port/ultra/memory.c (overlapping ranges merge; unregister
 * drops the range that contains the address); sample memory, which only the sound library registers, is a no-op. */
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pc_game_types.h"
#include "support.h"

static struct { uintptr_t start, end; } g_be[256];
static size_t g_nbe;

void pc_mem_register_texels(const void *start, size_t size)
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

void pc_mem_unregister_texels(const void *start)
{
    uintptr_t a = (uintptr_t)start;
    for (size_t i = 0; i < g_nbe; i++)
        if (g_be[i].start <= a && a < g_be[i].end) {
            memmove(&g_be[i], &g_be[i + 1], (g_nbe - i - 1) * sizeof g_be[0]);
            g_nbe--;
            return;
        }
}

int pc_mem_is_texel_memory(const void *p)
{
    uintptr_t a = (uintptr_t)p;
    for (size_t i = 0; i < g_nbe; i++)
        if (g_be[i].start <= a && a < g_be[i].end)
            return 1;
    return 0;
}

size_t test_texel_range_count(void) { return g_nbe; }

void test_texel_reset(void) { g_nbe = 0; }

void pc_mem_register_samples(const void *start, size_t size) { (void)start, (void)size; }
void pc_mem_unregister_samples(const void *start) { (void)start; }
int pc_mem_is_sample_memory(const void *p) { (void)p; return 0; }

/* The models' vertices the WAD converter registers (port/ultra/memory.c's model data): the ranges, unsorted. */
static struct { uintptr_t start, end; } g_model_data[64];
static size_t g_nmodel_data;

void pc_mem_register_model_data(const void *start, size_t size)
{
    if (g_nmodel_data == sizeof g_model_data / sizeof g_model_data[0])
        abort();
    g_model_data[g_nmodel_data].start = (uintptr_t)start;
    g_model_data[g_nmodel_data++].end = (uintptr_t)start + size;
}

void pc_mem_unregister_model_data(const void *start)
{
    for (size_t i = 0; i < g_nmodel_data; i++)
        if (g_model_data[i].start <= (uintptr_t)start && (uintptr_t)start < g_model_data[i].end) {
            g_model_data[i] = g_model_data[--g_nmodel_data];
            return;
        }
}

int pc_mem_is_model_data(const void *p)
{
    for (size_t i = 0; i < g_nmodel_data; i++)
        if (g_model_data[i].start <= (uintptr_t)p && (uintptr_t)p < g_model_data[i].end)
            return 1;
    return 0;
}

size_t test_model_data_count(void) { return g_nmodel_data; }

/* The cars a race holds (pc_game_types.h), for tests that build game C reading PC_RACE_CARS: the N64's 6 unless the
 * test sets test_race_cars, which is both the setting (pc_cars_setting) and the race's count (pc_race_cars), so a
 * test sets the count and calls a loop without a race's start. pc_race_cars_begin latches the setting into the count
 * (port/config/race_cars.c's rule), here the same number; test_race_begins counts its calls. */
int test_race_cars = 6;
int test_race_begins;
void pc_race_cars_begin(void) { test_race_begins++; }
int pc_cars_setting(void) { return test_race_cars; }
int pc_race_cars(void) { return test_race_cars; }

/* game.frame_rate (pc_game_fps.h; the game's is port/config/game_fps.c), for tests that build game C stepping per
 * frame: original. A test that links port/config/game_fps.c (assets_fps) has the game's own instead: PC_TEST_GAME_FPS.
 * Not weak symbols: a PE executable (mingw-w64) resolves no reference to a weak definition of data. */
#ifndef PC_TEST_GAME_FPS
volatile int pc_fps60;
int pc_frame_steps = 1;
float pc_frame_scale = 1.0f;
void pc_game_fps_begin(void) {}
void pc_game_frame_note(int retraces) { (void)retraces; }
float pc_fps_filter(float e) { return e; }   /* not libm: not every test links it */
#endif

/* [audio] voices and engines (pc_game_types.h; the game's is port/config/voices.c), for tests that build the game's
 * sound code: test_mx_voices voices (the N64's 32 unless a test sets more), the engines at original, the counters
 * kept. A test that links port/config/voices.c (assets_voices) has the game's own instead: PC_TEST_VOICES. */
int test_mx_voices = 32;
unsigned long long test_snd_count[PC_SND_COUNTERS];
int test_snd_msg_most;
#ifndef PC_TEST_VOICES
int pc_mx_voices(void) { return test_mx_voices; }
int pc_snd_messages(void) { return test_mx_voices == 32 ? PC_SND_N64_MSGS : PC_SND_MSG_SLOTS; }
void pc_snd_setup(unsigned char *voices, unsigned char *sfx, int *emitters) { (void)voices, (void)sfx, (void)emitters; }
void pc_snd_count(int what) { if (what >= 0 && what < PC_SND_COUNTERS) test_snd_count[what]++; }
void pc_snd_msg_taken(int index) { if (index + 1 > test_snd_msg_most) test_snd_msg_most = index + 1; }
void pc_snd_most(int what, int value) { (void)what, (void)value; }
int pc_engines(void) { return PC_ENGINES_ORIGINAL; }
int pc_engines_choosing(void) { return 0; }
void pc_engines_note(int handle, int listener, float distance) { (void)handle, (void)listener, (void)distance; }
void pc_engines_choose(int listeners) { (void)listeners; }
float pc_engine_gain(int handle, float gain) { (void)handle; return gain; }
void pc_engine_state(int handle, int state) { (void)handle, (void)state; }
void pc_snd_walk_begin(void) {}
int pc_snd_over_budget(unsigned int tag, int rank, float budget, int audible)
{
    (void)tag, (void)audible;
    return (float)rank >= budget;
}
#endif

/* A pool past its end (pc_game_types.h; the game's is port/ultra/pool_overflow.c), for tests that build game C with a
 * checked pool: the test's handler if it set one (assets_pools), else the test fails. */
void (*test_pool_overflow)(const char *pool, int used, int capacity);
void pc_pool_overflow(const char *pool, int used, int capacity)
{
    if (test_pool_overflow != NULL) {
        test_pool_overflow(pool, used, capacity);
        return;
    }
    fprintf(stderr, "test: %s ran past its end: %d of %d\n", pool, used, capacity);
    abort();
}

/* The tests' memory (support.h): one block, made the first time it is asked for. */
uint32_t test_memory_bytes = 8u << 20;   /* the N64's 8 MiB */
static uint8_t *g_memory;

uint8_t *test_memory(void)
{
    static uint8_t *block;
    if (!g_memory) {
        block = calloc(1, (size_t)test_memory_bytes + TEST_MEMORY_ALIGN);
        if (!block) {
            fprintf(stderr, "test: no %u bytes of memory\n", test_memory_bytes);
            abort();
        }
        g_memory = (uint8_t *)(((uintptr_t)block + TEST_MEMORY_ALIGN - 1) & ~(uintptr_t)(TEST_MEMORY_ALIGN - 1));
    }
    return g_memory;
}

/* port/ultra/memory.c's game heap block (which port/ultra/heap.c reads): the tests' memory past what lies below the
 * heap on the N64, to its end. (Not a weak definition: mingw's linker doesn't take one for another object's
 * reference.) */
uint8_t *pc_mem_heap(uint32_t *size)
{
    *size = test_memory_bytes - PC_HEAP_OFFSET;
    return test_memory() + PC_HEAP_OFFSET;
}

/* The game heap's memory and reports; a test that links port/ultra/heap.c instead (assets_heap) is built with
 * TEST_HEAP_C. The memory is heap.c's: the tests' memory past what lies below the heap on the N64 (the game image,
 * low memory), to mem_size, so the heap has the N64's size and alignment. */
#ifndef TEST_HEAP_C
void *pc_heap_memory(uint32_t mem_size, uint32_t *size)
{
    *size = mem_size > PC_HEAP_OFFSET ? mem_size - PC_HEAP_OFFSET : 0;
    return test_memory() + PC_HEAP_OFFSET;
}
void pc_heap_created(void *heap) { (void)heap; }
void pc_heap_allocated(void *heap) { (void)heap; }
void pc_heap_none(void *heap, uint32_t size, uint32_t free_bytes, uint32_t largest)
{
    fprintf(stderr, "test: the game heap has no free block of %u bytes (%u free, %u the largest)\n", size, free_bytes,
            largest);
}
void pc_heap_short(const char *what, uint32_t need) { (void)what, (void)need; }
#endif

/* port/ultra's log, for what heap.c and its callers print. */
int pc_verbose;
void pc_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "test: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}
uint32_t pc_os_mem_size(void) { return test_memory_bytes; }
/* video.detail (port/config/detail.c): SetupWaveModel's player count for its LOD markers, as the N64 has it. */
int pc_detail_players(int players) { return players; }
