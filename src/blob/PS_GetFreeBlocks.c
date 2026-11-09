/* PS_GetFreeBlocks: a pak's free pages (MAT3 paksysdc.c). */
#include "game/game.h"

/* The free pages (PAK_BLOCK_SIZE bytes, rounded up) on pak's pak, from its last osPfsFreeBlocks. */
u32 PS_GetFreeBlocks(s32 pak) {
    return (u32) (gPaks[pak].FreeSpace + 0xFF) >> 8;
}
