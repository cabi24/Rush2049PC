#include "game/game.h"

void WalkBST(void *arg0, s32 (*arg1)(void *, s32)) {
    s32 sp1C;

    sp1C = 0;
    WalkBSTInternal((intptr_t) arg0, M2C_FIELD(arg0, void **, 8), &sp1C, arg1);
}
