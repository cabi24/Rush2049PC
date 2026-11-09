#include "game/game.h"

/* PC: arg (the request list) and aux (its done queue) are pointers: sample_dma_next.c's DmaList, in its
 * own storage (docs/port/MUSYX_DATA.md 8.1) */
typedef struct { s8 active; s8 pad1; s16 id; s16 count; s16 pad6; intptr_t arg; intptr_t aux; } CallSlot;
#define sample_dma_list pc_sample_dma_list
extern CallSlot sample_dma_list;

s32 musyx_start_dma_list(intptr_t arg0, s32 arg1, intptr_t arg2) {
    if (arg1 != 0) {
        sample_dma_list.arg = arg0;
        sample_dma_list.id = arg1;
        sample_dma_list.aux = arg2;
        sample_dma_list.count = 0;
        sample_dma_list.active = 1;
        sample_dma_next();
    }
    return 1;
}
