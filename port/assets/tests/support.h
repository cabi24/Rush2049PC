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
extern int test_race_cars;          /* the setting: pc_cars_setting(), and pc_race_cars() after pc_race_cars_begin() (6) */
/* The RDRAM window at PC_RDRAM_BASE, mapped as port/ultra maps it (NULL if that address is taken), for tests whose
 * files must hold 32-bit window addresses as the game's heap blocks do. */
uint8_t *test_rdram(void);
/* Puts back the GOT slots of symbols a test defines at absolute N64 addresses (`.set name, 0x8014A250`), as
 * rush2049pc does first thing (port/ultra/absfix.c): game data that sits at its N64 address in the window. */
int pc_fix_absolute_got(void);

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
