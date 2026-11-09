/* PS_GetFreeFiles: a pak's free directory entries (MAT3 paksysdc.c). */
#include "game/game.h"

/* The number of pak's 16 directory entries that hold no file. */
s32 PS_GetFreeFiles(s32 pak) {
    s32 n = 0;
    s32 k;
    tPak *p = &gPaks[pak];
    for (k = 0; k < PAK_MAX_FILES; k++) {
        if (p->files[k].state.file_size == 0) {
            n++;
        }
    }
    return n;
}
