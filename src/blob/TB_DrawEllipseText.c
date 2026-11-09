#include "game/game.h"

void TB_DrawEllipseText(s16 arg0, s16 arg1, s16 arg2, void *arg3) {
    u8 buf[0x80];

    TB_EllipseText(buf, arg3, arg2);
    TB_DrawText(arg0, (s32) arg1, buf);
}

