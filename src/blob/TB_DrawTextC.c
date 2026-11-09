#include "game/game.h"

void TB_DrawTextC(s16 arg0, s16 arg1, s32 arg2, void *arg3) {
    TB_SetTextColor(arg2);
    TB_DrawText(arg0, (s32) arg1, arg3);
}
