/* pak_get_free_bytes: a pak's free bytes (the N64's form of MAT3 paksysdc.c's PS_GetFreeSpace). */
#include "game/game.h"

/* The free bytes on pak's pak, from its last osPfsFreeBlocks. */
s32 pak_get_free_bytes(s32 pak) {
    return gPaks[pak].FreeSpace;
}
