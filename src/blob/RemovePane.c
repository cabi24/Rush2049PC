#include "game/game.h"

extern void RemoveBlit(intptr_t sound_id);

/* PC: a pane group (FiveParts, InitPane.c) keeps its five blits pointer-sized from +0x10; the m2c body
 * reads them as s32 at the N64 offsets (+0xC, 4 apart). */
typedef struct { s8 active; u8 own; u8 rgba1[4]; u8 rgba2[4]; u8 other10[2]; void *part[5]; } PcFiveParts;
void RemovePane(s8 *arg0) {
    PcFiveParts *group = (PcFiveParts *) arg0;
    s32 i;

    if (group != NULL && group->active != 0) {
        for (i = 0; i < 5; i++) {
            if (group->part[i] != NULL) {
                RemoveBlit((intptr_t) group->part[i]);
                group->part[i] = NULL;
            }
        }
        group->active = 0;
    }
}
