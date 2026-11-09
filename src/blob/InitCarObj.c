#include "game/game.h"

VIEW(s32, unitmatrix);
#define unitmatrix unitmatrix_view

s32 ZOID_AddObject(s32 id, void *owner, s16 parent, s32 flags);

/* This wrapper ends in a call to ZOID_AddObject (MBOX_NewObject), whose result stays in v0 through the void
 * return; every caller reads it. On the PC the wrapper returns it (docs/port/PROTOTYPES.md). */
#define PC_V0_S32 s32
#define PC_V0_RETURN return
PC_V0_S32 InitCarObj(s32 id, s16 layer, s16 parent, s32 flags, s16 row, s16 col, s32 dynamic)
{
    void *owner;

    if (dynamic != 0) {
        owner = ZOID_NewMatrix();
    } else if (col == -1) {
        owner = &unitmatrix;
        flags |= 0x80;
    } else {
        owner = gCarMatrices[row / 13 * 12 + col];
    }
    PC_V0_RETURN ZOID_AddObject(id, owner, parent, ((layer << 8) ^ 0xF00) | flags);
}
