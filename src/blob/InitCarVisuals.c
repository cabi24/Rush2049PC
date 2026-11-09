#include "game/game.h"

/* PC: the car's 21 visuals are 0x20 bytes (struct pc_visual): func is 8 bytes at +0x18, objnum at +0xA. */
void InitCarVisuals(intptr_t arg0) {
    s16 i;
    struct pc_visual *v;

    v = (struct pc_visual *) ((s8 *) arg0 + 0x110);
    for (i = 0; i < 0x15; i++) {
        v[i].func = 0;
        v[i].objnum = -1;
    }
}

