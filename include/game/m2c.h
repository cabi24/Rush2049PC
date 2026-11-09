/* include/game/m2c.h: what the decompiler (m2c) wrote into the code it made and the code still uses. Each use is a
 * place the per-area cleanup has yet to make plain C: a struct field instead of M2C_FIELD's raw offset, a real type
 * instead of M2C_UNK, the instruction m2c couldn't express instead of M2C_ERROR. */
#ifndef GAME_M2C_H
#define GAME_M2C_H

#include "game/basic.h"

/* A value whose type m2c didn't find (a 32-bit word on the N64). */
typedef s32 M2C_UNK;

/* The field at a raw byte offset of a structure: `*(type_ptr)((s8 *)expr + offset)`. */
#define M2C_FIELD(expr, type_ptr, offset) (*(type_ptr)((s8 *)(expr) + (offset)))

/* An instruction m2c could not translate; 0 on the PC. */
#define M2C_ERROR(desc) (0)

/* The constants its context gave the code it wrote (the game's files include no C library header). */
/* The original's debug print, compiled out on the N64 (DEBUG_PRINT(("format", ...))). */
#define DEBUG_PRINT(args)

#define NULL ((void *)0)
#define TRUE 1
#define FALSE 0

#endif /* GAME_M2C_H */
