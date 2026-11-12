/* port/assets/tests/support.h: helpers for the converter tests (support.c). */
#ifndef PC_ASSETS_TEST_SUPPORT_H
#define PC_ASSETS_TEST_SUPPORT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "pc_port.h"

size_t test_be_range_count(void);   /* ranges registered with pc_mem_register_be */
void test_be_reset(void);
size_t test_model_data_count(void); /* ranges registered with pc_mem_register_model_data (the WADs' vertices) */
extern int test_mx_voices;          /* [audio] voices: pc_mx_voices() (32) */
extern unsigned long long test_snd_count[];   /* pc_snd_count's, by PC_SND_* */
extern int test_snd_msg_most;       /* pc_snd_msg_taken's high-water mark */
extern int test_race_cars;          /* the setting and the race's count: pc_cars_setting(), pc_race_cars() (6) */
extern int test_race_begins;        /* how many times the game C called pc_race_cars_begin() */
/* pc_pool_overflow (pc_game_types.h) calls this when a test sets it; otherwise a pool past its end fails the test. */
extern void (*test_pool_overflow)(const char *pool, int used, int capacity);
/* The tests' memory: one block of host memory, test_memory_bytes long (the N64's 8 MiB; a test of game.memory sets
 * more before the first call), aligned to TEST_MEMORY_ALIGN as port/ultra's heap block is, zeroed, at no fixed place:
 * the game heap's memory (pc_heap_memory, pc_mem_heap: from PC_HEAP_OFFSET, the heap's place in the N64's RDRAM, so the
 * heap has the N64's size and alignment) and what a test plants for the game's code (support.c). */
#define TEST_MEMORY_ALIGN 0x10000u
extern uint32_t test_memory_bytes;
uint8_t *test_memory(void);

/* The host offset in a model record (struct pc_model, port/include/pc_game_types.h) of MAT3's (the N64's) offset n,
 * for tests that name the record's fields by MAT3's offsets: its five pointers are 8 bytes on the PC (car and tune,
 * the four lasttp, torquecurve aligned to 8, movingSurface), so a field after one lies further on. */
#define MODEL_OFS(n) ((n) < 0x4 ? (n) : (n) < 0x8 ? (n) + 4 : (n) < 0x3A0 ? (n) + 8 : \
                      (n) < 0x3B0 ? 0x3A8 + ((n) - 0x3A0) * 2 : (n) < 0x40C ? (n) + 0x18 : (n) < 0x410 ? 0x428 : \
                      (n) < 0x648 ? (n) + 0x20 : (n) < 0x64C ? 0x668 : (n) + 0x24)

/* Writing big-endian test files. */
static inline void test_put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static inline void test_put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v;
}

static int test_failures;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond);   \
            test_failures++;                                                           \
        }                                                                              \
    } while (0)
#define TEST_DONE() (test_failures ? (fprintf(stderr, "%d failed\n", test_failures), 1) : 0)

#endif
