/* PS_GetBlocks: bytes to pak pages (MAT3 paksysdc.c). */
#include "game/game.h"

/* The number of pak pages (PAK_BLOCK_SIZE bytes) that bytes bytes take. */
s32 PS_GetBlocks(s32 bytes) {
    return (s32) (bytes + 0xFF) >> 8;
}
