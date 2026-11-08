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
extern int test_mx_voices;          /* [audio] voices: pc_mx_voices() (32) */
extern unsigned long long test_snd_count[];   /* pc_snd_count's, by PC_SND_* */
extern int test_snd_msg_most;       /* pc_snd_msg_taken's high-water mark */
extern int test_race_cars;          /* the setting and the race's count: pc_cars_setting(), pc_race_cars() (6) */
extern int test_race_begins;        /* how many times the game C called pc_race_cars_begin() */
/* pc_pool_overflow (pc_game_types.h) calls this when a test sets it; otherwise a pool past its end fails the test. */
extern void (*test_pool_overflow)(const char *pool, int used, int capacity);
/* The RDRAM window at PC_RDRAM_BASE, mapped as port/ultra maps it (NULL if that address is taken), for tests whose
 * files must hold 32-bit window addresses as the game's heap blocks do. test_rdram_bytes is its size (PC_RDRAM_SIZE,
 * the N64's; a test of game.memory sets more before the first call), which pc_rdram_size() and support.c's
 * pc_n64_ptr and pc_n64_addr follow. */
extern uint32_t test_rdram_bytes;
uint8_t *test_rdram(void);
/* Puts back the GOT slots of symbols a test defines at absolute N64 addresses (TEST_ABS below), as rush2049pc does
 * first thing (port/ultra/absfix.c): game data that sits at its N64 address in the window. */
int pc_fix_absolute_got(void);

/* ---------------------------------------------------------------- the window's places */

/* Game data a test places at its N64 address in its window, as the game's data arenas put it (port/data), for game
 * code that keeps the datum's address in a u32 or reaches it by that address. This is the N64's memory map: wave 3 of
 * docs/port/PCONLY.md gives the game's data PC storage, and these go. The addresses are the symbol file's
 * (symbol_addrs.us.txt), written as plain numbers because the assembler takes them as they are written.
 *
 *   TEST_ABS(name)          defines the game's symbol at its address (a `.set`, which the test can't take a GOT
 *                           reference to: the test reaches the datum through TEST_AT)
 *   TEST_ABS_IN(name, of, n)  defines a name n bytes into datum `of` (a field the game names on its own)
 *   TEST_N64(name)          its N64 address (u32), TEST_AT(name) the same as a host pointer into the window */
#define TEST_N64_gCarParts 0x80139320           /* the car parts per car, 0x40 bytes a car */
#define TEST_N64_gCheckpoints 0x80151CE8        /* the checkpoint block */
#define TEST_N64_gTune 0x801116D0               /* the tune table, 44 bytes an entry */
#define TEST_N64_stdtorquecurve 0x8011F724      /* the torque curve */
#define TEST_N64_D_8011F814 0x8011F814          /* the tire records after it */
#define TEST_N64_gObjList_biased 0x8014254C     /* an id in the object list */
#define TEST_N64_gTargetArray 0x8014D280        /* the battle and target nodes, 130 of 0x70 bytes */
#define TEST_N64_D_80118C10 0x80118C10          /* the level objects' bodies and static states */
#define TEST_N64_skyMatrices 0x80124FD0         /* each window's sky matrix, a u32 window address */
#define TEST_N64_gCamera 0x80150B70             /* Camera[], 0x98 bytes each */
#define TEST_N64_ghost_file_handles 0x8013F1F8  /* the N64's ghost file handle slots */
#define TEST_N64_ghost_file_records 0x8013F390  /* and records */
#define TEST_N64_game_bss_end 0x8017A640        /* the end of the game's BSS: the heap starts there */
#define TEST_N64_osMemSize 0x80000318           /* libultra's boot parameter (N64_BOOT_PARAMS + 0x18) */
#define TEST_STR_(x) #x
#define TEST_STR(x) TEST_STR_(x)
#define TEST_ABS(name) __asm__(".globl " #name "\n.set " #name ", " TEST_STR(TEST_N64_##name))
#define TEST_ABS_IN(name, of, n) __asm__(".globl " #name "\n.set " #name ", " TEST_STR(TEST_N64_##of) " + " #n)
#define TEST_N64(name) ((uint32_t)TEST_N64_##name)
#define TEST_AT(name) ((uint8_t *)(uintptr_t)TEST_N64_##name)

/* Scratch places in the window for what a test plants there and the game code reaches by a u32 window address (heap
 * blocks, tables, packets): from 3 MiB on, past where the game's data would be, as the game heap is. */
#define TEST_SCRATCH (PC_RDRAM_BASE + 0x300000u)

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
