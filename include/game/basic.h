/* include/game/basic.h: the game's integer and floating-point types (uintptr_t and intptr_t come from the prelude,
 * port/include/pc_prelude.h), and EXTENT */
#ifndef GAME_BASIC_H
#define GAME_BASIC_H

typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;

/* Volatile forms, for data another thread or an interrupt changes. */
typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;
typedef volatile s8 vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;
typedef volatile s64 vs64;

/* EXTENT(n): the number of elements of a datum's array as the data loader lays it out. Only the data loader's
 * generator sees it (port/data/game_decls.c defines GAME_DECLS_EXTENTS); the game's files see the array open-ended,
 * `T x[]`, as they always did: much of the game's C indexes a table past the end it declares, into the data after
 * it, so the compiler must not take the bound for the truth (the reason for -fno-aggressive-loop-optimizations). */
#ifdef GAME_DECLS_EXTENTS
#define EXTENT(n) n
#else
#define EXTENT(n)
#endif

#endif /* GAME_BASIC_H */
