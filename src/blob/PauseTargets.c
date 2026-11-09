#include "game/game.h"

typedef struct {
    u16 unk00;
    u16 unk02;
    u8 pad04[0x60 - 0x04];
    s32 unk60;
    s32 unk64;
} TargetListNode;

VIEW(s32, gTargetList);
#define gTargetList gTargetList_view

/* PC: the head (initTargetStructs.c's Pool, host +0x18) is the active list of the level-object node pool gTargetList,
 * whose nodes keep their N64 layout in the arena block gTargetArray and link each other by 32-bit window addresses
 * (the pool's n64 flag; docs/port/POINTERS.md 10). Read as a pointer, the link would take the node's flags at +4 as
 * its upper half. */
#define NODE_NEXT(v) ((TargetListNode *) PC_N64P(M2C_FIELD(v, u32 *, 0)))
void PauseTargets(void) {
    struct { u8 b0, n64, pad2[2]; s32 count, size; u8 *mem; TargetListNode *head; u8 *free; } *pool = (void *) &gTargetList;
    TargetListNode *var_v0;

    var_v0 = pool->head;
    if (var_v0 != NULL) {
        do {
            M2C_FIELD(var_v0, s8 *, 0x64) = 2;
            var_v0->unk60 = -1;
            var_v0 = NODE_NEXT(var_v0);
        } while (var_v0 != NULL);
    }
}
